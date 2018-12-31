# パーサと抽象構文木の構築

パーサは，再帰的深さ探索とオペレータ優先探索の組み合わせで実装される．
Kaleidscope言語における後者はバイナリ表現を，前者は他のすべての表現をパースする．
まず，抽象構文木について説明する．

## 抽象構文木・・・The Abstract Syntax Tree

ASTは，プログラムが後段でコードを生成するときに翻訳が簡単になるような振る舞いを獲得する．
我々は，基本的にその言語のそれぞれ構造に対する一つのオブジェクトがほしい．
そして，ASTは，言語を正しくモデリングすべきである．
Kaleidoscopeでは，変数宣言，表現，関数宣言などが必要である．

```
/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() {}
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
};
```
上のクラスが構文木のノードとなるルートクラス．
`NumberExprAST`は，数値リテラルを保存するためのノードになる．
このクラスが，後段でコンパイラがどんな数値が保存されているかを知るための手段になる．

しかし，現状，ASTを作っただけでは，木の中身にアクセスする方法がないので，便利に使えない．
クラスに仮想関数を加えて，`print`を簡単に実行できるようにしたりする必要がある．
ここで，もうちょっと要素を追加する．

```
/// VariableExprAST - 変数を保存するための要素
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
};

/// BinaryExprAST - +とかの演算子を保存するための要素？
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
    : Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

/// CallExprAST - 関数呼び出しと引数を保存するための木
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
    : Callee(Callee), Args(std::move(Args)) {}
};
```

とりあえず，この基本的な言語が定義するノードはこれがすべてである．
実は，条件制御がないため，チューリング完全ではないが，それは後で追加で実装していくことにする．

次に必要なのは，関数のインタフェースと関数自身である．

```
/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &name, std::vector<std::string> Args)
    : Name(name), Args(std::move(Args)) {}

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
};
```

Kaleidoscopeでは，関数は，引数の数しか保存しない．
これは，Kaleidoscopeでは，変数が`double`型しか存在しないため，それらの型を区別する必要がないためである．
一般的な言語の場合，`ExprAST`は，型を保存するフィールドを持たねばならない．

## パーサの基本

ここまでで，ビルドすべき木を設計できたが，次に，木を作るパーサを設計する必要がある．
"x+y"のような表現があるとき("x+y"は，lexerによって3つのトークンに分割される)，それを以下のような表現に変換したい．

```
auto LHS = llvm::make_unique<VariableExprAST>("x");
auto RHS = llvm::make_unique<VariableExprAST>("y");
auto Result = std::make_unique<BinaryExprAST>('+', std::move(LHS), std::move(RHS));
```

これを実現するために，色々関数を作っていく．
`CurTok`は，トークンのバッファで，パーサが注目してるトークンそのものである．
`getNextToken`は，つぎのトークンを取ってくると同時に`CurTok`を更新する．

```
/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() {
  return CurTok = gettok();
}
```

エラーハンドリングは，こんな感じにする．
本来ならば，もっとユーザフレンドリなものにすべきだが，我々のチュートリアルならば，この程度でよい．

```
/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "LogError: %s\n", Str);
  return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}
```

こういった関数群を使って，数値リテラルをパースするパーサの初歩的な部分を実装していく．

##  Basic Expression Parsing

まずは、もっとも簡単なプロセスなので、数字のリテラルから始める。
このグラマーのそれぞれの生成物に対して、それを生成する関数を定義していく。
数字のリテラルを処理するコードは、次のようになる。

```
/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = llvm::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return std::move(Result);
}
```

このメソッドは、チェックしているトークンの種類が`tok_number`であるときに実行される。
数値のノードを生成し、新しいトークンを取得し、生成したものを戻り値として返す。
`std::move`は、所有権の話。
`Result`は、この後、破棄され、戻り値を受け取った先のスコープに所有権が移される。

ここにいくつかおもしろい点が見受けられる。
もっとも重要なことは、この関数が、生成物に対応するトークンの全てを処理し、次のトークンを返すということである。
これは、再帰的深さ優先探索では、至極普通な方法である。
いい例を出してみよう。
次は、かっこを処理するコードである。

