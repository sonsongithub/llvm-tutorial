# JIT Sample codes

## MCJIT

MCJIT is the second generation of the JIT compiler of LLVM(MC means "Machine Code?").
MCJIT is supported on Since LLVM 2.9 to the present version.
It is already recommended to migrate the MCJIT code to ORC.
Old Kaleidoscope source codes were based on MCJIT.
If you want to extract the code to JIT compile from such as sample code(old kaleidoscope), refer this codes.

[MCJIT sample code](./jit_mc.cpp)

## ORC

ORC means "On Request Compilation".
ORC development has focused on supporting for the concurrent JIT compilation, lazy compiliation and isolation some layers. 

From [4],
> The majority of the ORCv1 layers and utilities were renamed with a ‘Legacy’ prefix in LLVM 8.0, and have deprecation warnings attached in LLVM 9.0. In LLVM 12.0 ORCv1 will be removed entirely.

If you want to extract the code to JIT compile, refer this codes.

[ORC sample code](./jit_orc_lljit.cpp)

## Preferences

1. https://llvm.org/devmtg/2011-11/Grosbach_Anderson_LLVMMC.pdf
2. https://llvm.org/devmtg/2016-11/Slides/Hames-ORC.pdf
3. https://llvm.org/docs/MCJITDesignAndImplementation.html
4. https://llvm.org/docs/ORCv2.html