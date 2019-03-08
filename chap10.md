# Kaleidoscope: まとめ，他の使いやすいLLVMのちょっとしたこと
## チュートリアルの結論
LLVMで言語を実装するチュートリアルの最終章にようこそ．
このチュートリアルのコースでは，使いようのないおもちゃであった小さなKaleidoscopeという言語を，多少はおもしろい言語に（しかし，まだ使えないけど）まで育ててきました．

ここがどれくらい遠いのか，ここまででどれくらいコードが必要だったかを考えるのはおもしろいことです．
我々は，lexer，パーサ，抽象構文木，コード生成器，インタラクティブな処理のためのループを開発し，また，スタンドアローンで動作するデバッグ情報を発行するようにしたりしてきた．
これらのコードは，1000行足らずのものでした．

我々の小さな言語は，いくつかのおもしろい機能をサポートします．
その機能とは，ユーザ定義の二項あるいは単項演算子，即時評価されるJITコンパイラ，SSA構造で動作する制御フローなどです．

このチュートリアルのアイデアは，言語を定義し，構築し，それで遊ぶことがいかに簡単であるかを示すためのものです．
コンパイラを作ることために，恐ろしい，あるいは意味不明なプロセスは必要ありません．
今，あなたは，基本をほぼ理解したはずです．
私は，Kaleidoscopeのコードをハックしていくことをお勧めします．
例えば，以下のような機能を追加してみていかがでしょうか．

### グローバル変数
グローバル変数は，現代のソフトウェアエンジニアリングにおいては，議論の対象となるものではあるが，Kaleidoscopeのコンパイラそれ自身のように．ちょっとしたハックを手短に済ませたいときには便利なものである．
幸運にも，現在のコードは，グローバル変数を追加するのは簡単だ．
つまり，解決できない変数があった場合，それを拒絶する前に，グローバル変数のシンボルテーブルをチェックするようにすればよいのである．
新しいグローバル変数を作るためには，LLVMの`GlobalVariable`クラスのインスタンスを作ればよい．

### 型付変数
Kaleidoscopeは，今の所，double型の変数のみをサポートしている．
これは，言語をとても美しくします．
なぜなら，一つの型のみをサポートするということは，型を指定する必要がないということだからです．
他の言語は，難しい方法で，これを取り扱います．
最も簡単な方法は，それぞれの変数定義に型を明示させるようにすることです．
そして，シンボルテーブルで変数の型も`Value*`と一緒に保存するようにすることです．

### 配列，構造体，ベクトルなどのデータ構造
一度，複数の型を追加すると，いくつかの，そして，多くのおもしろい方法で，型システムを拡張することができるようになります．
単純な配列はとても簡単ですが，多くのアプリケーションにとって，当然とても便利なものです．
それらを追加するのは，ほとんどの場合，LLVMの`getelementptr`命令がどうやって動作しているのかを学ぶいい練習になります．
つまり，それは，便利で，ちょっと風変わりなもので，`getelementptr`命令自体がFAQの対象となります．

### 標準的なランタイム
現状の言語は，ユーザに任意の外部関数にアクセスすることを許容します．
我々は，その特徴を`printd`や`putchard`のような形で使ってきました．
よりハイレベルな言語構成に拡張しようとするときに，より低レベルな命令を呼び出すならば，この構成は非常に理にかなったものと言えます．
例えば，もし，言語にハッシュテーブルを追加するなら，インラインで処理を書くのではなく，ランタイムに処理を追加する方がおそらく理にかなっています．

### メモリ管理
現状では，Kaleidoscope上のスタックにのみアクセスできます．
それは，ヒープメモリを確保するのに便利ですが，`malloc/free`のようなインタフェースを呼び出したり，ガーベッジコレクタを使って，確保することもできます．
もし，ガーベッジコレクションを使ってみたいのであれば，LLVMは，オブジェクトの移動，スタックを`scan/update`する必要があるアルゴリズムを含む，"Accurate Garbage Collection"をサポートすることを覚えておいてください．

