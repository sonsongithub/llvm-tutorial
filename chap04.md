# JITと最適化

## はじめに
４章へようこそ．
３章までは，シンプルな言語の実装や，LLVM IRを生成するためのコードなどの解説をしてきた．
本章では，自分で作った言語の最適化機能とJITコンパイラの二つ新しい技術について紹介する．
これらは，Kaleidoscope言語を，洗練し，高速にする．

## 瑣末な定数の折りたたみ
３章で紹介した内容は，エレガントで拡張が簡単である．
しかしながら，それは不幸にも，すごい，すばらしいコードを生成するかと言うとそうでもない．
一方で，`IRBuilder`は，シンプルなコードをコンパイルするとき，わかりやすく最適化してくれる．

```
ready> def test(x) 1+2+x;
Read function definition:
define double @test(double %x) {
entry:
    %addtmp = fadd double 3.000000e+00, %x
    ret double %addtmp
}
```

このコードは，入力をパースして，抽象構文木を直訳したものになっていない．
直訳したなら，以下のようになっているはずである．

```
ready> def test(x) 1+2+x;
Read function definition:
define double @test(double %x) {
entry:
    %addtmp = fadd double 2.000000e+00, 1.000000e+00
    %addtmp1 = fadd double %addtmp, %x
    ret double %addtmp1
}
```

上の例のような定数の折りたたみは，特によくあるケースであり，とても重要な最適化のひとつである．
多くの言語が，抽象構文木の表現の中で定数の折りたたみを実装している．

LLVMを使う場合，抽象構文木側（のパーサ？）で，これをサポートする必要はない．
すべての命令は，LLVM IRビルダーを通して，LLVM IRを構築するため，ビルダをコールしたときに，ビルダ自身が，定数折りたたみを実行するチャンスがあるかをチェックすることになる．
もし，折りたたみを実行する余地があるなら，ビルダは，定数折りたたみを実行し，計算のための命令を生成する代わりに，定数を返すことになる．

一方，`IRBuilder`は，インラインで分析することしかできないため，以下のようなちょっとややこしいコードの場合，うまく処理することができない．

```
ready> def test(x) (1+2+x)*(x+(1+2));
ready> Read function definition:
define double @test(double %x) {
entry:
        %addtmp = fadd double 3.000000e+00, %x
        %addtmp1 = fadd double %x, 3.000000e+00
        %multmp = fmul double %addtmp, %addtmp1
        ret double %multmp
}
```

このケースでは，乗算の左の項と，右の項は，まったく同じ値である．
この場合，我々が期待するのは，`x+3`を２回計算する代わりに，`temp = x + 3; result = temp * temp;`といったコードが生成されることである．

不幸にも，ローカルな分析をどれだけ積み上げても，このコードを最適化することはできない．
このコードは、二つのコードの変形を必要とする。
それは、冗長な加算命令を除去するための表現の結合と、共通部分式除去(CSE,評価する式に共通する部分を除去すること)である。
幸運にもllvmは、`passes`という形で誰もが使える幅広い最適化機能を提供してくれる。

## LLVMの最適化Pass

LLVMは、様々な問題をこなし、それぞれに異なるトレードオフを持つ多くの最適化経路を提供する。
他のシステムと異なり、llvmは、ある最適化の考え方が、すべての言語、すべてのコンテキストにおいて正しいとするような間違った考え方に固執しない。
LLVMは、コンパイラを実装する人に、どのオプティマイザを、どんな順番で、どんな状況で、を完全に指定させる。

