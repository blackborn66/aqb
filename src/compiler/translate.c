#include <stdio.h>
#include <string.h>

#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"
#include "types.h"
#include "errormsg.h"

typedef struct patchList_ *patchList;
struct patchList_
{
    Temp_label *head;
    patchList   tail;
};

struct Cx
{
    patchList trues;
    patchList falses;
    T_stm     stm;
};

struct Tr_accessList_
{
    Tr_access     head;
    Tr_accessList tail;
};

struct Tr_level_
{
    F_frame    frame;
    Temp_label name;
};

struct Tr_access_
{
    Tr_level   level;
    F_access   access;
};

struct Tr_exp_
{
    enum  {Tr_ex, Tr_nx, Tr_cx} kind;
    union {
        T_exp     ex;
        T_stm     nx;
        struct Cx cx;
    } u;
};

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail)
{
    Tr_expList el = checked_malloc(sizeof(*el));
    el->head = head;
    el->tail = tail;
    return el;
}

Tr_exp Tr_expListHead(Tr_expList el)
{
    return el->head;
}

Tr_expList Tr_expListTail(Tr_expList el)
{
    return el->tail;
}

static Tr_access Tr_Access(Tr_level level, F_access access)
{
    Tr_access a = checked_malloc(sizeof(*a));
    a->level = level;
    a->access = access;
    return a;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail)
{
    Tr_accessList l = checked_malloc(sizeof(*l));
    l->head = head;
    l->tail = tail;
    return l;
}

Tr_access Tr_accessListHead(Tr_accessList al)
{
    return al->head;
}

Tr_accessList Tr_accessListTail(Tr_accessList al)
{
    return al->tail;
}

static Tr_level global_level = NULL;

Tr_level Tr_global(void) 
{
    if (global_level == NULL) 
    {
        global_level = checked_malloc(sizeof(*global_level));
        global_level->frame  = NULL;
    }
    return global_level;
}

Tr_level Tr_newLevel(Temp_label name, Ty_tyList formalTys)
{
    Tr_level lv = checked_malloc(sizeof(*lv));

    lv->frame  = F_newFrame(name, formalTys);
    lv->name   = name;

    return lv;
}

Temp_label Tr_getLabel(Tr_level level)
{
    return level->name;
}

Tr_accessList Tr_formals(Tr_level level)
{
    F_accessList accessList = F_formals(level->frame);
    Tr_accessList a = NULL, last_a = NULL;
    for (; accessList; accessList = accessList->tail)
    {
        if (last_a == NULL)
        {
            a = Tr_AccessList(Tr_Access(level, accessList->head), NULL);
            last_a = a;
        }
        else
        {
            last_a->tail = Tr_AccessList(Tr_Access(level, accessList->head), NULL);
            last_a = last_a->tail;
        }
    }
    return a;
}

static string replace_type_suffix(string varname)
{
    int  l        = strlen(varname);
    char postfix  = varname[l-1];
    string res    = varname;
    string suffix = NULL;

    switch (postfix)
    {
        case '$':
            suffix = "s";
            break;
        case '%':
            suffix = "i";
            break;
        case '&':
            suffix = "l";
            break;
        case '!':
            suffix = "f";
            break;
        case '#':
            suffix = "d";
            break;
    }
    if (suffix)
    {
        res = String(res);
        res[l-1] = 0;
        res = strprintf("%s_%s", res, suffix);
    }
    return res;
}

/* Basic expressions */

static patchList PatchList(Temp_label *head, patchList tail)
{
    patchList p = checked_malloc(sizeof(*p));
    p->head = head;
    p->tail = tail;
    return p;
}

void doPatch(patchList tList, Temp_label label)
{
    for (; tList; tList = tList->tail)
        *(tList->head) = label;
}

static patchList joinPatch(patchList first, patchList second)
{
    patchList last;
    if (!first)
        return second;
    for (last = first; last->tail; last = last->tail);  /* go to end of list */
    last->tail = second;
    return first;
}

static Tr_exp Tr_Ex(T_exp ex)
{
    Tr_exp e = checked_malloc(sizeof(*e));
    e->kind = Tr_ex;
    e->u.ex = ex;
    return e;
}

