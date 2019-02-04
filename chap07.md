# Extending the Language: Mutable Variables

## はじめに
７章にようこそ．ここまでは，実直で，シンプルとは言っても関数型と言っていい言語を構築してきた．
ここまでの旅で，いくつかのパーサ技術，抽象構文木を構築し，表示する方法，LLVM IRをビルドする方法，結果的に得られるLLVM IRを最適化する方法，そして，それをJITコンパイルする方法を学んできた．

Kaleidoscopeは，関数型元号としておもしろい一方で，それが関数型であるために，LLVM IRを生成するのが簡単すぎてしまう事実がある．
特に，関数型言語は，`SSA`形式でLLVM IRを直接ビルドするのを簡単にする．
LLVMは，入力されるコードが`SSA`形式であることを要求するため，これは，かなりよい性質である一方，書き換え可能な変数を持つ命令型言語のためにコードを生成する方法が，初心者にとってわかりにくくなってしまう傾向がある．

この章を短く，（幸せな感じに）まとめると，フロントエンドを`SSA`形式でビルドする必要はないということになる．
LLVMは，`SSA`形式以外の方法のサポートも，よく調整され，テストされているが，その形式が動作する仕組みは，一部の人には，ちょっと予想できない風になっている．

## なぜ，これが難しいのか？
`SSA`命令セットにおいて，なぜ，mutableがそんなに難しいのかを理解するために，以下のような極端なC言語の例を考えよう．

```
int G, H;
int test(_Bool Condition) {
  int X;
  if (Condition)
    X = G;
  else
    X = H;
  return X;
}
```

プログラムが実行されるパスに依存して値がかわる変数`X`がある．
`return`が実行される前に`X`として二つの異なる値が代入される可能性があるため，`phi`ノードが二つの値をマージするために挿入される．
このサンプルをコンパイルした結果得られるように，実際に我々が期待するLLVM IRは，以下のようになる．

```
@G = weak global i32 0   ; type of @G is i32*
@H = weak global i32 0   ; type of @H is i32*

define i32 @test(i1 %Condition) {
entry:
  br i1 %Condition, label %cond_true, label %cond_false

cond_true:
  %X.0 = load i32* @G
  br label %cond_next

cond_false:
  %X.1 = load i32* @H
  br label %cond_next

cond_next:
  %X.2 = phi i32 [ %X.1, %cond_false ], [ %X.0, %cond_true ]
  ret i32 %X.2
}
```

このサンプルでは，グローバル変数`G`と`H`からの読み出しは，LLVM IRで明示されており，`if`の文(`cond_true/cond_false`)の`then/else`ブランチで実行される．
入力される値をマージするために，`cond_next`ブロック内の`X.2`のphiノードは，制御フローがどこから来たかに基づき，右の値を選択する．
つまり，`cond_false`ブロックから制御フローが来た場合，`X.2`は，`X.1`の値を取得する．
一方で，もし，`cond_true`ブロックから制御フローが来た場合は，`X.0`の値を取得する．
本章の意図は，`SSA`形式の詳細を説明することではない．
より詳しい情報は，オンラインのコンテンツを参照されたい．

この記事に対する質問は，"mutableに割り当てるとき誰がphiノードを設置するのか"ということだろう．
ここでの問題は，LLVMは，IRが`SSA`形式であることを要求することである．
つまり，LLVMには，SSAではないモードがない．
しかし，`SSA`命令は，簡単なアルゴリズムやデータ構造では実装できないため，`SSA`のロジックを再実装しなければならないのは，あらゆるフロントエンドにとって不便だし，時間のかかることだ．

## LLVMにおけるメモリ
ここでの"トリック"は，LLVMにおいて，すべてのレジスタの値は，SSA形式でなければならないという性質があるが，LLVMは，SSA形式におけるメモリオブジェクトに関してはその限りではないということである．
上の例では，`G`と`H`からのロードは，`G`と`H`に直接アクセスしており，リネームしたり，バージョニングしたりしていない．
これは，メモリオブジェクトをバージョニングにしようとする，他のコンパイラとは異なる挙動である．
LLVMにおいて，LLVM IRにメモリのデータフロー分析の結果を埋め込む代わりに，要求に応じて，`Analysis Pass`で，データフローを取り扱う．

