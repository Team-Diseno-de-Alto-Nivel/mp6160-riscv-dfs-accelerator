// Batching experiment for dfs_accel invocation overhead (#92 follow-up, not
// wired into any make target -- exploratory, run manually).
//
// #92 found a ~12,000 ns floor per accelerator invocation on the KV260,
// regardless of problem size, and traced it to the fixed cost of talking to
// the accelerator (AXI-lite register writes, interrupt round trip, result
// DMA) rather than the DFS work itself. This tool checks how much of that
// floor is the DMA readback specifically, by running the SAME case N times
// back-to-back two ways and comparing:
//
//   individual: Start -> Sync -> DMA-readback-result, every iteration --
//               exactly what benchmark_cynq.cpp already does.
//   batched:    Start -> Sync every iteration (can't skip either of
//               these -- dfs_accel's top module is NOT pipelined per
//               csynth.rpt, so a new Start() isn't safe before the
//               current run's AP_DONE), but the DMA readback of the
//               result is skipped on every iteration except the last.
//
// If "batched" comes in meaningfully faster per call than "individual", the
// DMA readback is a real chunk of the floor and worth optimizing (e.g. only
// reading results back once per batch in a real deployment). If it's about
// the same, the floor is dominated by the interrupt wait instead, and this
// isn't the lever to pull.
//
// Usage: ./benchmark_batch_cynq <bitstream.bit> <cases.json> <case_name> [iterations] [warmup]

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <cynq/cynq.hpp>

#include "dfs_accel_case_io.h"

using namespace cynq;  // NOLINT

namespace {

double ElapsedNs(std::chrono::steady_clock::time_point t0,
                 std::chrono::steady_clock::time_point t1) {
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

void PrintStats(const std::string& label, const std::vector<double>& samples) {
  std::vector<double> sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  double mean = 0.0;
  for (double v : samples) mean += v;
  mean /= static_cast<double>(samples.size());
  const size_t mid = sorted.size() / 2;
  const double median = (sorted.size() % 2 == 0)
                            ? (sorted[mid - 1] + sorted[mid]) / 2.0
                            : sorted[mid];
  std::printf("%-12s min=%.0f median=%.0f max=%.0f mean=%.0f (ns, n=%zu)\n",
             label.c_str(), sorted.front(), median, sorted.back(), mean,
             samples.size());
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4 || argc > 6) {
    std::fprintf(stderr,
                 "usage: %s <bitstream.bit> <cases.json> <case_name> "
                 "[iterations=50] [warmup=5]\n",
                 argv[0]);
    return 1;
  }
  const std::string bitstream = argv[1];
  const std::string cases_path = argv[2];
  const std::string case_name = argv[3];
  const int iterations = argc > 4 ? std::atoi(argv[4]) : 50;
  const int warmup = argc > 5 ? std::atoi(argv[5]) : 5;
  if (iterations <= 0) {
    std::fprintf(stderr, "iterations must be > 0\n");
    return 1;
  }

  std::ifstream f(cases_path);
  if (!f) {
    std::fprintf(stderr, "cannot open %s\n", cases_path.c_str());
    return 1;
  }
  std::stringstream ss;
  ss << f.rdbuf();

  std::vector<tinyjson::Case> cases;
  try {
    cases = tinyjson::Parser(ss.str()).ParseCases();
  } catch (const std::exception& e) {
    std::fprintf(stderr, "cases.json parse error: %s\n", e.what());
    return 1;
  }

  const tinyjson::Case* c = nullptr;
  for (const auto& cand : cases) {
    if (cand.name == case_name) {
      c = &cand;
      break;
    }
  }
  if (!c) {
    std::fprintf(stderr, "case '%s' not found in %s\n", case_name.c_str(),
                cases_path.c_str());
    return 1;
  }
  std::printf("Case: %s/%s (%ux%u, %u words)\n", c->algorithm.c_str(),
             c->name.c_str(), c->rows, c->cols, c->num_words);

  std::printf("----- Initialising platform -----\n");
  auto platform = IHardware::Create(HardwareArchitecture::UltraScale, bitstream);
  auto clocks = platform->GetClocks();
  for (size_t i = 0; i < clocks.size(); ++i) clocks[i] = (i == 0) ? 200.f : -1.f;
  platform->SetClocks(clocks);
  const auto confirmed_clocks = platform->GetClocks();
  std::printf("Clock 0 after SetClocks: %.3f MHz\n",
             confirmed_clocks.empty() ? 0.0 : confirmed_clocks[0]);

  auto accel = platform->GetAccelerator(kAccelAddr);
  auto mover = platform->GetDataMover(kAccelAddr);

  auto grid_buf = mover->GetBuffer(c->grid.size(), 0, MemoryType::Cacheable);
  auto words_buf = mover->GetBuffer(c->words.size(), 0, MemoryType::Cacheable);
  auto result_buf = mover->GetBuffer(4 * sizeof(uint32_t), 0, MemoryType::Cacheable);

  uint8_t* grid_host = grid_buf->HostAddress<uint8_t>().get();
  for (size_t i = 0; i < c->grid.size(); ++i) grid_host[i] = static_cast<uint8_t>(c->grid[i]);
  uint8_t* words_host = words_buf->HostAddress<uint8_t>().get();
  for (size_t i = 0; i < c->words.size(); ++i) words_host[i] = static_cast<uint8_t>(c->words[i]);
  grid_buf->Sync(SyncType::HostToDevice);
  words_buf->Sync(SyncType::HostToDevice);

  accel->Attach(kAddrGridIn, grid_buf);
  accel->Attach(kAddrWordsIn, words_buf);
  accel->Attach(kAddrResultOut, result_buf);

  // Must outlive every Start() call -- CYNQ re-reads these pointers each
  // time, not just at Attach() (same gotcha documented in benchmark_cynq.cpp).
  uint32_t rows = c->rows, cols = c->cols, mode = c->mode, flags = c->flags,
           num_words = c->num_words;
  accel->Attach(kAddrRows, &rows, RegisterAccess::WO);
  accel->Attach(kAddrCols, &cols, RegisterAccess::WO);
  accel->Attach(kAddrMode, &mode, RegisterAccess::WO);
  accel->Attach(kAddrFlags, &flags, RegisterAccess::WO);
  accel->Attach(kAddrNumWords, &num_words, RegisterAccess::WO);

  for (int i = 0; i < warmup; ++i) {
    accel->Start(StartMode::Once);
    accel->Sync();
    result_buf->Sync(SyncType::DeviceToHost);
  }

  // Pass A: individual -- Start, Sync, DMA-readback, every iteration.
  std::vector<double> individual;
  individual.reserve(iterations);
  RunResult last_individual{};
  for (int i = 0; i < iterations; ++i) {
    const auto t0 = std::chrono::steady_clock::now();
    accel->Start(StartMode::Once);
    accel->Sync();
    result_buf->Sync(SyncType::DeviceToHost);
    const auto t1 = std::chrono::steady_clock::now();
    individual.push_back(ElapsedNs(t0, t1));
    const uint32_t* r = result_buf->HostAddress<uint32_t>().get();
    last_individual = {r[0], r[1], r[2], r[3]};
  }

  // Pass B: batched -- Start+Sync every iteration (can't pipeline, see file
  // header), DMA-readback only on the last one.
  const auto tb0 = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    accel->Start(StartMode::Once);
    accel->Sync();
    if (i == iterations - 1) result_buf->Sync(SyncType::DeviceToHost);
  }
  const auto tb1 = std::chrono::steady_clock::now();
  const double batched_total_ns = ElapsedNs(tb0, tb1);
  const double batched_per_call_ns = batched_total_ns / iterations;
  const uint32_t* rb = result_buf->HostAddress<uint32_t>().get();
  const RunResult last_batched{rb[0], rb[1], rb[2], rb[3]};

