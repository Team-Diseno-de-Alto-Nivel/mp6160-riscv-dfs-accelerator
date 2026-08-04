// On-board validation of dfs_accel against the 21 golden cases (issue #67),
// using CYNQ instead of PYNQ -- see README.md for why (PYNQ 3.x needs
// pyxrt, which isn't available on this board's XRT install).
//
// Mirrors src/hls/tb/dfs_accel_tb.cpp's cosim check (#63): same 21 cases,
// same pass/fail bar -- a case only fails if its result VALUE disagrees
// with cases.json's expected_value; a traversal-counter mismatch alone is
// reported (status "ok*") but doesn't fail the case.
//
// Usage: ./validate_cynq <bitstream.bit> <cases.json>
//
// The cases.json parser, register map, and result layout live in
// dfs_accel_case_io.h, shared with benchmark_cynq.cpp (#91).

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cynq/cynq.hpp>

#include "dfs_accel_case_io.h"

using namespace cynq;  // NOLINT

RunResult RunCase(std::shared_ptr<IAccelerator> accel,
                  std::shared_ptr<IDataMover> mover,
                  const tinyjson::Case& c) {
  // Cacheable, not Dual: on Zynq there's one shared DDR, not a separate
  // "device" region needing a real DMA IP (this design has none -- m_axi
  // goes straight through smartconnect to the PS HP port). Dual +
  // mover->Upload()/Download() hangs indefinitely here, presumably because
  // the XRT DataMover backend expects a real DMA IP at kAccelAddr.
  auto grid_buf = mover->GetBuffer(c.grid.size(), 0, MemoryType::Cacheable);
  auto words_buf = mover->GetBuffer(c.words.size(), 0, MemoryType::Cacheable);
  auto result_buf =
      mover->GetBuffer(4 * sizeof(uint32_t), 0, MemoryType::Cacheable);

  uint8_t* grid_host = grid_buf->HostAddress<uint8_t>().get();
  for (size_t i = 0; i < c.grid.size(); ++i)
    grid_host[i] = static_cast<uint8_t>(c.grid[i]);
  uint8_t* words_host = words_buf->HostAddress<uint8_t>().get();
  for (size_t i = 0; i < c.words.size(); ++i)
    words_host[i] = static_cast<uint8_t>(c.words[i]);

  grid_buf->Sync(SyncType::HostToDevice);
  words_buf->Sync(SyncType::HostToDevice);

  accel->Attach(kAddrGridIn, grid_buf);
  accel->Attach(kAddrWordsIn, words_buf);
  accel->Attach(kAddrResultOut, result_buf);

  uint32_t rows = c.rows, cols = c.cols, mode = c.mode, flags = c.flags,
          num_words = c.num_words;
  accel->Attach(kAddrRows, &rows, RegisterAccess::WO);
  accel->Attach(kAddrCols, &cols, RegisterAccess::WO);
  accel->Attach(kAddrMode, &mode, RegisterAccess::WO);
  accel->Attach(kAddrFlags, &flags, RegisterAccess::WO);
  accel->Attach(kAddrNumWords, &num_words, RegisterAccess::WO);

  accel->Start(StartMode::Once);
  accel->Sync();

  result_buf->Sync(SyncType::DeviceToHost);
  uint32_t* result = result_buf->HostAddress<uint32_t>().get();

  return {result[0], result[1], result[2], result[3]};
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s <bitstream.bit> <cases.json>\n", argv[0]);
    return 1;
  }
  const std::string bitstream = argv[1];
  const std::string cases_path = argv[2];

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

  // build_bitstream.tcl (#67) built this at 200 MHz -- CYNQ doesn't read
  // the .hwh, so it doesn't know that; set it explicitly.
  auto clocks = platform->GetClocks();
  for (size_t i = 0; i < clocks.size(); ++i)
    clocks[i] = (i == 0) ? 200.f : -1.f;
  platform->SetClocks(clocks);

  // Read back rather than trust the call silently succeeded -- confirms
  // what CYNQ itself now believes clock 0 is set to.
  const auto confirmed_clocks = platform->GetClocks();
  if (!confirmed_clocks.empty())
    std::printf("Clock 0 after SetClocks: %.3f MHz\n", confirmed_clocks[0]);

  auto accel = platform->GetAccelerator(kAccelAddr);
  auto mover = platform->GetDataMover(kAccelAddr);

  int total = 0, failures = 0, counter_mismatches = 0;
  std::printf("%-32s %-18s %8s %8s  status\n", "algorithm", "case", "expect",
             "hw");

  for (const auto& c : cases) {
    ++total;
    RunResult r = RunCase(accel, mover, c);

    const bool value_ok = static_cast<long>(r.value) == c.expected_value;
    const bool counters_ok =
        r.expanded_nodes == c.expected_expanded_nodes &&
        r.visited_cells == c.expected_visited_cells &&
        r.peak_stack_depth == c.expected_peak_stack_depth;

    if (!value_ok)
      ++failures;
    else if (!counters_ok)
      ++counter_mismatches;

    const char* status = !value_ok ? "FAIL" : (counters_ok ? "ok" : "ok*");
    std::printf("%-32s %-18s %8ld %8u  %s\n", c.algorithm.c_str(),
               c.name.c_str(), c.expected_value, r.value, status);
  }

  std::printf("\n%d/%d cases match the golden result (on-board)\n",
             total - failures, total);
  if (counter_mismatches > 0)
    std::printf("%d case(s) marked ok* differ in traversal counters\n",
               counter_mismatches);

  return failures == 0 ? 0 : 1;
}