つまるところ，関数におけるmutableに対するスタック変数（スタック上にあるから，メモリ上にある）を作りたいということである．
このトリックを応用するために，まず，LLVMがスタック変数をどのように表現するのかを説明する必要がある．

LLVMでは，すべてのメモリへのアクセスは，明示的に`load/store`命令で行われ，そのアクセスは，アドレスを指定するオペレータを持たない，必要としないように設計されている．
グローバル変数`@G/@H`の型が，`i32`で定義されているにも関わらず，実際には`i32*`型になっていることに気をつけてほしい．
これが意味することは，`@G`は，グローバル変数領域にある`i32`型の変数のための領域を定義しているということであるが，その名前は，実際には，スペースのアドレスを参照している．
グローバル変数定義を使って，宣言する代わりに，LLVMの`alloca`を使って宣言されることを除いて，スタック変数は同じように動作する．

```
define i32 @example() {
entry:
  %X = alloca i32           ; type of %X is i32*.
  ...
  %tmp = load i32* %X       ; load the stack value %X from the stack.
  %tmp2 = add i32 %tmp, 1   ; increment it
  store i32 %tmp2, i32* %X  ; store it back
  ...
```

このコードは，LLVM IRでスタック変数をどうやって宣言したり，操作したりするかを説明するサンプルになっている．
`alloca`命令で確保されたスタックメモリは，十分にグローバル？一般？どこからでも参照可能？であり，関数にスタックのスロットのアドレスを渡すことができるし，他の変数にそれを保存したりすることができる．
上の例では，phiノードを使わずに，`alloc`のテクを使って，サンプルを書き直せる．

```
@G = weak global i32 0   ; type of @G is i32*
@H = weak global i32 0   ; type of @H is i32*

define i32 @test(i1 %Condition) {
entry:
  ; メモリを確保する
  %X = alloca i32           ; type of %X is i32*.
  ; %Conditionに応じて，分岐する
  br i1 %Condition, label %cond_true, label %cond_false

cond_true:
  ; %X.0に@Gをロードする
  %X.0 = load i32* @G
  ; allocaで確保したメモリ領域に%X.0の値をロードする
  store i32 %X.0, i32* %X   ; Update X
  ; %cond_nextブランチへ移動する
  br label %cond_next

cond_false:
  %X.1 = load i32* @H
  store i32 %X.1, i32* %X   ; Update X
  br label %cond_next

cond_next:
  ; メモリ領域にある%Xから値を戻り値となる%X.2にロードする．
  %X.2 = load i32* %X       ; Read X
  ret i32 %X.2
}
```

これによって，phiノードをまったく作る必要性なしに，任意のmutableを扱う方法を手に入れたのである．

1. それぞれのmutableは，スタックの確保ということになる．
1. 変数を読み取るということは，スタックからロードするということになる．
1. 変数を更新するということは，スタックにそれを保存するということになる．
1. 変数のアドレスの取得は，ちょうど，スタックアドレスを直接使えば良い．

このソリューションは，即物的な問題をといてくれる一方，他の問題を生み出す．
つまり，どう考えても，スタック上での処理が増大し，パフォーマンスの問題が発生するのである．
幸運にも，LLVMのオプティマイザは，このケースを扱える`mem2reg`と名付けられた，チューンされた最適化passを持ち，このpassは，`alloca`で確保された値をSSAレジスタへ昇格させ，正しくphiノードを挿入する．
このサンプルをそのpassに通すと，以下のようなコードが得られる．

```
$ llvm-as < example.ll | opt -mem2reg | llvm-dis
@G = weak global i32 0
@H = weak global i32 0

define i32 @test(i1 %Condition) {
entry:
  br i1 %Condition, label %cond_true, label %cond_false

cond_true:
  %X.0 = load i32* @G
  br label %cond_next

cond_false:
  %X.1 = load i32* @H
  br label %cond_next

cond_next:
  %X.01 = phi i32 [ %X.1, %cond_false ], [ %X.0, %cond_true ]
  ret i32 %X.01
}
```

