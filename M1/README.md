 # Build LLVM on Apple Silicon(M1)

## How bo build

### Build cmake

I recommend that you install `cmake` via [cmake.org](https://cmake.org).
You can use `cmake` with adding the path to `PATH`. 

<img width="342" alt="スクリーンショット 2021-11-07 13 06 29" src="https://user-images.githubusercontent.com/33768/140631963-1cb4ed7d-fc69-4883-8081-7619ccbc841c.png">

### Build LLVM

At first, check your Terminal.app without using Rosetta.

<img width="251" alt="image" src="https://user-images.githubusercontent.com/33768/124542560-91d6d600-de5e-11eb-8222-659d5b04eb31.png">

To avoid using libxml2, edit `CMakeLists.txt` as follows,

```
set(LLVM_ENABLE_LIBXML2 "OFF" CACHE STRING "Use libxml2 if available. Can be ON, OFF, or FORCE_ON")
```

Without this modification, linker can not build llvm because it can not find and link libxml.
I do not use libxml in my codes. If you want to use libxml in LLVM, I can not help you.

Next, prepare and build any files using cmake. My MacBookPro M1 crashed when I ran the `make -j` command because the build process used up all the memory.　It is recommended that you specify the number of processes with the `j` option.

```
mkdir build
cd build
cmake .. $LLVM_SRC_DIR -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="ARM;X86;AArch64"
make -j 4
```

## Build sources

I do not copy any llvm files into `/usr/`, because I want to use multiple versions of LLVM switching between them.

For example, we can build source codes using LLVM as follows,

```
 /usr/bin/clang++ add.cpp -o add `<path to llvm>/build/bin/llvm-config --cxxflags --ldflags --libs --libfiles core orcjit mcjit native --system-libs`
```

## Compile speed

I compared the compile speed between Apple Silicon M1 and Intel Core i9, using `add.cpp`.

* Apple Silicon(M1) 1.93[sec]
* Intel(Core i9) 4.6[sec]

As a result, M1 was 2.4 times faster than Core i9.

## Visual Studio Code

Visual studio terminal runs on i386 at default. Therefore, I enforcely compile the files for ARM using the following settings.

```
{
    "tasks": [
        {
            "type": "shell",
            "label": "clang++ build active file",
            "command": "arch",
            "args": [
                "-arm64",
                "/usr/bin/clang++",
                "-g",
                "-O0",
                "${fileDirname}/${fileBasename}",
                "-o",
                "${fileDirname}/${fileBasenameNoExtension}",
                "`/Users/sonson/Downloads/llvm-11.0.0.src/build/bin/llvm-config", "--cppflags`",
                "-std=c++17",
                "`/Users/sonson/Downloads/llvm-11.0.0.src/build/bin/llvm-config", "--ldflags", "--libs", "--libfiles", "core","orcjit","mcjit", "native", "--system-libs`"
            ]
        }
    ],
    "version": "2.0.0"
}
```

## Debugging in Visual studio code.

* Use CodeLLDB and following `launch.json`.

```
{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "clang++",
            "type": "lldb",
            "request": "launch",
            // "targetArchitecture": "arm64",
            "program": "${fileDirname}/${fileBasenameNoExtension}",
            "args": [],
            // "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            // "environment": [],
            // "externalConsole": false,
            // "MIMode": "lldb",
            "preLaunchTask": "clang++ build active file"
        }
    ]
}
```