  // Pass C: breakdown -- same individual pattern as Pass A, but with a
  // timestamp between each step, so the ~285,000 ns floor splits into
  // "tell it to start" (AXI-lite Start register write) vs. "wait for it to
  // finish" (interrupt round trip) vs. DMA readback (already known to be a
  // ~1% share from Pass A/B above, kept here as a cross-check).
  std::vector<double> start_ns, sync_ns, dma_ns;
  start_ns.reserve(iterations);
  sync_ns.reserve(iterations);
  dma_ns.reserve(iterations);
  for (int i = 0; i < iterations; ++i) {
    const auto t0 = std::chrono::steady_clock::now();
    accel->Start(StartMode::Once);
    const auto t1 = std::chrono::steady_clock::now();
    accel->Sync();
    const auto t2 = std::chrono::steady_clock::now();
    result_buf->Sync(SyncType::DeviceToHost);
    const auto t3 = std::chrono::steady_clock::now();
    start_ns.push_back(ElapsedNs(t0, t1));
    sync_ns.push_back(ElapsedNs(t1, t2));
    dma_ns.push_back(ElapsedNs(t2, t3));
  }

  std::printf("\n");
  PrintStats("individual", individual);
  std::printf("%-12s total=%.0f per_call=%.0f (ns, n=%d)\n", "batched",
             batched_total_ns, batched_per_call_ns, iterations);
  std::printf("\n--- breakdown (Pass C) ---\n");
  PrintStats("start()", start_ns);
  PrintStats("sync()", sync_ns);
  PrintStats("dma", dma_ns);

  std::vector<double> sorted_individual = individual;
  std::sort(sorted_individual.begin(), sorted_individual.end());
  const size_t mid = sorted_individual.size() / 2;
  const double individual_median =
      (sorted_individual.size() % 2 == 0)
          ? (sorted_individual[mid - 1] + sorted_individual[mid]) / 2.0
          : sorted_individual[mid];
  const double delta_pct =
      100.0 * (individual_median - batched_per_call_ns) / individual_median;
  std::printf(
      "\nDMA-readback share of the per-call floor: %.1f%% "
      "(individual median %.0f ns -> batched per-call %.0f ns)\n",
      delta_pct, individual_median, batched_per_call_ns);

  const bool ok = last_individual.value == last_batched.value &&
                 static_cast<long>(c->expected_value) == static_cast<long>(last_individual.value);
  std::printf("\nresult check: %s (expected=%ld individual=%u batched=%u)\n",
             ok ? "ok" : "MISMATCH", c->expected_value, last_individual.value,
             last_batched.value);

  return ok ? 0 : 1;
}