`mem2reg` passは，SSA形式を構築する一般的な“iterated dominance frontier”アルゴリズムを実装し，（非常に一般的な）冗長なケースの消去を加速させるたくさんのオプティマイザを内部的に持っている．
`mem2reg`の最適化passは，mutableを取り扱うための答えとなる．
そして，本文書は，このpassに依存することを強くお勧めする．
`mem2reg`は，ある条件においては，変数に対してのみ動作することに気をつけてほしい．

1. `mem2reg`は，`alloca`ドリブンである．`mem2reg`は，`alloca`を探索し，もし，それを発見すると，それをレジスタに昇格させる．このpassは，グローバル変数やヒープ上に確保された変数には適応されない．
2. `mem2reg`は，関数のエントリブロックにある`alloca`命令のみを探索する．エントリブロックにあることは，それが一度だけ実行されることを保障するため，分析がシンプルになる．
3. `mem2reg`は，使われ方が直接的なロードと保存である`alloca`のみ昇格させる．もし，スタックオブジェクトのアドレスは関数に渡される，あるいは，変わったポインタの計算がなされたりするなら，`alloca`は，昇格されない．
4. `mem2reg`は，first classの値（ポインタやスカラ値，ベクトル）のための`alloca`であり，かつ，そのポインタのアロケーションサイズが１のとき（あるいは`.ll`ファイルにない）にのみ動作する．`mem2reg`は，構造体や配列をレジスタに昇格させる能力を持たない．一方，`sroa`パスは，強力で，構造体や共用体や配列など多くのものを昇格させることができることに注意する．

これらの特徴のすべてを満たすことは，ほとんどの命令型言語にとって，簡単なことだ．そして，ここでは，それをKaleidoscopeでやってみせよう．
ここで，あなたが最後にしてくる質問は，こんな感じではないだろうか．
「私はフロントエンドのために，こんなナンセンスなことをわざわざやらないといけないのか？」
`mem2reg`の最適化パスを使わず，SSA命令を直接使うのはよくないのだろうか．結論から言うと，そうしない極端によい理由がない限り，SSA形式をビルドするためにこの技術を使うことを強く勧める．

1. よく，検証・テストされている： clangは，この技術をローカルのmutableのために使っている．LLVMを使うもっとも有名なコンパイラのほとんどは，それらの変数のハンドリングに，この技術を使っている．バグは，すぐに発見されるし，すぐに修正される．
2. めちゃくちゃ速い：`mem2reg`は，十分に一般的なケースと同様に共通のケースにおいて，`mem2reg`の処理を高速化する特殊なケースをたくさんもっている．例えば，一つのブロックでのみ使われる変数や一度だけしか代入しな変数のための速いパス，使われないphiノードの挿入を避けるヒューリスティクなどが実装されている．
3. デバッグのための情報が必要：LLVMにおけるデバッグ情報は，変数のアドレスを持っていることを当てにしているため，デバッグ情報は，そのアドレスにアタッチされる．この技術は，デバッグ情報のこのスタイルに自然に組み込まれる．

他に何もなければ，今実装しているフロントエンドを起動させるのを，この技術は，より簡単にし，とても，実装しやすい．
さぁ，Kaleidoscopeにmutableを実装してみよう．

## Kaleidoscopeのmutable
ここで，ソート問題を解こうとしているとき，Kaleidoscopeという言語のコンテキストで，これが何に似ているのかを見ていこう．
今から，Kaleidoscopeに二つの機能を追加する．

1. `=`演算子で変数の値を変更する機能．
2. 新しい変数を定義する機能．

一つ目は，ここでの本質ではあるが，現在，宣言される変数と，関数の引数としての変数，それらを再定義する，程度のことしかできない．
さらに，新しい変数を定義する能力は，mutableを使えるようにするかと無関係に便利だ．
これの例を示そう．

```
# Define ':' for sequencing: as a low-precedence operator that ignores operands
# and just returns the RHS.
def binary : 1 (x y) y;

# Recursive fib, we could do this before.
def fib(x)
  if (x < 3) then
    1
  else
    fib(x-1)+fib(x-2);

# Iterative fib.
def fibi(x)
  var a = 1, b = 1, c in
  (for i = 3, i < x in
     c = a + b :
     a = b :
     b = c) :
  b;

# Call it.
fibi(10);
```

変数の更新を許可するため，我々は，既存の変数が`alloca`トリックを使うように変更しなければならない．
一度，それをやると，新しい演算子を追加できるし，新しい変数の定義もKaleidoscopeがサポートするように拡張できるようになる．

