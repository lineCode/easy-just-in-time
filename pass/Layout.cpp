#include <easy/attributes.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DebugInfo.h>

#define DEBUG_TYPE "easy-register-layout"
#include <llvm/Support/Debug.h>

#include <llvm/Support/raw_ostream.h>

#include "Utils.h"

using namespace llvm;

namespace easy {
  struct RegisterLayout : public ModulePass {
    static char ID;

    RegisterLayout()
      : ModulePass(ID) {};

    bool runOnModule(Module &M) override {

      SmallVector<Function*, 8> LayoutFunctions;
      collectLayouts(M, LayoutFunctions);

      if(LayoutFunctions.empty())
        return false;

      Function* Register = declareRegisterLayout(M);
      registerLayouts(LayoutFunctions, Register);

      return true;
    }

    static void collectLayouts(Module &M, SmallVectorImpl<Function*> &LayoutFunctions) {
      for(Function &F : M)
        if(F.getSection() == LAYOUT_SECTION)
          LayoutFunctions.push_back(&F);
    }

    static Function* GetLayoutId(Function* F) {
      // fragile!
      ReturnInst* Ret = cast<ReturnInst>(F->getEntryBlock().getTerminator());
      ConstantExpr* BitCast = cast<ConstantExpr>(Ret->getOperand(0));
      Function* LayoutInternalFunction = cast<Function>(BitCast->getOperand(0));
      return LayoutInternalFunction;
    }

    static void registerLayouts(SmallVectorImpl<Function*> &LayoutFunctions, Function* Register) {

      FunctionType* RegisterTy = Register->getFunctionType();
      Type* IdTy = RegisterTy->getParamType(0);
      Type* NTy = RegisterTy->getParamType(1);

      // register the layout info in a constructor
      Function* Ctor = GetCtor(*Register->getParent(), "register_layout");
      IRBuilder<> B(Ctor->getEntryBlock().getTerminator());

      for(Function *F : LayoutFunctions) {
        Function* LayoutIdFun = GetLayoutId(F);
        size_t N = F->getFunctionType()->getNumParams();
        Value* Id = B.CreatePointerCast(LayoutIdFun, IdTy);
        B.CreateCall(Register, {Id, ConstantInt::get(NTy, N, false)});
      }
    }

    static Function* declareRegisterLayout(Module &M) {
      StringRef Name = "easy_register_layout";
      if(Function* F = M.getFunction(Name))
        return F;

      LLVMContext &C = M.getContext();
      DataLayout const &DL = M.getDataLayout();

      Type* Void = Type::getVoidTy(C);
      Type* I8Ptr = Type::getInt8PtrTy(C);
      Type* SizeT = DL.getLargestLegalIntType(C);

      FunctionType* FTy =
          FunctionType::get(Void, {I8Ptr, SizeT}, false);
      return Function::Create(FTy, Function::ExternalLinkage, Name, &M);
    }
  };

  char RegisterLayout::ID = 0;

  llvm::Pass* createRegisterLayoutPass() {
    return new RegisterLayout();
  }
}
