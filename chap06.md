# ユーザ定義演算子

## はじめに
６章へようこそ．
ここまでで，かなり最小限ではあるが，十分機能的で，使いやす言語を作ることができた．
しかし，いまだに大きな問題がある．
Kaleidoscopeには，使いやすい演算子（割り算や論理否定，不等号など）を持っていないのだ．

この章では，シンプルかつ美しいKaleidoscopeに，ちょっと逸脱して，ユーザ定義演算子を追加する．
この逸脱は，ややもすると，Kaleidoscopeがシンプルかつ"汚い"言語にになってしまうかもしれないが，同時により強力な言語になる．
自分で自分の言語を作ることの利点の一つに，自分で何がよくて，何が悪いかを決められる点がある．
このチュートリアルでは，おもしろいパーサの技術を紹介する方法として，ユーザ定義の演算子を追加することをよしとする．

このチュートリアルの終わりには，マンデルブロー集合をレンダリングするKaleidoscopeで作ったサンプルを実行することになる．
これは，Kaleidoscopeを作ったサンプルであり，その機能セットのサンプルとなる．

## ユーザ定義演算子の考え方
まず，Kaleidoscopeに追加する演算子のオーバーロードは，C++のような言語ではより一般的だ．
C++では，既存の演算子を再定義することだけが許され，その文法までは変更したり，新しい演算子を追加したり，優先順位を変更したりすることはできない．
本章では，Kaleidoscopeにこれらの機能を追加し，ユーザがサポートされる演算子に磨きをかけられるようにする．

このようなチュートリアルでユーザ定義演算子に関わるポイントは，手で書いたパーサを使うことの強力さと柔軟さを見せつけることにある．
さらに，実装していくパーサは，表現のための文法，演算子のパースの優先順位のほとんどのために，再帰的な血統を利用する．
詳細は，２章を見て欲しい．
演算子の優先順位を使うことによって，プログラマに新しい演算子を簡単に使わせることができる．
文法は，JITが実行されるたびに，動的に拡張される．

今から加える二つの特定の機能は，二項演算子と同じようなプログラミング可能な単一演算子である（現在，Kaleidoscopeは，まったく単一演算子を持っていないわけではない）．
例えば，以下のコードがある．

```
# Logical unary not.
def unary!(v)
  if v then
    0
  else
    1;

# Define > with the same precedence as <.
def binary> 10 (LHS RHS)
  RHS < LHS;

# Binary "logical or", (note that it does not "short circuit")
def binary| 5 (LHS RHS)
  if LHS then
    1
  else if RHS then
    1
  else
    0;

# Define = with slightly lower precedence than relationals.
def binary= 9 (LHS RHS)
  !(LHS < RHS | LHS > RHS);
```

多くの言語は，言語それ自身における一般的なランタイムライブラリを実装できるようになるよう切望している．
Kaleidoscopeでは，ライブラリ内の重要なパーツを実装できる．

これらの機能の実装について，二つのパーツへブレイクダウンしていく．
それらは，ユーザ定義の二項演算子をサポートする実装と，単一演算子を追加することである．

## ユーザ定義の二項演算子
ユーザ定義の二項演算子のサポートの追加は，極めて簡単だ．
まず，トークン定義と，単一/二項演算子のためのキーワードを追加する．

```
enum Token {
  ...
  // operators
  tok_binary = -11,
  tok_unary = -12
};
...
static int gettok() {
...
    if (IdentifierStr == "for")
      return tok_for;
    if (IdentifierStr == "in")
      return tok_in;
    if (IdentifierStr == "binary")
      return tok_binary;
    if (IdentifierStr == "unary")
      return tok_unary;
    return tok_identifier;
```

これは，ここまでの章と同じようにlexerに単一とバイナリのキーワードを追加しただけである．
現在の抽象構文木でいいところは，opcodeとして，アスキーコードを使うことで，二項演算子の一般性を表現していることだ．
ここで拡張された演算子のために，これと同じ表現を使う．このため，新しい抽象構文木やパーサのサポートは必要ではない．

