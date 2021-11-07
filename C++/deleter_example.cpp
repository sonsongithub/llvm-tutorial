#include <iostream>
#include <memory>

class Hoge {
 public:
    Hoge();
    ~Hoge();
    Hoge(const Hoge &o);
    void print();
};

Hoge::Hoge() {
    printf("Init Hoge - %p\n", this);
}

Hoge::Hoge(const Hoge &o) {
    printf("Copy Hoge - %p -> %p\n", &o, this);
}

Hoge::~Hoge() {
    printf("Destruct Hoge - %p\n", this);
}

void Hoge::print() {
    printf("print Hoge - %p\n", this);
}

void deleter_hoge(Hoge* p) {
    std::cout << "Implment, here, the code which must be executed before deleting(or call destructor?) Hoge instance." << std::endl;
    std::cout << "deleter_hoge - " << static_cast<void*>(p) << std::endl;
    delete p;
}

int main() {
    std::shared_ptr<Hoge> p1(new Hoge(), deleter_hoge);
}