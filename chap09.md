# デバッグ情報の追加
## はじめに
９章にようこそ．
ここまでで，まずまずな，ちょっとしたプログラミング言語を作ってきた．
もし，その中で何か変なことがおこったとき，あなたはどうやって，作ってきたコードをデバッグしただろうか．

ソースレベルのデバッグは，デバッガがバイナリを翻訳することを助けるフォーマットされたデータを使い，プログラマが書いたコードへマシンの状態をトレースする．
LLVMでは，一般的にDWARFと呼ばれるフォーマットを使う．
DWARFは，コンパクトに型やソースの位置，変数の位置を表現するエンコード方式である．

本章を簡単にまとめると，プログラミング言語にデバッグ情報を追加するためにしないといけないこと，DWARFに，デバッグ情報を変換する方法について学ぶことになる．

### 警告
JITを通してデバッグすることはできない．
このため，我々は，プログラムを小さく，スタンドアローンで動くようにコンパイルする必要がある．
このため，言語を実行するために，コードをコンパイルする方法に多少，変更を加えることになる．
これは，インタラクティブなJITというより，Kaleidoscopeで作ったシンプルなソースをコンパイルする何かを作ることになる．
それは，たくさんコードを変更する必要をさけるため，今はひとつの"top level"コマンドしか持つことができない限界を受け入れる必要がある．

ここに，これからコンパイルするアプリケーションで，コンパイルするコードがある．

```
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1)+fib(x-2);

fib(10)
```

## 何故これが難しいのか？
デバッグ情報は，いくつかの理由で非常に取り扱いが難しいーほとんどの場合，最適化されたコードが原因だが．
はじめに，最適化は．ソースコードの位置を維持することを難しくする．
LLVM IRでは，オリジナルソースコードの位置関係を，それぞれのIRレベルの命令になっても，保持させることができる．
最適化パスは，新しく生成された命令に対しても，ソースコードの位置を保持すべきであるが，命令がマージされると，ひとつの位置しか保持できない．
これは，最適化プログラムを通して，ステップするとき，コードの周りでジャンプすることの理由である．
二つ目に，最適化は，最適化によって変数を消去する方法，他の変数をメモリを共有する方法，あるいは追跡できないような方法，いずれかの方法で，変数を動かす可能性がある．
このチュートリアルの目的のために，ここでは，最適化は避けることにする（あとで，パッチのセットのひとつとして，最適化のあとにデバッグ情報を追加する方法を説明する？）．

## 先行コンパイル
JITの複雑さに思い悩まず，ソースコードへデバッグ情報を追加する様子のみにハイライトを当てるため，フロントエンドによって発行されたIRを，実行，デバッグ，結果の確認ができるようなシンプルなスタンドアローンアプリケーションへとコンパイルできるように，Kaleidoscopeに２，３の修正を加えようと思う．

はじめに，無名関数を"main"関数にする．
この無名関数は，我々のtop level文を含む．

```
-    auto Proto = llvm::make_unique<PrototypeAST>("", std::vector<std::string>());
+    auto Proto = llvm::make_unique<PrototypeAST>("main", std::vector<std::string>());
```

これは，関数に，"main"という名前を与えるだけのシンプルなものだ．

次に，コマンドラインとして動作するためのコードを削除する．

```
@@ -1129,7 +1129,6 @@ static void HandleTopLevelExpression() {
 /// top ::= definition | external | expression | ';'
 static void MainLoop() {
   while (1) {
-    fprintf(stderr, "ready> ");
     switch (CurTok) {
     case tok_eof:
       return;
@@ -1184,7 +1183,6 @@ int main() {
   BinopPrecedence['*'] = 40; // highest.
   // Prime the first token.
-  fprintf(stderr, "ready> ");
   getNextToken();
```

最後に，すべての最適化のためのPassとJITをdisableする．
そのため，パースとコード生成をやった後に，LLVM IRがエラーを起こすようになる．

