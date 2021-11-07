[オリジナル](https://llvm.org/docs/tutorial/BuildingAJIT2.html)

# Building a JIT: Adding Optimizations – An introduction to ORC Layers

Chapter 2 Introduction
Optimizing Modules using the IRTransformLayer
Full Code Listing
This tutorial is under active development. It is incomplete and details may change frequently. Nonetheless we invite you to try it out as it stands, and we welcome any feedback.

# JITを作ろう：最適化を加える - ORCレイヤーの紹介

## 第二章　IRTransformLayerを使ったモジュールの最適化

このチュートリアルは鋭意執筆中です．
完全ではありませんし，たびたび，詳細が変わりうります．
しかしながら，このまま読んで，試していただき，フィードバックを待ちたいと思います．

## Introduction

> 注意：　このチュートリアルは，最近のORCのアップデートに追いついていません．１〜２章までの文章は更新されていますが，３〜５章のコードは，コンパイル・実行できますが，更新されていません．

第二章にようこそ．
このシリーズの第一章では，基本的なJITクラス`KaleidoscopeJIT`を取り上げました．
`KaleidoscopeJIT`クラスは，LLVM IRモジュールを引数にとり，メモリ上に実行可能なコードを生成します．
KaleidoscopeJITは，`IRCompileLayer`と`ObjectLinkingLayer`というORCのレイヤーをそのまま組み合わせることで，従来は，重労働であったのに，短いコードでこれを実現します．
この章では，`KaleidoscopeJIT`にIRの最適化機能を追加するために，`IRTransformLayer`という新しいレイヤーを使って，ORCのレイヤーの概念をさらに学んでいきます．

## `IRTransformLayer`を使ったモジュールの最適化

“Implementing a language with LLVM”チュートリアルの四章で，llvmの`FunctionPassManager`をLLVM IRを最適化するための手段として紹介しました．
この章の細部まで読んだ読者は，それが最適化の集合体であり，`Module`上でそのPassManagerを走らせると，意味的には同じ形式で，モジュールをより最適なものに変換するものである（つまり手短に言うと，我々は，モジュールを最適化するために`llvm::FunctionPassManager`のインスタンスを作成する）理解していると思います．
チュートリアルでは，`FunctionPassManager`は，`KaleidoscopeJIT`の外側で作成され，モジュールは，それが追加される前に最適化されました．
この章では，代わりに，JITのタイミングで最適化するようにします．

ここから，この章では，ORCのレイヤーについて学ぶ動機を解説しますが，長期的な観点から，最適化をJITの一部とすることは，重要な利益になります．
つまり，遅延コンパイルを始める時に（例えば，それぞれの関数のコンパイルを初めて実行される時まで遅延するなど），JITによって最適化が管理されているということは，すべての最適化を事前に行うのではなく，最適化も遅延実行できることを意味します．

JITに最適化のサポートを追加するため、`KaleidoscopeJIT`を第一章から取得し、ORCの`IRTransformLayer`をトップに組み込む。まず、`IRTransformLayer`がどのように動作するのかを見ていくことにします。しかし、そのインタフェースは、シンプルです。つまり、このレイヤーのためのコンストラクタは、execution sessionとレイヤー、さらにIR最適化関数への参照を引数に取ります。
そのIR最適化関数は、`addModule`通して、それぞれの`Module`へ適応されます。

```
class KaleidoscopeJIT {
private:
  ExecutionSession ES;
  RTDyldObjectLinkingLayer ObjectLayer;
  IRCompileLayer CompileLayer;
  IRTransformLayer TransformLayer;

  DataLayout DL;
  MangleAndInterner Mangle;
  ThreadSafeContext Ctx;

public:

  KaleidoscopeJIT(JITTargetMachineBuilder JTMB, DataLayout DL)
      : ObjectLayer(ES,
                    []() { return llvm::make_unique<SectionMemoryManager>(); }),
        CompileLayer(ES, ObjectLayer, ConcurrentIRCompiler(std::move(JTMB))),
        TransformLayer(ES, CompileLayer, optimizeModule),
        DL(std::move(DL)), Mangle(ES, this->DL),
        Ctx(llvm::make_unique<LLVMContext>()) {
    ES.getMainJITDylib().setGenerator(
        cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(DL)));
  }
```

拡張した`KaleidoscopeJIT`クラスは，第一章と同じところから始まるが，`CompileLayer`の後に，`TransformLayer`を新しいメンバとして加える．
`OptimizeLayer`を`ExecutionSession`とアウトプットレイヤー，変換の関数への参照で初期化する．
我々が使う変換の関数は，`optimizeModule`のstaticメソッドとして定義する．

```
// ...
return cantFail(OptimizeLayer.addModule(std::move(M),

```                                        std::move(Resolver)));
// ...
```

次に，`CompileLayer::add`の呼び出しを，`OptimizeLayer::add`の呼び出しに置き換えるため，`addModule`メソッドを更新する必要がある．

```
static Expected<ThreadSafeModule>
optimizeModule(ThreadSafeModule M, const MaterializationResponsibility &R) {
  // Create a function pass manager.
  auto FPM = llvm::make_unique<legacy::FunctionPassManager>(M.get());

  // Add some optimizations.
  FPM->add(createInstructionCombiningPass());
  FPM->add(createReassociatePass());
  FPM->add(createGVNPass());
  FPM->add(createCFGSimplificationPass());
  FPM->doInitialization();

  // Run the optimizations over all functions in the module being added to
  // the JIT.
  for (auto &F : *M)
    FPM->run(F);

  return M;
}
```

JITの最後の部分で，実際の最適化を実行するためのプライベートメソッド`optimizeModule`を追加します．
この関数は，`ThreadSafeModule`型で，入力して変換されるモジュールと，新しいクラス`MaterializationResponsibility`への参照を引数に取ります．
`MaterializationResponsibility`引数は，例えば，JITコンパイルされたコードが呼び出し，アクセスを試みる，モジュール内の定義と言った，モジュールを変換するために，JITの状態を問い合わせるために使われます．
ここからは，この引数は無視することにして，一般的な最適化のパイプラインを使います．
これをするために， `FunctionPassManager`をセットアップし，いくつかのパスをそれに追加し，モジュールにある関数ごとに最適化を実行させ，変換されたモジュールを受け取ります．
指定した最適化は，“Implementing a language with LLVM”の４章で使ったものと同じです．
これらの議論や一般的なIRの最適化を深く理解するなら，その章を読み直した方がよいでしょう．

`KaleidoscopeJIT`に加える変更という観点では，モジュールが`addModule`を通して追加されるとき，`OptimizeLayer`は，以下で説明する`CompileLayer`へ変換されたモジュールを渡す前に，`optimizeModule`関数を呼ぶことになります．
もちろん，わざわざ`IRTransformLayer`を使わずに，`addModule`関数の中で`optimizeModule`を直接呼び出すこともできますが，そうすることで，レイヤーがどのように構成されているかを確認することができます．
そして，それは，レイヤーのコンセプトにわかりやすい取っ付きを与えてくれます．
なぜなら，`IRTransformLayer`は，その中でももっとも実装しやすい，シンプルなレイヤーだからです．

```
// From IRTransformLayer.h:
class IRTransformLayer : public IRLayer {
public:
  using TransformFunction = std::function<Expected<ThreadSafeModule>(
      ThreadSafeModule, const MaterializationResponsibility &R)>;

