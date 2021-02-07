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

```bash
./driver a.out
```

![](resources/smldbg.gif)
