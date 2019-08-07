#include "debugger.h"

#include "command_parser.h"
#include "util.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace smldbg {

Debugger::Debugger(int argc, char** argv) : m_is_running(false) {
    m_target = argv[1];
    auto elf_stream = std::make_unique<std::ifstream>(m_target);
    if (!(*elf_stream)) {
        std::cerr << "Unable to open target " << m_target << ".\n";
        std::exit(1);
    }

    m_elf = elf::ELF(std::move(elf_stream));
    m_dwarf = dwarf::Dwarf(&m_elf);
}

void Debugger::exec() {
    CommandParser command_parser;
    while (true) {
        // Display the prompt and wait for user input.
        std::cout << "smldbg >> ";
        std::string input;
        std::getline(std::cin, input);

        // Parse the requested command and any arguments.
        const CommandWithArguments command_with_args =
            command_parser.parse(input);

        // We're limited in what we can do if the target isn't running.
        if (!m_is_running) {
            const bool valid_command =
                command_with_args.command == Command::Start ||
                command_with_args.command == Command::Quit;
            if (!valid_command) {
                std::cerr << "The target is not currently running.\n";
                continue;
            }
        }

        // Handle the requested command.
        switch (command_with_args.command) {
        case Command::Break:
            if (command_with_args.arguments->empty()) {
                std::cerr << "Expected a breakpoint location.\n";
                break;
            }
            // Breakpoint is either of the form 'function' or 'file:line'
            if (command_with_args.arguments->find(':') != std::string::npos) {
                const auto tokens =
                    util::tokenize(*command_with_args.arguments, ':');
                break_on_line_and_file(std::stoul(tokens[1]), tokens[0]);
            } else {
                break_on_function(*command_with_args.arguments);
            }
            break;
        case Command::BackTrace:
            backtrace();
            break;
        case Command::Continue:
            continue_execution();
            break;
        case Command::Delete:
            delete_all_breakpoints();
            break;
        case Command::Finish:
            continue_to_end_of_stack_frame();
            break;
        case Command::Info:
            print_hardware_registers();
            break;
        case Command::Next:
            next();
            break;
        case Command::Print:
            if (!command_with_args.arguments) {
                std::cout << "Expected a variable name\n.";
                break;
            }
            if (const auto value =
                    get_variable_value(*command_with_args.arguments);
                value) {
                std::cout << *value << "\n";
            } else {
                std::cout << "Unable to retrieve value for variable "
                          << *command_with_args.arguments << ".\n";
            }
            break;
        case Command::Quit:
            std::cout << "Sending SIGTERM to process " << m_pid << "\n";
            kill(m_pid, SIGTERM);
            exit(0);
        case Command::Set: {
            const std::vector<std::string> args =
                util::tokenize(*command_with_args.arguments, ' ');
            if (args.size() != 2) {
                std::cerr << "Expected a variable name and value.\n";
                break;
            }
            set_variable_value(args[0], std::stoi(args[1]));
            break;
        }
        case Command::Step: {
            const int step_count =
                command_with_args.arguments->empty()
                    ? 1
                    : std::stoi(*command_with_args.arguments);
            for (unsigned i = 0, e = step_count; i < e; ++i)
                step();
            break;
        }
        case Command::Start:
            start();
            break;
        case Command::Unknown:
            break;
        }
    }
}

void Debugger::start() {
    // Sanity check.
    if (m_is_running)
        return;

    // Fork the current process and load the target image.
    int wait_status = 0;
    const auto pid = fork();
    if (pid == 0) {
        ptrace(PTRACE_TRACEME, nullptr, 0, nullptr);
        std::cout << "Starting: " << m_target << "\n";
        execl(m_target.c_str(), m_target.c_str(), nullptr);
    } else if (pid > 0) {
        m_pid = pid;
        waitpid(pid, &wait_status, 0);
    } else {
        std::cerr << "fork() failed with code " << pid << "\n";
        std::exit(1);
    }

    m_is_running = true;
    break_on_function("main");
    continue_execution();
}

void Debugger::wait_for_target() {
    int status = 0;
    waitpid(m_pid, &status, 0);

    // TODO: Handle restarting processes.
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        print_waitpid_status(status);
        std::exit(1);
    }
}