```
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
```

この関数は、パーサの面白い点を多分に説明するものである。

1. `LogError`関数の使い方。この関数が呼ばれるとき、現在のトークンが`(`であることを期待されている。しかし、それに続く表現をパースした後、`)`がない可能性もある。
2. `ParseExpression`を再帰的に呼ぶ構造になっている点。`ParseExpression`が`ParseParenExpr`をコールすることになるのは、後で確認する。これは強力な実装だ。なぜなら、文法を再帰的に処理できるようにするし、それぞれの成果物をとてもシンプルにする。かっこ自体は、ASTのノードの構造に組み込まれない。組み込むこともできるが、かっこの大切な役割は、パーサのガイドであり、変数のグルーピングである。一度ASTが構築されれば、かっこはもはや必要ないのである。

次にシンプルな、変数と関数呼出のハンドリングのためのコードを示す。

```
/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken();  // eat identifier.

  if (CurTok != '(') // Simple variable ref.
    return llvm::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken();  // eat (
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (1) {
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
```

この関数も，他の関数と同じようなスタイルになる．
現在のトークンは`tok_identifier`トークンであるかどうかを問い合わせることを想定している．
関数は，再帰とエラーハンドリングを使う．
この関数のおもしろいときは，現在の識別子がスタンドアローンで識別できるのか，関数呼び出しなのか，を先読みで決定するというところにある．
識別子の後のトークンが`(`トークンであるかどうか，`VariableExprAST`や`CallExprAST`ノードを適切に構築しているかを見るためにこれをチェックする．

我々のシンプルな表現パーサのロジックを一箇所にまとめ，それを一箇所のエントリーポイントにまとめる．
これをprimary表現と呼ぶ．
そう呼ぶ理由は，後々明らかになる．

```
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
```

この関数の定義を理解しよう。
我々が個々の関数内部に`CurTok`の状態があることを前提とするのは明らかである。
これは、先読みで表現の優先順位？を決定するために使われ、関数呼び出しをパースする。

## Binary Expression Parsing
二項演算のパースは，かなり難しい．
これは，しばしば曖昧で，一意に決定されないことがあるためである．
たとえば，"x+y*z"が文字列として与えられた場合，パーサは， “(x+y)*z”あるいは“x+(y*z)”を選択しなければならない．
数学のルールとして，我々は後者を選択することになるが，それは，乗算は，加算よりも優先度が高いからである．

これを処理する方法はたくさんあるが，もっとも美しく，効率のよい方法は，"Operator-Precedence Parsing"である．
これは，再帰的に二項演算の優先度を処理していく方法である．
この方法を使うには，まず最初に優先度テーブルが必要になる．

```
/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0) return -1;
  return TokPrec;
}

int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;  // highest.
  ...
}
```

Kaleidoscopeの基本的な形式のため，４つの二項演算子のみをサポートする．
賢明な読者諸君にとって，これを拡張していくのはよい勉強になるだろう．
`GetTokPrecedence`は，トークンが二項演算子でなければ-1を，二項演算子であれば，その優先度を返す．
マップを作っておけば，新しい演算子を追加したり，特定の演算子の処理方法に依存するようなことがなくなる．
しかしながら，マップをつかわず，　`GetTokPrecedence`の内部で演算子の比較処理を実装するのも，そんなに難しい話ではないし，固定配列長でやってもいい．

上で定義した`GetTokPrecedence`を使って，我々は，二項演算子のパーサの実装を始められる．
"Operator-Precedence Parsing"のベースとなるアイデアは，潜在的にあいまい性を持つ二項演算子による表現をひとつずつ解消していくことである．
例えば，“a+b+(c+d)*e*f+g”を例に考えてみる．
"Operator-Precedence Parsing"は，二項演算子で区切られた"primary expression"の流れとして考えることができる．
具体的には，最初にaという"primary expression"があり，その後，`[+, b]`，`[+, (c+d)]`，`[*, e]`，`[*, f]`および`[+, g]`のペアが続く．
括弧は，"primary expression"であるため，二項演算子は，(c+d)のような入れ子になった表現に困ることはない．

