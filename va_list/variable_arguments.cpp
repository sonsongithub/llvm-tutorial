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
#include <string>
#include <vector>
#include <iostream>

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/InitLLVM.h"

// http://llvm.org/docs/LangRef.html#int-varargs

// base code

// ; This struct is different for every platform. For most platforms,
// ; it is merely an i8*.
// %struct.va_list = type { i8* }

// ; For Unix x86_64 platforms, va_list is the following struct:
// ; %struct.va_list = type { i32, i32, i8*, i8* }

// define i32 @test(i32 %X, ...) {
//   ; Initialize variable argument processing
//   %ap = alloca %struct.va_list
//   %ap2 = bitcast %struct.va_list* %ap to i8*
//   call void @llvm.va_start(i8* %ap2)

//   ; Read a single integer argument
//   %tmp = va_arg i8* %ap2, i32

//   ; Demonstrate usage of llvm.va_copy and llvm.va_end
//   %aq = alloca i8*
//   %aq2 = bitcast i8** %aq to i8*
//   call void @llvm.va_copy(i8* %aq2, i8* %ap2)
//   call void @llvm.va_end(i8* %aq2)

//   ; Stop processing of arguments.
//   call void @llvm.va_end(i8* %ap2)
//   ret i32 %tmp
// }

// declare void @llvm.va_start(i8*)
// declare void @llvm.va_copy(i8*, i8*)
// declare void @llvm.va_end(i8*)

// C
extern "C" void print_int(int i) {
    printf("%d\n", i);
}

// C
extern "C" void print_double(double i) {
    printf("%f\n", i);
}

using llvm::LLVMContext;
using llvm::StructType;
using llvm::Type;
using llvm::Module;
using llvm::FunctionType;
using llvm::Function;

StructType* register_struct_va_list(LLVMContext* context) {
    std::vector<Type*> members;
    members.push_back(Type::getInt32Ty(*context));
    members.push_back(Type::getInt32Ty(*context));
    members.push_back(Type::getInt8PtrTy(*context));
    members.push_back(Type::getInt8PtrTy(*context));

    StructType *const struct_va_list = StructType::create(*context, "struct.va_list");
    struct_va_list->setBody(members);

    return struct_va_list;
}

void add_functions(LLVMContext* context, Module *module) {
    // load functions for variable arguments.
    FunctionType *function_type_print
        = FunctionType::get(Type::getVoidTy(*context), Type::getInt8PtrTy(*context), false);
    module->getOrInsertFunction("llvm.va_start", function_type_print);
    module->getOrInsertFunction("llvm.va_end", function_type_print);

    FunctionType *printIntFunctionType
        = FunctionType::get(Type::getVoidTy(*context), Type::getInt64Ty(*context), false);
    module->getOrInsertFunction("print_int", printIntFunctionType);

    FunctionType *printDoubleFunctionType
        = FunctionType::get(Type::getVoidTy(*context), Type::getDoubleTy(*context), false);
    module->getOrInsertFunction("print_double", printDoubleFunctionType);
}

Function* define_main_function(LLVMContext* context, Module *module) {
    // define function
    auto main_function_name = "originalFunction";
    std::vector<Type *> args(1, Type::getInt64Ty(*context));
    FunctionType *functionType = FunctionType::get(Type::getDoubleTy(*context), args, true);
    return Function::Create(functionType, Function::ExternalLinkage, main_function_name, module);
}

