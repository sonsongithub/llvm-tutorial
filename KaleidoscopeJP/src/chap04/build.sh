LLVM_CONFIG="/usr/local/Cellar/llvm/10.0.1_1/bin/llvm-config"
clang++ -c ./main.cpp -o ./main.o `${LLVM_CONFIG} --cxxflags`
clang++ -o ./a.out ./main.o `${LLVM_CONFIG} --ldflags --libs --libfiles --system-libs core`