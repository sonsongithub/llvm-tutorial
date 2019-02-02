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

1. よく，検証，テストされている： clangは，この技術をローカルのmutableのために使っている．LLVMを使うもっとも有名なコンパイラのほとんどは，それらの変数のハンドリングに，この技術を使っている．バグは，すぐに発見されるし，すぐに修正される．
2. めちゃくちゃ速い：`mem2reg`は， has a number of special cases that make it fast in common cases as well as fully general. For example, it has fast-paths for variables that are only used in a single block, variables that only have one assignment point, good heuristics to avoid insertion of unneeded phi nodes, etc.
3. Needed for debug info generation: Debug information in LLVM relies on having the address of the variable exposed so that debug info can be attached to it. This technique dovetails very naturally with this style of debug info.

If nothing else, this makes it much easier to get your front-end up and running, and is very simple to implement. Let’s extend Kaleidoscope with mutable variables now!

## Mutable Variables in Kaleidoscope
Now that we know the sort of problem we want to tackle, let’s see what this looks like in the context of our little Kaleidoscope language. We’re going to add two features:

The ability to mutate variables with the ‘=’ operator.
The ability to define new variables.
While the first item is really what this is about, we only have variables for incoming arguments as well as for induction variables, and redefining those only goes so far :). Also, the ability to define new variables is a useful thing regardless of whether you will be mutating them. Here’s a motivating example that shows how we could use these:

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

In order to mutate variables, we have to change our existing variables to use the “alloca trick”. Once we have that, we’ll add our new operator, then extend Kaleidoscope to support new variable definitions.

## Adjusting Existing Variables for Mutation
The symbol table in Kaleidoscope is managed at code generation time by the ‘NamedValues’ map. This map currently keeps track of the LLVM “Value*” that holds the double value for the named variable. In order to support mutation, we need to change this slightly, so that NamedValues holds the memory location of the variable in question. Note that this change is a refactoring: it changes the structure of the code, but does not (by itself) change the behavior of the compiler. All of these changes are isolated in the Kaleidoscope code generator.

At this point in Kaleidoscope’s development, it only supports variables for two things: incoming arguments to functions and the induction variable of ‘for’ loops. For consistency, we’ll allow mutation of these variables in addition to other user-defined variables. This means that these will both need memory locations.

To start our transformation of Kaleidoscope, we’ll change the NamedValues map so that it maps to AllocaInst* instead of Value*. Once we do this, the C++ compiler will tell us what parts of the code we need to update:

static std::map<std::string, AllocaInst*> NamedValues;
Also, since we will need to create these allocas, we’ll use a helper function that ensures that the allocas are created in the entry block of the function:

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

This funny looking code creates an IRBuilder object that is pointing at the first instruction (.begin()) of the entry block. It then creates an alloca with the expected name and returns it. Because all values in Kaleidoscope are doubles, there is no need to pass in a type to use.

With this in place, the first functionality change we want to make belongs to variable references. In our new scheme, variables live on the stack, so code generating a reference to them actually needs to produce a load from the stack slot:

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

As you can see, this is pretty straightforward. Now we need to update the things that define the variables to set up the alloca. We’ll start with ForExprAST::codegen() (see the full code listing for the unabridged code):

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

This code is virtually identical to the code before we allowed mutable variables. The big difference is that we no longer have to construct a PHI node, and we use load/store to access the variable as needed.

To support mutable argument variables, we need to also make allocas for them. The code for this is also pretty simple:

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

For each argument, we make an alloca, store the input value to the function into the alloca, and register the alloca as the memory location for the argument. This method gets invoked by FunctionAST::codegen() right after it sets up the entry block for the function.

The final missing piece is adding the mem2reg pass, which allows us to get good codegen once again:

```
// Promote allocas to registers.
TheFPM->add(createPromoteMemoryToRegisterPass());
// Do simple "peephole" optimizations and bit-twiddling optzns.
TheFPM->add(createInstructionCombiningPass());
// Reassociate expressions.
TheFPM->add(createReassociatePass());
...
```

It is interesting to see what the code looks like before and after the mem2reg optimization runs. For example, this is the before/after code for our recursive fib function. Before the optimization:

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