```
@@ -1108,17 +1108,8 @@ static void HandleExtern() {
 static void HandleTopLevelExpression() {
   // Evaluate a top-level expression into an anonymous function.
   if (auto FnAST = ParseTopLevelExpr()) {
-    if (auto *FnIR = FnAST->codegen()) {
-      // We're just doing this to make sure it executes.
-      TheExecutionEngine->finalizeObject();
-      // JIT the function, returning a function pointer.
-      void *FPtr = TheExecutionEngine->getPointerToFunction(FnIR);
-
-      // Cast it to the right type (takes no arguments, returns a double) so we
-      // can call it as a native function.
-      double (*FP)() = (double (*)())(intptr_t)FPtr;
-      // Ignore the return value for this.
-      (void)FP;
+    if (!F->codegen()) {
+      fprintf(stderr, "Error generating code for top level expr");
     }
   } else {
     // Skip token for error recovery.
@@ -1439,11 +1459,11 @@ int main() {
   // target lays out data structures.
   TheModule->setDataLayout(TheExecutionEngine->getDataLayout());
   OurFPM.add(new DataLayoutPass());
+#if 0
   OurFPM.add(createBasicAliasAnalysisPass());
   // Promote allocas to registers.
   OurFPM.add(createPromoteMemoryToRegisterPass());
@@ -1218,7 +1210,7 @@ int main() {
   OurFPM.add(createGVNPass());
   // Simplify the control flow graph (deleting unreachable blocks, etc).
   OurFPM.add(createCFGSimplificationPass());
-
+  #endif
   OurFPM.doInitialization();

   // Set the global so the code gen can use this.
```

この相対的に小さい小さいセットの変化は，我々に，Kaleidoscope言語の一部を，コマンドラインを通して，実行可能なプログラムにコンパイルできるというポイントをもたらす．

```
Kaleidoscope-Ch9 < fib.ks | & clang -x ir -
```

これによって，現在の作業中のディレクトリに`a.out/a.exe`が作成される．

## コンパイル単位
DWARFで書かれたコードのセクションのためのtop-levelコンテナは，コンパイル単位である．
これは，個々の翻訳されたユニットに対する型と関数のデータを含む（ソースコードでいうひとつのファイル）．
だから，我々がする必要があることは，`fib.ks`ファイルのための，コンパイル単位を構築することである．

## DWARFの発行のためのセットアップ
`IRBuilder`クラスに似た，LLVM IRファイルのためのデバッグ用のメタデータを構築するのを助ける`DIBuilder`クラスがある．
そのクラスは，`IRBuilder`とLLVM IRに同じように対応するが，良い名前が付けられている．
それを使うことは，あなたが`IRBuilder`や`Instruction`の名前に親しんでいるというより，DWARFの専門用語に慣れていることを要求する．
しかし，もしあなたがメタデータのフォーマットについて一般できなドキュメントを通して読んだことがあるなら，それはもうちょっとわかりやすくなる．
IRレベルの説明を構築するために，このクラスを使うことにする．
`IRBuilder`クラスの構築は，モジュールを引数にとるため，モジュールを構築した後に，手短に，構築する必要がある．
多少使いやすくするために，グローバルのstatic変数として，`IRBuilder`クラスを構築する．

次に，我々がよく使うデータのいくつかをキャッシュするために，小さなコンテナを作る．
最初は，コンパイル単位である．しかし，複数型付けされた表現に困るべきではないため，我々の一つの型のために，ちょっとしたコードを書くことになる．

```
static DIBuilder *DBuilder;

struct DebugInfo {
    DICompileUnit *TheCU;
    DIType *DblTy;

    DIType *getDoubleTy();
} KSDbgInfo;

DIType *DebugInfo::getDoubleTy() {
    if (DblTy)
        return DblTy;

    DblTy = DBuilder->createBasicType("double", 64, dwarf::DW_ATE_float);
    return DblTy;
}
```

メイン文の最後の方で，モジュールを構築するときに，以下のようなコードを書く．

```
DBuilder = new DIBuilder(*TheModule);

KSDbgInfo.TheCU = DBuilder->createCompileUnit(
    dwarf::DW_LANG_C, DBuilder->createFile("fib.ks", "."),
    "Kaleidoscope Compiler", 0, "", 0);
```

ここで，気をつけるべきことがいくつかある．
初めの一つは，Kaleidoscopeと呼ばれる言語のためのコンパイル単位を作成しようとしている間，Cのための言語定数を使った．
これは，デバッガが，それが認識しない言語の呼び出し規則やデフォルトABI(Application binary interface)を理解しているとは言えないため，我々は，LLVMのコード生成におけるCのABIをフォローするので，その方法はかなり精確ではある．
これは，デバッガから関数を呼び出したり，実行できることから明らかである．

