# デバッグ情報の追加
## はじめに
９章にようこそ．
ここまでで，まずまずな，ちょっとしたプログラミング言語を作ってきた．
もし，その中で何か変なことがおこったとき，あなたはどうやって，作ってきたコードをデバッグしましたか？

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
````

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

Lastly we’re going to disable all of the optimization passes and the JIT so that the only thing that happens after we’re done parsing and generating code is that the LLVM IR goes to standard error:

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

This relatively small set of changes get us to the point that we can compile our piece of Kaleidoscope language down to an executable program via this command line:

```
Kaleidoscope-Ch9 < fib.ks | & clang -x ir -
```

which gives an a.out/a.exe in the current working directory.

## Compile Unit
The top level container for a section of code in DWARF is a compile unit. This contains the type and function data for an individual translation unit (read: one file of source code). So the first thing we need to do is construct one for our fib.ks file.

## DWARF Emission Setup
Similar to the IRBuilder class we have a DIBuilder class that helps in constructing debug metadata for an LLVM IR file. It corresponds 1:1 similarly to IRBuilder and LLVM IR, but with nicer names. Using it does require that you be more familiar with DWARF terminology than you needed to be with IRBuilder and Instruction names, but if you read through the general documentation on the Metadata Format it should be a little more clear. We’ll be using this class to construct all of our IR level descriptions. Construction for it takes a module so we need to construct it shortly after we construct our module. We’ve left it as a global static variable to make it a bit easier to use.

Next we’re going to create a small container to cache some of our frequent data. The first will be our compile unit, but we’ll also write a bit of code for our one type since we won’t have to worry about multiple typed expressions:

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

And then later on in main when we’re constructing our module:

```
DBuilder = new DIBuilder(*TheModule);

KSDbgInfo.TheCU = DBuilder->createCompileUnit(
    dwarf::DW_LANG_C, DBuilder->createFile("fib.ks", "."),
    "Kaleidoscope Compiler", 0, "", 0);
```

There are a couple of things to note here. First, while we’re producing a compile unit for a language called Kaleidoscope we used the language constant for C. This is because a debugger wouldn’t necessarily understand the calling conventions or default ABI for a language it doesn’t recognize and we follow the C ABI in our LLVM code generation so it’s the closest thing to accurate. This ensures we can actually call functions from the debugger and have them execute. Secondly, you’ll see the “fib.ks” in the call to createCompileUnit. This is a default hard coded value since we’re using shell redirection to put our source into the Kaleidoscope compiler. In a usual front end you’d have an input file name and it would go there.

One last thing as part of emitting debug information via DIBuilder is that we need to “finalize” the debug information. The reasons are part of the underlying API for DIBuilder, but make sure you do this near the end of main:

```
DBuilder->finalize();
```

before you dump out the module.

## Functions
Now that we have our Compile Unit and our source locations, we can add function definitions to the debug info. So in PrototypeAST::codegen() we add a few lines of code to describe a context for our subprogram, in this case the “File”, and the actual definition of the function itself.

So the context:

```
DIFile *Unit = DBuilder->createFile(KSDbgInfo.TheCU.getFilename(),KSDbgInfo.TheCU.getDirectory());
```
giving us an DIFile and asking the Compile Unit we created above for the directory and filename where we are currently. Then, for now, we use some source locations of 0 (since our AST doesn’t currently have source location information) and construct our function definition:

```
DIScope *FContext = Unit;
unsigned LineNo = 0;
unsigned ScopeLine = 0;
DISubprogram *SP = DBuilder->createFunction(
    FContext, P.getName(), StringRef(), Unit, LineNo,
    CreateFunctionType(TheFunction->arg_size(), Unit),
    false /* internal linkage */, true /* definition */, ScopeLine,
    DINode::FlagPrototyped, false);
TheFunction->setSubprogram(SP);
```

and we now have an DISubprogram that contains a reference to all of our metadata for the function.