一方，関数定義の`def binary| 5`の部分で，新しいオペレータの定義を表現できる．
我々の文法では，関数定義のための「名前」は，プロトタイプ宣言として，パースされ，抽象構文木の`PrototypeAST`ノードへ挿入される．
プロトタイプとして，新しいユーザ定義演算子を表現するために，以下のようにして，`PrototypeAST`ノードを拡張しなければならない．

```
/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its argument names as well as if it is an operator.
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator;
  unsigned Precedence;  // Precedence if a binary op.

public:
  PrototypeAST(const std::string &name, std::vector<std::string> Args,
               bool IsOperator = false, unsigned Prec = 0)
  : Name(name), Args(std::move(Args)), IsOperator(IsOperator),
    Precedence(Prec) {}

  Function *codegen();
  const std::string &getName() const { return Name; }

  bool isUnaryOp() const { return IsOperator && Args.size() == 1; }
  bool isBinaryOp() const { return IsOperator && Args.size() == 2; }

  char getOperatorName() const {
    assert(isUnaryOp() || isBinaryOp());
    return Name[Name.size() - 1];
  }

  unsigned getBinaryPrecedence() const { return Precedence; }
};
```

基本的に，プロトタイプのための名前を知っていることに加えて，それが演算子であるかどうかが，追跡できればよい．
また，それが演算子であれば，パーサの優先順位がわかればよい．
優先順位は，二項演算子にのみ使われる（以下で見るように，単一演算子には，優先順位が適応されない）．
ユーザ定義演算子のためのプロトタイプを表現する方法を以下に示す．

```
/// prototype
///   ::= id '(' id* ')'
///   ::= binary LETTER number? (id, id)
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  std::string FnName;

  unsigned Kind = 0;  // 0 = identifier, 1 = unary, 2 = binary.
  unsigned BinaryPrecedence = 30;

  switch (CurTok) {
  default:
    return LogErrorP("Expected function name in prototype");
  case tok_identifier:
    FnName = IdentifierStr;
    Kind = 0;
    getNextToken();
    break;
  case tok_binary:
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected binary operator");
    FnName = "binary";
    FnName += (char)CurTok;
    Kind = 2;
    getNextToken();

    // Read the precedence if present.
    if (CurTok == tok_number) {
      if (NumVal < 1 || NumVal > 100)
        return LogErrorP("Invalid precedence: must be 1..100");
      BinaryPrecedence = (unsigned)NumVal;
      getNextToken();
    }
    break;
  }

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken();  // eat ')'.

  // Verify right number of names for operator.
  if (Kind && ArgNames.size() != Kind)
    return LogErrorP("Invalid number of operands for operator");

  return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames), Kind != 0,BinaryPrecedence);
}
```

これは，かなり率直なパースのためのコードである．
そして，こんな感じのコードは，今までにたくさん見てきた．
このコードの一つ面白いところは，二項演算子のための`FnName`をセットアップする行である．
これは，新しく定義された`@`演算子のために，`binary@`のような名前を構築する．
このコードは，組み込みの`null`文字を含めて，LLVMのシンボルテーブルの中のシンボル名は，どんな文字列も使っていいという事実を利用している．

