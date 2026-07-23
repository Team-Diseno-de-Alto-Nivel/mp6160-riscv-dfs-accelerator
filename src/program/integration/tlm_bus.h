#pragma once
// TlmBus bridges the MMIO driver (src/program/driver/accelerator_driver.h) onto a
// TLM 2.0 initiator socket: every write32/read32 becomes a b_transport call on the
// accelerator's register/memory-map surface. This is the coarse whole-traversal
// integration path exercised by INT-1 (#29) end-to-end.

#include <tlm.h>

#include <cstdint>
#include <functional>

#include "driver/accelerator_driver.h"

namespace dfs::integration {

using TransportFn = std::function<void(tlm::tlm_generic_payload&, sc_core::sc_time&)>;

class TlmBus final : public driver::Bus {
  public:
    explicit TlmBus(TransportFn transport) : transport_(std::move(transport)) {}

    void write32(std::uint64_t offset, std::uint32_t value) override {
        access(offset, &value, tlm::TLM_WRITE_COMMAND);
    }

    std::uint32_t read32(std::uint64_t offset) const override {
        std::uint32_t value = 0;
        access(offset, &value, tlm::TLM_READ_COMMAND);
        return value;
    }

  private:
    void access(std::uint64_t offset, std::uint32_t* value, tlm::tlm_command cmd) const {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(cmd);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(value));
        trans.set_data_length(sizeof(std::uint32_t));
        trans.set_streaming_width(sizeof(std::uint32_t));
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        transport_(trans, delay);
    }

    TransportFn transport_;
};

}  // namespace dfs::integration
