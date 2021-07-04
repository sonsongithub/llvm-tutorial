## Building a JIT: Per-function Lazy Compilation

1. Chapter 3 Introduction
1. Lazy Compilation
1. Full Code Listing

This tutorial is under active development. It is incomplete and details may change frequently. Nonetheless we invite you to try it out as it stands, and we welcome any feedback.

### Chapter 3 Introduction
Warning: This text is currently out of date due to ORC API updates.

The example code has been updated and can be used. The text will be updated once the API churn dies down.

Welcome to Chapter 3 of the “Building an ORC-based JIT in LLVM” tutorial. This chapter discusses lazy JITing and shows you how to enable it by adding an ORC CompileOnDemand layer the JIT from Chapter 2.

### Lazy Compilation
When we add a module to the KaleidoscopeJIT class from Chapter 2 it is immediately optimized, compiled and linked for us by the IRTransformLayer, IRCompileLayer and RTDyldObjectLinkingLayer respectively. This scheme, where all the work to make a Module executable is done up front, is simple to understand and its performance characteristics are easy to reason about. However, it will lead to very high startup times if the amount of code to be compiled is large, and may also do a lot of unnecessary compilation if only a few compiled functions are ever called at runtime. A truly “just-in-time” compiler should allow us to defer the compilation of any given function until the moment that function is first called, improving launch times and eliminating redundant work. In fact, the ORC APIs provide us with a layer to lazily compile LLVM IR: CompileOnDemandLayer.

The CompileOnDemandLayer class conforms to the layer interface described in Chapter 2, but its addModule method behaves quite differently from the layers we have seen so far: rather than doing any work up front, it just scans the Modules being added and arranges for each function in them to be compiled the first time it is called. To do this, the CompileOnDemandLayer creates two small utilities for each function that it scans: a stub and a compile callback. The stub is a pair of a function pointer (which will be pointed at the function’s implementation once the function has been compiled) and an indirect jump through the pointer. By fixing the address of the indirect jump for the lifetime of the program we can give the function a permanent “effective address”, one that can be safely used for indirection and function pointer comparison even if the function’s implementation is never compiled, or if it is compiled more than once (due to, for example, recompiling the function at a higher optimization level) and changes address. The second utility, the compile callback, represents a re-entry point from the program into the compiler that will trigger compilation and then execution of a function. By initializing the function’s stub to point at the function’s compile callback, we enable lazy compilation: The first attempted call to the function will follow the function pointer and trigger the compile callback instead. The compile callback will compile the function, update the function pointer for the stub, then execute the function. On all subsequent calls to the function, the function pointer will point at the already-compiled function, so there is no further overhead from the compiler. We will look at this process in more detail in the next chapter of this tutorial, but for now we’ll trust the CompileOnDemandLayer to set all the stubs and callbacks up for us. All we need to do is to add the CompileOnDemandLayer to the top of our stack and we’ll get the benefits of lazy compilation. We just need a few changes to the source:

```
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
```

```
class KaleidoscopeJIT {
private:
  std::unique_ptr<TargetMachine> TM;
  const DataLayout DL;
  RTDyldObjectLinkingLayer ObjectLayer;
  IRCompileLayer<decltype(ObjectLayer), SimpleCompiler> CompileLayer;

  using OptimizeFunction =
      std::function<std::shared_ptr<Module>(std::shared_ptr<Module>)>;

  IRTransformLayer<decltype(CompileLayer), OptimizeFunction> OptimizeLayer;

  std::unique_ptr<JITCompileCallbackManager> CompileCallbackManager;
  CompileOnDemandLayer<decltype(OptimizeLayer)> CODLayer;

public:
  using ModuleHandle = decltype(CODLayer)::ModuleHandleT;
First we need to include the CompileOnDemandLayer.h header, then add two new members: a std::unique_ptr<JITCompileCallbackManager> and a CompileOnDemandLayer, to our class. The CompileCallbackManager member is used by the CompileOnDemandLayer to create the compile callback needed for each function.

KaleidoscopeJIT()
    : TM(EngineBuilder().selectTarget()), DL(TM->createDataLayout()),
      ObjectLayer([]() { return std::make_shared<SectionMemoryManager>(); }),
      CompileLayer(ObjectLayer, SimpleCompiler(*TM)),
      OptimizeLayer(CompileLayer,
                    [this](std::shared_ptr<Module> M) {
                      return optimizeModule(std::move(M));
                    }),
      CompileCallbackManager(
          orc::createLocalCompileCallbackManager(TM->getTargetTriple(), 0)),
      CODLayer(OptimizeLayer,
               [this](Function &F) { return std::set<Function*>({&F}); },
               *CompileCallbackManager,
               orc::createLocalIndirectStubsManagerBuilder(
                 TM->getTargetTriple())) {
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}
```

