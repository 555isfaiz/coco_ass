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
        return false;

    SmallVector<Instruction*, 32> worklist;
    SmallVector<Instruction*, 32> used;
    bool changed = false;
    auto iter = instructions(F).end();
    bool last_inst = true;
    do
    {
        iter--;
        Instruction *I = &*iter;
        LOG_LINE("instruction: " << *I);
        if (last_inst)
        {
            last_inst = false;
            worklist.push_back(I);
            used.push_back(I);
            while (!worklist.empty()) 
            {
                Instruction *I = worklist.pop_back_val();
                for (Use &U : I->operands()) 
                {
                    if (!isa<Instruction>(U.get()))
                        continue;
                        
                    LOG_LINE("instruction: " << I << " uses: " << *U);
                    worklist.push_back(cast<Instruction>(U));
                    used.push_back(cast<Instruction>(U));
                }
            }
        }
        // else
        // {

        // }

    } while (iter != instructions(F).begin());

    while (iter != instructions(F).end())
    {
        LOG_LINE("walking: " << *iter);
        Instruction *I = &*iter;
        LOG_LINE("walking: " << I);
        bool Iused = false;
        for (auto i : used)
        {
            if (i == I)
                Iused = true;
        }

        if (!Iused)
        {
            bool can_remove = true;
            if (LoadInst *L = dyn_cast<LoadInst>(I))
            {
                if (L->isVolatile())
                    can_remove = false;
            }

            if (StoreInst *S = dyn_cast<StoreInst>(I))
            {
                if (S->isVolatile())
                    can_remove = false;
            }
            
            if (!I->isSafeToRemove())
                can_remove = false;

            if (can_remove)
            {
                changed = true;
                LOG_LINE("removing: " << I);
                LOG_LINE("removing: " << *I);
                I->eraseFromParent();
            }
        }
        LOG_LINE("iter then: " << *iter);
        iter++;
        LOG_LINE("iter now: " << *iter);
    }
    
    return changed;
}

char Adce::ID = 0;
RegisterPass<Adce> X("coco-adce", "CoCo Aggresive dead code elimination");