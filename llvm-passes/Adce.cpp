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
    private:
        bool contains(SmallVector<Instruction*, 32> list, Instruction *I);
    };
}

bool Adce::contains(SmallVector<Instruction*, 32> list, Instruction *I)
{
    for (auto i : list)
    {
        if (i == I)
            return true;
    }
    return false;
}

bool Adce::runOnFunction(Function &F)
{
    if (!shouldInstrument(&F))
        return false;

    SmallVector<Instruction*, 32> worklist;
    SmallVector<Instruction*, 32> used;
    SmallVector<Instruction*, 32> unused;
    bool changed = false;
    auto iter = instructions(F).end();
    bool last_inst = true;

    // determin used instructions by UD chain
    do
    {
        iter--;
        Instruction *I = &*iter;
        LOG_LINE("instruction: " << *I);
        bool should_visit = last_inst;

        if (isa<CallInst>(I))
            should_visit = true;

        if (isa<StoreInst>(I))
            should_visit = true;

        if (LoadInst *L = dyn_cast<LoadInst>(I))
        {
            if (L->isVolatile())
                should_visit = true;
        }

        if (should_visit)
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
                        
                    if (contains(used, cast<Instruction>(U)))
                        continue;

                    LOG_LINE("instruction: " << I << " uses: " << *U);
                    worklist.push_back(cast<Instruction>(U));
                    used.push_back(cast<Instruction>(U));
                }
            }
        }

    } while (iter != instructions(F).begin());

    // pick out the unused instructions
    while (iter != instructions(F).end())
    {
        Instruction *I = &*iter;

        if (!contains(used, I))
        {
            LOG_LINE("removing: " << *I);
            unused.push_back(I);
        }
        iter++;
    }

    // remove unused
    for (auto *I : unused)
    {
        changed = true;
        I->eraseFromParent();
    }
    
    return changed;
}

char Adce::ID = 0;
RegisterPass<Adce> X("coco-adce", "CoCo Aggresive dead code elimination");