確固たる例として、llvmは、モジュール全体を最適化するパスもまたサポートする。このときの全体最適化とは、リンク時にファイル全体を、あるいはプログラムの一部を、パスが最適できる限りコード全体を通して、最適化するものである。
LLVMは、他の関数は全く見ずに、同時に一つの関数ただ操作するだけの関数単位でのパスもサポートする。
パスの詳しい解説やどうやって実行するかについては、[How to Write a Pass](https://llvm.org/docs/WritingAnLLVMPass.html)や、[List of LLVM Passes](https://llvm.org/docs/Passes.html)を参考にされたい。

Kaleidosopeのために、我々は、今、関数を即座に、すぐに、あるいはユーザが入力すると同時に生成している。
この設定で、最高の最適化経験が得られることを我々は、目指しておらず、できる限り、簡単かつすぐ作れるコードを作ることを目的としている。
そうであるとは言い難いが、我々は、ユーザが関数を入力したときに、関数ごとに動作する最適化を走らせるサンプルを選ぶことにする。
もし、“静的なKaleidoscopeコンパイラ”を作りたいのなら、ファイル全体をパースしてしまう直前まで、オプティマイザを実行するのを延期することを除いて、今あるコードを使えばいいだろう。

関数ごとの最適化を実行するために、我々が実行したいllvmの最適化を構成する`FunctionPassManager`をセットアップする必要がある。
一度、これを実行すると、我々は、実行したい最適化のセットをmanagerに追加できる。
最適化したい、それぞれのモジュールごとに新しい`FunctionPassManager`が必要になる。
そのため、モジュールとpass managerの両方を作成し、初期化する関数を書く必要がある。

```
void InitializeModuleAndPassManager(void) {
  // Open a new module.
  TheModule = llvm::make_unique<Module>("my cool jit", TheContext);

  // Create a new pass manager attached to it.
  TheFPM = llvm::make_unique<FunctionPassManager>(TheModule.get());

  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->add(createInstructionCombiningPass());
  // Reassociate expressions.
  TheFPM->add(createReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->add(createCFGSimplificationPass());

  TheFPM->doInitialization();
}
```

このコードは、全体を管理するモジュール`TheModule`を初期化し、その`TheModule`にアタッチされる`TheFPM`(関数のパスマネージャ)を初期化する。
一度、パスマネージャがセットアップされると、我々は、llvmパスの塊を追加するために、直列につないだ`add`を使える。

この例では、4つの最適化passを追加している。
ここで選んだpassは、かなり標準的な,
様々なコードに使いやすい最適化のセットとなっている。
ここでは、それが何をするのか詳しく説明しないが、信じてほしい、これらのpassは、まず最初に使うのによい例なのだ。

一度、`PassManager`がセットアップされると、それを使う準備をする必要がある。
新しく作った関数が構成された（`FunctionAST::codegen()`の内部で実行される）後に、`PassManager`を実行することでそれができる。
ただし、その関数がクライアント側に返される前に準備を実行しなければならない。

```
if (Value *RetVal = Body->codegen()) {
  // Finish off the function.
  Builder.CreateRet(RetVal);

  // Validate the generated code, checking for consistency.
  verifyFunction(*TheFunction);

  // Optimize the function.
  TheFPM->run(*TheFunction);

  return TheFunction;
}
```

見えればわかるように、このコードはかなり愚直な実装になっている。
`FunctionPasssManager`は、所定のLLVMの`Function*`の実装を改善しながら、最適化し、更新する。
これを用いて、上述のテストを再度実行できる、すなわち、下記のような結果を得られる。

```
ready> def test(x) (1+2+x)*(x+(1+2));
ready> Read function definition:
define double @test(double %x) {
entry:
        %addtmp = fadd double %x, 3.000000e+00
        %multmp = fmul double %addtmp, %addtmp
        ret double %multmp
}
```

期待踊りに、この関数のすべての実行から余計な浮動小数点の命令が省かれ、いい感じに最適化されたコードが得られた。

LLVMは、特定の環境で使われうる多種多様な最適化手法を提供する。
様々なpassのドキュメントが公開されているが、それらだけでは残念ながら不十分である。
他の情報源は、`clang`が実行するpassを見ることだ。
`opt`ツールは、コマンドラインから、passを使う体験を提供してくれるので、それらが何をしているかを理解できるだろう。

さて、今は、我々は、フロントエンドになる合理的なコードを得ることができた。
さぁ、次はそれを実行してみよう。

## JITコンパイラを追加する

LLVM IRで使えるコードは、それに適応できる様々なツールを提供する。
例えば、上でやったようにさいてきかできるし、テキストやバイナリ形式でダンプできるし、いくつかのターゲット向けにアセンブラにコンパイルできるし、それをJITコンパイルできる。
LLVM IR表現のいいところは、コンパイラの異なるモジュール、パーツ間で`共通に使える通貨`のように振舞ってくれることにある。

このセクションでは、我々が作ってきた翻訳機にJITコンパイル機能を追加する。
Kaleidoscopeに我々が求めるものは、打ち込んだ関数をそのまま保持？コンパイル？することだが、即座にtop-level表現として、打ち込んだ瞬間から評価もしてほしいのである。
例えば、もし、`1+2`と打ち込んだ時は、それらを即座に評価し、`3`と出力してほしいのである。もし、入力したものが関数であった場合、コンパイラは、コマンドラインからただちに呼べるようにしてほしいのである。

これを実行するために、まず最初に、現在のネイティブターゲット向けのコードを生成するための環境を用意し、JITを宣言かつ初期化する。
これは、`InitializeNativeTarget<hoge>`を呼べば良い。そして、グローバル変数として`TheJIT`を追加し、`main`でそれらを初期化する。

```
static std::unique_ptr<KaleidoscopeJIT> TheJIT;
...
int main() {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();

  TheJIT = llvm::make_unique<KaleidoscopeJIT>();

  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}
```

さらに、JITのためのデータレイアウトもセットアップする必要がある。

```
void InitializeModuleAndPassManager(void) {
  // Open a new module.
  TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
  TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

  // Create a new pass manager attached to it.
  TheFPM = llvm::make_unique<FunctionPassManager>(TheModule.get());
  ...
```

`KaleidoscopeJIT`クラスは、チュートリアルのために作った特殊な、シンプルなJITである。
このクラスのソースコードは、LLVMのソースの中の`llvm-src/examples/Kaleidoscope/include/KaleidoscopeJIT.h`にある。
後々、このクラスがどう動くのか、新しい機能でどう拡張されていくのかについて説明するが、今のところ、天下り的に使うことにする。
そのAPIは、非常にシンプルで、`addModule`は，LLVM IRのモジュールをJITに追加し，実行できるようにする．また，`removeModule`は，モジュールが内包するコードに結びつけれられたメモリを開放し，`findSymbol`を使うと，コンパイルされたコードへのポインタを見つけられる．

我々は，このシンプルなAPIを使い，以下のようにtop-level表現をパースするコードを変更する．

```
static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {

      // JIT the module containing the anonymous expression, keeping a handle so
      // we can free it later.
      auto H = TheJIT->addModule(std::move(TheModule));
      InitializeModuleAndPassManager();

      // Search the JIT for the __anon_expr symbol.
      auto ExprSymbol = TheJIT->findSymbol("__anon_expr");
      assert(ExprSymbol && "Function not found");

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = (double (*)())(intptr_t)ExprSymbol.getAddress();
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Delete the anonymous expression module from the JIT.
      TheJIT->removeModule(H);
    }
```

パースと`codegen`が成功すると，次にJITにtop-level表現を保持するモジュールを追加します．
我々は，`addModule`を呼び出すことでこれを実行できる．
`addModule`は，モジュール内のすべての関数コード生成のトリガとなり，返されるハンドルは，JITからモジュールを取り除く時に用いる．
一度，モジュールがJITに追加されると，モジュールはもはや変更できない．
このため，我々は，`InitializeModuleAndPassManager`を呼び出して，引き続くコードを保持する新しいモジュールを開く．

一度，モジュールをJITへ追加すると，最後に生成されたコードへのポインタを取得する必要がある．
我々は，JITの`findSymbol`メソッドを呼び出し，`__anon_expr`というtop-level表現の名前を引き渡し，これを実行する．
この関数をJITに追加するので，`findSymbol`は結果を返すかどうか`assert`でチェックする．

次に，シンボル上の`__anon_expr`関数のアドレスを`getAddress()`を使って取得する．
我々は，引数を取らない，`double`型を返すLLVM関数にtop-level表現をコンパイルすることを思い出して欲しい．
LLVM JITコンパイラは，ネイティブのABIにマッチするため，これは，プログラマは，結果のポインタをその関数のポインタにキャストでき，直接，呼び出すことができる．
つまり，アプリケーションに静的にリンクするようなネイティブのマシンコードと，JITコンパイラが出力するコードには，違いはないのである．

最終的に，我々は，top-level表現の再評価をサポートしないため，割り当てられたメモリを開放する時は，JITからモジュールを取り除くことになる．
しかし，思い出して欲しいのは，早い段階で`InitializeModuleAndPassManager`を使って，２，３行のラインを作ったモジュールは，今だに開かれており，新しいコードが追加されるのを待っている状態であることだ．
**
これらの二つの変更を加えると，Kaleidoscopeがどう動くのかがわかるはずです．

```
ready> 4+5;
Read top-level expression:
define double @0() {
entry:
  ret double 9.000000e+00
}

Evaluated to 9.000000
```

基本的には，このコードは動きそうです．
関数のダンプは，入力されたtop-level表現を構成する“no argument function that always returns double”を表示する．
これは，とても基本的な機能のデモだが，それ以上に何かできるだろうか．

```
ready> def testfunc(x y) x + y*2;
Read function definition:
define double @testfunc(double %x, double %y) {
entry:
  %multmp = fmul double %y, 2.000000e+00
  %addtmp = fadd double %multmp, %x
  ret double %addtmp
}

ready> testfunc(4, 10);
Read top-level expression:
define double @1() {
entry:
  %calltmp = call double @testfunc(double 4.000000e+00, double 1.000000e+01)
  ret double %calltmp
}

Evaluated to 24.000000

ready> testfunc(5, 10);
ready> LLVM ERROR: Program used external function 'testfunc' which could not be resolved!
```

関数定義と呼び出しは動作するが，最後の行で何か悪いことが起こることになる．
呼び出しは有効に見えるが，何が起こったんだろうか．
`Module`は，JITのためにメモリを確保したユニット(オブジェクト？)であると，APIから予想されるだろうか？
そして，`testfunc`は，無名な表現を保持する同じモジュールの一部である．
その無名表現を保持するメモリを解放するためにそのモジュールを削除するとき，そのモジュールに含まれる`testfunc`の定義も削除される．
そして，２回目に`testfunc`を読み出そうしたとき，JITは，もはやそれを見つけることはできないのだ．

これを修正する手っ取り早い方法は，無名表現を関数定義から，別のモジュールに切り出すことだ．
呼び出されるそれぞれの関数がプロトタイプを持ち，それらが呼び出される前にJITに追加される限り，JITは，幸運にも，モジュールの境界を超えて，関数呼び出しを解決する．
無名表現を異なるモジュールに動かすことによって，関数定義の残りの部分に手を加えずに，無名表現を削除できる．

事実，ここから，すべての関数をそれ自身のモジュールに突っ込むことにする．そうすることで，この環境をよりREPLっぽくする`KaleidoscopeJIT`の便利な性質をうまく活用できるようになる，つまり，（固有の定義を持つ）関数は，JITに（モジュールではなく）何度でも追加することができるのである．

```
ready> def foo(x) x + 1;
Read function definition:
define double @foo(double %x) {
entry:
  %addtmp = fadd double %x, 1.000000e+00
  ret double %addtmp
}

ready> foo(2);
Evaluated to 3.000000

ready> def foo(x) x + 2;
define double @foo(double %x) {
entry:
  %addtmp = fadd double %x, 2.000000e+00
  ret double %addtmp
}

ready> foo(2);
Evaluated to 4.000000
```

それぞれの関数をそのモジュール内で有効にさせるため，我々は，それぞれのモジュール内の一つ前の関数宣言を再生成する方法が必要になる．

```
static std::unique_ptr<KaleidoscopeJIT> TheJIT;

...

Function *getFunction(std::string Name) {
  // First, see if the function has already been added to the current module.
  if (auto *F = TheModule->getFunction(Name))
    return F;

  // If not, check whether we can codegen the declaration from some existing
  // prototype.
  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  // If no existing prototype exists, return null.
  return nullptr;
}

...

Value *CallExprAST::codegen() {
  // Look up the name in the global module table.
  Function *CalleeF = getFunction(Callee);

...

Function *FunctionAST::codegen() {
  // Transfer ownership of the prototype to the FunctionProtos map, but keep a
  // reference to it for use below.
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;
```

このコードは，それぞれの関数の，もっとも最近のプロトタイプ宣言を保持するグローバル変数，`FunctionProtos`を加えることから始まる．
さらに，便利がいいように，`TheModule->getFunction()`の代わりに，`getFunction()`を追加する．
この便利な関数は，既存の関数宣言を探すために，`TheModule`の中を検索し，既存の宣言が見つからない場合，`FunctionProtos`から新しい宣言を生成する．
`In CallExprAST::codegen()`の中にある`getFunction()`を`TheModule->getFunction()`に差し替える．そして，`FunctionAST::codegen()`では，ハッシュである`FunctionProtos`のマッピングを最初にアップデートし，その後に`getFunction()`を呼び出す．
これが完了すると，事前に宣言した関数を，現在モジュール内の関数宣言として得られるようになる．

さらに，`HandleDefinition`と`HandleExtern`を更新する，

```
static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      TheJIT->addModule(std::move(TheModule));
      InitializeModuleAndPassManager();
    }
  } else {
    // Skip token for error recovery.
     getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}
```

‌`HandleDefinition`内部で，新しく定義された関数をJITへ転送するための２行を追加し，新しいモジュールを開く．
`HandleExtern`内部で，`FunctionProtos`へプロトタイプ宣言を追加する１行を追加する．

これらの変更の後，REPLに再度挑戦してみよう（無名関数のダンプを削除し，ここまでにいいアイデアを考えてみて欲しい）．

```
ready> def foo(x) x + 1;
ready> foo(2);
Evaluated to 3.000000

ready> def foo(x) x + 2;
ready> foo(2);
Evaluated to 4.000000
```

動いた！

このシンプルなコードでさえ，すごい能力を発揮する・・・下の結果を見て欲しい．

```
ready> extern sin(x);
Read extern:
declare double @sin(double)

ready> extern cos(x);
Read extern:
declare double @cos(double)

ready> sin(1.0);
Read top-level expression:
define double @2() {
entry:
  ret double 0x3FEAED548F090CEE
}

Evaluated to 0.841471

ready> def foo(x) sin(x)*sin(x) + cos(x)*cos(x);
Read function definition:
define double @foo(double %x) {
entry:
  %calltmp = call double @sin(double %x)
  %multmp = fmul double %calltmp, %calltmp
  %calltmp2 = call double @cos(double %x)
  %multmp4 = fmul double %calltmp2, %calltmp2
  %addtmp = fadd double %multmp, %multmp4
  ret double %addtmp
}

ready> foo(4.0);
Read top-level expression:
define double @3() {
entry:
  %calltmp = call double @foo(double 4.000000e+00)
  ret double %calltmp
}

Evaluated to 1.000000
```

ちょっと待って欲しい，JITは，`sin`と`cos`をどうやって知ったのだろうか．その答えは，驚くほどシンプルだ．
`KaleidoscopeJIT`は，指定されたモジュール内で有効でないシンボルを見つけるために使う指定された愚直なシンボルの名前解決のルールを持つ．
`KaleidoscopeJIT`は，もっとも新しい定義を見つけるため，JITにすでに追加されたすべてのモジュールの中を〜最後に追加されたものから順にもっとも古く追加されたものまで〜探索する．
もし，JITの中で関数定義が見つけられなかった場合，`Kaleidoscope`プロセス自身の上で`dlsym("sin")`を呼び出し，それをfallbackする．
JITのアドレス空間の中で`sin`が定義されたため，`KaleidoscopeJIT`は，直接，`sin`の`limb`バージョンを呼び出すように，モジュール内の呼び出しを修正する．
しかし，いくつかのケースで，これは`sin`と`cos`は，一般的な数学の関数の名前なので，`constant folder`は，上で実装した`sin(1.0)`の中のように，定数を使って呼ばれるとき，正しい結果へ直接関数呼び出しを評価することになる．

将来，このシンボル解決のルールの微調整することで，セキュリティ（JITのコードで使えるシンボルを制限する）から，シンボル名をベースにした動的なコード生成，遅延評価（lazy complication）などの，様々な便利機能を提供する方法を理解できるようになるだろう．

即物的なシンボル解決ルールの利点は，operation(操作？なんの？)を実装する任意のC++のコードを書くことで，言語を拡張することができる．
例えば，

```
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}
```

上のコードで，エラーコンソールに`double`の値を出力する関数を追加できる．
Windowsの場合，動的シンボルローダが`GetProcAddress`を使ってシンボルを見つけるため，実際に関数をエクスポートする必要があることに注意されたい．

今，

```
extern putchard(x); putchard(120);
```

のように書けば，コンソールに文字を文字コードを指定して出力できるようになった（つまり，120は'x'のアスキーコードなので，このコードの場合，コンソールには小文字のxが出力される）．
同じようなコードで，ファイルI/Oも実現できるし，多くの機能を`Kaleidoscope`で実装できるのである．

これで，JITと最適化の章を終わりにします．
この時点で，チューリング完全ではないプログラミング言語をコンパイルできるようになり，ユーザのやりたいように最適化し，JITコンパイルできるようになった．
次は，言語に制御フローを持たせ，その方法にそって，LLVM IRのおもしろい課題にトライしていくことにしよう．

## コードリスト

```
# Compile
clang++ -g toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 -o toy
# Run
./toy
```

もし，Linuxでコンパイルする場合は，`-rdynamic`オプションを加える必要がある．これは，ランタイム時に正しく外部関数を解決するためである．

### ビルド方法・・・訳者追記
ビルドには，llvmのソースに含まれる`Kaleidoscope`の[ソースコード](hhttps://github.com/llvm-mirror/llvm/blob/release_70/examples/Kaleidoscope/)の中のヘッダが必要である．
ヘッダは，[ここ](https://github.com/llvm-mirror/llvm/blob/release_70/examples/Kaleidoscope/include/)にある．
この最新版(現状，brewでインストールされるllvmはv7.0なので，`release_70`のブランチ)のヘッダをダウンロードする．
しかし，このままでは，ビルドできない環境もあるようだ．
そのときは，`llvm-config`のオプションに`orcjit`を加えるとよいようだ[^https://twitter.com/giginet/status/1080744056722878464]．
ゆえに，コードリストは，以下のコマンドでビルドするとよい．

```
clang++ -g tutorial04.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit mcjit native` -O3 -o tutorial04
```

### コードリスト

```
#include "../include/KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,
  tok_extern = -3,

  // primary
  tok_identifier = -4,
  tok_number = -5
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

/// gettok - Return the next token from standard input.
static int gettok() {
  static int LastChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar))
    LastChar = getchar();

  if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }

  if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  if (LastChar == '#') {
    // Comment until end of line.
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

namespace {

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;

  virtual Value *codegen() = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}

  Value *codegen() override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}

  Value *codegen() override;
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  Value *codegen() override;
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}

  Value *codegen() override;
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
      : Name(Name), Args(std::move(Args)) {}

  Function *codegen();
  const std::string &getName() const { return Name; }
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}

  Function *codegen();
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();

