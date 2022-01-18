#define DEBUG_TYPE "ADCE"
#include <unistd.h>
#include "utils.h"

namespace
{
    class Adce : public ModulePass
    {
    public:
        static char ID;
        Adce() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) override;
    private:
        bool eliminateDeadCode(Function &f);
    };
}

bool Adce::eliminateDeadCode(Function &f)
{
    SmallVector<Instruction*, 32> unused;
    bool changed = false;
    for (auto &II : instructions(f))
    {
        Instruction *I = &II;
        LOG_LINE("Visiting instruction " << I);

        if (LoadInst *L = dyn_cast<LoadInst>(I))
        {
            if (L->isVolatile())
                continue;
        }

        if (StoreInst *S = dyn_cast<StoreInst>(I))
        {
            if (S->isVolatile())
                continue;
        }

        if (!I->users().empty())
        {
            LOG_LINE("user not empty " << I);
            continue;
        }
        
        if (!I->isSafeToRemove())
        {
            LOG_LINE("not safe to remove" << I);
            continue;
        }

        LOG_LINE("removing " << I);
        unused.push_back(I);
    }

    for (auto &I : unused)
    {
        I->eraseFromParent();
        changed = true;
    }
    
    return changed;
}

bool Adce::runOnModule(Module &M)
{
    bool changed = false;
    for (Function &F : M) 
    {
        if (!shouldInstrument(&F))
            continue;

        LOG_LINE("Visiting function " << F.getName());
        changed |= eliminateDeadCode(F);
    }
    return changed;
}

char Adce::ID = 0;
RegisterPass<Adce> X("coco-adce", "CoCo Aggresive dead code elimination");