void Debugger::continue_execution() {
    ptrace(PTRACE_CONT, m_pid, 0, nullptr);
    wait_for_target();

    // Check if we have stopped on a breakpoint.
    const auto rip = get_register_value(HardwareRegister::rip);
    if (m_breakpoints.count(rip - 1) == 0)
        return;

    // Fixup the breakpoint.
    auto& [address, breakpoint] = *m_breakpoints.find(rip - 1);
    breakpoint.step_over();

    // Print some information about the breakpoint we hit.
    std::cout << "Hit breakpoint at " << std::hex << "0x" << address;
    if (const auto source_location =
            m_dwarf.source_location_from_program_counter(address, false);
        source_location)
        std::cout << " (" << source_location->file << ":" << std::dec
                  << source_location->line << ")";
    std::cout << "\n";
}

void Debugger::continue_to_end_of_stack_frame() {
    // Get the return address from the stack. This assumes that targets
    // have been build with -fno-omit-frame-pointer or equivalent.
    const auto rbp = get_register_value(HardwareRegister::rbp) + 8;
    const auto address = ptrace(PTRACE_PEEKTEXT, m_pid, rbp, nullptr);

    // Print the return address and the associated source location.
    std::cout << "Run till end of current stack frame (0x" << std::hex
              << address;
    if (const auto source_location =
            m_dwarf.source_location_from_program_counter(address, false);
        source_location) {
        std::cout << ", " << source_location->file << ":" << std::dec
                  << source_location->line;
    }
    std::cout << ")\n";

    // Create a temporary breakpoint at the return address.
    Breakpoint breakpoint(m_pid, address);
    breakpoint.enable();

    // Run the target to the return address.
    ptrace(PTRACE_CONT, m_pid, 0, nullptr);
    wait_for_target();

    // Clean up the temporary breakpoint.
    breakpoint.step_over();
    breakpoint.disable();
}

void Debugger::next() {
    // This is a slightly naive way to do source level 'step-over'. We
    // essentially want to single step until we change source location, with one
    // caveat, we don't want to enter function calls. We can do this by checking
    // each instruction to see if it is a call, if it is, place a breakpoint on
    // the next instruction and run to the breakpoint.

    // Get the current program counter and the corresponding source location.
    auto rip = get_register_value(HardwareRegister::rip);
    const auto location =
        m_dwarf.source_location_from_program_counter(rip, false);
    if (!location) {
        std::cerr << "No debug information available for source file.\n";
        return;
    }

    // Keep going until we change the source location.
    std::optional<dwarf::SourceLocation> next_location;
    while (true) {
        // Check if the current instruction is a call.
        const auto data = ptrace(PTRACE_PEEKTEXT, m_pid, rip, nullptr);
        if (const auto opcode = (data & 0xff); opcode == 0xe8) {
            // Set a breakpoint on the next instruction and continue.
            // Assume here that we have E8 cd (i.e. 5 bytes).
            Breakpoint breakpoint(m_pid, rip + 5);
            breakpoint.enable();
            ptrace(PTRACE_CONT, m_pid, 0, nullptr);
            wait_for_target();
            breakpoint.step_over();
            breakpoint.disable();
        } else {
            // Not a call, so safe to single step.
            ptrace(PTRACE_SINGLESTEP, m_pid, 0, nullptr);
            wait_for_target();
        }

        // Get the source location associated with the current program counter.
        rip = get_register_value(HardwareRegister::rip);
        next_location =
            m_dwarf.source_location_from_program_counter(rip, false);

        if (next_location && (next_location->line != location->line ||
                              next_location->file != location->file)) {
            // Skip locations that can't be attributed to any source lines.
            if (next_location->line == 0) {
                continue;
            } else {
                break;
            }
        }
    }

    // Print some information about where we stopped.
    std::cout << "Stopped at " << std::hex << rip << "(" << next_location->file
              << ":" << std::dec << next_location->line << ")\n";
}

void Debugger::step() {
    // Get the current program counter and the corresponding source location.
    auto rip = get_register_value(HardwareRegister::rip);
    const auto location =
        m_dwarf.source_location_from_program_counter(rip, false);
    if (!location) {
        std::cerr << "No debug information available for source file.\n";
        return;
    }

    // Single step until we hit a different source line.
    std::optional<dwarf::SourceLocation> next_location;
    while (true) {
        ptrace(PTRACE_SINGLESTEP, m_pid, 0, nullptr);
        wait_for_target();

        // Get the source location associated with the current program counter.
        rip = get_register_value(HardwareRegister::rip);
        next_location =
            m_dwarf.source_location_from_program_counter(rip, false);
        if (next_location && (next_location->line != location->line ||
                              next_location->file != location->file)) {
            break;
        }
    }

    // Print some information about where we stopped.
    std::cout << "Stopped at address 0x" << std::hex << next_location->address
              << " (" << next_location->file << ":" << std::dec
              << next_location->line << ")\n";
}

