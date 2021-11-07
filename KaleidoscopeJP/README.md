## llvm-tutorial
LLVM Tutorialを勉強するリポジトリ

## How to compile samples

KaleidoscopeのChapter04以降で必要になるdynamic linkのコードが一部ビルドで正しく動きません．
今の所，llvmのgithubミラーの`release_60`ブランチだと正しく動作するようです．
brewなどのパッケージでインストールされたllvmの場合，うまく動作しないので，githubから`release_60`ブランチのソースをチェックアウトし，コンパイルして使ってください．コンパイラは，macOSの場合，デフォルトのclang++を使えばよいようです．

### LLVMのビルド

```
git clone https://github.com/llvm-mirror/llvm.git
cd llvm
git checkout -b release_60 origin/release_60
mkdir build
cd build
CC=gcc CXX=g++                              \
cmake -DCMAKE_INSTALL_PREFIX=/usr           \
      -DLLVM_ENABLE_FFI=ON                  \
      -DCMAKE_BUILD_TYPE=Release            \
      -DLLVM_BUILD_LLVM_DYLIB=ON            \
      -DLLVM_LINK_LLVM_DYLIB=ON             \
      -DLLVM_TARGETS_TO_BUILD="host;"       \
      -DLLVM_BUILD_TESTS=ON                 \
      -Wno-dev -G Ninja ..                  &&
ninja
```

### LLVMのソース中のサンプルのビルドと動作確認

```
cd ./examples/Kaleidoscope/Chapter4/
g++ ./toy.cpp `../../../build/bin/llvm-config --cxxflags --ldflags --libs --libfiles --system-libs`
echo "extern printd(x);printd(1.0);" | ./a.out
```

正しくビルドできていれば，上記コードは，以下のような結果を出す．

```
ready> ready> Read extern: 
declare double @printd(double)

ready> ready> 1.000000
Evaluated to 0.000000
ready> ready> > Chapter4 sonson$ 
```

## Table of contents
1. [Chapter 01](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap01.md)
2. [Chapter 02](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap02.md)
3. [Chapter 03](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap03.md)
3. [Chapter 04](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap04.md)
3. [Chapter 05](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap05.md)
3. [Chapter 06](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap06.md)
3. [Chapter 07](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap07.md)
3. [Chapter 08](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap08.md)
3. [Chapter 09](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap09.md)
3. [Chapter 10](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap10.md)

## License
このリポジトリは，LLVMのソースコード，ドキュメントをベースに作成し，[LLVM Release License](http://releases.llvm.org/7.0.0/LICENSE.TXT)に従い，コンテンツを作成しています．
