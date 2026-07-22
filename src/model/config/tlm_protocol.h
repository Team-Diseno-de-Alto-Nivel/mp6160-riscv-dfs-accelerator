#pragma once
// TLM 2.0 transaction format between host and accelerator (HWC-3).

#include <cstdint>

namespace dfs::tlm_proto {

enum class Command : std::uint8_t {
    LoadGrid,
    SetParams,
    SetEnable,   // ON/OFF toggle
    Start,
    ReadStatus,
    ReadResult,
};

// TLM extension carrying the command and a scalar operand.
struct CommandExtension /* : tlm::tlm_extension<CommandExtension> */ {
    Command command = Command::ReadStatus;
    std::uint32_t operand = 0;
    // TODO(HWC-3): implement clone()/copy_from().
};

}  // namespace dfs::tlm_proto
