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

std::vector<std::shared_ptr<Hoge>> local_shared_ptr_vector() {
    std::vector<std::shared_ptr<Hoge>> vec = {};

    vec.push_back(std::make_shared<Hoge>());

    std::shared_ptr<Hoge> p(new Hoge());

    vec.push_back(p);

    return vec;
}

std::shared_ptr<Hoge> hoge(int i) {
    auto ptr_vec = local_shared_ptr_vector();
    return ptr_vec[i];
}

int main() {
    auto p = hoge(0);
    p->print();
    return 0;
}
