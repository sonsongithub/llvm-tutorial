# 制御フロー

## はじめに
５章にようこそ．ここまでで，LLVM IRを出力したり，JITを実装したりしてきたが，`Kaleidoscope`自体は，まったく使い物にならないものだった．
それは，`Kaleidoscope`が，関数呼び出しと`return`以外に制御フローを持たないからである．
これは，条件分岐が実装できないことを意味しており，その能力を大きく制限するものである．
本章では，`Kaleidoscope`に，`if/then/else`表現と，`for`ループを追加する．

## `if/then/else`
`Kaleidoscope`が`if/then/else`をサポートするように拡張することは，率直にいって，当たり前の話である．
このためには，lexer，パーサ，抽象構文木，LLVMコードジェネレータに新しいコンセプトを追加しなければならない．
時間をかけながら，新しいアイデアを見つけながら，拡張し，言語を育てていくことがいかに簡単かを示す例として，この例は非常によい．

この拡張をどうやって加えるか，に取り掛かる前に，「何」が欲しいかについて議論する．基本的なアイデアは，以下のようなコードが書けるようになることである．

```
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1)+fib(x-2);
```

`Kaleidoscope`では，`construct`は，すべて表現である．すなわち，`statement`は存在しない．
`if/then/else`のような表現は，ほかと同じように戻り値を必要とする．
我々がもっとも関数的な形式を使っているため，それを，その条件として評価させ,
`then`や，`else`の値を，その条件がどう解決されたかに基づいて返すようにする．
これは，C言語の三項演算子`?`表現にとてもよく似ている．

`if/then/else`のsemantics(適当な訳語がない)は，ブーリアンと同じ値である`0.0`を`false`として扱い，それ以外のすべての値を`true`として扱う．
最初の条件が`true`の場合，はじめのsubexpressionが評価され，返される．また，条件がfalseの場合，二番目のsubexpressionが評価され．返される．
`Keleidoscopse`は，副作用（関数型言語ではないということ）を許容するため，この振る舞いを突き詰めることは重要である．

さて，何が欲しいのかが明らかになたので，次は，これを要素にわけていこう．

### `if/then/else`のためのlexerの拡張
lexerの拡張は，愚直である．まず，新しい列挙子を追加する．

```
// control
tok_if = -6,
tok_then = -7,
tok_else = -8,
```

これらのトークンを認識するために，以下のように識別を追加する．

```
...
if (IdentifierStr == "def")
  return tok_def;
if (IdentifierStr == "extern")
  return tok_extern;
if (IdentifierStr == "if")
  return tok_if;
if (IdentifierStr == "then")
  return tok_then;
if (IdentifierStr == "else")
  return tok_else;
return tok_identifier;
```

### 抽象構文木の拡張
次い，lexerが生成するトークンを処理し，抽象構文木を作る．
パーサのロジックは，単純である．
以下のように，`if/then/else`を保持するノードを実装する．

```
/// ifexpr ::= 'if' expression 'then' expression 'else' expression
static std::unique_ptr<ExprAST> ParseIfExpr() {
  getNextToken();  // eat the if.

  // condition.
  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_then)
    return LogError("expected then");
  getNextToken();  // eat the then

  auto Then = ParseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != tok_else)
    return LogError("expected else");

  getNextToken();

  auto Else = ParseExpression();
  if (!Else)
    return nullptr;

  return llvm::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                      std::move(Else));
}
```

次に，primary expressionをフックする．

```
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
  case tok_if:
    return ParseIfExpr();
  }
}
```

### `if/then/else`のためのLLVM IR
さて，パーサと抽象構文木が作れたので，次に，LLVMのコード生成をサポートする．
これは，`if/then/else`を使った例のもっともおもしろいところでもある．
なぜなら，新しい考え方を紹介し始めるポイントだからである．
ここまでのコードは，ここまでの章で説明したことと何も変わらないのである．

ここで書きたいコードを動機づけるものとして，以下の`Kaleidoscopseの`例を見てみよう．

```
extern foo();
extern bar();
def baz(x) if x then foo() else bar();
```

もし，最適化をオフにしていれば，以下のような'Kaleidoscope`は，以下のようなLLVM IRコードを生成するようになる．

```
declare double @foo()

declare double @bar()

define double @baz(double %x) {
entry:
  %ifcond = fcmp one double %x, 0.000000e+00
  br i1 %ifcond, label %then, label %else

then:       ; preds = %entry
  %calltmp = call double @foo()
  br label %ifcont

else:       ; preds = %entry
  %calltmp1 = call double @bar()
  br label %ifcont

ifcont:     ; preds = %else, %then
  %iftmp = phi double [ %calltmp, %then ], [ %calltmp1, %else ]
  ret double %iftmp
}
```

