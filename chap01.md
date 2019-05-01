
# Tutorial
"LLVMで言語を実装する"のチュートリアルへようこそ．
このチュートリアルは，シンプルな言語の実装を通じて，それがいかに簡単で楽しいものかを伝えることを目的にする．
このチュートリアルは，LLVMを始めるだけでなく，他の言語へ拡張できるフレームワークを構築する手助けにもなる．
このチュートリアルで出てくるコードは，その他のLLVMの特定の何かをハックするための砂場としても使えるはずだ．

このチュートリアルの最終ゴールは，これから作る言語を，常にどうやって構築していくかを説明しながら，だんだんと明らかにしていくことである．
これは，幅広い言語設計について知識やLLVMの特定の使い方をカバーし，それらを実現する手法を見せて，説明していく．
ただし，困惑するくらいたくさんある詳細については，無視する．

このチュートリアルは，本当にコンパイラ技術やLLVMを具体的に教えるもので，最近の，健全なソフトウェアエンジニアリングの原理原則について教えるものではない．
実践的に，これは，説明を簡単にするために，たくさんのショートカットを使うことを意味する．
例えば，コードがグローバル変数を使う，ビジターパターンのような良いデザインパターンを使わない・・・などなど・・しかし，コードはシンプルでる．
もし，将来のプロジェクトのために基本として，コードを深くいじったり，使うなら，これらの欠陥を治すのは，そんなに難しいことではないだろう．

このチュートリアルでは，すでに知っていることや，あまり興味がない章は，簡単にスキップできるようにしたつもりだ．

チュートリアルの終わりまでに書き上げるコードは，コメントや空行を含めないで1000行に満たないものになる．
この小規模のコードで，字句解析，パーサ，抽象構文器，コード生成，JITコンパイラを含めた合理的なコンパイラを作り上げる．
ほかのシステムは，おもしろい"hello world"チュートリアルをやる一方で，このチュートリアルの幅広さは，LLVMの長所のよい証拠であり，もし，あなたが言語やコンパイラ設計で興味にあるなら，そうであることをあなたはじっくり考えるべきであると，私は考える．

このチュートアルについて，特に言いたいことは，我々は，あなたが，この言語を拡張し，自身の手でそれで遊ぶようになることだ．

コードを手に取り，それをクラックしまくってほしいーコンパイラを怖がる必要はないー言語で遊ぶことはすごく楽しい遊びになるはずだ．

## 基本言語

このチュートリアルは，我々が"Kaleidoscope"（美しい形や見かけという意味から）と呼んでいるおもちゃの言語で構成される．
Kaleidoscopeは，関数定義，条件分岐，数学関数が使える手続き型言語である．
チュートリアルの終わりまでに，Kaleidoscopeは，`if/then/else`構造，`for`ループ，ユーザ定義演算子，JITコンパイラ，JITコンパイルのためのコマンドラインツールをサポートするように拡張されていく．

物事をシンプルにするために，Kaleidoscopeでは，データ型として，64bitの浮動小数点型のみをサポートするようにする（C言語でいうところの`double`型）．

すべての値は暗黙に倍精度であり，言語は型宣言を必要としない．
これは，言語の文法を非常に簡単にする．
例えば，フィボナッチ数列を計算するコードは，以下のようになる．

```
# x番目のフィボナッチ数列を計算するコード
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1)+fib(x-2)
    
# この表現で，40番目のフィボナッチ数列を得る
fib(40)
```

さらに，Kaleidoscopeでstandard libraryの関数を呼べるようにする（LLVM JITがこれを簡単にする）．
これは，`extern`キーワードを使って，今までに使ったことのある関数を定義できることを意味する（これはまた，相互に再帰関数を実装するのにも役立つ）．
例えば，以下のようなコードを書ける．

```
extern sin(arg);
extern cos(arg);
extern atan2(arg1 arg2);

atan2(sin(.4), cos(42))
```

さらにおもしろい例は，任意のレベルのマンデルブロー集合を表示するKaleidoscopeアプリケーションを書く6章にある．

さぁ，この言語の実装をはじめよう．

