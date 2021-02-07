# smldbg
A small, educational debugger with no dependencies.

## Prerequisites
To build the project and run the tests, the following are required:
 - A C++20 compliant compiler (tested with `clang++ 11.0.0` and `g++ 10.2.0`)
 - CMake 3.12 or higher
 - The [Google Test Framework](https://github.com/google/googletest)

## Building the Project

```bash
git checkout git@github.com:progbits/smldbg
cd smldbg
mkdir build && cd build
cmake ..
make
```

## Running the Debugger
To run the debugger, compile the following small example program with optimization disabled and debug symbols enabled (normally flags `-o0` and `-g` for most compilers).

```cpp
#include <iostream>

void qux(int value) {
  std::cout << "value: " << value << "\n";
}

void baz(int value) {
  qux(value + 3);
}

void bar(int value) {
  baz(value + 2);
}

void foo(int value) {
  bar(value + 1);
}

int main() {
  foo(1);
  bar(3);
  return 0;
}
```

The following commands will step through the program, set breakpoints on both methods and source code locations and inspect the current program state.

```shell
start               # Start debugging the executable
step 2              # Move the instruction pointer forwards 2 instructions
info registers      # Print the current register values
bt                  # Print the backtrace of the current stack
break main.cpp:12   # Add a new breakpoint at line 12
cont                # Run until the next breakpoint is hit
bt                  # Print the backtrace of the current stack
print value         # Print the variable `value`
break qux           # Set a breakpoint on the function `qux(...)`
cont                # Run until the next breakpoint is hit
bt                  # Print the backtrace of the current stack
print value         # Print the variable `value`
set value 99        # Set the variable `value` to 99
step                # Move the instruction pointer forwards 1 instruction
finish              # Run until the end of the current method
finish              # Run until the end of the current method
bt                  # Print the backtrace of the current stack
finish              # Run until the end of the current method
finish              # Run until the end of the current method
delete              # Delete all breakpoint
next                # Run until the next line
step                # Move the instruction pointer forwards 1 instruction
```

![](resources/smldbg.gif)