  IRTransformLayer(ExecutionSession &ES, IRLayer &BaseLayer,
                   TransformFunction Transform = identityTransform);

  void setTransform(TransformFunction Transform) {
    this->Transform = std::move(Transform);
  }

  static ThreadSafeModule
  identityTransform(ThreadSafeModule TSM,
                    const MaterializationResponsibility &R) {
    return TSM;
  }

  void emit(MaterializationResponsibility R, ThreadSafeModule TSM) override;

private:
  IRLayer &BaseLayer;
  TransformFunction Transform;
};
```

```
// From IRTransfomrLayer.cpp:

IRTransformLayer::IRTransformLayer(ExecutionSession &ES,
                                   IRLayer &BaseLayer,
                                   TransformFunction Transform)
    : IRLayer(ES), BaseLayer(BaseLayer), Transform(std::move(Transform)) {}

void IRTransformLayer::emit(MaterializationResponsibility R,
                            ThreadSafeModule TSM) {
  assert(TSM.getModule() && "Module must not be null");

  if (auto TransformedTSM = Transform(std::move(TSM), R))
    BaseLayer.emit(std::move(R), std::move(*TransformedTSM));
  else {
    R.failMaterialization();
    getExecutionSession().reportError(TransformedTSM.takeError());
  }
}
```

これが，`llvm/include/llvm/ExecutionEngine/Orc/IRTransformLayer.h`と`llvm/lib/ExecutionEngine/Orc/IRTransformLayer.cpp`にある`IRTransformLayer`の実装です．
このクラスは，ふたつとてもシンプルな仕事にかかわっています．
一つ目は，変換関数のオブジェクトを介し，このレイヤーを通じて発行されたIRモジュールを実行することです．
ふたつ目は，ORCの`IRLayer`のインタフェースを実装することです(このクラス自身がORCレイヤーのコンセプトに沿ったものになっています．詳細は後で解説します)．

クラスのほとんどは，安直に実装されています．
変換の関数のためのtypedef，メンバーの初期化をしてるコンストラクタ，変換関数のセッター，デフォルトの最適化なしの変換です．
最も重要なメソッドは，`emit`で，このメソッドは，`IRLayer`のインタフェースの半分を占めます．
`emit`は，我々の変換をそれぞれのモジュールに施す．
`emit`が呼ばれたときに，変換が成功すると，ベースレイヤーに変換されたモジュールを渡す．
また，変換が失敗したときには，この関数は，そのスレッドを終了する前に，`MaterializationResponsibility::failMaterialization`（このJITクライアントは，コンパイルを待っているコードがコンパイルに失敗したことを他の待機中のスレッド上，知ることになる）を呼び，エラーログを出します．

`IRLayer`のインタフェースの残りの半分は，`IRLayer`クラスから，何も変えずに流用することにします．

```
Error IRLayer::add(JITDylib &JD, ThreadSafeModule TSM, VModuleKey K) {
  return JD.define(llvm::make_unique<BasicIRLayerMaterializationUnit>(
      *this, std::move(K), std::move(TSM)));
}
```

`llvm/lib/ExecutionEngine/Orc/Layer.cpp`からとってきたこのコードは，`ThreadSafeModule`を`MaterializationUnit`の中に（このケースでは，`BasicIRLayerMaterializationUnit`）くるんで，与えられた`JITDylib`に追加します．
ほとんどのレイヤーは，`IRLayer`から派生したもので，`add`メソッドのデフォルト実装をベースにしています．

`add`と`emit`というふたつの操作は，一緒にレイヤーのコンセプトを構成しています．
つまり，レイヤーは，コンパイラのパイプラインの一部をラップしたものであり，そのパイプラインのAPIは，ORCに対して透過的ではないが，インタフェースを使って，必要な時にORCから呼び出すことができる．
`add`メソッドは，入力として，コードの表現を`module`で受け取り（今回は，LLVM IR module），ターゲットとなる`JITDylib`に保存する．また，`add`メソッドは，受け取ったモジュール内で定義されたシンボルが要求されたときに，レイヤーの`emit`メソッドに，モジュールに引き渡すように処理する．
この一連のタスクは，ベースレイヤーの`emit`メソッドを呼び出すことで，完了する．
例えば，このチュートリアルの`IRTransformLayer`は，変換されたIRをコンパイルするために，`IRCompileLayer`に引き渡し，`IRCompileLayer`は，コンパイラによって生成されたオブジェクトファイルをその場でリンクするために，`ObjectLayer`へ引き渡すといった風になる．

ここまで，LLVM IRを最適化し，コンパイルする方法について学んできたが，コンパイルが必要になるタイミングについては，考えてこなかった．
我々が作ったREPLは，それぞれの関数が他のコードから参照されると，それが実行時に実際に呼ばれるかどうかにかかわらず，すぐに最適化され，コンパイルされます．
次章では，我々は，その関数が実行時に初めて呼ばれるまでコンパイルしない，完全な遅延コンパイルを紹介します．
ここで，面白いトレードオフが発生する．
それは，遅延コンパイルを行うと，一番最初の関数を実行するまでの時間は短縮されるが，新しい関数が呼び出されるたびにコンパイルのために一時的に処理を停止する必要が出てくることだ．
もし，コード生成だけを遅延化し，最適化は念入りにやりたいのであれば，起動時にすべてを最適化するように長い処理時間を使うことになるが，関数呼び出し時には，それぞれの関数のコード生成だけをやることになるので，相対的に停止時間が短くなるかもしれない．
最適化とコード生成の両方を遅延したいときには，最初の関数呼び出しは，かなり高速化できるが，ある関数が最初に実行されたときに，最適化とコード生成の両方を行う必要があるので，停止時間は，より長くなる．
インラインのような手続き間の最適化を考慮する場合は，問題はよりおもしろいものになる．
これらは，複雑なトレードオフであり，それらに合うすべての解決方法は存在しないが，構成可能なレイヤーを提供することで，JITを実装しようとする人に，その決定権を渡すことはできる．
そして，その実装する人は，レイヤーをうまく組み合わせ，異なる状況下での実験を簡単に組み立てられる．

## コードリスト

# コンパイル

```
clang++ -g toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native` -O3 -o toy
```

