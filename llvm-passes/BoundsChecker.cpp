#define DEBUG_TYPE "BoundsChecker"
#include "utils.h"

namespace {
    class CloneFuncBundle {
    public:
        Function *F;
        SmallVector<Argument*, 16> array_args;
        SmallVector<Argument*, 16> array_size_args;
        Function *newF;
        CloneFuncBundle(Function *F, SmallVector<Argument*, 16> array_args) : F(F), array_args(array_args) {}
        CloneFuncBundle (const CloneFuncBundle &obj)
        {
            F = obj.F;
            array_args = obj.array_args;
            array_size_args = obj.array_size_args;
        }

        inline void setNewF(Function *new_f)
        {
            this->newF = new_f;
        }

        void recalculateArrayArg()
        {
            SmallVector<Argument*, 16> new_array_args;
            int j = 0;
            for (auto &arg : F->args())
            {
                for (size_t i = 0; i < array_args.size(); i++)
                {
                    // contains
                    if (array_args[i] == &arg)
                    {
                        new_array_args.push_back(newF->getArg(j));
                        break;
                    }
                }
                j++;
            }
            array_args = new_array_args;
        }
    };

    class BoundsChecker : public ModulePass {
    public:
        static char ID;
        BoundsChecker() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) override;
    private:
        SmallVector<CloneFuncBundle, 16> replace_f;
        Function *CheckBoundF;
        Type *i32;
        void changeFunction(DenseMap<Value*, Value*> &map, IRBuilder<> B, CloneFuncBundle &C);
        Value* getPtrSize(Function *F, DenseMap<Value*, Value*> &map, IRBuilder<> B, Value *V, bool from_phi = false);
        Value* getGEPOffset(IRBuilder<> B, Instruction *I);
        bool processGEP(Function *F, IRBuilder<> B, DenseMap<Value*, Value*> &map, Instruction *I);
    };
}

void BoundsChecker::changeFunction(DenseMap<Value*, Value*> &map, IRBuilder<> B, CloneFuncBundle &C)
{
    // fill new arg types - start
    SmallVector<Type *, 16> new_types;

    for (size_t ii = 0; ii < C.array_args.size(); ii++)
        new_types.push_back(i32);
    // fill new arg types - end

    // create new function
    SmallVector<Argument *, 16> new_args;
    auto new_f = addParamsToFunction(C.F, makeArrayRef(new_types), new_args);
    C.setNewF(new_f);
    C.recalculateArrayArg();

    // set names of new func's args - start
    SmallVector<int, 16> array_arg_index;
    for (size_t ii = 0; ii < C.array_args.size(); ii++)
    {
        int index = 0;
        for (auto &aa : new_f->args())
        {
            if (&aa == C.array_args[ii])
            {
                array_arg_index.push_back(index);
                break;
            }
            index++;
        }
        new_args[ii]->setName(C.array_args[ii]->getName() + "_size");
        C.array_size_args.push_back(new_args[ii]);
        map[C.array_args[ii]] = new_args[ii];
    }
    // set names of new func's args - end

    // replace call site - start
    for (auto U : C.F->users())
    {
        SmallVector<Value *, 16> call_args;
        CallInst *call = dyn_cast<CallInst>(U);

        for (auto &a : call->args())
            call_args.push_back(a);

        for (size_t ii = 0; ii < array_arg_index.size(); ii++)
            call_args.push_back(getPtrSize(call->getParent()->getParent(), map, B, call->getOperand(array_arg_index[ii]), true));

        B.SetInsertPoint(call);
        /*auto new_call = */B.CreateCall(new_f, makeArrayRef(call_args));
        // ReplaceInstWithInst(call, new_call);
        // I dont know why but using ReplaceInstWithInst cause seg fault somehow

        call->eraseFromParent();
    }
    // replace call site - end

    for (auto &II : instructions(new_f))
    {
        if (II.getOpcode() == Instruction::GetElementPtr)
            processGEP(new_f, B, map, &II);
    }

    C.F->eraseFromParent();
    C.F = NULL;
}

