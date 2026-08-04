// On-board sustained-throughput benchmark for dfs_accel over the 21 golden
// cases (issue #91). Companion to validate_cynq.cpp (#67, correctness-only)
// -- driver.py's docstring already flags that per-case buffer allocation
// "would matter for a throughput benchmark (that's #91, not this)", so this
// tool hoists buffer setup/Attach out of the timed region and adds
// warm-up + repeated timed iterations on top of the same CYNQ setup.
//
// Timing method: PS-side wall clock (std::chrono::steady_clock) bracketing
// Start()+Sync()+the result Sync(DeviceToHost), not a new hardware counter
// IP. The block design (src/vivado/scripts/build_bd.tcl) has no timer/
// performance-counter IP today, and adding one means rebuilding the
// bitstream through the full Vivado pipeline -- a much higher-friction,
// higher-risk change than a PS-side timer, which the issue's own wording
// ("un contador de hardware o timer del PS") allows outright.
//
// Usage: ./benchmark_cynq <bitstream.bit> <cases.json> [csv_out] [iterations] [warmup]
//   csv_out    default: benchmark_metrics.csv
//   iterations default: 50  (timed runs per case, after warm-up)
//   warmup     default: 5   (untimed runs per case, discarded)
//
// CSV schema: the same 11 base columns as results/metrics.csv (see
// docs/guides/metrics-schema.md), so scripts/plot_results.py reads this
// file unmodified, plus trailing hardware-specific columns. accelerator_on
// is always 1 (this tool only exercises the accelerated path -- the ON/OFF
// contrast is a simulation-level experiment, not an on-board one, see
// docs/paper/main.tex's experiment plan) and instruction_count is always 0
// (no cost-model instrumentation exists inside the HLS kernel).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cynq/cynq.hpp>

#include "dfs_accel_case_io.h"

using namespace cynq;  // NOLINT