# 実行

```
./toy
```

```
//===- KaleidoscopeJIT.h - A simple JIT for Kaleidoscope --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Contains a simple JIT definition for use in the kaleidoscope tutorials.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H
#define LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <memory>

namespace llvm {
namespace orc {

class KaleidoscopeJIT {
private:
  ExecutionSession ES;
  RTDyldObjectLinkingLayer ObjectLayer;
  IRCompileLayer CompileLayer;
  IRTransformLayer OptimizeLayer;

  DataLayout DL;
  MangleAndInterner Mangle;
  ThreadSafeContext Ctx;

public:
  KaleidoscopeJIT(JITTargetMachineBuilder JTMB, DataLayout DL)
      : ObjectLayer(ES,
                    []() { return llvm::make_unique<SectionMemoryManager>(); }),
        CompileLayer(ES, ObjectLayer, ConcurrentIRCompiler(std::move(JTMB))),
        OptimizeLayer(ES, CompileLayer, optimizeModule),
        DL(std::move(DL)), Mangle(ES, this->DL),
        Ctx(llvm::make_unique<LLVMContext>()) {
    ES.getMainJITDylib().setGenerator(
        cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
            DL.getGlobalPrefix())));
  }

  const DataLayout &getDataLayout() const { return DL; }

  LLVMContext &getContext() { return *Ctx.getContext(); }

  static Expected<std::unique_ptr<KaleidoscopeJIT>> Create() {
    auto JTMB = JITTargetMachineBuilder::detectHost();

    if (!JTMB)
      return JTMB.takeError();

    auto DL = JTMB->getDefaultDataLayoutForTarget();
    if (!DL)
      return DL.takeError();

    return llvm::make_unique<KaleidoscopeJIT>(std::move(*JTMB), std::move(*DL));
  }

  Error addModule(std::unique_ptr<Module> M) {
    return OptimizeLayer.add(ES.getMainJITDylib(),
                             ThreadSafeModule(std::move(M), Ctx));
  }

  Expected<JITEvaluatedSymbol> lookup(StringRef Name) {
    return ES.lookup({&ES.getMainJITDylib()}, Mangle(Name.str()));
  }

private:
  static Expected<ThreadSafeModule>
  optimizeModule(ThreadSafeModule TSM, const MaterializationResponsibility &R) {
    // Create a function pass manager.
    auto FPM = llvm::make_unique<legacy::FunctionPassManager>(TSM.getModule());

    // Add some optimizations.
    FPM->add(createInstructionCombiningPass());
    FPM->add(createReassociatePass());
    FPM->add(createGVNPass());
    FPM->add(createCFGSimplificationPass());
    FPM->doInitialization();

    // Run the optimizations over all functions in the module being added to
    // the JIT.
    for (auto &F : *TSM.getModule())
      FPM->run(F);

    return TSM;
  }
};

} // end namespace orc
} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H
```