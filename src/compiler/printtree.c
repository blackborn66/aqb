/*
 * printtree.c - functions to print out intermediate representation (IR) trees.
 *
 */
#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"

static void indent(FILE *out, int d)
{
    int i;
    for (i = 0; i <= d; i++)
        fprintf(out, " ");
}

static char bin_oper[][14] = {
   "PLUS",  "MINUS",  "MUL", "DIV",
   "XOR",   "EQV",    "IMP", "NEG", "NOT", "AND", "OR",
   "POWER", "INTDIV", "MOD"};

static char rel_oper[][6] = { "EQ", "NE", "LT", "GT", "LE", "GE"};

void printStm(FILE *out, T_stm stm, int d)
{
    if (!stm)
    {
        indent(out,d);
        fprintf(out, "NULL");
        return;
    }

    switch (stm->kind) {
    case T_SEQ:
        indent(out,d);
        fprintf(out, "SEQ(\n"); printStm(out, stm->u.SEQ.left,d+1);  fprintf(out, ",\n"); 
        printStm(out, stm->u.SEQ.right,d+1); fprintf(out, ")");
        break;
    case T_LABEL:
        indent(out,d); fprintf(out, "LABEL %s", S_name(stm->u.LABEL));
        break;
    case T_JUMP:
        indent(out,d); fprintf(out, "JUMP(%s)", S_name(stm->u.JUMP));
        break;
    case T_CJUMP:
        indent(out,d); fprintf(out, "CJUMP(%s,", rel_oper[stm->u.CJUMP.op]);
        printExp(out, stm->u.CJUMP.left,d+1); fprintf(out, ","); 
        printExp(out, stm->u.CJUMP.right,d+1); fprintf(out, ",");
        fprintf(out, "%s,", S_name(stm->u.CJUMP.ltrue));
        fprintf(out, "%s", S_name(stm->u.CJUMP.lfalse)); fprintf(out, ")");
        break;
    case T_MOVE:
        indent(out,d); fprintf(out, "MOVE("); 
        printExp(out, stm->u.MOVE.src,d+1); 
        fprintf(out, " -> ");
        printExp(out, stm->u.MOVE.dst,d+1); 
        fprintf(out, ")");
        break;
    case T_NOP:
        indent(out,d); fprintf(out, "NOP ");
        break;
    case T_EXP:
        indent(out,d); fprintf(out, "EXP("); printExp(out, stm->u.EXP, d+1); 
        fprintf(out, ")");
        break;
    }
}

void printExp(FILE *out, T_exp exp, int d)
{
    switch (exp->kind)
    {
        case T_BINOP:
            fprintf(out, "BINOP(%s,", bin_oper[exp->u.BINOP.op]);
            printExp(out, exp->u.BINOP.left,d+1); fprintf(out, ",");
            printExp(out, exp->u.BINOP.right,d+1); fprintf(out, ")");
            break;
        case T_MEM:
            fprintf(out, "MEM");
            fprintf(out, "("); printExp(out, exp->u.MEM.exp,d+1); fprintf(out, ")");
            break;
        case T_TEMP:
            fprintf(out, "TEMP t%s",
  	      	 	    Temp_look(Temp_getNameMap(), exp->u.TEMP));
            break;
        case T_HEAP:
            fprintf(out, "HEAP %s", S_name(exp->u.HEAP));
            break;
        case T_ESEQ:
            fprintf(out, "ESEQ(\n"); 
            printStm(out, exp->u.ESEQ.stm,d+1);
            fprintf(out, ",");
            printExp(out, exp->u.ESEQ.exp,d+1); fprintf(out, ")");
            break;
        case T_CONST:
            fprintf(out, "CONST %d", exp->u.CONST);
            break;
        case T_CALLF:
        {
            T_expList args = exp->u.CALLF.args;
            fprintf(out, "CALLF(%s: ", S_name(exp->u.CALLF.fun));
            for (;args; args=args->tail)
            {
                printExp(out, args->head,d+2);
                if (args->tail)
                    fprintf(out, ",");
            }
            fprintf(out, ")");
            break;
        }
        case T_CAST:
            fprintf(out, "CAST(");
            Ty_print(exp->u.CAST.ty_from);
            fprintf(out, "->");
            Ty_print(exp->ty);
            fprintf(out, ": ");
            printExp(out, exp->u.CAST.exp, d+1);
            fprintf(out, ")");
            break;
        case T_FP:
            fprintf(out, "FramePtr");
            break;
    } /* end of switch */
}

void printStmList (FILE *out, T_stmList stmList) 
{
  for (; stmList; stmList=stmList->tail) {
    printStm(out, stmList->head,0); fprintf(out, "\n");
  }
}
