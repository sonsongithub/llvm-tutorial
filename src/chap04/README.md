このサンプルもmacOS, Ubuntuでllvm7.0では動作しない．
`printd`を呼び出すタイミングでクラッシュする・・・・・．

```
> sudo apt-get install llvm-6.0 llvm-6.0-examples llvm-6.0-tools clang-6.0
> sudo apt-get install llvm-7 llvm-7-tools llvm-7-examples clang-7
> cp -r /usr/share/doc/llvm-6.0-examples ~/llvm-6.0-examples
> cp -r /usr/share/doc/llvm-7-examples ~/llvm-7-examples
> cd ~/llvm-6.0-examples/examples/Kaleidoscope/include
> gzip -d ./KaleidoscopeJIT.h.gz
> cd ../Chapter4/
> gzip -d ./toy.cpp.gz 
> clang++-6.0 -g -rdynamic toy.cpp `llvm-config-6.0 --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 -o toy
> ./toy 
ready> extern printd(X);
ready> Read extern: 
declare double @printd(double)

ready> printd(1.0);
ready> 1.000000
Evaluated to 0.000000
ready> 
> cd ~/llvm-7-examples/examples/Kaleidoscope
> cd include/
> gzip -d ./KaleidoscopeJIT.h.gz 
> cd ../Chapter4
> gzip -d ./toy.cpp.gz 
> clang++-7 -g -rdynamic toy.cpp `llvm-config-7 --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 -o toy
> ./toy 
> ready> extern printd(X);
ready> Read extern: 
declare double @printd(double)

ready> printd(1.0);
ready> Segmentation fault (core dumped)
> 
```