/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = llvm::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat (.
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ).
  return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken(); // eat identifier.

  if (CurTok != '(') // Simple variable ref.
    return llvm::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken(); // eat (
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();

  return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  }
}

/// binoprhs
///   ::= ('+' primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  // If this is a binop, find its precedence.
  while (true) {
    int TokPrec = GetTokPrecedence();

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;

    // Okay, we know this is a binop.
    int BinOp = CurTok;
    getNextToken(); // eat binop

    // Parse the primary expression after the binary operator.
    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    // Merge LHS/RHS.
    LHS =
        llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

/// expression
///   ::= primary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken(); // eat def.
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = ParseExpression())
    return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = llvm::make_unique<PrototypeAST>("__anon_expr",
                                                 std::vector<std::string>());
    return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken(); // eat extern.
  return ParsePrototype();
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;
static std::unique_ptr<legacy::FunctionPassManager> TheFPM;
static std::unique_ptr<KaleidoscopeJIT> TheJIT;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

Function *getFunction(std::string Name) {
  // First, see if the function has already been added to the current module.
  if (auto *F = TheModule->getFunction(Name))
    return F;

  // If not, check whether we can codegen the declaration from some existing
  // prototype.
  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  // If no existing prototype exists, return null.
  return nullptr;
}

