#pragma once

#include "breakpoint.h"
#include "dwarf.h"
#include "elf.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace smldbg {

class Debugger {
public:
    Debugger(int argc, char** argv);

    // Run the main event loop.
    void exec();

private:
    enum class HardwareRegister {
        r15,
        r14,
        r13,
        r12,
        rbp,
        rbx,
        r11,
        r10,
        r9,
        r8,
        rax,
        rcx,
        rdx,
        rsi,
        rdi,
        orig_rax,
        rip,
        cs,
        eflags,
        rsp,
        ss,
        fs_base,
        gs_base,
        ds,
        es,
        fs,
        gs
    };

    struct RegisterDesc {
        HardwareRegister hardware_register;
        int dwarf_index;
        std::string name;
    };

    // Emulate the gdb/lldb 'start' command. Create the target process, set a
    // breakpoint on main, and run to the breakpoint.
    void start();

    // Wait for the target process (|m_pid|).
    void wait_for_target();

    // Run child process until new signal is raised.
    void continue_execution();

    // Run the child process to the end of the current stack frame (step out).
    void continue_to_end_of_stack_frame();

    // Run the program to the next source line in the current file (step over).
    void next();

    // Do a source level single step (step in).
    void step();

    // Set a breakpoint on the named function.
    void break_on_function(std::string_view method);

    // Set a breakpoint on the specified line of a named source file.
    void break_on_line_and_file(uint64_t line, std::string_view file);

    // Delete all of the previously set breakpoints.
    void delete_all_breakpoints();

    // Return the current value of the specified register.
    uint64_t get_register_value(HardwareRegister desc);

    // Return the value of the named variable in the current context.
    std::optional<uint64_t> get_variable_value(std::string_view variable);

    // Set the value of the named variable in the current context.
    void set_variable_value(std::string_view variable, int32_t value);

    // Print a backtrace of the target process from the current context.
    void backtrace();

    // Dump the current values of each hardware register.
    void print_hardware_registers();

    // Print diagnostic information about the status returned from waitpid(...)
    void print_waitpid_status(int waitpid_status);

    elf::ELF m_elf; // The ELF format executable being debugged.
    dwarf::Dwarf
        m_dwarf; // The Dwarf interpreter instance associated with |m_elf|.

    std::string m_target; // Debug target path.
    bool m_is_running;    // Is the target currently running.
    int m_pid;            // PID of the target if |m_is_running| == true.

    std::unordered_map<uint64_t, Breakpoint>
        m_breakpoints; // Map program counter values to breakpoints..

    // Section 3.38
    // https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf
    const std::array<RegisterDesc, 28> m_registers = {
        {{HardwareRegister::r15, 15, "r15"},
         {HardwareRegister::r14, 14, "r14"},
         {HardwareRegister::r13, 13, "r13"},
         {HardwareRegister::r12, 12, "r12"},
         {HardwareRegister::rbp, 6, "rbp"},
         {HardwareRegister::rbx, 3, "rbx"},
         {HardwareRegister::r11, 11, "r11"},
         {HardwareRegister::r10, 10, "r10"},
         {HardwareRegister::r9, 9, "r9"},
         {HardwareRegister::r8, 8, "r8"},
         {HardwareRegister::rax, 0, "rax"},
         {HardwareRegister::rcx, 2, "rcx"},
         {HardwareRegister::rdx, 1, "rdx"},
         {HardwareRegister::rsi, 4, "rsi"},
         {HardwareRegister::rdi, 5, "rdi"},
         {HardwareRegister::orig_rax, -1, "orig_rax"}, // Undef. in Dwarf Spec.
         {HardwareRegister::rip, -1, "rip"},           // Undef. in Dwarf Spec.
         {HardwareRegister::cs, 51, "cs"},
         {HardwareRegister::eflags, 49, "eflags"},
         {HardwareRegister::rsp, 7, "rsp"},
         {HardwareRegister::ss, 52, "ss"},
         {HardwareRegister::fs_base, 58, "fs_base"},
         {HardwareRegister::gs_base, 59, "gs_base"},
         {HardwareRegister::ds, 53, "ds"},
         {HardwareRegister::es, 50, "es"},
         {HardwareRegister::fs, 54, "fs"},
         {HardwareRegister::gs, 55,
          .name = "gs"}}}; // Convenient mapping between hardware registers,
                           // dwarf register indexes and printable names.
};

} // namespace smldbg