[二項演算子，主表現]が続く主表現からパースを始める．

```
/// expression
///   ::= primary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}
```

`ParseBinOpRHS`は，ペアのシーケンスをパースする関数である．
その関数は，パースされた二項演算子の表現へのポインタと優先度を取る．
`x`は，完全に有効な表現であり，"二項演算子"が空であることが許される．
上の例では，最初の`a`は，`ParseBinOpRHS`に，現在のトークンである`+`と一緒に渡されることになる．

`ParseBinOpRHS`に渡される優先度の値は，関数が処理すべき，最小単位のオペレータの優先度を示す．
例えば，もし現在のペアが`[+, x]`であり，`ParseBinOpRHS`が優先度として40が渡された場合，`+`の優先度が20しかないので，トークンは何も処理されない．
これを気に留めながら，`ParseBinOpRHS`は，以下のコードから始まる

```
/// binoprhs
///   ::= ('+' primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  // If this is a binop, find its precedence.
  while (1) {
    int TokPrec = GetTokPrecedence();

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;
```

このコードは，まず現在のトークンの優先度を取得し，それが低すぎないかをチェックする．
優先度が-1のトークンは，エラーであると定義しているため，このチェックは，暗黙に処理すべき二項演算子のトークンの流れが尽きた時，処理すべきペアのストリームが終わるということにしている．
もし，このチェックにパスすると，トークンが二項演算子であり，以下の表現を含んでいると仮定でき，

```
// Okay, we know this is a binop.
int BinOp = CurTok;
getNextToken();  // eat binop

// Parse the primary expression after the binary operator.
auto RHS = ParsePrimary();
if (!RHS)
  return nullptr;
```

このコードは，二項演算子を処理し，記憶し，続く主表現をパースしていく．
これは，すべてのペアを作っていき，今回のサンプルでは，`+`と`b`のペアが一番最初に来る．

今，左辺とRHSシーケンスのペアのひとつをパースしようとしているとすると，表現を組み合わせ方を決めなければならない．
特に，`(a+b) <まだパースしてない二項演算子>`あるいは`a+(b <まだパースしてない二項演算子>)`とかが対象のときである．
これを決定するために，その優先度を決定するために，二項演算子を先読みし，現在の二項演算子の優先度と比較する．

```
// If BinOp binds less tightly with RHS than the operator after RHS, let
// the pending operator take RHS as its LHS.
int NextPrec = GetTokPrecedence();
if (TokPrec < NextPrec) {
```

`RHS`の右辺に対する二項演算子の優先度が，現状の演算子の優先度と等しいか，それより低いい場合，括弧は，"(a+b), 二項演算子"として処理される．
このサンプルの場合，現在の演算子は，`+`で，次の演算子が`+`であり，同じ優先度である．
この場合，まず，`a+b`に対するASTノードが作られるとして，次にパースは続いて，

```
  ... if body omitted ...
    }

    // Merge LHS/RHS.
    LHS = llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
                                           std::move(RHS));
  }  // loop around to the top of the while loop.
}
```

上の例では，`a+b+`を`(a+b)`に変え，そして，`+`をカレントトークンとして次のループに入る．
上のコードは，主表現として，`(c+d)`を処理・パースし，`[+, (c+d)]`を次の処理すべきペアとする．
次に，`if (TokPrec < NextPrec) {`のif文を評価し，主表現の右辺の二項演算子として`*`を採用する．
この場合，`*`の優先度は，`+`よりも高いので．if文の中に入ることになる．

ここで残された重要な問題は，"どうやってif文が右辺を最後までパースするか？"ということである．
特に，構文木を作るために，今回の例だと，`(c+d)*e*f`のすべてを二項演算子の右辺として処理する必要がある．
しかし，これをするためのコードは，驚くほど簡単なのである．
(上のコードをちょっとコピペするだけでよい.つまるところ，再帰的に二項演算子の右辺をパースしつづければよいのである．)

