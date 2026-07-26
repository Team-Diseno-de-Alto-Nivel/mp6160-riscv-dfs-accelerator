#pragma once
// TLM 2.0 transaction format between host and accelerator (HWC-3).
//
// Not wired into DfsAccelerator::b_transport — the real host paths decode
// by address against the memory map instead (config/memory_map.h). Kept
// around for a possible future out-of-band command channel.

#include <cstdint>

#include <tlm.h>

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
struct CommandExtension : tlm::tlm_extension<CommandExtension> {
    Command command = Command::ReadStatus;
    std::uint32_t operand = 0;

    tlm::tlm_extension_base* clone() const override {
        return new CommandExtension(*this);
    }

    void copy_from(tlm::tlm_extension_base const& other) override {
        const auto& src = static_cast<const CommandExtension&>(other);
        command = src.command;
        operand = src.operand;
    }
};

}  // namespace dfs::tlm_proto
