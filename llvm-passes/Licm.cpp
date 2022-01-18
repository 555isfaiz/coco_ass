#define DEBUG_TYPE "ADCE"
#include <unistd.h>
#include "utils.h"

namespace
{
    class Licm : public LoopPass
    {
    public:
        static char ID;
        Licm() : LoopPass(ID) {}
        virtual bool runOnLoop(Loop *L, LPPassManager &LPM) override;
    };
}

bool Licm::runOnLoop(Loop *L, LPPassManager &LPM)
{
    bool changed = false;
    return changed;
}

char Licm::ID = 0;
RegisterPass<Licm> X("coco-licm", "CoCo Loop-invariant code motion");