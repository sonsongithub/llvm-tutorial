# まとめ，その他のちょっとしたこと
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
上で述べた多くの言語もまた，"安全"な言語です．
つまり，メモリのアドレス空間を破壊したり，プロセスをクラッシュさせるようなJavaで書かれたプログラムを作ることはできません（JVMにバグないと仮定して）．
安全性は，言語設計，ランタイムサポート，そしてOSのサポートを組み合わせる必要があるすおもしろい性質です．

それは，LLVMで書かれた安全な言語を実装することは，あきらか可能です．
しかし，LLVM IRは，それ自身に安全性を保証していません．
LLVM IRは，危険性のあるポインターのキャストを許そうしますし，バグもありますし，バッファオーバーランも，その他，色々な問題を内包しています．
安全性は，LLVMの上位のレイヤーとして，実装されるべきで，都合のいいことに，いくつかの開発者グループで，この課題についての調査が始まっています．
これについて，もっと詳細を知りたい場合は，llvm-devのメーリングリストに尋ねてみてください．

### 言語固有の最適化
LLVMが多くの人をがっかりさせてしまうひとつの理由に，あるシステムにある問題の全てを解決してくれないという点にあります（すみません，"世界の飢餓"のような問題は，いつか，あなたが解決していかねばならないでしょう）．
ある特定の不満は，人々がLLVMが高レベル言語を最適化できないと受けてめていることにあります．
つまり，LLVMは，"多くの情報を失っている"と．

不幸にも，この文章の目的は，クリス ラートナーが書いた"コンパイラ設計論"のすべてを解説することではありませんが，代わりに，いくつかの意見を述べたいと思います．

はじめに，LLVMが情報を失っているという考えは正しいです．
例えば，この文章にもあったように，(デバッグ情報というより)あるLLVMの表現が，C言語の`int`から来たSSAの値なのか，C言語のILP32マシン上での`long`からきたものなのかを，識別する方法はありません．
これらの両方が，コンパイル時に`i32`の値へキャストされ，本来持っていた情報は失われます．
ここでの，もっと一般的な問題は，LLVMの型システムが，"名前の同一性"の代わりに，"構造的な等価性"を使っていることです．
言い換えると，高レベル言語では，異なる二つの型として扱われる変数が，LLVMでは，同じ構造を持つということに驚かされます（例えば，二つの違う構造体が一つの`int`フィールドを持っているときなど）．
つまり，これらの型は一つのLLVMの型にコンパイル時にキャストされ，もはや，それがどこから来たものなのか問い合わせる手段がなくなってしまいます．

二点目は，LLVMは情報を失っているにも関わらず，LLVMは，ターゲットが固定されません．
我々は，多くのいろんな方法で，LLVMを改善したり，拡張したりしつづけます．
追加されていく多くの新しい機能に加えて，我々は，IRが最適化に必要かつ重要な情報を捉えられるように拡張していきます（例えば，引数が符号が付いているか，ゼロであるか，ポインタのエイリアスについての情報など）．
拡張の多くは，ユーザが推し進めるものです．
つまり，人々は，LLVMがいくつかの特定の機能をサポートしてほしいと望んでいるので，彼らがそれを推し進め，拡張することになるのです．

三点目は，LLVMを使えば，言語固有の最適化を追加するのが可能かつ簡単であるということです．
そして，それをやるために多くの選択肢があります．
ちょっとした例として，その言語のためにコンパイルされたコードについて"知っている"ことを，言語特定の最適化Passを追加するのが簡単です．
言語のためにコンパイルされたコードについてのことを知っている言語固有の最適化パスを追加するのが簡単です．
C言語の場合，一般的なC言語のライブラリについて知っている最適化Passがあります．
`main()`の中で`exit(0)`をコールする場合，C言語，`exit`関数が行うことを明記しているため，その最適化Passは，そのコードを`return 0;`に最適化しても安全です．

シンプルなライブラリに関する知識に加えて，LLVM IRへ他の言語特有の情報を埋め込むこともできます．
もし，あなたは，何か特定の必要性，やらないといけないことがあり，壁にぶつかっているなら，そのトピックをllvm-devのメーリングリストに投げかけてください．
最悪の場合，常にLLVMをクソコード生成器として扱い，それをカバーするために，言語固有の抽象構文木上で，フロントエンドで高レベルの最適化を実装することになってしまいます．

## Tips
初めて見て，すぐにわかるわけではないLLVMを使い仕事し始めると，色々なtipsを知り始めます．
各員にそれらを再発見させるのではなく，本節では，これらの課題について説明したいと思います．

### 移植性の高い`offsetof/sizeof`の実装
ひとつおもしろいことは，もし，あなたが，あなたのコンパイラによって生成されたコードがターゲット非依存になるようにしようとするなら，LLVMの型のデータサイズや，構造体やクラスのフィールドのオフセットのサイズを知る必要があります．
例えば，メモリを確保する関数に型のサイズを渡す必要があるかもしれません．

不幸にも，これは，複数のターゲットに広くまたがった話です．
つまり，例えば，ポインタの幅は，ささいなことですがターゲットに固有なものです．
しかし，これを移植するときにも計算できるようにする`getelementptr`命令を使う賢い方法があります．

### スタックフレームのガーベッジコレクション
いくつかの言語は，明確にそれ自身のスタックフレームを管理しようとし，このため，しばしば，スタックフレームがガーベッジコレクトされたり，クロージャの実装が簡単になるようにしようとします．
明示的なスタックフレームよりも，これらの機能を実装した方法がよいことがありますが，もし，あなたがスタックフレームを実装したい場合，LLVMはそれをサポートします．
スタックフレームは，フロントエンドにコードを継続渡しスタイル(Continuation Passing Style)に変換すること，末尾呼び出しの使用を要求する．