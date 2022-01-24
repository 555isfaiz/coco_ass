#define DEBUG_TYPE "ADCE"
#include "utils.h"

namespace
{
    class ConstantPropagation : public FunctionPass
    {
    public:
        static char ID;
        ConstantPropagation() : FunctionPass(ID) {}
        virtual bool runOnFunction(Function &F) override;
    private:
        void convertInt(IRBuilder<> builder, DenseMap<StringRef, ConstantInt*> &map, Instruction *I);
        // void convertFloat(IRBuilder<> builder, DenseMap<StringRef, ConstantFP*> &map, Instruction *I);
        bool replace(Instruction *I, StringRef name, ConstantInt* cons); 
    };
}

void ConstantPropagation::convertInt(IRBuilder<> builder, DenseMap<StringRef, ConstantInt*> &map, Instruction *I)
{
    if (I->isBinaryOp())
    {
        APInt val = cast<ConstantInt>(&*(I->operands().begin()))->getValue();
            bool first = true;
            for (auto &U : I->operands())
            {
                if (first)
                {
                    first = false;
                    continue;
                }

                if (U == NULL)
                    continue;

                switch (I->getOpcode())
                {
                    case Instruction::Add:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val += c->getValue();
                        else if (auto i = dyn_cast<Instruction>(U))
                            val += map[i->getName()]->getValue();
                        break;
                    }

                    case Instruction::Sub:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val -= c->getValue();
                        else if (auto i = dyn_cast<Instruction>(U))
                            val -= map[i->getName()]->getValue();
                        break;
                    }

                    case Instruction::Mul:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val *= c->getValue();
                        else if (auto i = dyn_cast<Instruction>(U))
                            val *= map[i->getName()]->getValue();
                        break;
                    }

                    case Instruction::SDiv:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val = val.sdiv(c->getValue());
                        else if (auto i = dyn_cast<Instruction>(U))
                            val = val.sdiv(map[i->getName()]->getValue());
                        break;
                    }

                    case Instruction::Shl:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val <<= c->getValue();
                        else if (auto i = dyn_cast<Instruction>(U))
                            val <<= map[i->getName()]->getValue();
                        break;
                    }

                    case Instruction::AShr:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val = val.ashr(c->getValue());
                        else if (auto i = dyn_cast<Instruction>(U))
                            val = val.ashr(map[i->getName()]->getValue());
                        break;
                    }

                    case Instruction::LShr:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val = val.lshr(c->getValue());
                        else if (auto i = dyn_cast<Instruction>(U))
                            val = val.lshr(map[i->getName()]->getValue());
                        break;
                    }

                    case Instruction::And:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val &= c->getValue();
                        else if (auto i = dyn_cast<Instruction>(U))
                            val &= map[i->getName()]->getValue();
                        break;
                    }

                    case Instruction::Xor:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val ^= c->getValue();
                        else if (auto i = dyn_cast<Instruction>(U))
                            val ^= map[i->getName()]->getValue();
                        break;
                    }

                    case Instruction::Or:
                    {
                        if (auto c = dyn_cast<ConstantInt>(U))
                            val |= c->getValue();
                        else if (auto i = dyn_cast<Instruction>(U))
                            val |= map[i->getName()]->getValue();
                        break;
                    }
                    
                    default:
                        break;
                }
            }
            map[I->getName()] = builder.getInt(val);
            LOG_LINE("converted: " << I->getName() << " value: " << *map[I->getName()]);
    }
    else 
    {
        switch (I->getOpcode())
        {
            case Instruction::Store:
            case Instruction::Load:
            {
                map[I->getName()] = builder.getInt(cast<ConstantInt>(&*(I->operands().begin()))->getValue());
                break;
            }
            
            default:
                break;
        }
    }
}

// void ConstantPropagation::convertFloat(IRBuilder<> builder, DenseMap<StringRef, ConstantFP*> &map, Instruction *I)
// {
//     switch (I->getOpcode())
//     {
//         case Instruction::FAdd:
//         {
//             APFloat val(0.0f);
//             for (auto &U : I->operands())
//             {
//                 if (auto i = cast<Instruction>(U))
//                     val += map[i->getName()]->getValueAPF();
//                 else if (auto c = cast<ConstantFP>(U))
//                     val += c->getValueAPF();
//             }
//             map[I->getName()] = builder.getFloatTy(val);
//             break;
//         }
        
//         default:
//             break;
//     }
// }

bool ConstantPropagation::replace(Instruction *I, StringRef name, ConstantInt* cons)
{
    bool replaced = false;
    if (isa<LoadInst>(I))
        return replaced;

    if (cons == NULL)
        return replaced;

    int count = 0;
    for (auto &u : I->operands())
    {
        count++;

        // dont replace the second operand of "store"
        if (count == 2 && isa<StoreInst>(I))
            continue;

        if (u.get() == NULL)
            continue;

        if (u.get()->getName().equals(name))
        {
            I->replaceUsesOfWith(u, cons);
            LOG_LINE("replaced: " << *I);
        }
    }
    
    return replaced;
}

bool ConstantPropagation::runOnFunction(Function &F)
{
    bool changed = false;
    IRBuilder<> B(&F.getEntryBlock());
    DenseMap<StringRef, ConstantInt*> int_constants;
    DenseMap<StringRef, ConstantFP*> float_constants;
    for (BasicBlock &BB : F) 
    {
        for (Instruction &II : BB) 
        {
            bool can_convert = true;
            for (auto &U : II.operands())
            {
                if (U == NULL)
                    continue;
                // not a constant and can't be found in map
                if (!isa<Constant>(U)
                    && int_constants.find(U->getName()) == int_constants.end() 
                    && float_constants.find(U->getName()) == float_constants.end())
                    can_convert = false;
            }

            if (can_convert)
            {
                LOG_LINE("convert: " << II.getName());
                if (II.getType()->isIntegerTy())
                    convertInt(B, int_constants, &II);
                // else if (II.getType()->isFloatTy())
                //     convertFloat(B, float_constants, &II);

                bool replaced;
                for (auto user : II.users())
                    replaced = replace(dyn_cast<Instruction>(user), II.getName(), int_constants[II.getName()]);

                if (replaced)
                    changed = true;
            }
        }
    }

    return changed;
}

char ConstantPropagation::ID = 0;
RegisterPass<ConstantPropagation> X("coco-constprop", "CoCo Constant propagation");