void Debugger::break_on_function(std::string_view method) {
    const auto source_location = m_dwarf.source_location_from_function(method);
    if (!source_location) {
        std::cerr << method << " method not found.\n";
        return;
    }

    if (m_breakpoints.count(source_location->address)) {
        std::cout << "A breakpoint is already active at this address\n";
        return;
    }

    auto [breakpoint, added] = m_breakpoints.emplace(
        source_location->address, Breakpoint(m_pid, source_location->address));
    breakpoint->second.enable();

    // Print some information about the new breakpoint.
    std::cout << "Set Breakpoint #" << m_breakpoints.size() << " at address 0x"
              << std::hex << source_location->address << " ("
              << source_location->file << ":" << std::dec
              << source_location->line << ")"
              << "\n";
}

void Debugger::break_on_line_and_file(uint64_t line, std::string_view file) {
    const std::optional<uint64_t> program_counter =
        m_dwarf.program_counter_from_line_and_file(line, file);
    if (!program_counter) {
        std::cerr << "Unable to set breakpoint on " << file << ":" << line
                  << "\n";
        return;
    }

    if (m_breakpoints.count(*program_counter)) {
        std::cout << "A breakpoint is already active at pc " << *program_counter
                  << "\n";
        return;
    }

    auto [breakpoint, added] = m_breakpoints.emplace(
        *program_counter, Breakpoint(m_pid, *program_counter));
    breakpoint->second.enable();

    // Print some information about the new breakpoint.
    std::cout << "Breakpoint " << m_breakpoints.size() << " at 0x" << std::hex
              << *program_counter << " (" << file << ":" << std::dec << line
              << ")\n";
}

void Debugger::delete_all_breakpoints() {
    for (auto& [address, breakpoint] : m_breakpoints)
        breakpoint.disable();
    std::cout << "Deleted " << m_breakpoints.size() << " breakpoints.\n";
    m_breakpoints.clear();
}

uint64_t Debugger::get_register_value(HardwareRegister hardware_register) {
    user_regs_struct registers;
    ptrace(PTRACE_GETREGS, m_pid, 0, &registers);
    switch (hardware_register) {
    case HardwareRegister::r15:
        return registers.r15;
    case HardwareRegister::r14:
        return registers.r14;
    case HardwareRegister::r13:
        return registers.r13;
    case HardwareRegister::r12:
        return registers.r12;
    case HardwareRegister::rbp:
        return registers.rbp;
    case HardwareRegister::rbx:
        return registers.rbx;
    case HardwareRegister::r11:
        return registers.r11;
    case HardwareRegister::r10:
        return registers.r10;
    case HardwareRegister::r9:
        return registers.r9;
    case HardwareRegister::r8:
        return registers.r8;
    case HardwareRegister::rax:
        return registers.rax;
    case HardwareRegister::rcx:
        return registers.rcx;
    case HardwareRegister::rdx:
        return registers.rdx;
    case HardwareRegister::rsi:
        return registers.rsi;
    case HardwareRegister::rdi:
        return registers.rdi;
    case HardwareRegister::orig_rax:
        return registers.orig_rax;
    case HardwareRegister::rip:
        return registers.rip;
    case HardwareRegister::cs:
        return registers.cs;
    case HardwareRegister::eflags:
        return registers.eflags;
    case HardwareRegister::rsp:
        return registers.rsp;
    case HardwareRegister::ss:
        return registers.ss;
    case HardwareRegister::fs_base:
        return registers.fs_base;
    case HardwareRegister::gs_base:
        return registers.gs_base;
    case HardwareRegister::ds:
        return registers.ds;
    case HardwareRegister::es:
        return registers.es;
    case HardwareRegister::fs:
        return registers.fs;
    case HardwareRegister::gs:
        return registers.gs;
    default:
        std::cerr << "Unknown hardware register.\n";
    }
}