static Tr_exp Tr_Nx(T_stm nx)
{
    Tr_exp e = checked_malloc(sizeof(*e));
    e->kind = Tr_nx;
    e->u.nx = nx;
    return e;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm)
{
    struct Cx cx;
    cx.trues  = trues;
    cx.falses = falses;
    cx.stm    = stm;

    Tr_exp e = checked_malloc(sizeof(*e));
    e->kind = Tr_cx;
    e->u.cx = cx;

    return e;
}

static T_exp unEx(Tr_exp e)
{
    if (!e)
        return NULL;
    switch (e->kind)
    {
        case Tr_ex:
            return e->u.ex;

        case Tr_cx:
        {
            Temp_temp r = Temp_newtemp(Ty_Integer());
            Temp_label t = Temp_newlabel(), f = Temp_newlabel();
            doPatch(e->u.cx.trues, t);
            doPatch(e->u.cx.falses, f);
            Ty_ty ty = Ty_Integer(); // FIXME: introduce Bool type
            return T_Eseq(T_Move(T_Temp(r, ty), T_ConstInt(1, ty), ty),
                    T_Eseq(e->u.cx.stm,
                      T_Eseq(T_Label(f),
                        T_Eseq(T_Move(T_Temp(r, ty), T_ConstInt(0, ty), ty),
                          T_Eseq(T_Label(t),
                                  T_Temp(r, ty), ty), ty), ty), ty), ty);
        }

        case Tr_nx:
            return T_Eseq(e->u.nx, T_ConstInt(0, Ty_Integer()), Ty_Integer());
    }
    return NULL;
}

static T_stm unNx(Tr_exp e)
{
    switch (e->kind)
    {
        case Tr_ex:
            return T_Exp(e->u.ex);
        case Tr_nx:
            return e->u.nx;
        case Tr_cx:
        #if 0
        {
            Temp_temp r = Temp_newtemp(Ty_Integer());
            Temp_label t = Temp_newlabel(), f = Temp_newlabel();
            doPatch(e->u.cx.trues, t);
            doPatch(e->u.cx.falses, f);
            return T_Seq(T_MoveS2(T_Temp(r), T_ConstS2(1)),
                    T_Seq(e->u.cx.stm,
                      T_Seq(T_Label(f),
                        T_Seq(T_MoveS2(T_Temp(r), T_ConstS2(0)),
                          T_Seq(T_Label(t),
                                  T_Exp(T_Temp(r)))))));
        }
        #endif
            assert(0); // FIXME
    }
    EM_error(0, "*** internal error: unknown Tr_exp kind in unNx");
    return NULL;
}

static struct Cx nullCx(void)
{
    struct Cx cx;
    cx.trues  = NULL;
    cx.falses = NULL;
    cx.stm    = NULL;
    return cx;
}

static struct Cx unCx(Tr_exp e)
{
    switch (e->kind)
    {
        case Tr_ex:
        {
            T_exp te         = unEx(e);
            T_stm s          = T_Cjump(T_ne, te, unEx(Tr_zeroExp(te->ty)), NULL, NULL);
            patchList trues  = PatchList(&s->u.CJUMP.ltrue, NULL);
            patchList falses = PatchList(&s->u.CJUMP.lfalse, NULL);
            Tr_exp cx        = Tr_Cx(trues, falses, s);
            return cx->u.cx;
        }
        case Tr_cx:
            return e->u.cx;
        case Tr_nx:
        {
            EM_error(0, "Unable to unpack nx to cx");
            assert(0);
            return nullCx();
        }
    }
    EM_error(0, "unCx: internal error, unknown expression kind %d", e->kind);
    assert(0);
    return nullCx();
}

/* Fragments */

static F_fragList fragList = NULL;

