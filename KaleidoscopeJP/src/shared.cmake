
find_program(LLVM_CONFIG "llvm-config")
set(LLVM_CONFIG "/Users/sonson/Downloads/llvm-11.0.0.src/build/bin/llvm-config")

macro(SET_LLVM_COMPILE_CONFIG)
    set(LLVM_COMPILE_CONFIG)
    if(LLVM_CONFIG)
        message(STATUS "Found LLVM_CONFIG as ${LLVM_CONFIG}")
        set(CONFIG_COMMAND ${LLVM_CONFIG}
            "--cxxflags")
        execute_process(
            COMMAND ${CONFIG_COMMAND}
            RESULT_VARIABLE HAD_ERROR
            OUTPUT_VARIABLE LLVM_COMPILE_CONFIG
        )
        if(NOT HAD_ERROR)
            message(STATUS "bbbbb")
            string(REGEX REPLACE "[ \t]*[\r\n]+[ \t]*" " " LLVM_COMPILE_CONFIG ${LLVM_COMPILE_CONFIG})
        else()
            string(REPLACE ";" " " CONFIG_COMMAND_STR "${CONFIG_COMMAND}")
            message(STATUS "${CONFIG_COMMAND_STR}")
            message(FATAL_ERROR "llvm-config failed with status ${HAD_ERROR}")
        endif()
    else()
        message(FATAL_ERROR "llvm-config not found -- ${LLVM_CONFIG}")
    endif()
endmacro(SET_LLVM_COMPILE_CONFIG)

macro(SET_LLVM_LINK_CONFIG)
    set(LLVM_LINK_CONFIG)
    if(LLVM_CONFIG)
        message(STATUS "Found LLVM_CONFIG as ${LLVM_CONFIG}")
        set(CONFIG_COMMAND ${LLVM_CONFIG}
            "--cxxflags"
            "--ldflags"
            "--libs"
            "--libfiles"
            "--system-libs"
            "core")
        execute_process(
            COMMAND ${CONFIG_COMMAND}
            RESULT_VARIABLE HAD_ERROR
            OUTPUT_VARIABLE LLVM_LINK_CONFIG
        )
        if(NOT HAD_ERROR)
            string(REGEX REPLACE "\n" " " LLVM_LINK_CONFIG ${LLVM_LINK_CONFIG})
        else()
            string(REPLACE ";" " " CONFIG_COMMAND_STR "${CONFIG_COMMAND}")
            message(STATUS "${CONFIG_COMMAND_STR}")
            message(FATAL_ERROR "llvm-config failed with status ${HAD_ERROR}")
        endif()
    else()
        message(FATAL_ERROR "llvm-config not found -- ${LLVM_CONFIG}")
    endif()
endmacro(SET_LLVM_LINK_CONFIG)

macro(SET_CMAKE_PARAMETER)
    set(CMAKE_C_LINK_EXECUTABLE "/usr/bin/clang++")
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
endmacro(SET_CMAKE_PARAMETER)