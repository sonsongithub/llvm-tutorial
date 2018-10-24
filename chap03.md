# LLVM IRのためのコード生成

## はじめに

３章にようこそ．
本章では，抽象構文木をLLVM IRにどうやって変換するかを説明する．
ここでは，LLVMがどのように何をしているか？について多少なりとともに，それがいかに使いやすいかを解説することになる．
LLVM IRを生成するよりも，lexerやパーサを作る方がよっぽど骨の折れる作業なのである．

## コード生成のためのセットアップ

LLVM IRを生成するために，まず，シンプルなセットアップを実行する必要がある．
はじめに，それぞれのASTクラスにコード生成のための仮想関数(`codegen`)を定義する．

```
/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual Value *codegen() = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
  virtual Value *codegen();
};
...
```

`codegen()`は，ASTノードに沿い，依存しながら，IRを出力する．
そして，出力は，すべて`LLVM Value`オブジェクトである．
`Value`は，"静的単一代入レジスタ(Static Single Assignmentレジスタ)"あるいは，LLVMにおけるSSA型の値を表現するために使われるクラスである．
SSA値型の厳密な捉え方は，それらの値は，関連する指示実行のように計算され，指示が再度実行されない限り，新しい値がセットされることはない．
言い換えると，SSA値型を変化させる方法はない．
詳しくは，"静的単一代入(Static Single Assignment)"を読んでいただきたい．
SSAのコンセプトは，非常に自然なので，完全に理解するのは難しくない．

`ExprAST`クラスの階層に仮想関数を追加する代わりに，ビジターパターンを使うこともできるし，それ以外の方法で実装しても構わない．
繰り返すが，このチュートリアルは，ソフトウェアエンジニアのベストプラクティスを目指したものではない．
今回の目的に対しては，仮想関数を使うのがもっともシンプルだと考える．

二つ目に必要なものは，パーサを作るときにも使った`LogError`関数だ．
この関数は，コード生成のときにエラーを出力するために使う．

```
static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}
```

コード生成の間に静的変数を使うことになる．
`TheContext`は，llvmのコアのデータ構造や型，定数テーブルなど多くの要素を保持する隠蔽されたオブジェクトである．
利用するためには，ただ一つのインスタンスを生成し，APIにそれを引き渡す必要性だけがあることを知っておけば，その詳細について，詳しく理解する必要性はない．

`Builder`オブジェクトは，llvm命令セットの生成を簡単にするためのヘルパーオブジェクトである．
`IRBuilder`クラステンプレートのインスタンスは，命令を追加した場所を追跡したり，新しい命令セットを作るためのメソッドを持つ．

`TheModule`は，関数やグローバル変数を保持するllvmの構造体である．
色々な方法で，それは，LLVM IRがコードを保持するために利用するトップレベル構造である．
この構造体は，我々が生成するIRのオブジェクトのすべてのメモリを保持することになる．
ゆえに，`codegen`関数が返す値は，ユニークポインタではなく，ただの名前のポインタである．
IRのオブジェクトは，どうやら，この`TheModule`がすべてメモリ管理するようである．

`NamedValues`マップは，現在のスコープで定義されている値と，それらがLLVMでどう表現されているかを追跡するためにある．
Kaleidscopeのこの形式では，参照されうるのは，関数のパラメータのみである．
関数のパラメータは，その関数の実装のためのコードが生成されるとき，このマップに作られる．

これらの基本を押さえた上で，それぞれの表現に対するコード生成の仕方について説明していくことにする．
これが`Builder`が何かにコードを生成するために生成されたように仮定していることに注意してほしい．
今，セットアップはすでに完了しており，それを使ってコードを生成していく．

## 表現からコードを生成する

表現ノードに対するllvmコードを生成することは，とても愚直である．
つまり，コメントを入れても45行足らずのコードで，4つの主表現のノードからllvmコードを生成することができる．

```
Value *NumberExprAST::codegen() {
  return ConstantFP::get(TheContext, APFloat(Val));
}
```

LLVM IRにおいて，定数の数値は，`ConstantFP`クラスで表現される．
`ConstantFP`クラスは，内部的に`APFloat`の中に数値を保存する（`APFloat`は，任意の精度で浮動小数点の値を保持できる能力を持つ）．
このコードは，基本的に`ConstantFP`のインスタンスを返す．
LLVM IRでは，定数は，すべてユニークでかつ共有される．
このため，このAPIは，新しいインスタンスを返すときに，`new`や`create`にのようなメソッドや文法ではなく，`foo::get(…)`というメソッドを使っているのである（つまりこの場でインスタンスが生成されたとは限らないということ）．

