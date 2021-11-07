# C++復習？勉強？

* This document is my memo, so it is in Japanese, and English.
* This folder is especially tivial... because the author is studying C++.....
    * (このフォルダは，著者がC++の勉強のためにあるので，特にレベルが低いです．あまりに参考になさらぬよう．)

# llvm::Expected<T>

`llvm::Expected<T>`は，オブジェクトとエラーをラップするテンプレート．生成に失敗した時はエラーを返し，そうでないときは，オブジェクトを返す・・みたいなことが実装できる．

```
class Hoge {
 public:
    Hoge();
    ~Hoge();
    void print();
};

Hoge::Hoge() {
    printf("Init Hoge - %p\n", this);
}

Hoge::~Hoge() {
    printf("Destruct Hoge - %p\n", this);
}

void Hoge::print() {
    printf("print Hoge - %p\n", this);
}

std::error_code sampleErrorCode = std::make_error_code(std::errc::already_connected);

llvm::Expected<Hoge> create(int input) {
    if (input == 0)
        return llvm::createStringError(sampleErrorCode, "Something happened.");
    return std::move(Hoge());
}

int main(int argc, char *argv[]) {
    auto p = create(1);
    if (auto error = p.takeError()) {
        std::cout << "error" << std::endl;
    } else {
        p->print();
    }
    return 0;
}
```

[source](./expected_example.cpp)

# const

```
int main() {
    {
        int i = 0;
        int j = 0;

        // Can change a  reference but can not change value.
        int const *a = &i;
        const int* b = &i;
        
        // This is OK.
        a = &j;
        b = &j;
        // This is NG.
        // *a = 10;

    }
    {
        int i = 0;
        int j = 0;
        // Can not change a reference, but can update value at the reference.
        int* const b = &i;
        // This is NG.
        // b = &j;
        // This is OK.
        *b = 10;
    }
    {
        int i = 0;
        int j = 0;
        // All prohibited.
        const int* const b = &i;
        // NG
        // b = &j;
        // *b = 10;
    }

    return 0;
}
```

[source](./const_example.cpp)

# shared_ptr



# Deleter

`shared_ptr`で自動削除される前に実行したい処理がある場合，`deleter`を`shared_ptr`のコンストラクタのときに引き渡せる．`deleter`の引数で渡されるポインタは最後に`delete`する必要がある．

```
void deleter_hoge(Hoge* p) {
    std::cout << "Implment, here, the code which must be executed before deleting(or call destructor?) Hoge instance." << std::endl;
    std::cout << "deleter_hoge - " << static_cast<void*>(p) << std::endl;
    delete p;
}

int main() {
    std::shared_ptr<Hoge> p1(new Hoge(), deleter_hoge);
}
```

[source](./deleter_example.cpp)

# 参考文献

[1] https://proc-cpuinfo.fixstars.com/2016/03/c-html/
[2] https://yohhoy.hatenablog.jp/entry/2012/12/15/120839
[3] https://rinatz.github.io/cpp-book/ch07-08-assignment-operator/