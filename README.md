## llvm-tutorial
1. LLVM Tutorialを勉強するリポジトリ

## How to compile samples

clang++ --std=c++14 ./sample.cpp `llvm-config --cxxflags --ldflags --libs --libfiles --system-libs`