## 既存の変数を更新できるように調整する
Kaleidoscopeのシンボルテーブルは，`NamedValues`マップによって，コード生成時間に制御される．
このマップは，今の所，名前をつけた変数のための`double`型の値を保持するLLVMの`Value*`型を追跡し続ける．
mutateをサポートするために，これを多少変化させる必要があるため，`NamedValues`がメモリの位置を保持する．
この変更は，リファクタリング程度だ．
つまり，コードの構造は変えるが，コンパイラの振る舞い自体を変更するわけではない．
これらのすべての変更は，Kaleidoscopeのコード生成器に，隔離される．

現時点のKaleidoscopeでは，関数への引数，`for`文の一時変数の二つのことをサポートする．
一貫性のため，他のユーザ定義変数に加えて，これらの変数のmutateも許容する．
この一貫性は，これらの両方のメモリの位置を必要とすることを意味する．

Kaleidoscopeの変更を始めるにあたって，`NamedValues`マップを，`Value*`の代わりに`AllocaInst*`にマップするように変更する．
一度，これをやると，C++コンパイラが，コードのどの部分を変えるべきなのかを教えてくれる（ちょっと，待て，コンパイラのエラーが出たところを変えろということか・・・・メチャクチャやな）．

```
static std::map<std::string, AllocaInst*> NamedValues;
```

`alloca`を作る必要があるため，`alloca`は，関数のエントリブロックに作られたことを確認する補助関数を利用する．

```
/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                          const std::string &VarName) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                 TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), 0,
                           VarName.c_str());
}
```

これは，可愛く見えるコードは，エントリブロックのはじめの命令（`.begin()`）を指している`IRBuilder`オブジェクトを生成する．
そのオブジェクトは，期待される名前で`alloca`を生成し，それを返す．
Kaleidoscopeにおけるすべての値は，`double` 型なので，使われるべき型を渡す必要はない．

はじめの機能的な変更は，変数への参照に関するものである．
我々の新しいスキームでは，変数はスタック上で保存され，それらへのリファレンスを生成するコードは，スタックスロットからロードするコードを必要とする．

```
Value *VariableExprAST::codegen() {
  // Look this variable up in the function.
  Value *V = NamedValues[Name];
  if (!V)
    return LogErrorV("Unknown variable name");

  // Load the value.
  return Builder.CreateLoad(V, Name.c_str());
}
```

見ればわかるように，これは，率直なやり方だ．
今，`alloca`をセットアップするための変数を定義する何かを更新する必要がある．
はじめに，`ForExprAST::codegen()`から始めよう（省略されてないコードは，最後のコードリストを参照されたい）．

```
Function *TheFunction = Builder.GetInsertBlock()->getParent();

// Create an alloca for the variable in the entry block.
AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

// Emit the start code first, without 'variable' in scope.
Value *StartVal = Start->codegen();
if (!StartVal)
  return nullptr;

// Store the value into the alloca.
Builder.CreateStore(StartVal, Alloca);
...

// Compute the end condition.
Value *EndCond = End->codegen();
if (!EndCond)
  return nullptr;

// Reload, increment, and restore the alloca.  This handles the case where
// the body of the loop mutates the variable.
Value *CurVar = Builder.CreateLoad(Alloca);
Value *NextVar = Builder.CreateFAdd(CurVar, StepVal, "nextvar");
Builder.CreateStore(NextVar, Alloca);
...
```

このコードは，本質的に，mutableを許容する前のコードと同じである．
大きな違いは，もはやphiノードを構築しておらず，必要な時に変数にアクセスするために`load/store`を使っていることである．
mutable引数をサポートするため，それらを作るために，`alloca`を使う必要がある．
そのコードは，簡単で以下のようになる．

```
Function *FunctionAST::codegen() {
  ...
  Builder.SetInsertPoint(BB);

  // Record the function arguments in the NamedValues map.
  NamedValues.clear();
  for (auto &Arg : TheFunction->args()) {
    // Create an alloca for this variable.
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

    // Store the initial value into the alloca.
    Builder.CreateStore(&Arg, Alloca);

    // Add arguments to variable symbol table.
    NamedValues[Arg.getName()] = Alloca;
  }

  if (Value *RetVal = Body->codegen()) {
    ...
```