```
   // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }
    // Merge LHS/RHS.
    LHS = llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
                                           std::move(RHS));
  }  // loop around to the top of the while loop.
}
```

今，現在パースしている二項演算子よりも，"主表現の右辺"に対する二項演算子の優先度が高いと知っているとする．
そういった場合，`+`より高い優先度を持つ演算子とペアになったシーケンスが一緒にパースされ，"主表現の右辺"として返される方がよいとわかる．
これに対して，我々は，最小の優先度を`TokPrec+1`として一つ増やし，再帰的に`ParseBinOpRHS`を呼び出すことになります．
上の実装では，右辺として，`(c+d)*e*f`が構文木のノードとして返され，それとセットになる演算子は，`+`になります．

最後に，次のwhile文で，構文木に`+g`が追加されます．
14行足らずのコードで，我々は，美しい方法で，すべての一般的な二項演算表現を取り扱うことができました．
一気にコードを説明しました．
このコードがどうやって動くのかを把握するため，簡単ではない表現を入力し，それを確かめるとよいでしょう．

## 残りをパースする

やり残したことは，関数宣言の取り扱いです．
Kaleidoscopeでは，関数の実装宣言も，関数の宣言にも，`extern`を使います．
愚直に実装すると，以下のようになるだろう．

```
/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  // Read the list of argument names.
  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken();  // eat ')'.

  return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}
```

関数定義は，非常にシンプルなので，実装も難しくはない．

```
/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken();  // eat def.
  auto Proto = ParsePrototype();
  if (!Proto) return nullptr;

  if (auto E = ParseExpression())
    return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}
```

加えて，ユーザが作る関数の宣言に加えて，`sin`や`cos`のような関数を宣言するために`extern`をサポートします．
この`extern`は，本体を持たないプロトタイプ宣言ということになります．

```
/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken();  // eat extern.
  return ParsePrototype();
}
```

最後に，任意のトップレベルの表現を使えるようにし，その場でそれを評価できるようにする．
そういった表現は，引数がない無名関数を定義することで処理する．

```
/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = llvm::make_unique<PrototypeAST>("", std::vector<std::string>());
    return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}
```

これで，すべてのピースはそろった．
次に，これらのコードを実際に動かすための`driver`を実装することにする．

## Driver

driverは，トップレベルのループでピースをすべてパースする．
driverは，ここではそれ以上の意味はない．

```
/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (1) {
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
```

このコードでおもしろいところは，トップレベルのセミコロンを無視する点にある．
なぜそうするのだろうか．
基本的な理由は，cliで`4 + 5`をタイプした場合，パーサはそのタイプに続きがあるのか，ないのか判定できないためである．
例えば，次の行で，`def foo...`とタイプされた場合，`4+5`は，トップレベルの表現として処理することになる．
一方で，`*6`と続いた場合は，表現は途切れず続いていくことになる．
トップレベルのセミコロンは，`4+5;`とタイプすることで，区切りを入力できるようにするものである．

## まとめ

400行くらいのコードで，我々は，十分に，最小の言語のlexer，パーサ，構文木を定義した．
これを実行すれば，Kaleidoscopeのコードが正しいものかを判断するツールとなる．例えば，以下のような実行結果が得られる．

```
$ ./a.out
ready> def foo(x y) x+foo(y, 4.0);
Parsed a function definition.
ready> def foo(x y) x+y y;
Parsed a function definition.
Parsed a top-level expr
ready> def foo(x y) x+y );
Parsed a function definition.
Error: unknown token when expecting an expression
ready> extern sin(a);
ready> Parsed an extern
ready> ^D
$
```

このコードの拡張の余地は，たくさんある．
新しい，構文木のノードを定義したり，たくさんの拡張の方向性がある．
次章では，LLVM中間表現，LLVM IRを構文木から生成する方法について説明していく．