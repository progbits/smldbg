#pragma once

#include <cstdint>

namespace smldbg {

class Breakpoint {
public:
    Breakpoint() = default;
    Breakpoint(int pid, uint64_t address)
        : m_pid(pid), m_address(address), m_enabled(false) {}

    void enable();
    void disable();
    void step_over();

    bool enabled() const { return m_enabled; }
    uint64_t address() const { return m_address; }

public:
    int m_pid;
    uint64_t m_address;
    uint8_t m_data;

    bool m_enabled;
};

} // namespace smldbg
