clang++ --std=c++14 -c ./main.cpp -o ./main.o `llvm-config --cxxflags`
clang++ --std=c++14 -o ./a.out ./main.o `llvm-config --ldflags --libs --libfiles --system-libs core orcjit mcjit native`
