from util import ASTTransformer, NodeError
from ast import Type, Operator, VarDef, ArrayDef, Assignment, Modification, \
        If, Block, VarUse, BinaryOp, IntConst, Return, For, While, UnaryOp, BoolConst


class Desugarer(ASTTransformer):
    def __init__(self):
        self.varcache_stack = [{}]

    def makevar(self, name):
        # Generate a variable starting with an underscore (which is not allowed
        # in the language itself, so should be unique). To make the variable
        # unique, add a counter value if there are multiple generated variables
        # of the same name within the current scope.
        # A variable can be tagged as 'ssa' which means it is only assigned once
        # at its definition.
        name = '_' + name
        varcache = self.varcache_stack[-1]
        occurrences = varcache.setdefault(name, 0)
        varcache[name] += 1
        return name if not occurrences else name + str(occurrences + 1)

    def visitFunDef(self, node):
        self.varcache_stack.append({})
        self.visit_children(node)
        self.varcache_stack.pop()

    def visitModification(self, m):
        # from: lhs op= rhs
        # to:   lhs = lhs op rhs
        self.visit_children(m)
        return Assignment(m.ref, BinaryOp(m.ref, m.op, m.value)).at(m)
    
    def visitFor(self, f):
        self.visit_children(f)
        if f.iType != Type.get('int'):
            raise NodeError(f, 'Type mismatch: expected int, got %s', f.iType)

        # int i = 0;
        # while (i < ?) {
        #     for body
        #     i++
        # }
        varDefi = VarDef(f.iType, f.name, f.initExpr)
        cond = BinaryOp(VarUse(f.name), Operator('<'), f.topExpr)
        inc = Assignment(VarUse(f.name), BinaryOp(VarUse(f.name), Operator('+'), IntConst(1)))
        f.body.statements.append(inc)
        whileLoop = While(cond, f.body)
        whileLoop.isfor = True
        return Block([varDefi, whileLoop]).at(f)