それぞれの引数のために，`alloca`を作り，`alloca`へ関数に入力する値を保存し，引数として，メモリの位置として`alloca`を登録する．
この方法は，関数ためにエントリブロックがセットアップされた後すぐに`FunctionAST::codegen()`によって，起動される．

最後のピースは，`mem2reg`パスの追加だ．

```
// Promote allocas to registers.
TheFPM->add(createPromoteMemoryToRegisterPass());
// Do simple "peephole" optimizations and bit-twiddling optzns.
TheFPM->add(createInstructionCombiningPass());
// Reassociate expressions.
TheFPM->add(createReassociatePass());
...
```

`mem2reg`のオプティマイザを走らせる前と後でコードを見比べてみるとおもしろい．
例えば，これは，再帰的なフィボナッチ数列関数の，最適化前後のコードだ．
最適化前のコードは以下のようになる．

```
define double @fib(double %x) {
entry:
  %x1 = alloca double
  store double %x, double* %x1
  %x2 = load double, double* %x1
  %cmptmp = fcmp ult double %x2, 3.000000e+00
  %booltmp = uitofp i1 %cmptmp to double
  %ifcond = fcmp one double %booltmp, 0.000000e+00
  br i1 %ifcond, label %then, label %else

then:       ; preds = %entry
  br label %ifcont

else:       ; preds = %entry
  %x3 = load double, double* %x1
  %subtmp = fsub double %x3, 1.000000e+00
  %calltmp = call double @fib(double %subtmp)
  %x4 = load double, double* %x1
  %subtmp5 = fsub double %x4, 2.000000e+00
  %calltmp6 = call double @fib(double %subtmp5)
  %addtmp = fadd double %calltmp, %calltmp6
  br label %ifcont

ifcont:     ; preds = %else, %then
  %iftmp = phi double [ 1.000000e+00, %then ], [ %addtmp, %else ]
  ret double %iftmp
}
```

ここでは，入力のための`x`しか，変数がないが，ここで使っている極端にシンプルなコード生成戦略を理解できると思う．
エントリブロックでは，`alloca`が生成され，初期値がそれに保存される．
その変数への参照は，スタックからリロードされる．
`if/then/else`は，まったく変更されないし，phiノードも未だに存在している．
一方，そのために`alloca`を作れる一方で，phiノードを作る方が明らかに簡単で，phiノードをただ作っているだけである．

ここに，`mem2reg`のパスを走らせた後のコードを紹介しよう．

```
define double @fib(double %x) {
entry:
  %cmptmp = fcmp ult double %x, 3.000000e+00
  %booltmp = uitofp i1 %cmptmp to double
  %ifcond = fcmp one double %booltmp, 0.000000e+00
  br i1 %ifcond, label %then, label %else

then:
  br label %ifcont

else:
  %subtmp = fsub double %x, 1.000000e+00
  %calltmp = call double @fib(double %subtmp)
  %subtmp5 = fsub double %x, 2.000000e+00
  %calltmp6 = call double @fib(double %subtmp5)
  %addtmp = fadd double %calltmp, %calltmp6
  br label %ifcont

ifcont:     ; preds = %else, %then
  %iftmp = phi double [ 1.000000e+00, %then ], [ %addtmp, %else ]
  ret double %iftmp
}
```

これは，`mem2reg`にとって，他愛もないコードだ．
なぜなら，変数の再定義もまったくないためである．
これを見せることのポイントは，あなたが，そのような遅いコードを挿入することについて，注意がいっていることを収めるためだ:)．

最適化後の残りの部分は，こうなる．

```
define double @fib(double %x) {
entry:
  %cmptmp = fcmp ult double %x, 3.000000e+00
  %booltmp = uitofp i1 %cmptmp to double
  %ifcond = fcmp ueq double %booltmp, 0.000000e+00
  br i1 %ifcond, label %else, label %ifcont

else:
  %subtmp = fsub double %x, 1.000000e+00
  %calltmp = call double @fib(double %subtmp)
  %subtmp5 = fsub double %x, 2.000000e+00
  %calltmp6 = call double @fib(double %subtmp5)
  %addtmp = fadd double %calltmp, %calltmp6
  ret double %addtmp

ifcont:
  ret double 1.000000e+00
}
```

