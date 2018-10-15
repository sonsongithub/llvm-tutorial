# Kaleidoscope: Tutorial Introduction and the Lexer

## Tutorial Introduction

コンパイル方法

```
clang++ --std=c++14 ./toy.cpp `llvm-config --cxxflags --ldflags --libs --libfiles --system-libs`
```

## The Basic Language
Kaleidoscopeという言語を作ります．

この言語は，

1. if/else文
2. for文
3. ユーザ定義の演算子
4. シンプルなコマンドラインインタフェースをもつJITコンパイラ

という特徴を持つ．
また，この言語は，簡単のため変数は64bit浮動小数点実数しかない．
なので，この言語は，型宣言が不要な簡単なものになる．

```
# Compute the x'th fibonacci number.
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1)+fib(x-2)

# This expression will compute the 40th number.
fib(40)
```

Kaleidoscopeでは，Standard libraryの関数を呼べるようにする．
また，`extern`をつけて関数宣言できるようにする．
例えば，以下のようになる．

```
extern sin(arg);
extern cos(arg);
extern atan2(arg1 arg2);

atan2(sin(.4), cos(42))
```

## The Lexer

1. 語彙解析(Lexical Analysis)
1. トークンを定義する．トークンは，いくつかメタデータを持つ．数値ならば，実数の値など．

```
// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,
  tok_extern = -3,

  // primary
  tok_identifier = -4,
  tok_number = -5,
};

// リテラルで文字列を書いたとき
static std::string IdentifierStr; // Filled in if tok_identifier
// リテラルで数値を書いたとき
static double NumVal;             // Filled in if tok_number
```

この例でグローバル変数を多用しているが，これは簡単のためで，実際の実装では，このような手法は，実際の実装においてベストなソリューションではないと思う．

`Lexer`の実際の実装は，一つの関数`gettok`だけである．
`space`を飛ばしながら，`IdentifierStr`に保存していく．
変数名や変数宣言，関数宣言は，下で処理する．
一旦，アルファベットが見つかったら，この中で`space`まで読み込んで，処理する．

```
    // Skip any whitespace.
    while (isspace(LastChar))
        LastChar = getchar();

    if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
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

`strtod`・・文字列の始めの部分を`double`型の数値に変換する．
1.23.46のような値は，1.23に丸められる．

```
    if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), nullptr);
        return tok_number;
    }
```

`#`は，コメントアウトなので，行末あるいはファイルの最後まで飛ばす．

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

これで，ソースコードの中身は，すべてトークンに分割された．
次に，抽象構文木を作っていく．