F_fragList Tr_getResult(void) {
  return fragList;
}

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals, Tr_access ret_access)
{
    T_stm stm = unNx(body);

    if (ret_access)
    {
        T_exp ret_exp = unEx(Tr_simpleVar(ret_access));
        Ty_ty ty_ret = F_accessType(ret_access->access);
        stm = T_Seq(T_Move(ret_exp, unEx(Tr_zeroExp(ty_ret)),  ty_ret),
                T_Seq(stm,
                  T_Move(T_Temp(F_RV(), ty_ret), ret_exp, ty_ret)));
    }
    F_frag frag = F_ProcFrag(stm, level->frame);
    fragList    = F_FragList(frag, fragList);
}

Tr_access Tr_allocVar(Tr_level level, string name, Ty_ty ty)
{
    if (!level->frame) // global var?
    {
        Temp_label label = Temp_namedlabel(replace_type_suffix(name));

        F_frag frag = F_FillFrag(label, Ty_size(ty));
        fragList    = F_FragList(frag, fragList);

        return Tr_Access(level, F_allocGlobal(label, ty));
    }

    return Tr_Access(level, F_allocLocal(level->frame, ty));
}

/* Tree Expressions */

Tr_exp Tr_zeroExp(Ty_ty ty)
{
    switch (ty->kind)
    {
        case Ty_bool:
            return Tr_Ex(T_ConstBool(FALSE, ty));
        case Ty_integer:
        case Ty_long:
            return Tr_Ex(T_ConstInt(0, ty));
        case Ty_single:
            return Tr_Ex(T_ConstFloat(0, ty));
        default:
            EM_error(0, "*** translate.c:Tr_zeroExp: internal error");
            assert(0);
    }
}

Tr_exp Tr_oneExp(Ty_ty ty) {
    switch (ty->kind)
    {
        case Ty_bool:
            return Tr_Ex(T_ConstBool(TRUE, ty));
        case Ty_integer:
        case Ty_long:
            return Tr_Ex(T_ConstInt(1, ty));
        default:
            EM_error(0, "*** translate.c:Tr_oneExp: internal error");
            assert(0);
    }
}

#if 0
Tr_exp Tr_nullCx()
{
    // return Tr_Cx(NULL, NULL, T_Exp(T_ConstS2(0)));
    return Tr_Cx(NULL, NULL, NULL);
}

#endif
Tr_exp Tr_nopNx()
{
    return Tr_Nx(T_Nop());
}

Tr_exp Tr_boolExp(bool b, Ty_ty ty)
{
    return Tr_Ex(T_ConstBool(b, ty));
}

Tr_exp Tr_intExp(int i, Ty_ty ty)
{
    return Tr_Ex(T_ConstInt(i, ty));
}

Tr_exp Tr_floatExp(double f, Ty_ty ty)
{
    return Tr_Ex(T_ConstFloat(f, ty));
}

Tr_exp Tr_stringExp(string str)
{
    Temp_label strpos = Temp_newlabel();
    F_frag frag = F_StringFrag(strpos, str);
    fragList = F_FragList(frag, fragList);

    return Tr_Ex(T_Heap(strpos, Ty_String()));
}

Tr_exp Tr_simpleVar(Tr_access a)
{
    return Tr_Ex(F_Exp(a->access));
}

#if 0
Tr_exp Tr_fieldVar(Tr_exp var, int fieldIndex, Tr_level l)
{
    return Tr_Ex(T_Mem(
            T_Binop(T_plus, unEx(var),
              T_Binop(T_mul, T_Const(fieldIndex), T_Const(F_wordSize)))));
}

Tr_exp Tr_subscriptVar(Tr_exp var, Tr_exp sub, Tr_level l) {
    return Tr_Ex(T_Mem(
            T_Binop(T_plus, unEx(var),
              T_Binop(T_mul, unEx(sub), T_Const(F_wordSize)))));
}
#endif

