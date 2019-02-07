# オブジェクトコードにコンパイルする
## はじめに
８章にようこそ．
本章では，自分で作った言語をオブジェクトファイルにコンパイルする方法について説明する．
## ターゲットの選択
LLVMは，クロスコンパイルをサポートする．
今使っているマシンのアーキテクチャ用にコンパイルできるし，他のアーキテクチャのために簡単にコンパイルすることもできる．
このチュートリアルでは，現在使っているマシンをターゲットにする．
ターゲットにしたいアーキテクチャを指定するためには，“target triple”と呼ばれる文字列を使います．
これは，`<arch><sub>-<vendor>-<sys>-<abi>`のような形式をとります（詳しくはクロスコンパイルのドキュメントを読んでください）．
例えば，以下のコマンドで，clangが保持する，現在のターゲットの“target triple”をチェックできます．

```
$ clang --version | grep Target
Target: x86_64-unknown-linux-gnu
```

このコマンドを実行して表示される内容は，使っているマシンや，アーキテクチャ，OSなどによって異なります．
幸運にも，今使っているマシンの"target triple"をハードコードする必要はない．
LLVMは，今使っているマシンの"target triple"を返す，`sys::getDefaultTargetTriple`を提供する．

```
auto TargetTriple = sys::getDefaultTargetTriple();
```

LLVMは，ターゲットの機能性のすべてをリンクすることを要求しない．
例えば，もし，JITを使っているのであれば，アセンブリを表示する必要はない．
同じように，もし，あるアーキテクチャのみをターゲットとしているなら，それらのアーキテクチャのための機能だけをリンクできる．

例えば，以下のようなコードでオブジェクトコードを発行するために，すべてのターゲットを初期化する．

```
InitializeAllTargetInfos();
InitializeAllTargets();
InitializeAllTargetMCs();
InitializeAllAsmParsers();
InitializeAllAsmPrinters();
```

`Target`オブジェクトを取得するために，"target triple"を使う．

```
std::string Error;
auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

// Print an error and exit if we couldn't find the requested target.
// This generally occurs if we've forgotten to initialise the
// TargetRegistry or we have a bogus target triple.
if (!Target) {
  errs() << Error;
  return 1;
}
```

## ｀TargetMachine`
次に`TargetMachine`クラスが必要である．
このクラスは，現在ターゲットとしているマシンの完全な説明を提供してくれる．
もし，SSEのような特定の機能や，IntelのSandylakeのような特定のCPUをターゲットとしたい場合，このクラスを使う．

LLVMが知っている機能やCPUを知るために，`llc`コマンドを使える．

```
$ llvm-as < /dev/null | llc -march=x86 -mattr=help
Available CPUs for this target:

  amdfam10      - Select the amdfam10 processor.
  athlon        - Select the athlon processor.
  athlon-4      - Select the athlon-4 processor.
  ...

Available features for this target:


  16bit-mode            - 16-bit mode (i8086).
  32bit-mode            - 32-bit mode (80386).
  3dnow                 - Enable 3DNow! instructions.
  3dnowa                - Enable 3DNow! Athlon instructions.
  ...
```

今のサンプルでは，追加機能やオプション，"relocation model（なにこれ）"はなしで，一般的なCPUを使うことにする．

```
auto CPU = "generic";
auto Features = "";

TargetOptions opt;
auto RM = Optional<Reloc::Model>();
auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
```

## `Module`の初期化
これで，ターゲットやデータレイアウトを指定するためにモジュールを設定する準備が整った．
これは，厳密には必要はないが，フロントエンドのパフォーマンスガイドとしては，これをすることを勧める．
最適化は，ターゲットやデータレイアウトに関する知識から利益を得る．

```
TheModule->setDataLayout(TargetMachine->createDataLayout());
TheModule->setTargetTriple(TargetTriple);
```

## オブジェクトコードの発行
さぁ，オブジェクトコードを発行しよう．
まず，書き出したいファイルの場所を定義する．

```
auto Filename = "output.o";
std::error_code EC;
raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

if (EC) {
  errs() << "Could not open file: " << EC.message();
  return 1;
}
```

最終的に，オブジェクトコードを発行するパスを定義する．
そして，そのパスを実行する．

```
legacy::PassManager pass;
auto FileType = TargetMachine::CGFT_ObjectFile;

if (TargetMachine->addPassesToEmitFile(pass, dest, FileType)) {
  errs() << "TargetMachine can't emit a file of this type";
  return 1;
}

pass.run(*TheModule);
dest.flush();
```

## 全部一緒に
動作しただろうか．
試してみよう．
自分のコードをコンパイルする必要があるが，ここまでの章のコンパイルと`llvm-config`の引数が多少違うことに注意してほしい．

```
$ clang++ -g -O3 toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs all` -o toy
```

さぁ，動作させてみて，単純な平均を計算する関数を実装してみよう．
コーディングが終わったときにCtrl-Dでプログラムを終了させると，オブジェクトファイルが出力される．

```
$ ./toy
ready> def average(x y) (x + y) * 0.5;
^D
Wrote output.o
```

これで，オブジェクトファイルができた．
テストしてみよう．
C++でこのオブジェクトファイルをリンクし，それをコールするコードを書いてみよう．
例えば，以下のようなコードになる．

```
#include <iostream>

extern "C" {
    double average(double, double);
}

int main() {
    std::cout << "average of 3.0 and 4.0: " << average(3.0, 4.0) << std::endl;
}
```

以下のようにして，コンパイル，リンクし，実行してみよう．

```
$ clang++ main.cpp output.o -o main
$ ./main
average of 3.0 and 4.0: 3.5
```
