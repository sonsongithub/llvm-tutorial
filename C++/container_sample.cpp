// MIT License
//
// Copyright (c) 2021 sonson
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT W  ARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <memory>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <typeinfo>

class Storage {
    // std::shared_ptr<char> buffer;
 public:
    Storage();
    ~Storage();
    Storage(const Storage &o);
};

Storage::Storage() {
    // std::shared_ptr<char> p(new char[1024]);
    // buffer = p;
    std::cout << "Init Storage - " << std::endl;
}

Storage::~Storage() {
    std::cout << "Destruct Storage - " << std::endl;
}

class Hoge {
    std::shared_ptr<Storage> storage;
 public:
    Hoge();
    ~Hoge();
    Hoge(const Hoge &o);
    Hoge(const Hoge &&o);
    void print();
};

Hoge::Hoge() {
    std::shared_ptr<Storage> p(new Storage);
    storage = p;
    printf("Init Hoge - %p\n", this);
}

Hoge::Hoge(const Hoge &o) {
    std::shared_ptr<Storage> p(new Storage);
    storage = p;
    printf("Copy Hoge - %p -> %p\n", &o, this);
}

Hoge::Hoge(const Hoge &&o) {
    storage = o.storage;
    printf("Move Hoge - %p -> %p\n", &o, this);
}

Hoge::~Hoge() {
    printf("Destruct Hoge - %p\n", this);
}

void Hoge::print() {
    printf("print Hoge - %p\n", this);
}

std::vector<Hoge> create_vector() {
    std::vector<Hoge> vec = {};
    std::cout << "push_back" << std::endl;
    vec.push_back(Hoge());
    std::cout << "push_back" << std::endl;
    vec.push_back(Hoge());
    std::cout << "push_back" << std::endl;
    vec.push_back(Hoge());

    std::cout << "Before return" << std::endl;
    return vec;
}

void local_create_vector() {
    std::vector<Hoge> vec = {};
    vec.push_back(Hoge());
}

int main() {
    // std::cout << "local_create_vector" << std::endl;
    // local_create_vector();
    std::cout << "local_create_vector" << std::endl;
    auto vec = create_vector();
    return 0;
}