### 例外
LLVMは，他の言語で，コンパイルされたコードに，まったくコストをかけることなく割り込みをかける例外の生成をサポートします．
暗黙に各々の関数にエラーを返させたり，それをチェックすることによって，コードを生成することもできます．
また，`setjmp/longjmp`を明示的に使うこともできます．
そうするために，色々な種類の方法があるということです．

### オブジェクト指向，ジェネリクス，データベースへのアクセス，虚数，幾何・・・・
本当に，際限がないくらい，たくさんの機能を追加することができます．

### 特殊な分野に特化させる
ここまでは，特定の言語のためのコンパイラを作る，という多くの人が興味を持つエリアに，LLVMを応用することについて話してきました．
しかし，典型的ではない，特殊なコンパイラ技術を使う分野も他にたくさんあります．
例えば，LLVMは，OpenGLのグラフィックスのアクセラレーションを実装するのに使われたり，C++のコードをActionScriptに変換したりなど，色々な賢いアイデアを実現するために使われています．
正規表現のインタプリタをネイティブコードに，LLVMを使ってJITコンパイルするのは，あなたになるかもしれません．

### 楽しもう！
めちゃくちゃなことや，普通じゃないことに挑戦しましょう．
みんなが使っていたりする言語を作るのは，ちょっとおかしい言語を作ったり，普通とは違うことをしたり，それがどう出てきたかを理解したりすることに比べるといささか退屈です．
もし，そういったことに行き詰まっていたり，何か話したいことがあるなら，llvm-devのメーリングリストに気軽にメールするとよいでしょう．
つまり，そのメーリングリストは，言語に興味のある人がたくさん参加しており，さらに，彼らは，よろこんで，手助けをしてくれます．

このチュートリアルを終える前に，LLVM IRを生成するための，いくつかの"テク"について話したいと思います．
これらは，わかりきったことではないものの，割と微妙なものです．
しかし，LLVMの能力を利用したい人にとっては，非常に使いやすいものです．

## LLVM IRの性質
ここで，LLVM IR形式で書かれたコードについて，２，３のありふれた質問をします．
さあ，用意はいいですか？

### ターゲットからの独立
Kaleidoscopeは，"移植しやすい言語"の例でした．
つまり，Kaleidoscopeで書かれたプログラムは，他のターゲット上であっても，同じ方法で動作させることができます．
lisp, java, haskell, js, pythonなどの多くの他の言語は，この性質を備えています（これらの言語は移植しやすいのですが，そのライブラリが・・・そうではないのです・・・・）．

LLVMのよい一面として，IR上ではターゲットの独立性が維持されることです．
つまり，LLVMがサポートするターゲットであれば，Kaleidoscopeをコンパイルされたコードを動作させることができます．
他には，Cのコードを生成したり，LLVMが現状サポートしないターゲット上で動作するコードを実行させられます．
Cのコードを生成し，LLVMが現状サポートしないターゲットにコンパイルすることさえできます．

Kaleidoscopeのコンパイラがターゲットに依存しないコードを生成することを簡単に理解できるはずです．
なぜなら，コードが生成されるときにターゲットの情報を聞かれなかったからです．

事実，LLVMは，多くの人が楽しくなるような，コンパクトで，ターゲットに非依存な表現を提供します．
不幸にも，こういった人々は，言語の移植性について考える時に，C言語やC言語の派生言語についていつも考えているようです．

私が"不幸にも"といったのは，Cのコードを，配布する以外に，本当に移植性を高くする方法が存在しないためです．
もちろん，Cのソースコードは，実際には一般的に言って移植性がありません．例えば，32bitから64bitへ，本当に古いアプリケーションを移植できるでしょうか．

C言語の問題は（繰り返しますが，これは一般的にも言えることです），ターゲット固有の仮定でいっぱいになっているからです．
ひとつシンプルな例を挙げると，プリプロセッサは，それが入力されたテキストを処理するときに，しばしば，コードから，破壊的にターゲットからの独立性を取り去ってしまいます．

```
#ifdef __i386__
    int X = 1;
#else
    int X = 42;
#endif
```

このような問題は，より複雑な解決方法をエンジニアに引き起こす一方で，ソースコードのままアプリケーションを出荷するよりも良い，十分に一般的な方法では，解決できません．

