#pragma once
// TLM 2.0 transaction format between host and accelerator (HWC-3).
//
// Not currently wired into DfsAccelerator::b_transport (HWA-1): the actual
// host paths (src/program/driver/accelerator_driver.h via
// src/program/integration/tlm_bus.h) issue plain TLM_READ_COMMAND/
// TLM_WRITE_COMMAND transactions decoded by address against the memory map
// (config/memory_map.h), not this extension. It's kept available as a
// properly implemented tlm_extension for a future out-of-band or
// finer-grained command channel.

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