Value* BoundsChecker::getPtrSize(Function *F, DenseMap<Value*, Value*> &map, IRBuilder<> B, Value *V, bool from_phi)
{
    if (map.find(V) != map.end())
        return map[V];
    
    if (isa<Instruction>(V))
    {
        Instruction *I = dyn_cast<Instruction>(V);
        Value *operand;
        if (from_phi)
            operand = I;
        else 
            operand = dyn_cast<Value>(&*(I->operands().begin()));

        if (isa<AllocaInst>(operand))
        {
            AllocaInst *i = dyn_cast<AllocaInst>(operand);
            map[i] = i->getOperand(0);
            return i->getOperand(0);
        }
        else if (isa<Argument>(operand))
        {
            // do nothing here, just mark the function.
            
            Argument *arg = dyn_cast<Argument>(operand);

            for (auto &clone : replace_f)
            {
                if (clone.F == F)
                {
                    bool has_arg = false;
                    for (auto a : clone.array_args)
                    {
                        if (a == arg)
                        {
                            has_arg = true;
                            break;
                        }
                    }

                    if (!has_arg)
                        clone.array_args.push_back(arg);

                    return NULL;
                }

                if (clone.newF == F)
                {
                    for (size_t ii = 0; ii < clone.array_args.size(); ii++)
                    {
                        if (clone.array_args[ii] == arg)
                            return clone.array_size_args[ii];
                    }
                    LOG_LINE("bug here");
                }
            }

            // walk caller here, see if we need to create more functions
            for (auto U : F->users())
            {
                CallInst *call = dyn_cast<CallInst>(U);
                getPtrSize(call->getParent()->getParent(), map, B, call);
            }

            CloneFuncBundle cl(F, {arg});
            replace_f.push_back(cl);
            return NULL;
        }
        else if (isa<PHINode>(operand))
        {
            PHINode *p = dyn_cast<PHINode>(operand);
            B.SetInsertPoint(p);
            PHINode *new_p = B.CreatePHI(i32, p->getNumIncomingValues());
            new_p->setName(p->getName() + "_size");

            for (unsigned int ii = 0; ii < p->getNumIncomingValues(); ii++)
            {
                if (p == p->getIncomingValue(ii))
                    new_p->addIncoming(new_p, p->getIncomingBlock(ii));
                else
                    new_p->addIncoming(getPtrSize(F, map, B, p->getIncomingValue(ii), true), p->getIncomingBlock(ii));
            }

            map[p] = new_p;
            return new_p;
        }
        else if (isa<GetElementPtrInst>(operand) || isa<LoadInst>(operand))
        {
            Instruction *i = dyn_cast<Instruction>(operand);
            return getPtrSize(F, map, B, dyn_cast<Value>(&*(i->operands().begin())));
        }
    }
    else if (isa<Constant>(V))
    {
        Constant *i = dyn_cast<Constant>(V);
        if (isa<ArrayType>(i->getOperand(0)->getType()))
        {
            ArrayType *tt = dyn_cast<ArrayType>(i->getOperand(0)->getType());
            auto ret = ConstantInt::get(i32, tt->getNumElements());
            map[i] = ret;
            return ret;
        }
    }

    llvm_unreachable("Unknown array type");
    return NULL;
}

Value* BoundsChecker::getGEPOffset(IRBuilder<> B, Instruction *I)
{
    if (auto ci = dyn_cast<ConstantInt>(I->getOperand(1)))
    {
        APInt offset = ci->getValue();
        // if it is an instruction, then it is allocated here, not from function arg
        if (isa<Instruction>(&*(I->operands().begin())))
        {
            Instruction *array_def = dyn_cast<Instruction>(&*(I->operands().begin()));
            if (array_def->getOpcode() == Instruction::GetElementPtr)
            {
                auto o = getGEPOffset(B, array_def);
                offset += dyn_cast<ConstantInt>(o)->getValue();
            }
        }

        return B.getInt(offset);
    }

    // offset is a variable or argument
    if (!isa<GetElementPtrInst>(&*(I->operands().begin())))
        return I->getOperand(1);

    Instruction *array_gep = dyn_cast<Instruction>(&*(I->operands().begin()));
    B.SetInsertPoint(I);

    return B.CreateAdd(I->getOperand(1), getGEPOffset(B, array_gep));
}

bool BoundsChecker::processGEP(Function *F, IRBuilder<> B, DenseMap<Value*, Value*> &map, Instruction *I)
{
    // a string
    if (isa<Constant>(I->getOperand(0)))
        return false;

    auto offset = getGEPOffset(B, I);
    Value *size;
    if (map.find(I->getOperand(0)) == map.end())
        size = getPtrSize(F, map, B, I);
    else
        size = map[I->getOperand(0)];

    if (size == NULL)
        return false;

    B.SetInsertPoint(I);
    B.CreateCall(CheckBoundF, { offset, size });
    return true;
}

bool BoundsChecker::runOnModule(Module &M) {
    bool changed = false;

    LLVMContext &C = M.getContext();
    Type *VoidTy = Type::getVoidTy(C);
    i32 = Type::getInt32Ty(C);

    auto FnCallee = M.getOrInsertFunction("__coco_check_bounds",
                                          VoidTy, i32, i32);
    CheckBoundF = cast<Function>(FnCallee.getCallee());

    DenseMap<Value*, Value*> arries;
    for (auto &F : M)
    {
        if (!shouldInstrument(&F))
            continue;

        IRBuilder<> B(&F.getEntryBlock());
        for (auto &I : instructions(F))
        {
            if (I.getOpcode() == Instruction::GetElementPtr)
                changed |= processGEP(&F, B, arries, &I);
        }
    }

    for (auto &clone : replace_f)
    {
        IRBuilder<> B(&clone.F->getEntryBlock());
        changed |= true;
        changeFunction(arries, B, clone);
    }
    return changed;
}

char BoundsChecker::ID = 0;
static RegisterPass<BoundsChecker> X("coco-boundscheck", "Coco bounds check implementation.");