次のおもしろいところは，これらの二項演算子のために`codegen`をサポートすることである．
現在の構造のいては，このコードは，既存の二項演算子のノードのために，`default`ケースを`case`文に追加することになる．

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
    return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
  default:
    break;
  }

  // If it wasn't a builtin binary operator, it must be a user defined one. Emit
  // a call to it.
  Function *F = getFunction(std::string("binary") + Op);
  assert(F && "binary operator not found!");

  Value *Ops[2] = { L, R };
  return Builder.CreateCall(F, Ops, "binop");
}
```

上のコードに見えるように，新しいコードは，実際すごくシンプルである．
このコードは，シンボルテーブルにある適切な演算子を探し，それに対応する関数呼び出しを生成する．
ユーザ定義演算子は，ただの関数として構築されるため（"プロトタイプ"はつまるところ正しい名前を持つ関数になるため），すべて合点がいくと思われる．

コードの最後のピースが抜けていた，ちょっとしたtop-level表現のマジックを使って，

```
Function *FunctionAST::codegen() {
  // Transfer ownership of the prototype to the FunctionProtos map, but keep a
  // reference to it for use below.
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;

  // If this is an operator, install it.
  if (P.isBinaryOp())
    BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
  ...
```

基本的に，関数のコード生成の前に，もし，そのコードがユーザ定義演算子であるなら，それは，優先順位テーブルに登録されなければならない．
これは，すでにうまく作った二項演算子をパースするロジックを扱うために必要である．
十分に一般的な演算子の優先順月のパーサを作っているため，これを実現するたねにその文法を拡張するだけでよい．

今，我々は，使いやすいユーザ定義二項演算子を持っている．
これは，他の演算子のために築いた，ここまでの多くのフレームワークを築く．
ただ，これまでに単項演算子に関するフレームワークを作ってきていないので，単項演算子を追加することは，多少チャレンジングである．
それがなんなのかを理解してみよう．

## ユーザ定義単項演算子
今現在，Kaleidoscopeで単項演算子をサポートしていないため，単項演算子をサポートするために必要なものすべてをここから実装していかなければならない．
ここまでで，lexerに`unary`というキーワードのサポートだけはやってきた．
それに加えて，抽象構文木のノードを追加する必要がある．

```
/// UnaryExprAST - Expression class for a unary operator.
class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
    : Opcode(Opcode), Operand(std::move(Operand)) {}

  Value *codegen() override;
};
```

この抽象構文木のノードは，とてもシンプルでわかりやすい．
それは，単項演算子がひとつしか子ノードを取らないこと以外，二項演算子の抽象構文木のノードを直接反映している．
次に，パーサのロジックを追加する必要がある．
単項演算子のパーサは，いたってシンプルだ．
それを実行するための関数を追加しよう．

```
/// unary
///   ::= primary
///   ::= '!' unary
static std::unique_ptr<ExprAST> ParseUnary() {
  // If the current token is not an operator, it must be a primary expr.
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
    return ParsePrimary();

  // If this is a unary operator, read it.
  int Opc = CurTok;
  getNextToken();
  if (auto Operand = ParseUnary())
    return llvm::make_unique<UnaryExprAST>(Opc, std::move(Operand));
  return nullptr;
}
```

追加する文法は，まったく愚直なものだ．
もし，主表現をパースしている最中に単項演算子に出くわしたら，prefixとして，それを処理し，他の単項演算子として，残っている箇所をパースする．

これは，複数の単項演算子を扱えるようにしている（例えば，`!!x`のようなコード）．
単項演算子は，単項演算子は，曖昧なパース結果になり得ないので，優先順位を演算子に与える必要がない．

この関数にある問題は，コードのどこかで`ParseUnary`を呼び出す必要があることだ．
これを実装するため，`ParsePrimary`の呼び出しを，`ParseUnary`の呼び出しに変更する．

```
/// binoprhs
///   ::= ('+' unary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  ...
    // Parse the unary expression after the binary operator.
    auto RHS = ParseUnary();
    if (!RHS)
      return nullptr;
  ...
}
/// expression
///   ::= unary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParseUnary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}
```

これらの二つのシンプルな変更点でもって，単項演算子をパースし，抽象構文木を構築できるようになった．
次に，プロトタイプをサポートするためのパーサを追加し，単項演算子のプロトタイプをパースする必要がある．
ここで，上記の二項演算子のためのコードを拡張する．

```
/// prototype
///   ::= id '(' id* ')'
///   ::= binary LETTER number? (id, id)
///   ::= unary LETTER (id)
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  std::string FnName;

  unsigned Kind = 0;  // 0 = identifier, 1 = unary, 2 = binary.
  unsigned BinaryPrecedence = 30;

  switch (CurTok) {
  default:
    return LogErrorP("Expected function name in prototype");
  case tok_identifier:
    FnName = IdentifierStr;
    Kind = 0;
    getNextToken();
    break;
  case tok_unary:
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected unary operator");
    FnName = "unary";
    FnName += (char)CurTok;
    Kind = 1;
    getNextToken();
    break;
  case tok_binary:
    ...
```

二項演算子と同じように，演算子として使う文字を含む名前で，単項演算子を名付ける．
これは，コード生成時に助けになる．
まぁ，言うと，追加しないといけない最後のコードは，単項演算子のための`codegen`である．

```
Value *UnaryExprAST::codegen() {
  Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;

  Function *F = getFunction(std::string("unary") + Opcode);
  if (!F)
    return LogErrorV("Unknown unary operator");

  return Builder.CreateCall(F, OperandV, "unop");
}
```

このコードも，二項演算子のためのコードと，似ているが，よりシンプルである．
他の事前に定義されたオペレータを扱う必要がないので，それは，本来シンプルなものである．

## Kicking the Tires（日本語あるか？この表現）
信じがたいかもしれないが，ちょっとした拡張で，我々は，最後の章をカバーし，現実的に使える言語に，それを育てることになる．
これによって，I/O，数学，その他，多くの面白いことを実行できるようになる．
例えば，配列演算子を追加できる（`printd`は，指定された値と改行コードを表示するために定義される）．

```
ready> extern printd(x);
Read extern:
declare double @printd(double)

ready> def binary : 1 (x y) 0;  # Low-precedence operator that ignores operands.
...
ready> printd(123) : printd(456) : printd(789);
123.000000
456.000000
789.000000
Evaluated to 0.000000
```

また，他の`primitive`な演算子も定義できる．たとえば，

```
# Logical unary not.
def unary!(v)
  if v then
    0
  else
    1;

# Unary negate.
def unary-(v)
  0-v;

# Define > with the same precedence as <.
def binary> 10 (LHS RHS)
  RHS < LHS;

# Binary logical or, which does not short circuit.
def binary| 5 (LHS RHS)
  if LHS then
    1
  else if RHS then
    1
  else
    0;

# Binary logical and, which does not short circuit.
def binary& 6 (LHS RHS)
  if !LHS then
    0
  else
    !!RHS;

# Define = with slightly lower precedence than relationals.
def binary = 9 (LHS RHS)
  !(LHS < RHS | LHS > RHS);

# Define ':' for sequencing: as a low-precedence operator that ignores operands
# and just returns the RHS.
def binary : 1 (x y) y;
```

事前に`if/then/else`がサポートされる場合，I/Oのための面白い関数を定義できる．
例えば，次のような，文字の"濃度？"を反映する値を出力する関数がある．

```
ready> extern putchard(char);
...
ready> def printdensity(d)
  if d > 8 then
    putchard(32)  # ' '
  else if d > 4 then
    putchard(46)  # '.'
  else if d > 2 then
    putchard(43)  # '+'
  else
    putchard(42); # '*'
...
ready> printdensity(1): printdensity(2): printdensity(3):
       printdensity(4): printdensity(5): printdensity(9):
       putchard(10);
**++.
Evaluated to 0.000000
```

これらのシンプルなprimitive演算子をベースに，もっとおもしろいことを定義できる．t
例えば，ここに反復回数を決定する小さな関数がある．
その関数は，発散する複雑な平面上のある関数を引数にとる？

```
# Determine whether the specific location diverges.
# Solve for z = z^2 + c in the complex plane.
def mandelconverger(real imag iters creal cimag)
  if iters > 255 | (real*real + imag*imag > 4) then
    iters
  else
    mandelconverger(real*real - imag*imag + creal,
                    2*real*imag + cimag,
                    iters+1, creal, cimag);

# Return the number of iterations required for the iteration to escape
def mandelconverge(real imag)
  mandelconverger(real, imag, 0, real, imag);
```

この`z = z2 + c`という関数は，マンデルブロー集合の計算のための基本であり，美しく，小さな生命とも言える．
我々のマンデル収束関数は，その関数が複雑な軌道から脱出するために必要な反復回数を返す．この反復回数は，最大で２５５である．


This is not a very useful function by itself, but if you plot its value over a two-dimensional plane, you can see the Mandelbrot set. Given that we are limited to using putchard here, our amazing graphical output is limited, but we can whip together something using the density plotter above:

```
# Compute and plot the mandelbrot set with the specified 2 dimensional range
# info.
def mandelhelp(xmin xmax xstep   ymin ymax ystep)
  for y = ymin, y < ymax, ystep in (
    (for x = xmin, x < xmax, xstep in
       printdensity(mandelconverge(x,y)))
    : putchard(10)
  )

# mandel - This is a convenient helper function for plotting the mandelbrot set
# from the specified position with the specified Magnification.
def mandel(realstart imagstart realmag imagmag)
  mandelhelp(realstart, realstart+realmag*78, realmag,
             imagstart, imagstart+imagmag*40, imagmag);
```

Given this, we can try plotting out the mandelbrot set! Lets try it out:

```
ready> mandel(-2.3, -1.3, 0.05, 0.07);
*******************************+++++++++++*************************************
*************************+++++++++++++++++++++++*******************************
**********************+++++++++++++++++++++++++++++****************************
*******************+++++++++++++++++++++.. ...++++++++*************************
*****************++++++++++++++++++++++.... ...+++++++++***********************
***************+++++++++++++++++++++++.....   ...+++++++++*********************
**************+++++++++++++++++++++++....     ....+++++++++********************
*************++++++++++++++++++++++......      .....++++++++*******************
************+++++++++++++++++++++.......       .......+++++++******************
***********+++++++++++++++++++....                ... .+++++++*****************
**********+++++++++++++++++.......                     .+++++++****************
*********++++++++++++++...........                    ...+++++++***************
********++++++++++++............                      ...++++++++**************
********++++++++++... ..........                        .++++++++**************
*******+++++++++.....                                   .+++++++++*************
*******++++++++......                                  ..+++++++++*************
*******++++++.......                                   ..+++++++++*************
*******+++++......                                     ..+++++++++*************
*******.... ....                                      ...+++++++++*************
*******.... .                                         ...+++++++++*************
*******+++++......                                    ...+++++++++*************
*******++++++.......                                   ..+++++++++*************
*******++++++++......                                   .+++++++++*************
*******+++++++++.....                                  ..+++++++++*************
********++++++++++... ..........                        .++++++++**************
********++++++++++++............                      ...++++++++**************
*********++++++++++++++..........                     ...+++++++***************
**********++++++++++++++++........                     .+++++++****************
**********++++++++++++++++++++....                ... ..+++++++****************
***********++++++++++++++++++++++.......       .......++++++++*****************
************+++++++++++++++++++++++......      ......++++++++******************
**************+++++++++++++++++++++++....      ....++++++++********************
***************+++++++++++++++++++++++.....   ...+++++++++*********************
*****************++++++++++++++++++++++....  ...++++++++***********************
*******************+++++++++++++++++++++......++++++++*************************
*********************++++++++++++++++++++++.++++++++***************************
*************************+++++++++++++++++++++++*******************************
******************************+++++++++++++************************************
*******************************************************************************
*******************************************************************************
*******************************************************************************
Evaluated to 0.000000
ready> mandel(-2, -1, 0.02, 0.04);
**************************+++++++++++++++++++++++++++++++++++++++++++++++++++++
***********************++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*********************+++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
*******************+++++++++++++++++++++++++++++++++++++++++++++++++++++++++...
*****************+++++++++++++++++++++++++++++++++++++++++++++++++++++++++.....
***************++++++++++++++++++++++++++++++++++++++++++++++++++++++++........
**************++++++++++++++++++++++++++++++++++++++++++++++++++++++...........
************+++++++++++++++++++++++++++++++++++++++++++++++++++++..............
***********++++++++++++++++++++++++++++++++++++++++++++++++++........        .
**********++++++++++++++++++++++++++++++++++++++++++++++.............
********+++++++++++++++++++++++++++++++++++++++++++..................
*******+++++++++++++++++++++++++++++++++++++++.......................
******+++++++++++++++++++++++++++++++++++...........................
*****++++++++++++++++++++++++++++++++............................
*****++++++++++++++++++++++++++++...............................
****++++++++++++++++++++++++++......   .........................
***++++++++++++++++++++++++.........     ......    ...........
***++++++++++++++++++++++............
**+++++++++++++++++++++..............
**+++++++++++++++++++................
*++++++++++++++++++.................
*++++++++++++++++............ ...
*++++++++++++++..............
*+++....++++................
*..........  ...........
*
*..........  ...........
*+++....++++................
*++++++++++++++..............
*++++++++++++++++............ ...
*++++++++++++++++++.................
**+++++++++++++++++++................
**+++++++++++++++++++++..............
***++++++++++++++++++++++............
***++++++++++++++++++++++++.........     ......    ...........
****++++++++++++++++++++++++++......   .........................
*****++++++++++++++++++++++++++++...............................
*****++++++++++++++++++++++++++++++++............................
******+++++++++++++++++++++++++++++++++++...........................
*******+++++++++++++++++++++++++++++++++++++++.......................
********+++++++++++++++++++++++++++++++++++++++++++..................
Evaluated to 0.000000
ready> mandel(-0.9, -1.4, 0.02, 0.03);
*******************************************************************************
*******************************************************************************
*******************************************************************************
**********+++++++++++++++++++++************************************************
*+++++++++++++++++++++++++++++++++++++++***************************************
+++++++++++++++++++++++++++++++++++++++++++++**********************************
++++++++++++++++++++++++++++++++++++++++++++++++++*****************************
++++++++++++++++++++++++++++++++++++++++++++++++++++++*************************
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++**********************
+++++++++++++++++++++++++++++++++.........++++++++++++++++++*******************
+++++++++++++++++++++++++++++++....   ......+++++++++++++++++++****************
+++++++++++++++++++++++++++++.......  ........+++++++++++++++++++**************
++++++++++++++++++++++++++++........   ........++++++++++++++++++++************
+++++++++++++++++++++++++++.........     ..  ...+++++++++++++++++++++**********
++++++++++++++++++++++++++...........        ....++++++++++++++++++++++********
++++++++++++++++++++++++.............       .......++++++++++++++++++++++******
+++++++++++++++++++++++.............        ........+++++++++++++++++++++++****
++++++++++++++++++++++...........           ..........++++++++++++++++++++++***
++++++++++++++++++++...........                .........++++++++++++++++++++++*
++++++++++++++++++............                  ...........++++++++++++++++++++
++++++++++++++++...............                 .............++++++++++++++++++
++++++++++++++.................                 ...............++++++++++++++++
++++++++++++..................                  .................++++++++++++++
+++++++++..................                      .................+++++++++++++
++++++........        .                               .........  ..++++++++++++
++............                                         ......    ....++++++++++
..............                                                    ...++++++++++
..............                                                    ....+++++++++
..............                                                    .....++++++++
.............                                                    ......++++++++
...........                                                     .......++++++++
.........                                                       ........+++++++
.........                                                       ........+++++++
.........                                                           ....+++++++
........                                                             ...+++++++
.......                                                              ...+++++++
                                                                    ....+++++++
                                                                   .....+++++++
                                                                    ....+++++++
                                                                    ....+++++++
                                                                    ....+++++++
Evaluated to 0.000000
ready> ^D
```

At this point, you may be starting to realize that Kaleidoscope is a real and powerful language. It may not be self-similar :), but it can be used to plot things that are!
With this, we conclude the “adding user-defined operators” chapter of the tutorial. We have successfully augmented our language, adding the ability to extend the language in the library, and we have shown how this can be used to build a simple but interesting end-user application in Kaleidoscope. At this point, Kaleidoscope can build a variety of applications that are functional and can call functions with side-effects, but it can’t actually define and mutate a variable itself.
Strikingly, variable mutation is an important feature of some languages, and it is not at all obvious how to add support for mutable variables without having to add an “SSA construction” phase to your front-end. In the next chapter, we will describe how you can add variable mutation without building SSA in your front-end.

## Full Code Listing
Here is the complete code listing for our running example, enhanced with the support for user-defined operators. To build this example, use:

On some platforms, you will need to specify -rdynamic or -Wl,–export-dynamic when linking. This ensures that symbols defined in the main executable are exported to the dynamic linker and so are available for symbol resolution at run time. This is not needed if you compile your support code into a shared library, although doing that will cause problems on Windows.
Here is the code:
