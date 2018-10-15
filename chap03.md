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