Next we have to update our constructor to initialize the new members. To create an appropriate compile callback manager we use the createLocalCompileCallbackManager function, which takes a TargetMachine and a JITTargetAddress to call if it receives a request to compile an unknown function. In our simple JIT this situation is unlikely to come up, so we’ll cheat and just pass ‘0’ here. In a production quality JIT you could give the address of a function that throws an exception in order to unwind the JIT’d code’s stack.

Now we can construct our CompileOnDemandLayer. Following the pattern from previous layers we start by passing a reference to the next layer down in our stack – the OptimizeLayer. Next we need to supply a ‘partitioning function’: when a not-yet-compiled function is called, the CompileOnDemandLayer will call this function to ask us what we would like to compile. At a minimum we need to compile the function being called (given by the argument to the partitioning function), but we could also request that the CompileOnDemandLayer compile other functions that are unconditionally called (or highly likely to be called) from the function being called. For KaleidoscopeJIT we’ll keep it simple and just request compilation of the function that was called. Next we pass a reference to our CompileCallbackManager. Finally, we need to supply an “indirect stubs manager builder”: a utility function that constructs IndirectStubManagers, which are in turn used to build the stubs for the functions in each module. The CompileOnDemandLayer will call the indirect stub manager builder once for each call to addModule, and use the resulting indirect stubs manager to create stubs for all functions in all modules in the set. If/when the module set is removed from the JIT the indirect stubs manager will be deleted, freeing any memory allocated to the stubs. We supply this function by using the createLocalIndirectStubsManagerBuilder utility.

```
// ...
        if (auto Sym = CODLayer.findSymbol(Name, false))
// ...
return cantFail(CODLayer.addModule(std::move(Ms),
                                   std::move(Resolver)));
// ...

// ...
return CODLayer.findSymbol(MangledNameStream.str(), true);
// ...

// ...
CODLayer.removeModule(H);
// ...
```

Finally, we need to replace the references to OptimizeLayer in our addModule, findSymbol, and removeModule methods. With that, we’re up and running.

To be done: Chapter conclusion.**

### Full Code Listing
Here is the complete code listing for our running example with a CompileOnDemand layer added to enable lazy function-at-a-time compilation. To build this example, use:

### Compile
```
clang++ -g toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native` -O3 -o toy
```

### Run
```
./toy
```

Here is the code:

```
//===- KaleidoscopeJIT.h - A simple JIT for Kaleidoscope --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Contains a simple JIT definition for use in the kaleidoscope tutorials.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H
#define LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/TPCIndirectionUtils.h"
#include "llvm/ExecutionEngine/Orc/TargetProcessControl.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <memory>

namespace llvm {
namespace orc {

class KaleidoscopeJIT {
private:
  std::unique_ptr<TargetProcessControl> TPC;
  std::unique_ptr<ExecutionSession> ES;
  std::unique_ptr<TPCIndirectionUtils> TPCIU;

  DataLayout DL;
  MangleAndInterner Mangle;

  RTDyldObjectLinkingLayer ObjectLayer;
  IRCompileLayer CompileLayer;
  IRTransformLayer OptimizeLayer;
  CompileOnDemandLayer CODLayer;