Here there is only one variable (x, the input argument) but you can still see the extremely simple-minded code generation strategy we are using. In the entry block, an alloca is created, and the initial input value is stored into it. Each reference to the variable does a reload from the stack. Also, note that we didn’t modify the if/then/else expression, so it still inserts a PHI node. While we could make an alloca for it, it is actually easier to create a PHI node for it, so we still just make the PHI.

Here is the code after the mem2reg pass runs:

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

This is a trivial case for mem2reg, since there are no redefinitions of the variable. The point of showing this is to calm your tension about inserting such blatent inefficiencies :).

After the rest of the optimizers run, we get:

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

Here we see that the simplifycfg pass decided to clone the return instruction into the end of the ‘else’ block. This allowed it to eliminate some branches and the PHI node.

Now that all symbol table references are updated to use stack variables, we’ll add the assignment operator.

## New Assignment Operator
With our current framework, adding a new assignment operator is really simple. We will parse it just like any other binary operator, but handle it internally (instead of allowing the user to define it). The first step is to set a precedence:

```
int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
```

Now that the parser knows the precedence of the binary operator, it takes care of all the parsing and AST generation. We just need to implement codegen for the assignment operator. This looks like:

```
Value *BinaryExprAST::codegen() {
  // Special case '=' because we don't want to emit the LHS as an expression.
  if (Op == '=') {
    // Assignment requires the LHS to be an identifier.
    VariableExprAST *LHSE = dynamic_cast<VariableExprAST*>(LHS.get());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");
```

Unlike the rest of the binary operators, our assignment operator doesn’t follow the “emit LHS, emit RHS, do computation” model. As such, it is handled as a special case before the other binary operators are handled. The other strange thing is that it requires the LHS to be a variable. It is invalid to have “(x+1) = expr” - only things like “x = expr” are allowed.

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

Once we have the variable, codegen’ing the assignment is straightforward: we emit the RHS of the assignment, create a store, and return the computed value. Returning a value allows for chained assignments like “X = (Y = Z)”.

Now that we have an assignment operator, we can mutate loop variables and arguments. For example, we can now run code like this:

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

When run, this example prints “123” and then “4”, showing that we did actually mutate the value! Okay, we have now officially implemented our goal: getting this to work requires SSA construction in the general case. However, to be really useful, we want the ability to define our own local variables, let’s add this next!

## User-defined Local Variables
Adding var/in is just like any other extension we made to Kaleidoscope: we extend the lexer, the parser, the AST and the code generator. The first step for adding our new ‘var/in’ construct is to extend the lexer. As before, this is pretty trivial, the code looks like this:

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

The next step is to define the AST node that we will construct. For var/in, it looks like this:

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

var/in allows a list of names to be defined all at once, and each name can optionally have an initializer value. As such, we capture this information in the VarNames vector. Also, var/in has a body, this body is allowed to access the variables defined by the var/in.

With this in place, we can define the parser pieces. The first thing we do is add it as a primary expression:

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

Next we define ParseVarExpr:

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

Once all the variables are parsed, we then parse the body and create the AST node:

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

Now that we can parse and represent the code, we need to support emission of LLVM IR for it. This code starts out with:

```
Value *VarExprAST::codegen() {
  std::vector<AllocaInst *> OldBindings;

  Function *TheFunction = Builder.GetInsertBlock()->getParent();

  // Register all variables and emit their initializer.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second.get();
```

Basically it loops over all the variables, installing them one at a time. For each variable we put into the symbol table, we remember the previous value that we replace in OldBindings.

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

There are more comments here than code. The basic idea is that we emit the initializer, create the alloca, then update the symbol table to point to it. Once all the variables are installed in the symbol table, we evaluate the body of the var/in expression:

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

The end result of all of this is that we get properly scoped variable definitions, and we even (trivially) allow mutation of them :).

With this, we completed what we set out to do. Our nice iterative fib example from the intro compiles and runs just fine. The mem2reg pass optimizes all of our stack variables into SSA registers, inserting PHI nodes where needed, and our front-end remains simple: no “iterated dominance frontier” computation anywhere in sight.

## Full Code Listing
Here is the complete code listing for our running example, enhanced with mutable variables and var/in support. To build this example, use:

```
# Compile
clang++ -g toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 -o toy
# Run
./toy
```