Value *NumberExprAST::codegen() {
  return ConstantFP::get(TheContext, APFloat(Val));
}

Value *VariableExprAST::codegen() {
  // Look this variable up in the function.
  Value *V = NamedValues[Name];
  if (!V)
    return LogErrorV("Unknown variable name");
  return V;
}

Value *BinaryExprAST::codegen() {
  Value *L = LHS->codegen();
  Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  switch (Op) {
  case '+':
    return Builder.CreateFAdd(L, R, "addtmp");
  case '-':
    return Builder.CreateFSub(L, R, "subtmp");
  case '*':
    return Builder.CreateFMul(L, R, "multmp");
  case '<':
    L = Builder.CreateFCmpULT(L, R, "cmptmp");
    // Convert bool 0/1 to double 0.0 or 1.0
    return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
  default:
    return LogErrorV("invalid binary operator");
  }
}

Value *CallExprAST::codegen() {
  // Look up the name in the global module table.
  Function *CalleeF = getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("Unknown function referenced");

  // If argument mismatch error.
  if (CalleeF->arg_size() != Args.size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen());
    if (!ArgsV.back())
      return nullptr;
  }

  return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::codegen() {
  // Make the function type:  double(double,double) etc.
  std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(TheContext));
  FunctionType *FT =
      FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

  Function *F =
      Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

  // Set names for all arguments.
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

  return F;
}

