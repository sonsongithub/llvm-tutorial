## llvm-tutorial
LLVM Tutorialを勉強するリポジトリ

## How to compile samples

```
clang++ --std=c++14 ./sample.cpp `llvm-config --cxxflags --ldflags --libs --libfiles --system-libs`
```

## Table of contents
1. [Chapter 01](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap01.md)
2. [Chapter 02](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap02.md)
3. [Chapter 03](https://github.com/sonsongithub/llvm-tutorial/blob/master/chap03.md)
4. To be written.

## License
このリポジトリは，LLVMのソースコード，ドキュメントをベースに作成し，[LLVM Release License](http://releases.llvm.org/7.0.0/LICENSE.TXT)に従い，コンテンツを作成しています．