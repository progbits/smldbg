#pragma once

#include <optional>
#include <string>

namespace smldbg {

enum class Command {
    BackTrace,
    Break,
    Continue,
    Delete,
    Finish,
    Info,
    Next,
    Print,
    Quit,
    Set,
    Start,
    Step,
    Unknown,
};

struct CommandWithArguments {
    Command command;
    std::optional<std::string> arguments;
};

class CommandParser {
public:
    CommandParser() = default;

    // Parse user input into a Command and an optional argument string.
    CommandWithArguments parse(const std::string& user_input);
};

} // namespace smldbg