namespace {

constexpr int kDefaultIterations = 50;
constexpr int kDefaultWarmup = 5;

struct CaseHandle {
  std::shared_ptr<IMemory> grid_buf, words_buf, result_buf;
  // Scalar config registers, attached by pointer with RegisterAccess::WO.
  // Confirmed on hardware (not just assumed) that CYNQ re-reads this
  // pointer on every Start() rather than copying the value at Attach()
  // time -- an earlier version passed pointers to PrepareCase() locals,
  // which went out of scope the moment PrepareCase() returned, so every
  // Start() after that read stack garbage (19/21 cases failed, and the 2
  // that "passed" were coincidentally the ones expecting 0). These fields
  // must live exactly as long as this CaseHandle, i.e. for every Start()
  // call made against this case, not just for the Attach() call itself.
  uint32_t rows = 0, cols = 0, mode = 0, flags = 0, num_words = 0;
};

struct CaseStats {
  std::string algorithm, case_name;
  bool passed = false;
  long result_value = 0;
  uint64_t expanded_nodes = 0, visited_cells = 0, peak_stack_depth = 0;
  int iterations = 0, warmup_iterations = 0;
  double latency_ns_min = 0.0, latency_ns_median = 0.0, latency_ns_max = 0.0,
         latency_ns_stddev = 0.0;
};

// Buffer allocation, host->device copy, and register Attach() -- everything
// that doesn't change between repeated invocations of the same case. Done
// once per case, outside the timed loop (see file header).
//
// Fills an existing CaseHandle in place (out param) rather than returning
// one by value: CYNQ keeps live pointers to h->rows/h->cols/etc. (see
// CaseHandle's comment), so this handle must never move or get copied
// between Attach() and the last Start() that uses it -- a returned-by-value
// CaseHandle isn't guaranteed to keep the same address (NRVO is an
// optimization, not a guarantee), which is exactly the bug this shape
// avoids.
void PrepareCase(std::shared_ptr<IAccelerator> accel,
                 std::shared_ptr<IDataMover> mover, const tinyjson::Case& c,
                 CaseHandle* h) {
  h->grid_buf = mover->GetBuffer(c.grid.size(), 0, MemoryType::Cacheable);
  h->words_buf = mover->GetBuffer(c.words.size(), 0, MemoryType::Cacheable);
  h->result_buf =
      mover->GetBuffer(4 * sizeof(uint32_t), 0, MemoryType::Cacheable);

  uint8_t* grid_host = h->grid_buf->HostAddress<uint8_t>().get();
  for (size_t i = 0; i < c.grid.size(); ++i)
    grid_host[i] = static_cast<uint8_t>(c.grid[i]);
  uint8_t* words_host = h->words_buf->HostAddress<uint8_t>().get();
  for (size_t i = 0; i < c.words.size(); ++i)
    words_host[i] = static_cast<uint8_t>(c.words[i]);

  h->grid_buf->Sync(SyncType::HostToDevice);
  h->words_buf->Sync(SyncType::HostToDevice);

  accel->Attach(kAddrGridIn, h->grid_buf);
  accel->Attach(kAddrWordsIn, h->words_buf);
  accel->Attach(kAddrResultOut, h->result_buf);

  h->rows = c.rows;
  h->cols = c.cols;
  h->mode = c.mode;
  h->flags = c.flags;
  h->num_words = c.num_words;
  accel->Attach(kAddrRows, &h->rows, RegisterAccess::WO);
  accel->Attach(kAddrCols, &h->cols, RegisterAccess::WO);
  accel->Attach(kAddrMode, &h->mode, RegisterAccess::WO);
  accel->Attach(kAddrFlags, &h->flags, RegisterAccess::WO);
  accel->Attach(kAddrNumWords, &h->num_words, RegisterAccess::WO);
}

// A single timed invocation: Start, block for completion, and pull the
// result back -- the only work that should count as accelerator latency,
// as opposed to the one-time setup in PrepareCase().
double TimedRun(std::shared_ptr<IAccelerator> accel,
                std::shared_ptr<IMemory> result_buf) {
  const auto t0 = std::chrono::steady_clock::now();
  accel->Start(StartMode::Once);
  accel->Sync();
  result_buf->Sync(SyncType::DeviceToHost);
  const auto t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

CaseStats BenchmarkCase(std::shared_ptr<IAccelerator> accel,
                        std::shared_ptr<IDataMover> mover,
                        const tinyjson::Case& c, int iterations, int warmup) {
  CaseHandle h;
  PrepareCase(accel, mover, c, &h);

  for (int i = 0; i < warmup; ++i) {
    accel->Start(StartMode::Once);
    accel->Sync();
    h.result_buf->Sync(SyncType::DeviceToHost);
  }

  std::vector<double> samples;
  samples.reserve(iterations);
  RunResult last{};
  for (int i = 0; i < iterations; ++i) {
    samples.push_back(TimedRun(accel, h.result_buf));
    const uint32_t* result = h.result_buf->HostAddress<uint32_t>().get();
    last = {result[0], result[1], result[2], result[3]};
  }

  CaseStats s;
  s.algorithm = c.algorithm;
  s.case_name = c.name;
  s.result_value = static_cast<long>(last.value);
  s.passed = s.result_value == c.expected_value;
  s.expanded_nodes = last.expanded_nodes;
  s.visited_cells = last.visited_cells;
  s.peak_stack_depth = last.peak_stack_depth;
  s.iterations = iterations;
  s.warmup_iterations = warmup;

  std::vector<double> sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  s.latency_ns_min = sorted.front();
  s.latency_ns_max = sorted.back();
  const size_t mid = sorted.size() / 2;
  s.latency_ns_median = (sorted.size() % 2 == 0)
                            ? (sorted[mid - 1] + sorted[mid]) / 2.0
                            : sorted[mid];

  double mean = 0.0;
  for (double v : samples) mean += v;
  mean /= static_cast<double>(samples.size());
  double variance = 0.0;
  for (double v : samples) variance += (v - mean) * (v - mean);
  variance /= static_cast<double>(samples.size());
  s.latency_ns_stddev = std::sqrt(variance);

  return s;
}

// Self-contained CSV writer: mirrors dfs::report()'s 11-column base schema
// (src/program/harness/report.cpp) byte-for-byte, plus trailing hardware
// columns. Deliberately not #include-ing harness/report.h/report.cpp here:
// onboard-deploy's scp flattens every deployed file into a single directory
// on the board (Makefile:137-157), so pulling in files from
// src/program/harness/ would need its own subdirectory + include-path
// surgery on the deploy step for very little payoff (RunMetrics/report()'s
// console tables are a couple dozen lines). Reproducing the schema locally
// keeps this tool exactly as dependency-free as validate_cynq.cpp already
// is on purpose.
bool WriteCsv(const std::string& path, const std::vector<CaseStats>& stats,
             double hw_clock_mhz) {
  std::FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return false;

  std::fprintf(f,
               "algorithm,case,accelerator_on,passed,result,latency_ns,"
               "instruction_count,expanded_nodes,visited_cells,"
               "peak_stack_depth,throughput_cells_per_s,hw_clock_mhz,"
               "iterations,warmup_iterations,latency_ns_min,"
               "latency_ns_median,latency_ns_max,latency_ns_stddev,"
               "throughput_ops_sustained\n");

  for (const auto& s : stats) {
    const double throughput_cells_per_s =
        s.latency_ns_median > 0.0
            ? static_cast<double>(s.expanded_nodes) / (s.latency_ns_median * 1e-9)
            : 0.0;
    const double throughput_ops_sustained =
        s.latency_ns_median > 0.0 ? 1e9 / s.latency_ns_median : 0.0;

    std::fprintf(
        f,
        "%s,%s,%d,%d,%ld,%.0f,%llu,%llu,%llu,%llu,%.2f,"
        "%.3f,%d,%d,%.0f,%.0f,%.0f,%.2f,%.2f\n",
        s.algorithm.c_str(), s.case_name.c_str(), 1, s.passed ? 1 : 0,
        s.result_value, s.latency_ns_median,
        static_cast<unsigned long long>(0),  // instruction_count: n/a on-board
        static_cast<unsigned long long>(s.expanded_nodes),
        static_cast<unsigned long long>(s.visited_cells),
        static_cast<unsigned long long>(s.peak_stack_depth),
        throughput_cells_per_s, hw_clock_mhz, s.iterations,
        s.warmup_iterations, s.latency_ns_min, s.latency_ns_median,
        s.latency_ns_max, s.latency_ns_stddev, throughput_ops_sustained);
  }

  std::fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3 || argc > 6) {
    std::fprintf(stderr,
                 "usage: %s <bitstream.bit> <cases.json> "
                 "[csv_out=benchmark_metrics.csv] [iterations=%d] [warmup=%d]\n",
                 argv[0], kDefaultIterations, kDefaultWarmup);
    return 1;
  }
  const std::string bitstream = argv[1];
  const std::string cases_path = argv[2];
  const std::string csv_out = argc > 3 ? argv[3] : "benchmark_metrics.csv";
  const int iterations = argc > 4 ? std::atoi(argv[4]) : kDefaultIterations;
  const int warmup = argc > 5 ? std::atoi(argv[5]) : kDefaultWarmup;
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

  std::printf("----- Initialising platform -----\n");
  auto platform = IHardware::Create(HardwareArchitecture::UltraScale, bitstream);

  // Same gotcha as validate_cynq.cpp: CYNQ doesn't read the .hwh, so the
  // 200 MHz build_bitstream.tcl (#67) target clock has to be set explicitly.
  auto clocks = platform->GetClocks();
  for (size_t i = 0; i < clocks.size(); ++i)
    clocks[i] = (i == 0) ? 200.f : -1.f;
  platform->SetClocks(clocks);

  const auto confirmed_clocks = platform->GetClocks();
  const double hw_clock_mhz = confirmed_clocks.empty() ? 0.0 : confirmed_clocks[0];
  std::printf("Clock 0 after SetClocks: %.3f MHz\n", hw_clock_mhz);

  auto accel = platform->GetAccelerator(kAccelAddr);
  auto mover = platform->GetDataMover(kAccelAddr);

  std::printf("%-32s %-18s %6s %14s %14s %6s\n", "algorithm", "case", "ok",
             "lat_ns(med)", "OP/s(sustain)", "iters");

  std::vector<CaseStats> all_stats;
  all_stats.reserve(cases.size());
  int total = 0, failures = 0;
  for (const auto& c : cases) {
    ++total;
    CaseStats s = BenchmarkCase(accel, mover, c, iterations, warmup);
    if (!s.passed) ++failures;

    const double ops_sustained =
        s.latency_ns_median > 0.0 ? 1e9 / s.latency_ns_median : 0.0;
    std::printf("%-32s %-18s %6s %14.0f %14.2f %6d\n", s.algorithm.c_str(),
               s.case_name.c_str(), s.passed ? "ok" : "FAIL",
               s.latency_ns_median, ops_sustained, s.iterations);

    all_stats.push_back(std::move(s));
  }

  std::printf("\n%d/%d cases passed (on-board benchmark)\n", total - failures,
             total);

  if (!WriteCsv(csv_out, all_stats, hw_clock_mhz)) {
    std::fprintf(stderr, "failed to write %s\n", csv_out.c_str());
    return 1;
  }
  std::printf("Wrote %s (%zu rows)\n", csv_out.c_str(), all_stats.size());

  return failures == 0 ? 0 : 1;
}