Tr_exp Tr_arOpExp(A_oper o, Tr_exp left, Tr_exp right, Ty_ty ty)
{
    T_binOp op;
    switch (o)
    {
        case A_addOp   : op = T_plus;     break;
        case A_subOp   : op = T_minus;    break;
        case A_mulOp   : op = T_mul;      break;
        case A_divOp   : op = T_div;      break;
        case A_xorOp   : op = T_xor;      break;
        case A_eqvOp   : op = T_eqv;      break;
        case A_impOp   : op = T_imp;      break;
        case A_negOp   : op = T_neg;      break;
        case A_notOp   : op = T_not;      break;
        case A_andOp   : op = T_and;      break;
        case A_orOp    : op = T_or;       break;
        case A_expOp   : op = T_power;    break;
        case A_intDivOp: op = T_intDiv;   break;
        case A_modOp   : op = T_mod;      break;
        default:
            EM_error(0, "*** translate.c: internal error: unhandled arithmetic operation: %d", o);
            assert(0);
    }

    return Tr_Ex(T_Binop(op, unEx(left), unEx(right), ty));
}

Tr_exp Tr_boolOpExp(A_oper o, Tr_exp left, Tr_exp right, Ty_ty ty)
{
    struct Cx leftcx = unCx(left);

    switch (o)
    {
        case A_notOp:
            // simply switch true and false around
            return Tr_Cx(leftcx.falses, leftcx.trues, leftcx.stm);

        case A_orOp:
        {
            Temp_label z = Temp_newlabel();
            struct Cx rightcx = unCx(right);

            T_stm s1 = T_Seq(leftcx.stm,
                        T_Seq(T_Label(z),
                         rightcx.stm));
            doPatch(leftcx.falses, z);
            return Tr_Cx(joinPatch(leftcx.trues, rightcx.trues), rightcx.falses, s1);
        }

        case A_andOp:
        {
            Temp_label z = Temp_newlabel();
            struct Cx rightcx = unCx(right);

            T_stm s1 = T_Seq(leftcx.stm,
                        T_Seq(T_Label(z),
                         rightcx.stm));
            doPatch(leftcx.trues, z);
            return Tr_Cx(rightcx.trues, joinPatch(leftcx.falses, rightcx.falses), s1);
        }

        default:
            EM_error(0, "*** translate.c: internal error: unhandled boolean operation: %d", o);
            assert(0);
    }
    return Tr_Nx(T_Nop());
}

Tr_exp Tr_condOpExp(A_oper o, Tr_exp left, Tr_exp right, Ty_ty ty)
{
    T_binOp op = T_eq;
    switch (o)
    {
        case A_eqOp:  op = T_eq; break;
        case A_neqOp: op = T_ne; break;
        case A_ltOp:  op = T_lt; break;
        case A_leOp:  op = T_le; break;
        case A_gtOp:  op = T_gt; break;
        case A_geOp:  op = T_ge; break;
        default:
            EM_error(0, "*** translate.c: internal error: unhandled conditional operation: %d", o);
            assert(0);
    }

    T_stm s = T_Cjump(op, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&s->u.CJUMP.ltrue, NULL);
    patchList falses = PatchList(&s->u.CJUMP.lfalse, NULL);
    return Tr_Cx(trues, falses, s);
}

Tr_exp Tr_assignExp(Tr_exp var, Tr_exp exp, Ty_ty ty) {
    return Tr_Nx(T_Move(unEx(var), unEx(exp), ty));
}

Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee)
{
    Temp_label t = Temp_newlabel();
    Temp_label f = Temp_newlabel();
    Temp_label m = Temp_newlabel();

    /* convert test to cx */
    if (test->kind == Tr_ex)
    {
        struct Cx testcx = unCx(test);
        test = Tr_Cx(testcx.trues, testcx.falses, testcx.stm);
    }
    else if (test->kind == Tr_nx)
    {
        EM_error(0, "if test exp cannot be nx");
    }

    doPatch(test->u.cx.trues, t);
    doPatch(test->u.cx.falses, f);

    T_stm s = T_Seq(unCx(test).stm,
                T_Seq(T_Label(t),
                  T_Seq(unNx(then),
                    T_Seq(T_Jump(m),
                      T_Seq(T_Label(f),
                        T_Seq(unNx(elsee),
                          T_Label(m)))))));

    return Tr_Nx(s);
}