`simplifycfg`パスは，`else`ブロックの最後に`return`命令を複製することを決めることを理解できる．
これは，いくつかのブランチやphiノードを除去することを許容する．

ここで，すべてのシンボルテーブルの参照が，スタック変数を使うために更新された．次に，代入演算子を追加しよう．

## 新しく追加される代入演算子
今手元にあるフレームワークに，新しく代入演算子を追加するのは，至ってシンプルだ．
他の二項演算子と同じように，それをパースし，内部的に処理すればよい（ユーザに定義させる代わりに）．
はじめに，優先順位を決める．

```
int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
```

パーサは，すでに二項演算子をどう扱えば良いか知っているので，パーサにパースと抽象構文木の生成を任せる．
やるべきことは，代入演算子のための`codegen`の実装だけである．

```
Value *BinaryExprAST::codegen() {
  // Special case '=' because we don't want to emit the LHS as an expression.
  if (Op == '=') {
    // Assignment requires the LHS to be an identifier.
    VariableExprAST *LHSE = dynamic_cast<VariableExprAST*>(LHS.get());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");
```

二項演算子の他のものとは違って，代入演算子は，"左辺を発行し，右辺を発行し，計算を実行する"というモデルに従わない．
これは，他の二項演算子を取り扱う前に，例外として扱う必要がある．
その他の変わったことといえば，左辺が常に変数でなければならないということである．つまり，`(x+1) = expr`のような式は許されないが，`x = expr`のような式は許される．

```
  // Codegen the RHS.
  Value *Val = RHS->codegen();
  if (!Val)
    return nullptr;

  // Look up the name.
  Value *Variable = NamedValues[LHSE->getName()];
  if (!Variable)
    return LogErrorV("Unknown variable name");

  Builder.CreateStore(Val, Variable);
  return Val;
}
...
```

一度，変数を持ってしまえば，代入のための`codegen`を実行することは，小細工を必要としない．
つまり，右辺の代入を発行し，保存し，計算した結果を返せばよいのである．
戻りは，`X = (Y = Z)`というように，再帰的に書いても構わない．

代入演算子を導入した今，我々は，ループ変数や引数もmutateにすることができる．例えば，以下のように，コードを実行できる．

```
# Function to print a double.
extern printd(x);

# Define ':' for sequencing: as a low-precedence operator that ignores operands
# and just returns the RHS.
def binary : 1 (x y) y;

def test(x)
  printd(x) :
  x = 4 :
  printd(x);

test(123);
```

これを実行すると，このサンプルは，`123`を表示し，その後，`4`を表示する．
つまり，変数が変更される様を表示しているのである．
ここで，ゴールまで実装できた．
これを動かすには，一般のケースで，SSA構成を必要とする．
しかし，本当に使いやすくするためには，自分のローカル変数を定義できるようにしないといけない．

## ユーザ定義のローカル変数
他の拡張と同じように`var/in`をKaleidoscopeに追加するのは，lexer，パーサ，抽象構文木，コード生成器に変更を加えなければならない．
新しく`var/in`を追加するためのはじめの一歩は，lexerの拡張である．
ここまでと同じように，この変更は大したものではない．

```
enum Token {
  ...
  // var definition
  tok_var = -13
...
}
...
static int gettok() {
...
    if (IdentifierStr == "in")
      return tok_in;
    if (IdentifierStr == "binary")
      return tok_binary;
    if (IdentifierStr == "unary")
      return tok_unary;
    if (IdentifierStr == "var")
      return tok_var;
    return tok_identifier;
...
```

次に，抽象構文木のノードを定義する．

```
/// VarExprAST - Expression class for var/in
class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
             std::unique_ptr<ExprAST> Body)
    : VarNames(std::move(VarNames)), Body(std::move(Body)) {}

  Value *codegen() override;
};
```

`var/in`は，名前のリストで，一度に変数を定義できるようにし，オプションで初期化もできるようにする．
`VarNames` vectorに，この情報を保存する．
`var/in`は，本体を持つ．
この本体は，`var/in`によって定義される変数にアクセスできる．

ここで，パーサの一部も定義する．
ここでやる最初のことは，主表現として，それを追加することである．