この制御フローのグラフ(Call flow graph, CFG)を可視化するには，LLVMのかっこいい機能を使えば良い．
もし，LLVM IRを`t.ll`ファイルに書き出したなら，`llvm-as < t.l | opt -analyze -view-cfg`を実行すれば，ウィンドウが開き，以下のようなグラフが表示される．

ちなみに，macOSで，表示する場合は，以下のようなコマンドでdotファイルをpngファイルに変換して閲覧するとよい．

```
opt -dot-cfg ./t.ll
dot -Tpng ./cfg.baz.dot -o ./callgraph.png
```

`dot`コマンドは，graphvizに含まれる．
graphvizは，brewでインストールすると良い．

```
brew install graphviz
```

![](./callgraph.png)

これ以外の方法として，`F->viewCFG()`や`F->viewCFGOnly()`を呼ぶ方法もある(`F`は，`Function`クラス)．
これを呼ぶときは，実際のコードにこれを実装して再コンパイルするか，デバッガでこのコードを呼ぶか，する．
LLVMは，こういったいい感じの可視化ツールがたくさん揃っている．

コード生成に話を戻すと，いたってシンプルである．
つまり，エントリーブロックが，条件表現（ここでは変数`x`）を評価し，`fcmp one`命令を使って，結果が`0.0`と比較する．
`fcmp one`は，"確固として異なっている"ことを確認する命令である（まったく同じ値ということ？）．
この表現の結果，つまり`true`あるいは`false`の値になる，に基づき，コードは，`then`あるいは`else`ブロックへジャンプする．

`then/else`ブロックの実行が一度終了すると，それらの両方のブランチは，` if/then/else`の後のコードを実行するために，`ifcont`ブロックへ戻る．
この場合，やらなければならないことは，関数の呼び出し元へ返すことである．
そのときの疑問は，コードは，どこに戻ればいいのかをどうやって判断するのだろうか．

その答えは，重要なSSA操作に関連する．
すなわち，それは，**Phi操作**である．
SSAのことをあまり知らない人は．wikipediaを読むとよい．
そして，検索エンジンを使えば，いい感じのほかのコードやらドキュメントも見つかる．

ショートバージョンは，**Phi操作**の実行がどのブロックコントロールから来たかを**‌覚えておく**ことを要求する．
この場合，もし，`then`ブロックから，コントロールが来た場合，`calltmp`の値を取得する．
もし，コントロールが`else`ブロックから来た場合，`calltmp1`の値を取得する．

この点では，"え・・・シンプルかつ綺麗なフロントエンドは，LLVMを使うために，SSAの形式でコードを生成する必要があるのか・・・"と考え始めているかもしれない．
幸運にも，そんなことはない．
このドキュメントでは，すごい特別なそうする理由がない限り，あなたが実装するフロントエンドにSSA構造のアルゴリズムを実装しないように，強くオススメする．
実際，**Phi**ノードを必要とするあなたの平均的な良い命令プログラミング言語のために書かれたコードのどこかにある２種類の値がある，

1. ユーザ変数に関わるコード : `x = 1; x = x + 1;`
2. この例でいう**Phi**ノードのような抽象構文木の構造に暗黙にある値．

これは，このチュートリアルの７章で，この二つの最初の一つについて説明する．
今は，この場合には，SSA構造は必要ではないと信じてもらいたい．
二つ目については，その７章で説明する技術を使う，あるいは**Phi**ノードを直接挿入する（そっちの方が便利であれば）という選択肢もある．
この場合，**Phi**ノードを生成することが実際のところ簡単であるため，我々もそれ選ぶことにする．

さあ，動機と概要については，これで十分だろう．
LLVM IRのコードを生成しよう．

## `if/then/else`のためのコード生成
このためのコードを生成するため，`IfExprAST`クラスに`codegen`メソッドを実装する．

```
Value *IfExprAST::codegen() {
  Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  // Convert condition to a bool by comparing non-equal to 0.0.
  CondV = Builder.CreateFCmpONE(
      CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");
```

このコードは，愚直だし，今まで見てきたコードと似ている．
条件のためのコードを発行し，1bitとして真偽値を取得し，ゼロと比較する．

```
Function *TheFunction = Builder.GetInsertBlock()->getParent();

// Create blocks for the then and else cases.  Insert the 'then' block at the
// end of the function.
BasicBlock *ThenBB =
    BasicBlock::Create(TheContext, "then", TheFunction);
BasicBlock *ElseBB = BasicBlock::Create(TheContext, "else");
BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");

Builder.CreateCondBr(CondV, ThenBB, ElseBB);
```

このコードは，`if/then/else` statementと関連し，上の例にあるブロックに直接関連する基本ブロックを作成する．
最初の行で，構築された現在の`Function`オブジェクトを取得する．
取得するために，現在の`BasicBlock`のためのビルダに問い合わせ，その"parent"オブジェクト（関数は，そこに埋め込まれている）に問い合わせている．