#if 0
Tr_exp Tr_whileExp(Tr_exp exp, Tr_exp body, Temp_label breaklbl) {
  Temp_label test = Temp_newlabel();
  Temp_label done = breaklbl;
  Temp_label loopstart = Temp_newlabel();

  T_stm s = T_Seq(T_Label(test),
              T_Seq(T_Cjump(T_ne, unEx(exp), T_Const(0), loopstart, done),
                T_Seq(T_Label(loopstart),
                  T_Seq(unNx(body),
                    T_Seq(T_Jump(T_Name(test), Temp_LabelList(test, NULL)),
                      T_Label(done))))));

  return Tr_Nx(s);
}

Tr_exp Tr_forExp(Tr_access i, Tr_level lv, Tr_exp explo, Tr_exp exphi, Tr_exp body, Temp_label breaklbl) {
  Temp_label test = Temp_newlabel();
  Temp_label loopstart = Temp_newlabel();
  Temp_label done = breaklbl;
  Temp_temp limit = Temp_newtemp();
  T_exp vari = unEx(Tr_simpleVar(i, lv));

  T_stm s = T_Seq(T_Move(vari, unEx(explo)),
              T_Seq(T_Label(test),
                T_Seq(T_Move(T_Temp(limit), unEx(exphi)),
                  T_Seq(T_Cjump(T_le, vari, T_Temp(limit), loopstart, done),
                    T_Seq(T_Label(loopstart),
                      T_Seq(unNx(body),
                        T_Seq(T_Move(vari, T_Binop(T_plus, vari, T_Const(1))),
                          T_Seq(T_Jump(T_Name(test), Temp_LabelList(test, NULL)),
                            T_Label(done)))))))));
  return Tr_Nx(s);
}
#endif

Tr_exp Tr_forExp(Tr_access loopVar, Ty_ty loopVarType, Tr_exp exp_from, Tr_exp exp_to, Tr_exp exp_step, Tr_exp body, Temp_label breaklbl)
{
    Temp_label test      = Temp_newlabel();
    Temp_label loopstart = Temp_newlabel();
    Temp_label done      = breaklbl;

    Temp_temp limit      = Temp_newtemp(loopVarType);
    T_exp loopv          = unEx(Tr_simpleVar(loopVar));

    T_stm initStm        = T_Move(loopv, unEx(exp_from), loopVarType);
    T_stm incStm         = T_Move(loopv, T_Binop(T_plus, loopv, unEx(exp_step), Ty_Integer()), loopVarType);
    T_stm limitStm       = T_Move(T_Temp(limit, loopVarType), unEx(exp_to), loopVarType);

    T_stm s = T_Seq(initStm,
                T_Seq(T_Label(test),
                  T_Seq(limitStm,
                    T_Seq(T_Cjump(T_le, loopv, T_Temp(limit, loopVarType), loopstart, done),
                      T_Seq(T_Label(loopstart),
                        T_Seq(unNx(body),
                          T_Seq(incStm,
                            T_Seq(T_Jump(test),
                              T_Label(done)))))))));
    return Tr_Nx(s);
}

#if 0
Tr_exp Tr_breakExp(Temp_label breaklbl) {
  return Tr_Nx(T_Jump(T_Name(breaklbl), Temp_LabelList(breaklbl, NULL)));
}

