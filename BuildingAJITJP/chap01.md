# Introduction

注意：　このチュートリアルは，最近のORCのアップデートに追いついていません．１〜２章までの文章は更新されていますが，３〜５章のコードは，コンパイル・実行できますが，更新されていません．

LLVMでORCベースのJITを作ろうにようこそ．
このチュートリアルは，LLVMのOn-Request-Compilation (ORC) APIを使って，JITコンパイラを作ることを目的にしています．
LLVMチュートリアルの言語の実装に使われている`KaleidoscopeJIT`クラスの簡単なバージョンから初めて，並列コンパイル，最適化，遅延コンパイル，遠隔実行などの新しい機能を紹介します．

このチュートリアルの目標は，ORC JIT APIを紹介し，LLVMの他の機能とこれらのAPIのinteractのやり方や，読者のユースケースにマッチしたカスタムJITを構築するために，それらをどうやって結合すればいいのかを説明します．

チュートリアルの構成は，以下のようになります．

* 1章：簡単な`KaleidoscopeJIT`クラスの解説．ここでは，ORCレイヤーの考え方を含む，ORC JIT APIの基本的な概念を紹介します．
* 2章：LLVM IRを最適化したり，コードを生成する新しいレイヤーを基本の`KaleidoscopeJIT`クラスに追加します．
* 3章：IRを遅延コンパイルするためのCompile-On-Demandレイヤーを追加します．
* 4章：関数が呼ばれるまで，IRの生成を遅延させるために，ORCのコンパイルコールバックAPIを直接使うカスタムレイヤーで，Compile-On-Demandレイヤーを置き換えることによって，自作のJITの遅延を実現します．
* 5章：JITリモートAPIを使って，限定された特権でリモートプロセスに，コードをJITコンパイルすることで，"process isolation"を追加します．

別ドキュメントの7章の“Implementing a language in LLVM tutorial”から，Kaleidoscope REPLをちょっと改造したバージョンを使うことになります．

最終的に，APIの世代で言うと，ORCは，LLVMのJIT APIの第三世代になります．
ORCは，MCJITに続く世代です．
このチュートリアルは，以前のバージョンのJIT APIでの開発経験を前提に書いたものではないですが，そういった経験や知識があると，よりそれぞれの要素がわかりやすくなるでしょう．
古いバージョンからORCへの移行が楽になるように，説明では，折りを見て，明確に古いAPIとORCの関連について説明することにします．

# JIT APIの基礎

JITコンパイラの目的は，古くからあるコンパイラのように先にコンパイルしておくのではなく，必要に応じて，その場でコードをコンパイルすることです．
これらの基本的な目的をサポートするために，二つの生のJIT APIがあります．

```
// IRモジュールを実行可能にする
Error addModule(std::unique_ptr<Module> M);
// JITに追加されるシンボル（関数や変数）へのポインタを探す
Expected<JITEvaluatedSymbol> lookup();
```

このAPIの基本的な使い方は，実行するmain関数を実行する時に使います．
例えば，以下のような形式になります．

```
JIT J;
J.addModule(buildModule());
auto *Main = (int(*)(int, char*[]))J.lookup("main").getAddress();
int Result = Main();
```

このチュートリアルで作っていくAPIは，このシンプルなテーマの派生です．
このAPIを使って，並列コンパイル，最適化，遅延コンパイルをサポートするためのJITの実装を洗練させていくことになります．
最後に，JITに抽象構文木のような高いレベルの表現を可能にするように拡張していくことになります．

# KaleidoscopeJIT

前節で紹介したAPIを使って，ここで，LLVMチュートリアルの言語を実装するために使った`KaleidoscopeJIT`クラスを調べていくことにします．
今から作っていくJITへの入力として，チュートリアルの7章からREPLコードを使います．
このチュートリアルでは，ユーザがexpressionを入力するたびに，REPLが，JITにその表現に該当するコードを含むIRモジュールを追加していくものでした．
もし，そのexpressionが，`1+1`や`sin(x)`のようなtop-level expressionである場合，REPLは，JITクラスのルックアップメソッドを，expressionに対するコードを探したり，実行したりするために使います．
さらに進んだ章では，JITとインタラクションできるようにREPLを改造していきますが，今のところ，この設定を前提として，JITの実装に注目していくことにします．