std::optional<uint64_t>
Debugger::get_variable_value(std::string_view variable) {
    // Try and find the location of the named variable in the current context.
    const auto variable_location = m_dwarf.variable_location(
        get_register_value(HardwareRegister::rip), variable);
    if (!variable_location) {
        std::cerr << "No symbol named " << variable << " in current context.\n";
        return std::nullopt;
    }

    // Currently only support frame base relative variable locations.
    const int64_t location =
        get_register_value(HardwareRegister::rbp) + *variable_location;
    const auto data = ptrace(PTRACE_PEEKDATA, m_pid, location, nullptr);
    return (data & (~uint32_t(0)));
}

void Debugger::set_variable_value(std::string_view variable_name,
                                  int32_t value) {
    const auto variable_location = m_dwarf.variable_location(
        get_register_value(HardwareRegister::rip), variable_name);
    if (!variable_location)
        return;

    // Get the data word at the variable address.
    const int64_t location =
        get_register_value(HardwareRegister::rbp) + *variable_location;
    auto data = ptrace(PTRACE_PEEKDATA, m_pid, location, nullptr);

    // Set the variable value.
    auto modified_data = (data & 0xFFFFFFFF00000000) | value;
    ptrace(PTRACE_POKEDATA, m_pid, location, (void*)modified_data);
}

void Debugger::backtrace() {
    // Get the name of the function we are currently in.
    const auto rip = get_register_value(HardwareRegister::rip);
    auto function_name = m_dwarf.function_from_program_counter(rip);

    // Print the function name and current frame count.
    int frame_count = 0;
    std::cout << "#" << frame_count++ << " : ";
    if (function_name) {
        std::cout << *function_name;
    } else {
        std::cout << "unknown";
    }

    // If we can, print the source location.
    if (auto location = m_dwarf.source_location_from_function(*function_name);
        location)
        std::cout << " (" << location->file << ":" << std::dec << location->line
                  << ")";
    std::cout << "\n";

    // Walk up the stack until we reach main.
    uint64_t frame_pointer = get_register_value(HardwareRegister::rbp);
    uint64_t return_address =
        ptrace(PTRACE_PEEKDATA, m_pid, frame_pointer + 8, nullptr);
    while (*function_name != "main") {
        // Get the name of the function we are currently in.
        function_name = m_dwarf.function_from_program_counter(return_address);

        // Print the function name and current frame count.
        std::cout << "#" << frame_count++ << " : ";
        if (function_name) {
            std::cout << *function_name;
        } else {
            std::cout << "unknown";
        }

        // If we can, print the source location.
        if (auto location =
                m_dwarf.source_location_from_function(*function_name);
            location) {
            std::cout << " (" << location->file << ":" << std::dec
                      << location->line << ")";
        }
        std::cout << "\n";

        // Move on to the next frame.
        frame_pointer = ptrace(PTRACE_PEEKDATA, m_pid, frame_pointer, nullptr);
        return_address =
            ptrace(PTRACE_PEEKDATA, m_pid, frame_pointer + 8, nullptr);
    }
}

void Debugger::print_hardware_registers() {
    for (const auto& reg : m_registers) {
        const uint64_t register_value = get_register_value(
            reg.hardware_register); // TODO: Fetch all register values once.
        std::cout << reg.name << " " << std::dec << register_value << std::hex
                  << " (0x" << register_value << ")\n";
    }
}

void Debugger::print_waitpid_status(int waitpid_status) {
    if (WIFEXITED(waitpid_status)) {
        std::cout << "The child terminated normally, that is, ";
        std::cout << "by calling exit(3) or _exit(2), ";
        std::cout << "or by returning from main().\n";
        std::cout << "Exit waitpid_status: ";
        std::cout << std::dec << WEXITSTATUS(waitpid_status) << ".\n";
    } else if (WIFSIGNALED(waitpid_status)) {
        std::cout << "Process was terminated by signal ";
        std::cout << std::dec << WTERMSIG(waitpid_status) << ".\n";
        if (WCOREDUMP(waitpid_status))
            std::cout << "Core dump!\n";
    } else if (WIFSTOPPED(waitpid_status)) {
        std::cout << "Process was stopped by signal ";
        std::cout << std::dec << strsignal(WSTOPSIG(waitpid_status)) << ".\n";
    } else if (WIFCONTINUED(waitpid_status)) {
        std::cout << "Process was resumed by delivery of SIGCONT.\n";
    } else {
        std::cout << "Unknown waitpid_status.\n";
    }
}

} // namespace smldbg