Tr_exp Tr_arrayExp(Tr_exp init, Tr_exp size) {
  return Tr_Ex(F_externalCall("initArray",
                T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_recordExp(Tr_expList el, int fieldCount) {
  /* Allocation */
  Temp_temp r = Temp_newtemp();
  T_stm alloc = T_Move(T_Temp(r),
                  F_externalCall("allocRecord",
                    T_ExpList(T_Const(fieldCount * F_wordSize), NULL)));

  /* Init fields */
  T_stm init = NULL, current = NULL;
  int fieldIndex = 0;
  for (; el; el = el->tail, ++fieldIndex) {
    if (init == NULL) {
      init = current = T_Seq(T_Move(T_Mem(T_Binop(T_plus,
                              T_Temp(r),
                              T_Const((fieldCount - 1 - fieldIndex) * F_wordSize))),
                                unEx(el->head)),
                        T_Exp(T_Const(0)));         /* statements in seq cannot be null */
    } else {
      current->u.SEQ.right = T_Seq(T_Move(T_Mem(T_Binop(T_plus,
                                    T_Temp(r),
                                    T_Const((fieldCount - 1 - fieldIndex) * F_wordSize))),
                                      unEx(el->head)),
                              T_Exp(T_Const(0)));   /* statements in seq cannot be null */
      current = current->u.SEQ.right;
    }
  }

  return Tr_Ex(T_Eseq(
            T_Seq(alloc, init),
              T_Temp(r)));
}
#endif

Tr_exp Tr_seqExp(Tr_expList el)
{
    T_stm stm = NULL;
    for (; el; el = el->tail)
    {
        if (stm)
        {
            stm = T_Seq(stm, unNx(el->head));
        }
        else
        {
            stm = unNx(el->head);
        }
    }
    return Tr_Nx(stm);
}

Tr_exp Tr_callExp(Tr_level funclv, Tr_level lv,
                  Temp_label name, Tr_expList rawel, Ty_ty ty)
{
    // cdecl calling convention (right-to-left order)
    T_expList el = NULL;
    for (; rawel; rawel = rawel->tail)
    {
        el = T_ExpList(unEx(rawel->head), el);
    }

    return Tr_Ex(T_CallF(name, el, ty));
}


Tr_exp Tr_castExp(Tr_exp exp, Ty_ty from_ty, Ty_ty to_ty)
{
    switch (from_ty->kind)
    {
        case Ty_bool:
            switch (to_ty->kind)
            {
                case Ty_bool:
                    return exp;
                case Ty_integer:
                case Ty_single:
                case Ty_long:
                    return Tr_Ex(T_Cast(unEx(exp), from_ty, to_ty));
                default:
                    EM_error(0, "*** translate.c:Tr_castExp: internal error: unknown type kind %d", to_ty->kind);
                    assert(0);
            }
            break;
        case Ty_integer:
            switch (to_ty->kind)
            {
                case Ty_integer:
                    return exp;
                case Ty_bool:
                case Ty_long:
                case Ty_single:
                    return Tr_Ex(T_Cast(unEx(exp), from_ty, to_ty));
                default:
                    EM_error(0, "*** translate.c:Tr_castExp: internal error: unknown type kind %d", to_ty->kind);
                    assert(0);
            }
            break;
        case Ty_long:
            switch (to_ty->kind)
            {
                case Ty_bool:
                case Ty_integer:
                case Ty_single:
                    return Tr_Ex(T_Cast(unEx(exp), from_ty, to_ty));
                case Ty_long:
                    return exp;
                default:
                    EM_error(0, "*** translate.c:Tr_castExp: internal error: unknown type kind %d", to_ty->kind);
                    assert(0);
            }
            break;
        case Ty_single:
            switch (to_ty->kind)
            {
                case Ty_bool:
                case Ty_integer:
                case Ty_long:
                    return Tr_Ex(T_Cast(unEx(exp), from_ty, to_ty));
                case Ty_single:
                    return exp;
                default:
                    EM_error(0, "*** translate.c:Tr_castExp: internal error: unknown type kind %d", to_ty->kind);
                    assert(0);
            }
            break;
        default:
            EM_error(0, "*** translate.c:Tr_castExp: internal error: unknown type kind %d", from_ty->kind);
            assert(0);
    }
    return NULL;
}

static void indent(FILE *out, int d)
{
    int i;
    for (i = 0; i <= d; i++)
        fprintf(out, " ");
}

void Tr_printExp(FILE *out, Tr_exp exp, int d)
{
    switch (exp->kind)
    {
        case Tr_ex:
            indent(out, d); fprintf(out, "ex(");
            printExp(out, exp->u.ex, d+1);
            fprintf(out,")\n");
            break;
        case Tr_nx:
            indent(out, d); fprintf(out, "nx(\n");
            printStm(out, exp->u.nx, d+1);
            fprintf(out,")\n");
            break;
        case Tr_cx:
            indent(out, d); fprintf(out, "cx(");
            printStm(out, exp->u.cx.stm, d+1);
            fprintf(out,")\n");
            break;
    }
}