## 字句解析器
言語の実装を始める．はじめに，テキストファイルを処理し，テキストが何を言ってるのかを認識する能力を持たせる必要がある．
伝統的な方法は，入力をトークンに分割する"字句解析器"を使うことだ．
字句解析器が返すそれぞれのトークンは，トークンコードと，潜在的なメタデータ（例えば，数字であれば数値）を含む．
はじめに，その一覧を下記に示す．

```
// 字句解析器は，トークンのコードとして，[0-255]を返す．
enum Token {
  tok_eof = -1,

  // コマンド
  tok_def = -2,
  tok_extern = -3,

  // 主表現
  tok_identifier = -4,
  tok_number = -5,
};
```

```
// トークンがtok_identifierなら，この変数に値を入れる
static std::string IdentifierStr; 
// トークンがtok_numberなら，この変数に値を入れる
static double NumVal;             
```

字句解析器によって返されるトークンは，`Token`の列挙型のひとつか，`+`のような"未知"の文字コード（ASCIIコードの値）となる．
現在のトークンが何らかの識別名である場合，グローバル変数の`IdentifierStr`に，その識別名が保存される．
もし，現在のトークンが，数字リテラルである場合は，グローバル変数の`NumVal`にその値を保存する．
ここで，注意してほしいのは，グローバル変数を使っているのは，簡単のためであって，それが実際に言語を実装するにあたってベストな選択ではないことだ．

字句解析器の実際の実装は，`gettok`という一つの関数でなされる．
`gettok`関数は，標準入力から得た次のトークンを返すために呼ばれる．
その実装は，以下のように始まる．

```
/// gettok - 標準入力から次のトークンを返す
static int gettok() {
  static int LastChar = ' ';

  // 空白はスキップされる
  while (isspace(LastChar))
    LastChar = getchar();
```

`gettok`は，C言語の`getchar()`を呼び出し，一文字ずつ標準入力から読み取る．
それは，読み出した文字を認識した後，最後の文字を読み取り保存するが，処理はしない．
その最後の文字は，`LastChar`に保存される．
上のコードで最初に実装されているのは，トークン間の空白が無視することで，これは，ループの最初に実行される．

次に，`gettok`がやるべきことは，`def`のような特定のキーワードを識別，認識することである．
Kaleidoscopeは，以下のシンプルなループでそれを実行する．

```
if (isalpha(LastChar)) { // [a-zA-Z][a-zA-Z0-9]*の正規表現を識別する
  IdentifierStr = LastChar;
  while (isalnum((LastChar = getchar())))
    IdentifierStr += LastChar;

  if (IdentifierStr == "def")
    return tok_def;
  if (IdentifierStr == "extern")
    return tok_extern;
  return tok_identifier;
}
```

このコードは，

Note that this code sets the ‘IdentifierStr’ global whenever it lexes an identifier. Also, since language keywords are matched by the same loop, we handle them here inline. Numeric values are similar:

```
if (isdigit(LastChar) || LastChar == '.') {   // Number: [0-9.]+
  std::string NumStr;
  do {
    NumStr += LastChar;
    LastChar = getchar();
  } while (isdigit(LastChar) || LastChar == '.');

  NumVal = strtod(NumStr.c_str(), 0);
  return tok_number;
}
```

This is all pretty straight-forward code for processing input. When reading a numeric value from input, we use the C strtod function to convert it to a numeric value that we store in NumVal. Note that this isn’t doing sufficient error checking: it will incorrectly read “1.23.45.67” and handle it as if you typed in “1.23”. Feel free to extend it :). Next we handle comments:

```
if (LastChar == '#') {
  // Comment until end of line.
  do
    LastChar = getchar();
  while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

  if (LastChar != EOF)
    return gettok();
}
```

We handle comments by skipping to the end of the line and then return the next token. Finally, if the input doesn’t match one of the above cases, it is either an operator character like ‘+’ or the end of the file. These are handled with this code:

```
  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}
```

With this, we have the complete lexer for the basic Kaleidoscope language (the full code listing for the Lexer is available in the next chapter of the tutorial). Next we’ll build a simple parser that uses this to build an Abstract Syntax Tree. When we have that, we’ll include a driver so that you can use the lexer and parser together.