```
/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
///   ::= ifexpr
///   ::= forexpr
///   ::= varexpr
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
  case tok_for:
    return ParseForExpr();
  case tok_var:
    return ParseVarExpr();
  }
}
```

次に，関数`ParseVarExpr`を実装する．

```
/// varexpr ::= 'var' identifier ('=' expression)?
//                    (',' identifier ('=' expression)?)* 'in' expression
static std::unique_ptr<ExprAST> ParseVarExpr() {
  getNextToken();  // eat the var.

  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

  // At least one variable name is required.
  if (CurTok != tok_identifier)
    return LogError("expected identifier after var");
The first part of this code parses the list of identifier/expr pairs into the local VarNames vector.

while (1) {
  std::string Name = IdentifierStr;
  getNextToken();  // eat identifier.

  // Read the optional initializer.
  std::unique_ptr<ExprAST> Init;
  if (CurTok == '=') {
    getNextToken(); // eat the '='.

    Init = ParseExpression();
    if (!Init) return nullptr;
  }

  VarNames.push_back(std::make_pair(Name, std::move(Init)));

  // End of var list, exit loop.
  if (CurTok != ',') break;
  getNextToken(); // eat the ','.

  if (CurTok != tok_identifier)
    return LogError("expected identifier list after var");
}
```

一度，すべての変数がパースされると，その本体もそのあとにパースし，抽象構文木のノードを生成する．

```
  // At this point, we have to have 'in'.
  if (CurTok != tok_in)
    return LogError("expected 'in' keyword after 'var'");
  getNextToken();  // eat 'in'.

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return llvm::make_unique<VarExprAST>(std::move(VarNames),
                                       std::move(Body));
}
```

今，コードをパースし，表現できた．
次に，それに対応するLLVM IRを発行できる必要がある．

```
Value *VarExprAST::codegen() {
  std::vector<AllocaInst *> OldBindings;

  Function *TheFunction = Builder.GetInsertBlock()->getParent();

  // Register all variables and emit their initializer.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second.get();
```

基本的に，すべての変数をループし，同時にそれらをセットしていく．
それぞれの変数を，シンボルテーブルに代入し，に入っていた事前の値を捨て，それを，`OldBindings`にセットする．

```
  // Emit the initializer before adding the variable to scope, this prevents
  // the initializer from referencing the variable itself, and permits stuff
  // like this:
  //  var a = 1 in
  //    var a = a in ...   # refers to outer 'a'.
  Value *InitVal;
  if (Init) {
    InitVal = Init->codegen();
    if (!InitVal)
      return nullptr;
  } else { // If not specified, use 0.0.
    InitVal = ConstantFP::get(TheContext, APFloat(0.0));
  }

  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
  Builder.CreateStore(InitVal, Alloca);

  // Remember the old variable binding so that we can restore the binding when
  // we unrecurse.
  OldBindings.push_back(NamedValues[VarName]);

  // Remember this binding.
  NamedValues[VarName] = Alloca;
}
```

この部分は，コードよりもコメントの方が多い．
基本的なアイデアは，初期化し，`alloca`を作成し，シンボルテーブルがそれを指すように更新することである．
一度，すべての変数がシンボルテーブルにセットされると，`var/in`の本体を評価する．

```
// Codegen the body, now that all vars are in scope.
Value *BodyVal = Body->codegen();
if (!BodyVal)
  return nullptr;
Finally, before returning, we restore the previous variable bindings:

  // Pop all our variables from scope.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
    NamedValues[VarNames[i].first] = OldBindings[i];

  // Return the body computation.
  return BodyVal;
}
```

これのすべての最後の結果は，正しく範囲に限定された変数の定義を得て，それらの変更を許容することである．

ここまでで，すべてを実装できた．
いい感じのフィボナッチ数列のサンプルは，コンパイルして，実行できるはずだ．
`mem2reg`パスは，SSAレジスタにスタック変数のすべてを最適化し，必要に応じて，phiノードを挿入し，フロントエンドは変わらずシンプルなままだ．
"iterated dominance frontier"計算？は，もうどこにも見当たらない．

## コードリスト
ここに完全なコードリストを示す．
以下のようにビルドするとよい．

```
# Compile
clang++ -g toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 -o toy
# Run
./toy
```