int main(int argc, char *argv[]) {
    using llvm::IRBuilder;
    using llvm::BasicBlock;
    using llvm::ConstantFP;
    using llvm::Value;

    // Init LLVM
    llvm::InitLLVM X(argc, argv);

    // create context
    auto context = std::make_unique<LLVMContext>();

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // Create a new module
    std::unique_ptr<Module> module(new Module("originalModule", *context));
    // LLVM IR builder
    IRBuilder<> builder(*context);

    StructType *const struct_va_list = register_struct_va_list(context.get());
    add_functions(context.get(), module.get());

    Function *function = define_main_function(context.get(), module.get());

    // Create a new basic block to start insertion into.
    BasicBlock *basicBlock = BasicBlock::Create(*context, "entry", function);
    builder.SetInsertPoint(basicBlock);

    // allocate mutable variables
    llvm::AllocaInst *a_va_list = builder.CreateAlloca(struct_va_list, 0, "va_list");
    llvm::AllocaInst *a_count = builder.CreateAlloca(llvm::Type::getInt64Ty(*context), 0, "count");
    llvm::AllocaInst *a_summation = builder.CreateAlloca(llvm::Type::getDoubleTy(*context), 0, "summation");

    // load the first argument.
    llvm::Value* arg = (function->arg_begin());
    builder.CreateStore(arg, a_count);
    // initialize summation
    builder.CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), a_summation);
    // get pointer to va_list struct
    auto pointer_va_list = builder.CreateBitCast(a_va_list, llvm::Type::getInt8PtrTy(*context), "&va_list");

    Function *func_va_start = module->getFunction("llvm.va_start");
    Function *func_va_end = module->getFunction("llvm.va_end");

    builder.CreateCall(func_va_start, pointer_va_list);

    BasicBlock *loop_block = BasicBlock::Create(*context, "loop", function);
    builder.CreateBr(loop_block);
    builder.SetInsertPoint(loop_block);

    auto step = llvm::ConstantInt::get(Type::getInt64Ty(*context), 1);

    // for loop
    auto current_count = builder.CreateLoad(a_count, "current_count");
    auto updated_count = builder.CreateSub(current_count, step, "updated_count");
    builder.CreateStore(updated_count, a_count);

    auto *value_from_va_list
        = new llvm::VAArgInst(pointer_va_list, llvm::Type::getDoubleTy(*context), "value", loop_block);
    llvm::Value *value = llvm::dyn_cast_or_null<Value>(value_from_va_list);

    auto current_summation = builder.CreateLoad(a_summation, "current_summation");
    auto updated_summation = builder.CreateFAdd(current_summation, value, "updated_summation");
    builder.CreateStore(updated_summation, a_summation);
    auto loop_flag
        = builder.CreateICmpSGT(updated_count, llvm::ConstantInt::get(Type::getInt64Ty(*context), 0), "loop_flag");

    BasicBlock *after_loop_block = BasicBlock::Create(*context, "afterloop", function);

    builder.CreateCondBr(loop_flag, loop_block, after_loop_block);

    builder.SetInsertPoint(after_loop_block);
    builder.CreateCall(func_va_end, pointer_va_list);

    auto result = builder.CreateLoad(a_summation, "result");

    builder.CreateRet(result);

    if (verifyFunction(*function)) {
        std::cout << ": Error constructing function!\n" << std::endl;
        return 1;
    }

    module->print(llvm::outs(), nullptr);

    if (verifyModule(*module)) {
        std::cout << ": Error module!\n" << std::endl;
        return 1;
    }

    auto jit = llvm::orc::LLJITBuilder().create();


    if (jit) {
        auto thread_safe_module = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));
        auto error = jit->get()->addIRModule(std::move(thread_safe_module));
        assert(!error && "LLJIT can not add handle module.");
        llvm::orc::JITDylib &dylib = jit->get()->getMainJITDylib();
        const llvm::DataLayout &dataLayout = jit->get()->getDataLayout();
        auto prefix = dataLayout.getGlobalPrefix();
        auto generator = llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(prefix));
        dylib.addGenerator(std::move(generator));
        auto symbol = jit->get()->lookup("originalFunction");
        auto f = reinterpret_cast<double(*)(int, ...)>(symbol->getAddress());
        std::cout << "Evaluated to " << f(5, 19.1, 3.1, 1.4, 10.1, 11.1) << std::endl;
    } else {
        std::cout << "Error - LLJIT can not be initialized." << std::endl;
    }

    return 0;
}