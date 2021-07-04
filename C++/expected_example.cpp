// MIT License
//
// Copyright (c) 2020 sonson
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

#include "llvm/IR/Verifier.h"

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