```
Value *VariableExprAST::codegen() {
  // Look this variable up in the function.
  Value *V = NamedValues[Name];
  if (!V)
    LogErrorV("Unknown variable name");
  return V;
}
```

変数への参照もまた，llvmを使うとシンプルに実装できる．
Kaleidoscopeのシンプルバージョンでは，値は，どこかで生成され，その値は常に有効であると仮定する．
実践的には，`NamedValues`マップにある値だけが関数の引数である．
このコードは，簡単に指定された名前が，マップに含まれているかを（あるいは，含まれていないか，全く知らない変数が参照されているか）確認する．
そして，その値を返す．
将来的には，ループの中だけで有効な変数やローカル変数もサポートする．

```
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
    return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext),
                                "booltmp");
  default:
    return LogErrorV("invalid binary operator");
  }
}
```

二項演算子からのコード生成は，ちょっとおもしろい．
ここでの，基本的なアイデアは，表現の左辺のためのコードを再帰的に生成し，そのあと，右辺のコードを生成し，最終的に二項演算の表現のためのコード生成を完了する．
このコードでは，単純なswitch文で，二項演算子の右辺のinstructionを生成する．

上の例では，llvmビルダークラスは，その値を表示し始める．
`IRBuilder`は，新しく生成された命令をどこに挿入すべきかを知っている．
ここであなたがすべきことは，どの命令を作るべきか，どのオペランドを使うべきか，生成される命令の名前を補助的に提供したりすることである．

llvmのいいところは，名前自体がヒントになっている点である．
例えば，もし上のコードが，複数の`addtmp`変数を生成するなら，llvmは，自動的に末尾の番号をインクリメントしながら，それらに自動的に名前を割り振っていく．
命令に対するローカル変数の名前は，純粋に任意だが，IRがダンプされたときに読みやすいようにしておくべきである．

llvm命令は，厳密なルールに従わなければならない．例えば，加算命令の`Left`と`Right`のオペレータは，同じ型を持たねばならず，加算の結果を保存する型と，オペレータの型も同じでなければならない．
Kaleidoscopeのすべての変数は，double型なので，これらの制約をあまり考える必要はない．

一方で，llvmでは，fcmp命令は，いつも`i1`型（1ビットの整数値）を返すことになっている．
これに付随する問題は，Kaleidoscopeは，値が0.0から1.0の値しか持たないということである．
つまり，double型しかサポートしないためである．
これらの文法を理解するため，我々は，fcmp命令に，uitofp命令をくっつけて使う．
uitofp命令は，入力の整数値を，それを符号なし浮動小数点値に変換する．
対照的に，もし我々がsiotfp命令を使うときは，Kaleidoscopeの'<'オペレータは，その入力の値に依存して，0.0と-1.0を返す．

```
Value *CallExprAST::codegen() {
  // Look up the name in the global module table.
  Function *CalleeF = TheModule->getFunction(Callee);
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
```

関数呼び出しのためのコード生成は，率直に実装できる．
上のコードは，最初にllvmのモジュールシンボルテーブルから，関数名を見つけ出す．
llvmモジュールは，我々がJITで実行する関数を保存するコンテナであることを思い出そう．
それぞれ，ユーザが指定しているものと同じ名前関数が与えられえおり，我々は，llvmシンボルテーブルを使って，関数の名前を解決する．

ここまでで，4つの基本的な表現を処理してきた．
llvmでは，これ以上のことを付け加えるのも簡単だ．

## 関数コード生成

プロトタイプ宣言や関数のためのコード生成は、細かいことをたくさん処理しなければならないため、ここまでの表現からコード生成するためのコードよりも、汚くなってしまう。
しかし、いくつかの重要な点を説明させてもらいたい。
はじめに、関数宣言のためのコード生成について説明する。
すなわち、それは、関数の実装と、外部のライブラリにある関数宣言のためのコードである。

```
Function *PrototypeAST::codegen() {
  // Make the function type:  double(double,double) etc.
  std::vector<Type*> Doubles(Args.size(),
                             Type::getDoubleTy(TheContext));
  FunctionType *FT =
    FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

  Function *F =
    Function::Create(FT, Function::ExternalLinkage, Name, TheModule);
```

この中の2,3行のコードの中に多くの重要な要素が詰め込まれている。
まず、この関数は、`Value*`ではなく、`Function*`を返す。
プロトタイプは、関数のための外部とのインタフェースを提供するため、コード生成時には、関数に対応するLLVM Functionを返すと考えると理解しやすい。

