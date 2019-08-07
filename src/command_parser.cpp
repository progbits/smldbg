#include "command_parser.h"

namespace smldbg {

CommandWithArguments CommandParser::parse(const std::string& user_input) {
    // Split any command arguments from the command itself.
    const auto split_index = user_input.find(' ');
    const std::string arguments = [=]() -> std::string {
        if (split_index == std::string::npos)
            return {};
        else
            return {user_input.begin() + split_index + 1, user_input.end()};
    }();

    // Match the minimum amount of |user_input| to identify the command.
    if (user_input.substr(0, 2) == "br")
        return {.command = Command::Break, .arguments = arguments};
    else if (user_input.substr(0, 2) == "bt")
        return {.command = Command::BackTrace, .arguments = {}};
    else if (user_input.find('c', 0) == 0)
        return {.command = Command::Continue, .arguments = {}};
    else if (user_input.find('d', 0) == 0)
        return {.command = Command::Delete, .arguments = {}};
    else if (user_input.find('f', 0) == 0)
        return {.command = Command::Finish, .arguments = {}};
    else if (user_input.find('i', 0) == 0)
        return {.command = Command::Info, .arguments = arguments};
    else if (user_input.find('n', 0) == 0)
        return {.command = Command::Next, .arguments = {}};
    else if (user_input.find('p', 0) == 0)
        return {.command = Command::Print, .arguments = arguments};
    else if (user_input.find('q', 0) == 0)
        return {.command = Command::Quit, .arguments = {}};
    else if (user_input.substr(0, 2) == "se")
        return {.command = Command::Set, .arguments = arguments};
    else if (user_input.substr(0, 3) == "sta")
        return {.command = Command::Start, .arguments = {}};
    else if (user_input.substr(0, 3) == "ste")
        return {.command = Command::Step, .arguments = arguments};
    else
        return {.command = Command::Unknown, .arguments = {}};
}
} // namespace smldbg
