#define DEBUG_TYPE "ADCE"
#include "utils.h"

namespace
{
    class Licm : public LoopPass
    {
    public:
        static char ID;
        Licm() : LoopPass(ID) {}
        virtual bool runOnLoop(Loop *L, LPPassManager &LPM) override;
    private:
        void hoist(Loop *L, SmallVector<Instruction*, 32> insts);
    };
}

void Licm::hoist(Loop *L, SmallVector<Instruction*, 32> insts)
{
    Instruction *last_br = &*(--(L->getLoopPreheader()->getInstList().end()));
    for (auto I : insts)
        I->moveBefore(last_br);
}

bool Licm::runOnLoop(Loop *L, LPPassManager &LPM)
{
    bool changed = false;
    SmallVector<Instruction*, 32> unreache_def;
    SmallVector<Instruction*, 32> to_remove;
    SmallVector<Instruction*, 32> worklist;

    for (BasicBlock *BB : L->blocks())
    {
        for (Instruction &II : *BB)
        {
            Instruction *I = &II;
            if (I->isTerminator())
                continue;

            if (LoadInst *L = dyn_cast<LoadInst>(I))
            {
                if (L->isVolatile())
                    continue;
            }

            bool use_def_inloop = false;
            for (Use &U : I->operands())
            {
                if (!isa<Instruction>(U.get()))
                    continue;

                if (LoadInst *L = dyn_cast<LoadInst>(I))
                {
                    if (L->isVolatile())
                        use_def_inloop = true;
                }

                if (L->contains(cast<Instruction>(U)->getParent()))
                    use_def_inloop = true;
                LOG_LINE("instruction: " << I << " uses: " << *U);
            }

            if (!use_def_inloop)
            {
                LOG_LINE("unreach def: " << *I);
                unreache_def.push_back(I);
                to_remove.push_back(I);
                changed = true;
            }
        }
    }

    hoist(L, to_remove);
    return changed;
}

char Licm::ID = 0;
RegisterPass<Licm> X("coco-licm", "CoCo Loop-invariant code motion");