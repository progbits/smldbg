#include "debugger.h"

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "No target provided...\n";
        return 1;
    }

    smldbg::Debugger debugger(argc, argv);
    debugger.exec();

    return 0;
}