Function *FunctionAST::codegen() {
  // Transfer ownership of the prototype to the FunctionProtos map, but keep a
  // reference to it for use below.
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;

  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
  Builder.SetInsertPoint(BB);

  // Record the function arguments in the NamedValues map.
  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[Arg.getName()] = &Arg;

  if (Value *RetVal = Body->codegen()) {
    // Finish off the function.
    Builder.CreateRet(RetVal);

    // Validate the generated code, checking for consistency.
    verifyFunction(*TheFunction);

    // Run the optimizer on the function.
    TheFPM->run(*TheFunction);

    return TheFunction;
  }

  // Error reading body, remove function.
  TheFunction->eraseFromParent();
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static void InitializeModuleAndPassManager() {
  // Open a new module.
  TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
  TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

  // Create a new pass manager attached to it.
  TheFPM = llvm::make_unique<legacy::FunctionPassManager>(TheModule.get());

  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->add(createInstructionCombiningPass());
  // Reassociate expressions.
  TheFPM->add(createReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->add(createCFGSimplificationPass());

  TheFPM->doInitialization();
}

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      TheJIT->addModule(std::move(TheModule));
      InitializeModuleAndPassManager();
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {
      // JIT the module containing the anonymous expression, keeping a handle so
      // we can free it later.
      auto H = TheJIT->addModule(std::move(TheModule));
      InitializeModuleAndPassManager();

      // Search the JIT for the __anon_expr symbol.
      auto ExprSymbol = TheJIT->findSymbol("__anon_expr");
      assert(ExprSymbol && "Function not found");

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = (double (*)())(intptr_t)cantFail(ExprSymbol.getAddress());
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Delete the anonymous expression module from the JIT.
      TheJIT->removeModule(H);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();

  TheJIT = llvm::make_unique<KaleidoscopeJIT>();

  InitializeModuleAndPassManager();

  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}
```