## Source Locations
The most important thing for debug information is accurate source location - this makes it possible to map your source code back. We have a problem though, Kaleidoscope really doesn’t have any source location information in the lexer or parser so we’ll need to add it.

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
  } else
    LexLoc.Col++;
  return LastChar;
}
```

In this set of code we’ve added some functionality on how to keep track of the line and column of the “source file”. As we lex every token we set our current current “lexical location” to the assorted line and column for the beginning of the token. We do this by overriding all of the previous calls to getchar() with our new advance() that keeps track of the information and then we have added to all of our AST classes a source location:

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

that we pass down through when we create a new expression:

```
LHS = llvm::make_unique<BinaryExprAST>(BinLoc, BinOp, std::move(LHS), std::move(RHS));
```

giving us locations for each of our expressions and variables.

To make sure that every instruction gets proper source location information, we have to tell Builder whenever we’re at a new source location. We use a small helper function for this:

```
void DebugInfo::emitLocation(ExprAST *AST) {
  DIScope *Scope;
  if (LexicalBlocks.empty())
    Scope = TheCU;
  else
    Scope = LexicalBlocks.back();
  Builder.SetCurrentDebugLocation(
      DebugLoc::get(AST->getLine(), AST->getCol(), Scope));
}
```

This both tells the main IRBuilder where we are, but also what scope we’re in. The scope can either be on compile-unit level or be the nearest enclosing lexical block like the current function. To represent this we create a stack of scopes:

```
std::vector<DIScope *> LexicalBlocks;
```

and push the scope (function) to the top of the stack when we start generating the code for each function:

```
KSDbgInfo.LexicalBlocks.push_back(SP);
```

Also, we may not forget to pop the scope back off of the scope stack at the end of the code generation for the function:

```
// Pop off the lexical block for the function since we added it
// unconditionally.
KSDbgInfo.LexicalBlocks.pop_back();
```

Then we make sure to emit the location every time we start to generate code for a new AST object:

```
KSDbgInfo.emitLocation(this);
```

## Variables
Now that we have functions, we need to be able to print out the variables we have in scope. Let’s get our function arguments set up so we can get decent backtraces and see how our functions are being called. It isn’t a lot of code, and we generally handle it when we’re creating the argument allocas in FunctionAST::codegen.

```
// Record the function arguments in the NamedValues map.
NamedValues.clear();
unsigned ArgIdx = 0;
for (auto &Arg : TheFunction->args()) {
  // Create an alloca for this variable.
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

  // Create a debug descriptor for the variable.
  DILocalVariable *D = DBuilder->createParameterVariable(
      SP, Arg.getName(), ++ArgIdx, Unit, LineNo, KSDbgInfo.getDoubleTy(),
      true);

  DBuilder->insertDeclare(Alloca, D, DBuilder->createExpression(),
                          DebugLoc::get(LineNo, 0, SP),
                          Builder.GetInsertBlock());

  // Store the initial value into the alloca.
  Builder.CreateStore(&Arg, Alloca);

  // Add arguments to variable symbol table.
  NamedValues[Arg.getName()] = Alloca;
}
```

Here we’re first creating the variable, giving it the scope (SP), the name, source location, type, and since it’s an argument, the argument index. Next, we create an lvm.dbg.declare call to indicate at the IR level that we’ve got a variable in an alloca (and it gives a starting location for the variable), and setting a source location for the beginning of the scope on the declare.

One interesting thing to note at this point is that various debuggers have assumptions based on how code and debug information was generated for them in the past. In this case we need to do a little bit of a hack to avoid generating line information for the function prologue so that the debugger knows to skip over those instructions when setting a breakpoint. So in FunctionAST::CodeGen we add some more lines:

```
// Unset the location for the prologue emission (leading instructions with no
// location in a function are considered part of the prologue and the debugger
// will run past them when breaking on a function)
KSDbgInfo.emitLocation(nullptr);
```

and then emit a new location when we actually start generating code for the body of the function:

```
KSDbgInfo.emitLocation(Body.get());
``` 

With this we have enough debug information to set breakpoints in functions, print out argument variables, and call functions. Not too bad for just a few simple lines of code!

## Full Code Listing
Here is the complete code listing for our running example, enhanced with debug information. To build this example, use:

### Compile

```
clang++ -g toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 -o toy
```

### Run

```
./toy
```