二つ目は，`createCompileUnit`を呼び出している`fib.ks`を見てもらいたい．
Kaleidoscopeコンパイラへソースコードを押し付けるためのにシェルのリダイレクトを利用するために，これはデフォルトのハードコードされた値である．
普通のフロントエンドでは，名前を入力し，その値が，そこへ保存される．

最後のひとつは，`DIBuilder`を通してデバッグ情報を発行するパートの部分が，デバッグ情報を"finalized"する必要があるということである．
`main`関数の最後の方で，モジュールをダンプす流前に，これをちゃんとやっているかを確認するが，その理由は，`DIBuilder`のための基本的なAPIの一部である．

```
DBuilder->finalize();
```

## 関数
今，コンパイル単位と，ソースコードの位置が与えられているとき，関数定義をデバッグ情報へ追加できる．
`PrototypeAST::codegen()`の中で，我々は，プログラムの一部のコンテキストを記述するためのコードを２，３行追加する．
この場合，関数自身の実際の定義とファイルそのものである．

```
DIFile *Unit = DBuilder->createFile(
    KSDbgInfo.TheCU.getFilename(),
    KSDbgInfo.TheCU.getDirectory()
);
```

上で作った`Compile Unit`と，今開いているコードのファイル名を引数に与えて，`DIFile`を取得する．
このとき，ソースの位置として０と（現在の抽象構文木には，ソースコードの行番号が保存されていない），関数定義を利用する．

```
DIScope *FContext = Unit;
unsigned LineNo = 0;
unsigned ScopeLine = 0;
DISubprogram *SP = DBuilder->createFunction(
    FContext,
    P.getName(),
    StringRef(),
    Unit,
    LineNo,
    CreateFunctionType(
       TheFunction->arg_size(),
       Unit
    ),
    false /* internal linkage */, true /* definition */, ScopeLine,
    DINode::FlagPrototyped,
    false
);
TheFunction->setSubprogram(SP);
```

今，関数のメタデータすべてへの参照を持つ`DISubprogram`を持っている．

## ソースコードの行番号
デバッグ情報でももっとも重要なのが，正確なソースコードの位置だ．
つまり，これは，生成されたIRやバイナリをソースコードにマッピングできるようにする．
けれども，ここで，我々は，Kaleidoscopeが．lexerやパーサにソースコードの一の情報を処理するように実装していないという問題がある．

```
struct SourceLocation {
    int Line;
    int Col;
};
static SourceLocation CurLoc;
static SourceLocation LexLoc = {1, 0};

static int advance() {
    int LastChar = getchar();
    if (LastChar == '\n' || LastChar == '\r') {
        LexLoc.Line++;
        LexLoc.Col = 0;
    } else {
        LexLoc.Col++;
    }
    return LastChar;
}
```

コードのこのセットの中で，ソースファイルの行と列を追跡する機能を追加した．
すべてのトークンを分けるときに，トークンが始まるところの行と列を"語彙の位置"としてセットする．
これを実行するため，`getchar()`を呼び出していたコードを，新しく作った関数，抽象構文木のノードにソースの位置を追加する`advance()`で書き換える．

```
class ExprAST {
    SourceLocation Loc;

    public:
        ExprAST(SourceLocation Loc = CurLoc) : Loc(Loc) {}
        virtual ~ExprAST() {}
        virtual Value* codegen() = 0;
        int getLine() const { return Loc.Line; }
        int getCol() const { return Loc.Col; }
        virtual raw_ostream &dump(raw_ostream &out, int ind) {
            return out << ':' << getLine() << ':' << getCol() << '\n';
        }
```

```
LHS = llvm::make_unique<BinaryExprAST>(
    BinLoc,
    BinOp,
    std::move(LHS),
    std::move(RHS)
);
```

新しい表現を生成したときは，表現と変数の位置を，関数に引き渡していく．

それぞれの命令が，正しいソースコードの位置情報を取得することを確実にするため，`Builder`に，今見ているところが新しいソースコードの位置であるかを問い合わせる必要がある．
それには，ちょっとしたヘルパー関数を使えばよい．

```
void DebugInfo::emitLocation(ExprAST *AST) {
    DIScope *Scope;
    if (LexicalBlocks.empty())
        Scope = TheCU;
    else
        Scope = LexicalBlocks.back();
    
    Builder.SetCurrentDebugLocation(
        DebugLoc::get(
            AST->getLine(),
            AST->getCol(),
            Scope
        )
    );
}
```

このコードは，同時にメインの`IRBuilder`にもどこを見ているかを問い合わせているが，スコープについても問い合わせている．
スコープは，コンパイル単位レベルになりうるし，現在の関数のような語彙的なブロックに近いものとすることもできる．
これを表現するために，以下のスコープのスタックを作る．

```
std::vector<DIScope *> LexicalBlocks;
```

そして，それぞれの関数のコードを生成を始める時に，スタックの一番上に関数のスコープを置く．

```
KSDbgInfo.LexicalBlocks.push_back(SP);
```

また，関数のコード生成が終わったタイミングで，スコープスタックを積み降ろすことを忘れてはならない．

```
// Pop off the lexical block for the function since we added it
// unconditionally.
KSDbgInfo.LexicalBlocks.pop_back();
```

そのとき，新しい抽象構文木のノードのためのコードを生成し始めるタイミングで，ソースコードの位置を発行するようにする．

```
KSDbgInfo.emitLocation(this);
```

## 変数
関数があるとすると，スコープ内にある変数をプリントアウトできるようにする必要がある．
関数の引数をセットアップしよう．
そうすれば，そこそこのバックトレースができるし，関数がどのように呼び出されているかを理解できる．
実際に，多くのコードは必要がないし，一般的に，`FunctionAST::codegen`内で引数の`alloca`を作っているときに，それをうまく処理しているのである．

```
// Record the function arguments in the NamedValues map.
NamedValues.clear();
unsigned ArgIdx = 0;
for (auto &Arg : TheFunction->args()) {
    // Create an alloca for this variable.
    AllocaInst *Alloca = CreateEntryBlockAlloca(
        TheFunction,
        Arg.getName()
    );

    // Create a debug descriptor for the variable.
    DILocalVariable *D = DBuilder->createParameterVariable(
        SP,
        Arg.getName(),
        ++ArgIdx,
        Unit,
        LineNo, 
        KSDbgInfo.getDoubleTy(),
        true
    );

    DBuilder->insertDeclare(
        Alloca,
        D,
        DBuilder->createExpression(),
        DebugLoc::get(LineNo, 0, SP),
        Builder.GetInsertBlock()
    );

    // Store the initial value into the alloca.
    Builder.CreateStore(&Arg, Alloca);

    // Add arguments to variable symbol table.
    NamedValues[Arg.getName()] = Alloca;
}
```

ここで，はじめに変数を作り，それにスコープ(`SP`)を与え，名前，ソースコードの位置，型を与える．そして，それが引数や引数のインデックスになる．
次に，`llvm.dbg.declare`呼び出しを生成することで，`alloca`内で変数をIRレベルの表現で取得することを示し（さらに，そのIRは，変数の位置も与える），宣言において，スコープの開始位置のソースコードでの位置を示します．

ここで特筆すべきおもしろいことは，様々なデバッガは，過去に，コードやデバッグ情報がどうやって生成されたか依存する仮説を持っているということである．
この場合，関数の頭出しのための行情報を生成するのを避けるハックをする必要がある．
このため，デバッガは，ブレイクポイントを設定するときに．これらの命令をスキップすることを知らなければならない．
このため，`FunctionAST::CodeGen`の中で，数行，書き足す必要がある．

```
// Unset the location for the prologue emission (leading instructions with no
// location in a function are considered part of the prologue and the debugger
// will run past them when breaking on a function)
KSDbgInfo.emitLocation(nullptr);
```

そして，実際に，関数の実装してのコードの生成が始まった行番号を発行する．

```
KSDbgInfo.emitLocation(Body.get());
``` 

これによって，我々は，関数内でブレイクポイントをセットするためのデバッグ情報を十分に持たせることができ，引数の変数をプリントアウトしたり，関数を呼び出しできるようになる．
このデバッガは，数行のコードを書くくらいなら，そんなに悪くないものです．
