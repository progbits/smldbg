#include "breakpoint.h"

#include <iostream>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

namespace smldbg {

void Breakpoint::enable() {
    // Get the word at |m_address| and store the first byte.
    auto peek_data = ptrace(PTRACE_PEEKTEXT, m_pid, (void*)m_address, 0);
    m_data = static_cast<uint8_t>(peek_data & 0xFF);

    // Replace the first byte with a trap and replace the word in the program
    // image.
    auto trap = ((peek_data & (~0xFF)) | 0xCC);
    ptrace(PTRACE_POKETEXT, m_pid, (void*)m_address, (void*)trap);
}

void Breakpoint::disable() {
    // Get word at |m_address|.
    auto data = ptrace(PTRACE_PEEKTEXT, m_pid, (void*)m_address, (void*)0);

    // Restore the first byte of |data| from the value stored when we set the
    // breakpoint and replace the word in the program image.
    auto restored = ((data & (~0xFF)) | m_data);
    ptrace(PTRACE_POKETEXT, m_pid, (void*)m_address, (void*)restored);
}

void Breakpoint::step_over() {
    disable();

    // Step back over the instruction we clobbered for our trap.
    user_regs_struct registers;
    ptrace(PTRACE_GETREGS, m_pid, 0, static_cast<void*>(&registers));
    registers.rip -= 1;
    ptrace(PTRACE_SETREGS, m_pid, 0, &registers);

    // Single-step the original instruction.
    int wait_pid_status = 0;
    ptrace(PTRACE_SINGLESTEP, m_pid, 0, 0);
    waitpid(m_pid, &wait_pid_status, 0);

    // Re-enable the breakpoint.
    enable();
}

} // namespace smldbg
