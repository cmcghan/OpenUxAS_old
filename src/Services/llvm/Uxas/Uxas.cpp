//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include <cxxabi.h>
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "uxas"

//STATISTIC(UxasCounter, "Counts number of functions greeted");

namespace {
  // Uxas - The first implementation, without getAnalysisUsage.
  struct Uxas : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    Uxas() : ModulePass(ID) {}

    //-- return the class name given a method of the class. if F does
    //-- not belong to a class return an empty string as name.
    StringRef getClassName(Function &F)
    {
      if(F.getFunctionType()->getNumParams()) {
        if(auto pt = dyn_cast<PointerType>(F.getFunctionType()->getParamType(0))) {
          if(auto *st = dyn_cast<StructType>(pt->getElementType()))
            //-- the name always starts with "class.", so get rid of that
            return st->getName().substr(6);
        }
      }

      return "";
    }

    //-- return demangled name
    std::string demangle(const StringRef &mangled)
    {
      int status = 0;
      std::string res(abi::__cxa_demangle(mangled.data(), 0, 0, &status));
      return res.substr(0, res.find('['));
    }

    //-- print all messages that function F subscribes to
    void printSubs(Function &F)
    {
      StringRef className = getClassName(F);
      if(className == "") return;
      
      errs() << "---------------------------------------------------------------------------------\n";
      errs() << "service : " << className << " === SUBSCRIBES TO ===>\n";
      
      //-- iterate over all basic blocks
      for(const auto &bb : F.getBasicBlockList()) {
        //-- iterate over statements
        for(const auto &ins : bb.getInstList()) {
          if(auto *II = dyn_cast<const InvokeInst>(&ins)) {
            const Function *cf = II->getCalledFunction();
            if(!cf->getName().contains("addSubscriptionAddress")) continue;            

            //-- the first argument to a method call is always the
            //-- this pointer. so get the second argument.
            auto *arg = II->getArgOperand(1);

            if(auto *Const = dyn_cast<Constant>(arg))            
              errs() << '\t' << demangle(Const->getName()) << '\n';
            else if(auto *II = dyn_cast<InvokeInst>(arg)) {
              errs() << '\t' << demangle(II->getCalledFunction()->getName()) << "()\n";
            }
          }          
        }
      }
    }

    //-- print all messages that function F publishes
    void printPubs(Function &F)
    {
      StringRef className = getClassName(F);
      if(className != serviceName) return;

      //-- iterate over all basic blocks
      for(const auto &bb : F.getBasicBlockList()) {
        //-- iterate over statements
        for(const auto &ins : bb.getInstList()) {
          if(auto *II = dyn_cast<const InvokeInst>(&ins)) {
            const Function *cf = II->getCalledFunction();
            if(cf == NULL || !cf->getName().contains("sendSharedLmcpObjectBroadcastMessage")) continue;   

            //-- the first argument to a method call is always the
            //-- this pointer. so get the second argument.
            auto *arg = II->getArgOperand(1);
            auto *type = arg->getType();
            assert(type);
            errs() << *type << '\n';
          }          
        }
      }
    }

    //-- name of the service class being analyzed
    StringRef serviceName;

    //-- the top level method
    bool runOnModule(Module &M) override
    {
      //-- first get the name of the service class by looking for the
      //-- configure method and seeing which class it belongs to
      for(Function &F : M.getFunctionList()) {
        if(!F.getName().contains("configure")) continue;
        StringRef className = getClassName(F);
        if(className == "") continue;
        serviceName = className;
        errs() << "got service name " << serviceName << '\n';
        printSubs(F);
        break;
      }
      
      errs() << "---------------------------------------------------------------------------------\n";
      errs() << "service : " << serviceName << " === PUBLISHES TO ===>\n";
      
      //-- now extract the pub-sub information
      for(Function &F : M.getFunctionList()) {
        if(!F.getName().contains("configure")) printPubs(F);
      }      
      
      return false;
    }
  };
}

char Uxas::ID = 0;
static RegisterPass<Uxas> X("uxas", "Uxas Pass");
