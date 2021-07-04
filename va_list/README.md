## Build a function with Variable Length Arguments on LLVM in C++   

You can find the documents about this at [here](https://llvm.org/docs/LangRef.html#int-varargs). User has to use intrinsic functions and type of LLVM, `va_start`, `va_end`, `va_list` and `va_copy`. But, LLVM has not prepared direct API to handle these functions and types so user handle these functins basic APIs.

### Goal

Our goal is to build a function which sums all arguments(type is `double`.). But, due to the constraints of variable length arguments, the first argument of a function means the number of variables.(I think variable length arguments should not be used readily in so many cases in C/C++ becase it can not handle each type of variable directly.)

If this function is implemented in C++, the code would look like the following.

```
double summation_va(int count,...) {
    double value = 0;
    double summation = 0;
    va_list vl;
    va_start(vl, count);
    for (int i = 0; i < count; i++) {
        value = va_arg(vl, double);
        summation += value;
    }
    va_end(vl);
    return summation; 
}
```

One can compile this code to llvm IR using clang.

```
; Function Attrs: nounwind
declare void @llvm.va_start(i8*) #1

; Function Attrs: nounwind
declare void @llvm.va_end(i8*) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @_Z4sum2iz(i32, ...) #0 {
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca [1 x %struct.__va_list_tag], align 16
  store i32 %0, i32* %2, align 4
  store i32 0, i32* %3, align 4
  store i32 0, i32* %4, align 4
  store i32 0, i32* %5, align 4
  %7 = getelementptr inbounds [1 x %struct.__va_list_tag], [1 x %struct.__va_list_tag]* %6, i64 0, i64 0
  %8 = bitcast %struct.__va_list_tag* %7 to i8*
  call void @llvm.va_start(i8* %8)
  store i32 0, i32* %3, align 4
  br label %9

9:                                                ; preds = %35, %1
  %10 = load i32, i32* %3, align 4
  %11 = load i32, i32* %2, align 4
  %12 = icmp slt i32 %10, %11
  br i1 %12, label %13, label %38

13:                                               ; preds = %9
  %14 = getelementptr inbounds [1 x %struct.__va_list_tag], [1 x %struct.__va_list_tag]* %6, i64 0, i64 0
  %15 = getelementptr inbounds %struct.__va_list_tag, %struct.__va_list_tag* %14, i32 0, i32 0
  %16 = load i32, i32* %15, align 16
  %17 = icmp ule i32 %16, 40
  br i1 %17, label %18, label %24

18:                                               ; preds = %13
  %19 = getelementptr inbounds %struct.__va_list_tag, %struct.__va_list_tag* %14, i32 0, i32 3
  %20 = load i8*, i8** %19, align 16
  %21 = getelementptr i8, i8* %20, i32 %16
  %22 = bitcast i8* %21 to i32*
  %23 = add i32 %16, 8
  store i32 %23, i32* %15, align 16
  br label %29

24:                                               ; preds = %13
  %25 = getelementptr inbounds %struct.__va_list_tag, %struct.__va_list_tag* %14, i32 0, i32 2
  %26 = load i8*, i8** %25, align 8
  %27 = bitcast i8* %26 to i32*
  %28 = getelementptr i8, i8* %26, i32 8
  store i8* %28, i8** %25, align 8
  br label %29

29:                                               ; preds = %24, %18
  %30 = phi i32* [ %22, %18 ], [ %27, %24 ]
  %31 = load i32, i32* %30, align 4
  store i32 %31, i32* %4, align 4
  %32 = load i32, i32* %4, align 4
  %33 = load i32, i32* %5, align 4
  %34 = add nsw i32 %33, %32
  store i32 %34, i32* %5, align 4
  br label %35

35:                                               ; preds = %29
  %36 = load i32, i32* %3, align 4
  %37 = add nsw i32 %36, 1
  store i32 %37, i32* %3, align 4
  br label %9

38:                                               ; preds = %9
  %39 = getelementptr inbounds [1 x %struct.__va_list_tag], [1 x %struct.__va_list_tag]* %6, i64 0, i64 0
  %40 = bitcast %struct.__va_list_tag* %39 to i8*
  call void @llvm.va_end(i8* %40)
  %41 = load i32, i32* %5, align 4
  ret i32 %41
}
```

This code is incredibly long. I suppose, this IR means that the code to get multiple variable length arguments from va_list has been expanded into native code. I want to implement a variable length arguments function with shorter IR codes.

### `struct.va_list`

User must define `struct.va_list` before building a variable length arguments fuction. But, `va_list` structure depends on the environment. By above documents, 

```
// ; This struct is different for every platform. For most platforms,
// ; it is merely an i8*.
// %struct.va_list = type { i8* }

// ; For Unix x86_64 platforms, va_list is the following struct:
// ; %struct.va_list = type { i32, i32, i8*, i8* }
```

I choose `type { i32, i32, i8*, i8* }`.

In C++, user can define the struct `va_list` with the following codes.

```
std::vector<Type*> members;
members.push_back(Type::getInt32Ty(*context));
members.push_back(Type::getInt32Ty(*context));
members.push_back(Type::getInt8PtrTy(*context));
members.push_back(Type::getInt8PtrTy(*context));

StructType *const struct_va_list = StructType::create(*context, "struct.va_list");
struct_va_list->setBody(members);
```

User prepares `va_start` and `va_end` functions. These functions can be called via `Function` type.

```
Function *func_va_start = module->getFunction("llvm.va_start");
Function *func_va_end = module->getFunction("llvm.va_end");
```

And, `va_list` should be allocated and got the pointer to itself using `CreateBitCast`. Besides, a loop counter and buffer to be saved the summation are allocated here. `a_count` holds the value of the count and `a_summation` does the value of the summation.

```
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

// call va_start
builder.CreateCall(func_va_start, pointer_va_list);
```

The codes  to this point is implemented in the initialization block. And call `va_start` to start loading variable length arguments. In C++,

```
double summation_va(int count,...) {
    double value = 0;
    double summation = 0;
    va_list vl;
    va_start(vl, count);
```

### Loop

Next, build a loop block. Define a constant value to update loop counter as `step`. The block loads the current counter value from buffer which is allocated by `alloca` and increments and stores it to the buffer to update the loop counter.

```
BasicBlock *loop_block = BasicBlock::Create(*context, "loop", function);
builder.CreateBr(loop_block);
builder.SetInsertPoint(loop_block);

auto step = llvm::ConstantInt::get(Type::getInt64Ty(*context), 1);

// for loop
auto current_count = builder.CreateLoad(a_count);
auto updated_count = builder.CreateSub(current_count, step);
builder.CreateStore(updated_count, a_count);
```

After handling the loop counter, implment the main task. One is allowed to fetch arguments using LLVM API `VAArgInst` from variable length arguments. This API works as `va_arg` in C/C++, but you have strictly match the type of argument which you are fetching. `VAArgInst` can be cast into `Value` using `llvm::dyn_cast_or_null`(is like C++’s `dynamic_cast<>`).

In case that you want to check type of the value which is fetched from `va_list` strictly, you have to add a code to check and cast, and pass the original struct which includes types of arguments from callee.

```
auto *value_from_va_list = new llvm::VAArgInst(pointer_va_list, llvm::Type::getDoubleTy(*context), "value", loop_block);
llvm::Value *value = llvm::dyn_cast_or_null<Value>(value_from_va_list);

// summation
auto current_summation = builder.CreateLoad(a_summation, "current_summation");
auto updated_summation = builder.CreateFAdd(current_summation, value, "updated_summation");
builder.CreateStore(updated_summation, a_summation);
```

At the end of the loop block, it checks if the counter is greater than 0 or not. The result of comparation is saved to `loop_flag`. Finally, add a conditional branch to check whether the code returns to the beginning of the loop block or skips to the last block. In the last block, load and return the result of summation.

```
auto loop_flag = builder.CreateICmpSGT(updated_count, llvm::ConstantInt::get(Type::getInt64Ty(*context), 0), "loop_flag");

BasicBlock *after_loop_block = BasicBlock::Create(*context, "afterloop", function);

builder.CreateCondBr(loop_flag, loop_block, after_loop_block);

builder.SetInsertPoint(after_loop_block);
builder.CreateCall(func_va_end, pointer_va_list);

auto result = builder.CreateLoad(a_summation, "result");

builder.CreateRet(result);
```

Finally, the following LLVM IR codes are generate accoriding to the above codes. I suppose that the code is reasonable.

```
; ModuleID = 'originalModule'
source_filename = "originalModule"

%struct.va_list = type { i32, i32, i8*, i8* }

; Function Attrs: nounwind
declare void @llvm.va_start(i8*) #0

; Function Attrs: nounwind
declare void @llvm.va_end(i8*) #0

define double @originalFunction(i64 %0, ...) {
entry:
  %va_list = alloca %struct.va_list, align 8
  %count = alloca i64, align 8
  %summation = alloca double, align 8
  store i64 %0, i64* %count, align 4
  store double 0.000000e+00, double* %summation, align 8
  %"&va_list" = bitcast %struct.va_list* %va_list to i8*
  call void @llvm.va_start(i8* %"&va_list")
  br label %loop

loop:                                             ; preds = %loop, %entry
  %current_count = load i64, i64* %count, align 4
  %updated_count = sub i64 %current_count, 1
  store i64 %updated_count, i64* %count, align 4
  %value = va_arg i8* %"&va_list", double
  %current_summation = load double, double* %summation, align 8
  %updated_summation = fadd double %current_summation, %value
  store double %updated_summation, double* %summation, align 8
  %loop_flag = icmp sgt i64 %updated_count, 0
  br i1 %loop_flag, label %loop, label %afterloop

afterloop:                                        ; preds = %loop
  call void @llvm.va_end(i8* %"&va_list")
  %result = load double, double* %summation, align 8
  ret double %result
}

attributes #0 = { nounwind }
```

### Summary

When you want to use variable length arugments, you have to define `va_list`,`va_start` and `va_end`. But, LLVM API provides only the `va_arg` API as `VAArgInst` class, unlike the rest of the functions. I think that no one should use readily use functions with variable length arguments from point of the view of the type check safety, but it is very useful in some cases.

[source code](./variable_arguments.cpp)