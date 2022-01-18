#define DEBUG_TYPE "ADCE"
#include <unistd.h>
#include "utils.h"

namespace
{
    class Adce : public FunctionPass
    {
    public:
        static char ID;
        Adce() : FunctionPass(ID) {}
        virtual bool runOnFunction(Function &F) override;
    };
}

bool Adce::runOnFunction(Function &F)
{
    if (!shouldInstrument(&F))
        return;

    SmallVector<Instruction*, 32> unused;
    bool changed = false;
    for (auto &II : instructions(F))
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

char Adce::ID = 0;
RegisterPass<Adce> X("coco-adce", "CoCo Aggresive dead code elimination");