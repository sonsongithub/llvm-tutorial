LLVM_CONFIG="<path to llvm-config>"
clang++ -c ./main.cpp -o ./main.o `${LLVM_CONFIG} --cxxflags`
clang++ -o ./a.out ./main.o `${LLVM_CONFIG} --ldflags --libs --libfiles --system-libs`