一度取得されると，コードは，３つのブロックを作成する．
コードは，`TheFunction`をコンストラクタに渡し，`then`ブロックを生成することに注意する．
これは，コンストラクタに，指定された関数の終わりに新しいブロックを自動的に挿入させる．
他の二つのブロックも作成されるが，関数には挿入されない．

一度，挿入先がセットされると，それらのどちらかを選ぶ条件付ブランチを発行できる．
ここで，新しいブロックを作ることは，暗黙に`IRBuilder`に影響を与えないため，`IRBuilder`は条件が入るブロックを，他に影響を与えず，挿入することに注意してもらいたい．
たとえ，`else`ブロックがいまだに関数に挿入されていなくても，`IRBuilder`は`then`ブロックと，`else`ブロックへブランチを作成することに注意してもらいたい．
これは，すべて問題ない．
すなわち，LLVMが将来的なリファレンスをサポートする一般的な方法なのである（この文章の意味がわからない）．

```
// Emit then value.
Builder.SetInsertPoint(ThenBB);

Value *ThenV = Then->codegen();
if (!ThenV)
  return nullptr;

Builder.CreateBr(MergeBB);
// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
ThenBB = Builder.GetInsertBlock();
```

条件付きブランチが挿入された後，`then`ブロックに，`builder`を挿入し始めるために，動かす．
厳密に言うと，これは，挿入先を，指定されたブロックの終わりに動かす．
しかし，`then`ブロックは空なので，ブロックの初めに挿入したことになるのである．

Once the insertion point is set, we recursively codegen the “then” expression from the AST. To finish off the “then” block, we create an unconditional branch to the merge block. One interesting (and very important) aspect of the LLVM IR is that it requires all basic blocks to be “terminated” with a control flow instruction such as return or branch. This means that all control flow, including fall throughs must be made explicit in the LLVM IR. If you violate this rule, the verifier will emit an error.

The final line here is quite subtle, but is very important. The basic issue is that when we create the Phi node in the merge block, we need to set up the block/value pairs that indicate how the Phi will work. Importantly, the Phi node expects to have an entry for each predecessor of the block in the CFG. Why then, are we getting the current block when we just set it to ThenBB 5 lines above? The problem is that the “Then” expression may actually itself change the block that the Builder is emitting into if, for example, it contains a nested “if/then/else” expression. Because calling codegen() recursively could arbitrarily change the notion of the current block, we are required to get an up-to-date value for code that will set up the Phi node.

```
// Emit else block.
TheFunction->getBasicBlockList().push_back(ElseBB);
Builder.SetInsertPoint(ElseBB);

Value *ElseV = Else->codegen();
if (!ElseV)
  return nullptr;

Builder.CreateBr(MergeBB);
// codegen of 'Else' can change the current block, update ElseBB for the PHI.
ElseBB = Builder.GetInsertBlock();
```

Code generation for the ‘else’ block is basically identical to codegen for the ‘then’ block. The only significant difference is the first line, which adds the ‘else’ block to the function. Recall previously that the ‘else’ block was created, but not added to the function. Now that the ‘then’ and ‘else’ blocks are emitted, we can finish up with the merge code:

```
 // Emit merge block.
  TheFunction->getBasicBlockList().push_back(MergeBB);
  Builder.SetInsertPoint(MergeBB);
  PHINode *PN =
    Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}
```

The first two lines here are now familiar: the first adds the “merge” block to the Function object (it was previously floating, like the else block above). The second changes the insertion point so that newly created code will go into the “merge” block. Once that is done, we need to create the PHI node and set up the block/value pairs for the PHI.

Finally, the CodeGen function returns the phi node as the value computed by the if/then/else expression. In our example above, this returned value will feed into the code for the top-level function, which will create the return instruction.

Overall, we now have the ability to execute conditional code in Kaleidoscope. With this extension, Kaleidoscope is a fairly complete language that can calculate a wide variety of numeric functions. Next up we’ll add another useful expression that is familiar from non-functional languages…

## forループ表現
Now that we know how to add basic control flow constructs to the language, we have the tools to add more powerful things. Let’s add something more aggressive, a ‘for’ expression:

```
extern putchard(char);
def printstar(n)
  for i = 1, i < n, 1.0 in
    putchard(42);  # ascii 42 = '*'

# print 100 '*' characters
printstar(100);
```

This expression defines a new variable (“i” in this case) which iterates from a starting value, while the condition (“i < n” in this case) is true, incrementing by an optional step value (“1.0” in this case). If the step value is omitted, it defaults to 1.0. While the loop is true, it executes its body expression. Because we don’t have anything better to return, we’ll just define the loop as always returning 0.0. In the future when we have mutable variables, it will get more useful.

As before, let’s talk about the changes that we need to Kaleidoscope to support this.