`FunctionType::get`の呼び出しは、与えられたプロトタイプのために使われる`FunctionType`を作成する。
Kaleidoscopeにおけるすべての関数の引数は、`double`型であるため、初めの行は、`LLVM double`型であるNのベクトルを作る。
そのとき、コードは、（可変長引数ではない、つまり`false`パラメータがこれを意味する）引数としてNという`double`型を取り、戻り値として`double`を一つ返し、関数型を作るための`FunctionType::get`メソッドを使う。
LLVMにおける型は、定数と同じようにユニークではないといけないため、型はnewするのではなく、LLVMランタイムが作った型をgetすることになる。

最終行で、プロトタイプに対応するIR関数を作る。
これは、関数に利用される型、リンク、名前に加えて，関数がどのモジュールに挿入されるかを明らかにする．
"External linkage"は，現在のモジュール外で関数が定義される，あるいは，モジュール外の関数から呼ばれることを意味する．
ここで渡される名前は，ユーザ指定する名前であり，`TheModule`が指定されるため，この名前は，`TheModule`のシンボルテーブルに登録されることになる．

```
// Set names for all arguments.
unsigned Idx = 0;
for (auto &Arg : F->args())
  Arg.setName(Args[Idx++]);

return F;
```

結局，プロトタイプ内で与えられた名前に従って，関数の引数それぞれの名前をセットする．
このステップは，厳密には必要ではないが，命名規則に一貫性を持たせるとIRを読みやすくし，後に続くコードがその名前を引数として考えられるにする．これは，プロトタイプの解析木で名前を見つけなければならないからそうするという意味ではないのである．

この点において，関数のプロトタイプは，実体を持っていない．
これは，LLVM IRが関数宣言をどう表現するかを示している．
Kaleidoscopeにおける`extern`をサポートするにためにこうする必要がある．
しかし，関数定義は，コードを生成し，関数の実態をアタッチする必要がある．

```
Function *FunctionAST::codegen() {
    // First, check for an existing function from a previous 'extern' declaration.
  Function *TheFunction = TheModule->getFunction(Proto->getName());

  if (!TheFunction)
    TheFunction = Proto->codegen();

  if (!TheFunction)
    return nullptr;

  if (!TheFunction->empty())
    return (Function*)LogErrorV("Function cannot be redefined.");
```

関数宣言のため，この指定された関数がすでに`TheModule`のシンボルテーブルに存在しないかを確認する．
このケースの場合には，一つの関数は，すでに`extern`を使って作られている．
`Module::getFunction`は，事前に関数が存在しない場合は，`null`を返し，`Prototype`から，関数のコードを生成する．
そうではない場合，まだ実体が作られていない状態などの関数が空であることを明言しないといけない．

```
// Create a new basic block to start insertion into.
BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
Builder.SetInsertPoint(BB);

// Record the function arguments in the NamedValues map.
NamedValues.clear();
for (auto &Arg : TheFunction->args())
  NamedValues[Arg.getName()] = &Arg;
```

今，`Builder`のセットアップが完了するところまできた．
初めの行は，"entry"という名前の新しい`basic block`を作る．
そして，このブロックは，`TheFunction`に挿入される．
二行目は，builderに新しい命令を新しいブロックの終わりに挿入すべきであることを伝えている．
LLVMにおけるBasic blocksは，制御フローグラフを定義する関数群の重要なパートとなる．
現在，我々は，制御フローを持たないので，この関数は，このポイントに，ひとつだけブロックを持つことになる．
この制御フローの問題は，５章でさらに改善していく．

次，`NamedValues`マップに関数の引数を追加する（はじめにマップをクリアした後）．
なので，引数は，`VariableExprAST`ノードからアクセス可能なのである．

```
if (Value *RetVal = Body->codegen()) {
  // Finish off the function.
  Builder.CreateRet(RetVal);

  // Validate the generated code, checking for consistency.
  verifyFunction(*TheFunction);

  return TheFunction;
}
```

一度，挿入するべきポイントがセットアップされ，`NamedValues`マップにデータが入力されると，次に，関数のルート表現のための`codegen()`を呼び出す．
もしエラーがなければ，このコードは，エントリーブロックに表現を計算するためのコードを出力し，計算すべき値を返す．
エラーを仮定しないとき，我々は，関数を完結するLLVM ret 命令を作成する．
一度，関数が作られると，我々は，LLVMから提供される`verifyFunction`をコールする．
この関数は，我々の作ったコンパイラがすべてを正しく実行しているかをチェックするため，生成されたコードの多様な一貫性のあるチェックを実行する．
この関数は，色々なバグを検出してくれるので，非常に重要である．
一度，関数のチェックが終了すると，それを返す．

```
    // Error reading body, remove function.
    TheFunction->eraseFromParent();
    return nullptr;
}
```

ここでやり残したことは，エラーハンドリングである．
簡単のため，我々は，`eraseFromParent`を呼び出して，愚直に関数自体を消すことでエラーをハンドリングすることにする．
これは，ユーザにコードが出てくる前にミスタイプした関数を再定義することを許してしまう．つまり，エラーが出た関数を消さない場合，シンボルテーブルは生き続け，その後に再定義できなくしてしまう．

このコードには，バグがある．
つまり，`FunctionAST::codegen()`がすでにあるIR関数を見つけた場合，定義自身のプロトタイプにそって署名が正しいかをチェックしない．
これは，関数定義の署名について，関数の引数名が間違っているような原因でコード生成に失敗するような，早い段階で`extern`で宣言された関数の優先度が高くなる．
これを修正する方法は，たくさんある．
以下のようなコードをちゃんと処理できる必要があるのだが，その修正方法を考えてほしい．

```
extern foo(a);     # ok, defines foo.
def foo(b) b;      # Error: Unknown variable name. (decl using 'a' takes precedence).
```

## ドライバーの更新・・・まとめ

ここでは，LLVMに対してのコード生成を十分に理解したとは言えない．
IRコールを色々，見てきたというだけである．
サンプルコードに，`HandleDefinition`, `HandleExtern`などの関数を加えることで，LLVM IRをダンプルすることができる．
これは，シンプルな関数に対するLLVM IRを見ていくのに非常に便利である．
例えば，以下のような実行結果が得られる．

```
ready> 4+5;
Read top-level expression:
define double @0() {
entry:
  ret double 9.000000e+00
}
```

パーサがトップレベルの表現で無名関数をどう処理するかを示している．
これは，次章で，JITをサポートするときに扱いやすい問題でもある．
コードは，文学的に説明しやすいだけでなく，`IRBuilder`によって定数が畳まれる以外の最適化が実行されていない．
次章では，ここに最適化を加えていくことになる．

```
ready> def foo(a b) a*a + 2*a*b + b*b;
Read function definition:
define double @foo(double %a, double %b) {
entry:
  %multmp = fmul double %a, %a
  %multmp1 = fmul double 2.000000e+00, %a
  %multmp2 = fmul double %multmp1, %b
  %addtmp = fadd double %multmp, %multmp2
  %multmp3 = fmul double %b, %b
  %addtmp4 = fadd double %addtmp, %multmp3
  ret double %addtmp4
}
```

これは，単純な算術計算の例である．
LLVM builderで呼び出した関数と，実際に出力される命令が似ていることに注意してほしい．

```
ready> def bar(a) foo(a, 4.0) + bar(31337);
Read function definition:
define double @bar(double %a) {
entry:
  %calltmp = call double @foo(double %a, double 4.000000e+00)
  %calltmp1 = call double @bar(double 3.133700e+04)
  %addtmp = fadd double %calltmp, %calltmp1
  ret double %addtmp
}
```

これは，関数呼び出しの例である．
この関数の呼び出しには，多様時間がかかる．
ここに，将来的に，再帰を使いやすくするため，条件付きの制御フローを追加していく．

```
ready> extern cos(x);
Read extern:
declare double @cos(double)

ready> cos(1.234);
Read top-level expression:
define double @1() {
entry:
  %calltmp = call double @cos(double 1.234000e+00)
  ret double %calltmp
}
```

これは，外部の`cos`関数を呼び出す例である．

```
ready> ^D
; ModuleID = 'my cool jit'

define double @0() {
entry:
  %addtmp = fadd double 4.000000e+00, 5.000000e+00
  ret double %addtmp
}

define double @foo(double %a, double %b) {
entry:
  %multmp = fmul double %a, %a
  %multmp1 = fmul double 2.000000e+00, %a
  %multmp2 = fmul double %multmp1, %b
  %addtmp = fadd double %multmp, %multmp2
  %multmp3 = fmul double %b, %b
  %addtmp4 = fadd double %addtmp, %multmp3
  ret double %addtmp4
}

define double @bar(double %a) {
entry:
  %calltmp = call double @foo(double %a, double 4.000000e+00)
  %calltmp1 = call double @bar(double 3.133700e+04)
  %addtmp = fadd double %calltmp, %calltmp1
  ret double %addtmp
}

declare double @cos(double)

define double @1() {
entry:
  %calltmp = call double @cos(double 1.234000e+00)
  ret double %calltmp
}
```

今作っているデモアプリを終了すると，アプリは，今作っているモジュールの全体のIRをダンプする．
ここで，それぞれに参照しあう，すべての関数の全体像を確認できる．

ここで，Kaleidoscopeの３章はおしまいです．
次は，JITによるコード生成と，コードの最適化について説明していきます．