つまり，移植が容易ではないC言語のおもしろいサブセットがあります．
もし，プリミティブな型のサイズを固定にしたい場合，既に存在するバイナリとのABIとの互換性を気にせず，そして，いくつかのマイナーな機能は喜んで諦めるべきです．そうすれば，コードの移植性を高めることができます．
これは，カーネル用の言語のような特殊なドメインにおいても，成立します．

### 安全性の保証
Many of the languages above are also “safe” languages: it is impossible for a program written in Java to corrupt its address space and crash the process (assuming the JVM has no bugs). Safety is an interesting property that requires a combination of language design, runtime support, and often operating system support.

It is certainly possible to implement a safe language in LLVM, but LLVM IR does not itself guarantee safety. The LLVM IR allows unsafe pointer casts, use after free bugs, buffer over-runs, and a variety of other problems. Safety needs to be implemented as a layer on top of LLVM and, conveniently, several groups have investigated this. Ask on the llvm-dev mailing list if you are interested in more details.

### Language-Specific Optimizations
One thing about LLVM that turns off many people is that it does not solve all the world’s problems in one system (sorry ‘world hunger’, someone else will have to solve you some other day). One specific complaint is that people perceive LLVM as being incapable of performing high-level language-specific optimization: LLVM “loses too much information”.

Unfortunately, this is really not the place to give you a full and unified version of “Chris Lattner’s theory of compiler design”. Instead, I’ll make a few observations:

First, you’re right that LLVM does lose information. For example, as of this writing, there is no way to distinguish in the LLVM IR whether an SSA-value came from a C “int” or a C “long” on an ILP32 machine (other than debug info). Both get compiled down to an ‘i32’ value and the information about what it came from is lost. The more general issue here, is that the LLVM type system uses “structural equivalence” instead of “name equivalence”. Another place this surprises people is if you have two types in a high-level language that have the same structure (e.g. two different structs that have a single int field): these types will compile down into a single LLVM type and it will be impossible to tell what it came from.

Second, while LLVM does lose information, LLVM is not a fixed target: we continue to enhance and improve it in many different ways. In addition to adding new features (LLVM did not always support exceptions or debug info), we also extend the IR to capture important information for optimization (e.g. whether an argument is sign or zero extended, information about pointers aliasing, etc). Many of the enhancements are user-driven: people want LLVM to include some specific feature, so they go ahead and extend it.

Third, it is possible and easy to add language-specific optimizations, and you have a number of choices in how to do it. As one trivial example, it is easy to add language-specific optimization passes that “know” things about code compiled for a language. In the case of the C family, there is an optimization pass that “knows” about the standard C library functions. If you call “exit(0)” in main(), it knows that it is safe to optimize that into “return 0;” because C specifies what the ‘exit’ function does.

In addition to simple library knowledge, it is possible to embed a variety of other language-specific information into the LLVM IR. If you have a specific need and run into a wall, please bring the topic up on the llvm-dev list. At the very worst, you can always treat LLVM as if it were a “dumb code generator” and implement the high-level optimizations you desire in your front-end, on the language-specific AST.

## Tips and Tricks
There is a variety of useful tips and tricks that you come to know after working on/with LLVM that aren’t obvious at first glance. Instead of letting everyone rediscover them, this section talks about some of these issues.

### Implementing portable offsetof/sizeof
One interesting thing that comes up, if you are trying to keep the code generated by your compiler “target independent”, is that you often need to know the size of some LLVM type or the offset of some field in an llvm structure. For example, you might need to pass the size of a type into a function that allocates memory.

Unfortunately, this can vary widely across targets: for example the width of a pointer is trivially target-specific. However, there is a clever way to use the getelementptr instruction that allows you to compute this in a portable way.

### Garbage Collected Stack Frames
Some languages want to explicitly manage their stack frames, often so that they are garbage collected or to allow easy implementation of closures. There are often better ways to implement these features than explicit stack frames, but LLVM does support them, if you want. It requires your front-end to convert the code into Continuation Passing Style and the use of tail calls (which LLVM also supports).