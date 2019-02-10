clang++ -c ./main.cpp -o ./main.o `llvm-config --cxxflags`
clang++ -o ./a.out ./main.o