`KaleidoscopeJIT`は，[KaleidoscopeJIT.h](https://github.com/llvm-mirror/llvm/blob/master/examples/Kaleidoscope/include/KaleidoscopeJIT.h)で定義されます．

```

#ifndef LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H
#define LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include <memory>

namespace llvm {
namespace orc {

class KaleidoscopeJIT {
private:
  ExecutionSession ES;
  RTDyldObjectLinkingLayer ObjectLayer;
  IRCompileLayer CompileLayer;

  DataLayout DL;
  MangleAndInterner Mangle;
  ThreadSafeContext Ctx;

public:
  KaleidoscopeJIT(JITTargetMachineBuilder JTMB, DataLayout DL)
      : ObjectLayer(ES,
                    []() { return llvm::make_unique<SectionMemoryManager>(); }),
        CompileLayer(ES, ObjectLayer, ConcurrentIRCompiler(std::move(JTMB))),
        DL(std::move(DL)), Mangle(ES, this->DL),
        Ctx(llvm::make_unique<LLVMContext>()) {
    ES.getMainJITDylib().setGenerator(
        cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(DL)));
  }
```

このクラスには，６つのメンバがあります．

1. `ExecutionSession` as `ES`．文字列プール，グローバルのmutex，エラー報告のための基盤などを含むJITコンパイルされたコードのためのコンテキストを供給するためのものです．
2. `RTDyldObjectLinkingLayer` as `ObjectLayer`． （直接使うことはないですが）JITにオブジェクトファイルを追加するために使います．
3. `IRCompileLayer` as `CompileLayer`．JITにLLVMモジュールを追加するために使います．モジュールは，ObjectLayer上に構築されます．
4. `DataLayout` as `DL`，`MangleAndInterner` as `Mangle`．これは，シンボルをマングリングするために使われます．
5. `ThreadSafeContext` as `Ctx`．JITのためのIRファイルを構築するときにクライアントが使用します．

次にクラスのコンストラクタを見ていきます．
コンストラクタは，`JITTargetMachineBuilder`と`DataLayout`を引数にとります．
`JITTargetMachineBuilder`は，`IRCompiler`によって使われ，`DataLayout`は，メンバ変数`DL`の初期化に使われます．
コンストラクタは，`ObjectLayer`の初期化から始まります．
`ObjectLayer`は，`ExecutionSession`へのリンクと，追加されるそれぞれのモジュールに対応するJITのメモリマネージャをビルドする関数オブジェクトを必要とします．
(JITメモリマネージャは，メモリ確保，メモリのアクセス権，JITコンパイルされたコードへの例外ハンドラの登録などを管理します．)
ここでは，この章で学ぶことに十分な基本的なメモリマネージメント機能を持ったそのまま使えるユーティリティである`SectionMemoryManager`を返すラムダ式を使うことにします．
次に，`CompileLayer`を初期化します．
`CompileLayer`は，以下の三つを必要とします．

1. `ExecutionSession`への参照．
2. オブジェクトレイヤーへの参照．
3. IRからオブジェクトファイルへの実際のコンパイルを実行するために使われるコンパイラのインスタンス

`ConcurrentIRCompiler`ユーティリティをそのまま使います．
これは，コンストラクタの`JITTargetMachineBuilder`引数を使って初期化できます．
`ConcurrentIRCompiler`ユーティリティは，コンパイラに必要とされる`llvm TargetMachines`（これはスレッドセーフでありません）をビルドするために，`JITTargetMachineBuilder`を使います．
この後，`DataLayout`，`MangleAndInterner`，`ThreadSafeContext`などのサポートメンバ変数を初期化します．
`DL`は，引数の`DataLayout`で，`Mangler`は，`ExecutionSession`とメンバ変数の`DL`，`Ctx`は，デフォルトのコストラクタで初期化します．
これで，全部のメンバ変数が初期化されたので，残るやるべきことをひとつやります．
それは，コード保存する`JITDylib`の設定の微調整です．
この`dylib`が，そこに追加していくシンボルだけではなく，REPLのプロセスから入力されるシンボルも保持できるように，それを改造したい訳です．
ここで，`DynamicLibrarySearchGenerator::GetForCurrentProcess`メソッドを使って，`DynamicLibrarySearchGenerator`を，`dylib`にアタッチすることでこれを実現します．

```
static Expected<std::unique_ptr<KaleidoscopeJIT>> Create() {
  auto JTMB = JITTargetMachineBuilder::detectHost();

  if (!JTMB)
    return JTMB.takeError();

  auto DL = JTMB->getDefaultDataLayoutForTarget();
  if (!DL)
    return DL.takeError();

  return llvm::make_unique<KaleidoscopeJIT>(std::move(*JTMB), std::move(*DL));
}

const DataLayout &getDataLayout() const { return DL; }

LLVMContext &getContext() { return *Ctx.getContext(); }
```

次に，名前のついたコンストラクタ`Create`について説明します．
`Create`は，ホストのプロセスのためのコードを生成するために`KaleidoscopeJIT`のインスタンスをビルドします．
この関数は，クラスの`detectHost`メソッドを使って，`JITTargetMachineBuilder`をはじめに生成し，ターゲットプロセスのためのデータレイアウトを生成するインスタンスを使い，それを実行します．
これらの処理は失敗する可能性があり，`Expected`型の値にラップされて返されるため，続ける前にエラーをチェックしなければなりません．
両方の操作が成功した場合，dereferenceオペレータを使って，結果をアンラップし，関数の最後の行にあるKaleidoscopeJITのコンストラクタに渡すことできます．

名前のついたコンストラクタに続いて，`getDataLayout()`と`getContext()`についてです．
これらは，JITによって（特にコンテキスト）作成・管理されるデータ構造を，IRモジュールをビルドするREPLコードが利用できるようにするために使われます．

```
void addModule(std::unique_ptr<Module> M) {
  cantFail(CompileLayer.add(ES.getMainJITDylib(),
                            ThreadSafeModule(std::move(M), Ctx)));
}

Expected<JITEvaluatedSymbol> lookup(StringRef Name) {
  return ES.lookup({&ES.getMainJITDylib()}, Mangle(Name.str()));
}
```

`addModule`が，初めてのJIT APIのメソッドになります．
このメソッドは，IRをJITに追加したり，実行可能にする機能を担います．
今回のJITの初期実装では，`CompileLayer`に，IRを追加することで，作ったモジュール"実行可能に"します．
この実装は，その`Module`をメインの`JITDylib`に保存します．
この処理は，モジュールにあるそれぞれの定義のための`JITDylib`にある新しいシンボルテーブルのエントリを作成し，その定義のどれかがルックアップされるまで，モジュールのコンパイルを遅延させます．
注意して欲しいのは，これは遅延コンパイルではなく，それが使われたときに初めて定義を参照するというだけです．
関数が実際呼ばれるまで，コンパイルを遅延させるのは，もっと後の章になってから説明します．
`Module`を追加するためには，そのインスタンスを，`ThreadSafeModule`でラップする必要があります．
`ThreadSafeModule`は，`Module`の`LLVMContext`のライフタイムをスレッドセーフに管理します．
このサンプルでは，すべてのモジュールは，`Ctx`メンバーを共有することになります．
JITが生きている間，`Ctx`は，存在することになります．
後の章で，並列コンパイルに切り替えるときには，我々は，モジュールごとに新しいコンテキストを使うことになります．

最後は`lookup`です．
`lookup`は，関数や変数のシンボル名を使って，JITに追加された，それらの定義のアドレスをルックアップできるようにします．
上で書いたように，`lookup`は，そのときにまだコンパイルされていない暗黙にシンボルに対するコンパイルのトリガーとなります．
実装する`lookup`は，検索するための`dylib`（今回のサンプルではメインのひとつだけ）のリスト，検索するシンボルの名前を`ExecutionSession::lookup`に渡します．
この名前には，ひと工夫必要で，はじめに検索しようとしているシンボルの名前をマングリングする必要があります．
ORC JITのコンポーネントは，平文で書かれたIRのシンボル名ではなく，内部的にマングリングされたシンボルを使います．
これは，静的コンパイラもリンカも同様です．
この仕組みのおかげで，JITコンパイルされたコードは，事前にコンパイルされたアプリケーション内のコードや他の共有ライブラリと，簡単に相互運用できます．
マングリングの種類は，ターゲットのプラットホームに依存する，`DataLayout`に依存することになります．
移植性を維持したり，マングリングされていない名前で検索できるようにするためには，`Mangle`のメンバ関数を使って，自分自身でマングリングする必要があります．

"Building a JIT"の第1章は，これで終わりです．
このコードは，基本ではありますが，JITが動いてるプロセスのコンテキストで，LLVM IRを実行可能し，利用するのに十分な機能を備えています．
次章では，より良いコードを生成するためにJITを拡張していく方法を紹介するとともに，ORCのレイヤーの概念についてより深く入り込んでいきます．