  JITDylib &MainJD;

  static void handleLazyCallThroughError() {
    errs() << "LazyCallThrough error: Could not find function body";
    exit(1);
  }

public:
  KaleidoscopeJIT(std::unique_ptr<TargetProcessControl> TPC,
                  std::unique_ptr<ExecutionSession> ES,
                  std::unique_ptr<TPCIndirectionUtils> TPCIU,
                  JITTargetMachineBuilder JTMB, DataLayout DL)
      : TPC(std::move(TPC)), ES(std::move(ES)), TPCIU(std::move(TPCIU)),
        DL(std::move(DL)), Mangle(*this->ES, this->DL),
        ObjectLayer(*this->ES,
                    []() { return std::make_unique<SectionMemoryManager>(); }),
        CompileLayer(*this->ES, ObjectLayer,
                     std::make_unique<ConcurrentIRCompiler>(std::move(JTMB))),
        OptimizeLayer(*this->ES, CompileLayer, optimizeModule),
        CODLayer(*this->ES, OptimizeLayer,
                 this->TPCIU->getLazyCallThroughManager(),
                 [this] { return this->TPCIU->createIndirectStubsManager(); }),
        MainJD(this->ES->createBareJITDylib("<main>")) {
    MainJD.addGenerator(
        cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
            DL.getGlobalPrefix())));
  }

  ~KaleidoscopeJIT() {
    if (auto Err = ES->endSession())
      ES->reportError(std::move(Err));
    if (auto Err = TPCIU->cleanup())
      ES->reportError(std::move(Err));
  }

  static Expected<std::unique_ptr<KaleidoscopeJIT>> Create() {
    auto SSP = std::make_shared<SymbolStringPool>();
    auto TPC = SelfTargetProcessControl::Create(SSP);
    if (!TPC)
      return TPC.takeError();

    auto ES = std::make_unique<ExecutionSession>(std::move(SSP));

    auto TPCIU = TPCIndirectionUtils::Create(**TPC);
    if (!TPCIU)
      return TPCIU.takeError();

    (*TPCIU)->createLazyCallThroughManager(
        *ES, pointerToJITTargetAddress(&handleLazyCallThroughError));

    if (auto Err = setUpInProcessLCTMReentryViaTPCIU(**TPCIU))
      return std::move(Err);

    JITTargetMachineBuilder JTMB((*TPC)->getTargetTriple());

    auto DL = JTMB.getDefaultDataLayoutForTarget();
    if (!DL)
      return DL.takeError();

    return std::make_unique<KaleidoscopeJIT>(std::move(*TPC), std::move(ES),
                                             std::move(*TPCIU), std::move(JTMB),
                                             std::move(*DL));
  }

  const DataLayout &getDataLayout() const { return DL; }

  JITDylib &getMainJITDylib() { return MainJD; }

  Error addModule(ThreadSafeModule TSM, ResourceTrackerSP RT = nullptr) {
    if (!RT)
      RT = MainJD.getDefaultResourceTracker();

    return OptimizeLayer.add(RT, std::move(TSM));
  }

  Expected<JITEvaluatedSymbol> lookup(StringRef Name) {
    return ES->lookup({&MainJD}, Mangle(Name.str()));
  }

private:
  static Expected<ThreadSafeModule>
  optimizeModule(ThreadSafeModule TSM, const MaterializationResponsibility &R) {
    TSM.withModuleDo([](Module &M) {
      // Create a function pass manager.
      auto FPM = std::make_unique<legacy::FunctionPassManager>(&M);

      // Add some optimizations.
      FPM->add(createInstructionCombiningPass());
      FPM->add(createReassociatePass());
      FPM->add(createGVNPass());
      FPM->add(createCFGSimplificationPass());
      FPM->doInitialization();

      // Run the optimizations over all functions in the module being added to
      // the JIT.
      for (auto &F : M)
        FPM->run(F);
    });

    return std::move(TSM);
  }
};

} // end namespace orc
} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_KALEIDOSCOPEJIT_H
```
