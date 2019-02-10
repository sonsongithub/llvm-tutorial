#clang++ --std=c++14 ./main.cpp `llvm-config --cxxflags --ldflags --libs --libfiles --system-libs`
clang++ -c ./main.cpp -o ./main.o `llvm-config --cxxflags`
clang++ -o ./a.out ./main.o `llvm-config --ldflags --libs --libfiles --system-libs`