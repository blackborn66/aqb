/*
 * assem.c - Functions to translate to Assem-instructions for
 *           the 68k assembly language using Maximal Munch.
 */

#include <stdio.h>
#include <stdlib.h> /* for atoi */
#include <string.h> /* for strcpy */
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "table.h"
#include "assem.h"
#include "frame.h"
#include "errormsg.h"

static Temp_tempLList LL(Temp_tempList head, Temp_tempLList tail)
{
    return Temp_TempLList(head, tail);
}

AS_instr AS_Instr (enum AS_mn mn, enum AS_w w, Temp_temp src, Temp_temp dst)
{
    AS_instr p = (AS_instr) checked_malloc (sizeof *p);

    p->mn     = mn;
    p->w      = w;
    p->src    = src ? Temp_TempList(src, NULL) : NULL;
    p->dst    = dst ? Temp_TempList(dst, NULL) : NULL;
    p->label  = NULL;
    p->imm    = 0;
    p->offset = 0;

    switch (mn)
    {
        case AS_TST_Dn:
            assert(dst==NULL);
            p->srcInterf = LL(F_aRegs(), NULL);
            p->dstInterf = NULL;
            break;
        case AS_SNE_Dn:
            assert(src==NULL);
            p->srcInterf = NULL;
            p->dstInterf = LL(F_aRegs(), NULL);
            break;
        case AS_SWAP_Dn:
        case AS_EXT_Dn:
        case AS_NOT_Dn:
            assert(src==dst);
            p->srcInterf = LL(F_aRegs(), NULL);
            p->dstInterf = LL(F_aRegs(), NULL);
            break;
        case AS_NOP:
            assert(w == AS_w_NONE);
            assert(dst==NULL);
            p->srcInterf = NULL;
            p->dstInterf = NULL;
            break;
        case AS_MOVE_AnDn_AnDn:
            assert(src);
            assert(dst);
            p->srcInterf = w == AS_w_B ? LL(F_aRegs(), NULL) : NULL;
            p->dstInterf = w == AS_w_B ? LL(F_aRegs(), NULL) : NULL;
            break;
        case AS_MOVE_fp_AnDn:
            assert(src==NULL);
            assert(dst);
            p->srcInterf = NULL;
            p->dstInterf = w == AS_w_B ? LL(F_aRegs(), NULL) : NULL;
            break;
        case AS_MOVE_spPI_AnDn:
            p->srcInterf = NULL;
            p->dstInterf = w == AS_w_B ? LL(F_aRegs(), NULL) : NULL;
            break;
        case AS_MOVE_AnDn_PDsp:
            p->srcInterf = w == AS_w_B ? LL(F_aRegs(), NULL) : NULL;
            p->dstInterf = NULL;
            break;
        case AS_UNLK_fp:
            p->srcInterf = NULL;
            p->dstInterf = NULL;
            break;
        case AS_MOVE_RAn_AnDn:  // move.x  (a5), d1
            p->srcInterf = LL(F_dRegs(), NULL);
            p->dstInterf = w == AS_w_B ? LL(F_aRegs(), NULL) : NULL;
            break;
        default:
            EM_error(0, "*** internal error: unknown mn %d!", mn);
            assert(0);
    }

    return p;
}

AS_instr AS_InstrEx (enum AS_mn mn, enum AS_w w, Temp_tempList src, Temp_tempList dst, Ty_const imm, long offset, Temp_label label)
{
    AS_instr p = (AS_instr) checked_malloc (sizeof *p);

    p->mn     = mn;
    p->w      = w;
    p->src    = src;
    p->dst    = dst;
    p->label  = label;
    p->imm    = imm;
    p->offset = offset;

    switch (mn)
    {
        case AS_ADD_Dn_Dn:
        case AS_SUB_Dn_Dn:
        case AS_MULS_Dn_Dn:
        case AS_MULU_Dn_Dn:
        case AS_DIVS_Dn_Dn:
        case AS_DIVU_Dn_Dn:
        case AS_OR_Dn_Dn:
        case AS_AND_Dn_Dn:
        case AS_EOR_Dn_Dn:
        case AS_ASR_Dn_Dn:
        case AS_ASL_Dn_Dn:
        case AS_LSR_Dn_Dn:
        case AS_LSL_Dn_Dn:
            assert(label==NULL);
            assert(imm==0);
            assert(offset==0);
            assert(src);
            assert(src->tail);
            assert(dst);
            assert(dst->tail==NULL);
            p->srcInterf = LL(F_aRegs(), LL(F_aRegs(), NULL));
            p->dstInterf = LL(F_aRegs(), NULL);
            break;
        case AS_CMP_Dn_Dn:
            assert(label==NULL);
            assert(imm==0);
            assert(offset==0);
            assert(src);
            assert(src->tail);
            assert(dst==NULL);
            p->srcInterf = LL(F_aRegs(), LL(F_aRegs(), NULL));
            p->dstInterf = NULL;
            break;
        case AS_NEG_Dn:
        case AS_NOT_Dn:
            assert(label==NULL);
            assert(imm==NULL);
            assert(offset==0);
            assert(src);
            assert(src->tail==NULL);
            assert(dst);
            assert(dst->tail==NULL);
            p->srcInterf = LL(F_aRegs(), NULL);
            p->dstInterf = LL(F_aRegs(), NULL);
            break;
        case AS_MOVE_Imm_AnDn:
            assert(src==NULL);
            p->srcInterf = NULL;
            p->dstInterf = w == AS_w_B ? LL(F_aRegs(), NULL) : NULL;
            break;
        case AS_LABEL:
            assert(p->label);
        case AS_ADD_Imm_sp:
        case AS_RTS:
        case AS_LINK_fp:
        case AS_BEQ:
        case AS_BNE:
        case AS_BLT:
        case AS_BGT:
        case AS_BLE:
        case AS_BGE:
        case AS_BCS:
        case AS_BHI:
        case AS_BLS:
        case AS_BCC:
        case AS_JMP:
        case AS_MOVE_Imm_PDsp:   // move.x  #23, -(sp)
            p->srcInterf = NULL;
            p->dstInterf = NULL;
            break;
        case AS_JSR_Label:
            assert(label);
            p->srcInterf = NULL;
            p->dstInterf = NULL;
            break;
        case AS_JSR_An:
            p->srcInterf = LL(F_dRegs(), NULL);
            p->dstInterf = NULL;
            break;
        case AS_ADD_Imm_Dn:
        case AS_SUB_Imm_Dn:
        case AS_MULS_Imm_Dn:
        case AS_MULU_Imm_Dn:
        case AS_DIVS_Imm_Dn:
        case AS_DIVU_Imm_Dn:
        case AS_OR_Imm_Dn:
        case AS_AND_Imm_Dn:
        case AS_EOR_Imm_Dn:
            assert(src);
            assert(dst);
            assert(src->head==dst->head);
            p->srcInterf = LL(F_aRegs(), NULL);
            p->dstInterf = LL(F_aRegs(), NULL);
            break;
        case AS_MOVE_Imm_Label:
        case AS_MOVE_ILabel_AnDn:
        case AS_MOVE_Label_AnDn:
            p->srcInterf = NULL;
            p->dstInterf = (w == AS_w_B) ? LL(F_aRegs(), NULL) : NULL;
            break;
        case AS_MOVE_AnDn_Label:
            p->srcInterf = (w == AS_w_B) ? LL(F_aRegs(), NULL) : NULL;
            p->dstInterf = NULL;
            break;
        case AS_MOVE_AnDn_RAn:  // move.x  d1, (a6)
            assert(dst==NULL);
            assert(label==NULL);
            assert(imm==NULL);
            assert(offset==0);
            p->srcInterf = LL(NULL, LL(F_dRegs(), NULL));
            p->dstInterf = NULL;
            break;
        case AS_MOVE_Imm_OAn:    // move.x  #23, 42(a6)
            assert(dst==NULL);
            assert(label==NULL);
            p->srcInterf = LL(F_dRegs(), NULL);
            p->dstInterf = NULL;
            break;
        case AS_MOVE_Imm_RAn:    // move.x  #23, (a6)
            assert(dst==NULL);
            assert(label==NULL);
            assert(offset==0);
            p->srcInterf = LL(F_dRegs(), NULL);
            p->dstInterf = NULL;
            break;
        case AS_MOVE_Ofp_AnDn:  // move.x  42(a5), d0
            assert(label==NULL);
            assert(src==NULL);
            p->srcInterf = NULL;
            p->dstInterf = (w == AS_w_B) ? LL(F_aRegs(), NULL) : NULL;
            break;
        case AS_MOVE_AnDn_Ofp:  // move.x  d0, 42(a5)
            assert(label==NULL);
            assert(dst==NULL);
            p->srcInterf = (w == AS_w_B) ? LL(F_aRegs(), NULL) : NULL;
            p->dstInterf = NULL;
            break;
        case AS_MOVE_Imm_Ofp:  // move.x  #imm, 42(a5)
            assert(label==NULL);
            assert(src==NULL);
            assert(dst==NULL);
            p->srcInterf = NULL;
            p->dstInterf = NULL;
            break;
        case AS_NOP:
            assert(w == AS_w_NONE);
            assert(dst==NULL);
            p->srcInterf = NULL;
            p->dstInterf = NULL;
            break;

        default:
            EM_error(0, "*** internal error: unknown mn %d!", mn);
            assert(0);
    }

    return p;
}

AS_instr AS_InstrInterf (enum AS_mn mn, enum AS_w w, Temp_tempList src, Temp_tempList dst,
                         Temp_tempLList srcInterf, Temp_tempLList dstInterf, Ty_const imm, long offset, Temp_label label)
{
    AS_instr p = (AS_instr) checked_malloc (sizeof *p);

    p->mn        = mn;
    p->w         = w;
    p->src       = src;
    p->dst       = dst;
    p->srcInterf = srcInterf;
    p->dstInterf = dstInterf;
    p->label     = label;
    p->imm       = imm;
    p->offset    = offset;

    return p;
}

AS_instrList AS_InstrList(AS_instr head, AS_instrList tail)
{
    AS_instrList p = (AS_instrList) checked_malloc (sizeof *p);

    p->head=head;
    p->tail=tail;

    return p;
}

/* put list b at the end of list a */
AS_instrList AS_splice(AS_instrList a, AS_instrList b)
{
    AS_instrList p;
    if (a==NULL)
        return b;
    for (p=a; p->tail!=NULL; p=p->tail);
    p->tail=b;
    return a;
}

AS_instrSet AS_InstrSet (void)
{
    AS_instrSet s = checked_malloc(sizeof(*s));

    s->first = NULL;
    s->last  = NULL;

    return s;
}

static AS_instrSetNode AS_InstrSetNode (AS_instr i)
{
    AS_instrSetNode n = checked_malloc(sizeof(*n));

    n->prev  = NULL;
    n->next  = NULL;
    n->instr = i;

    return n;
}

bool AS_instrSetContains (AS_instrSet as, AS_instr i)
{
    for (AS_instrSetNode n = as->first; n; n=n->next)
    {
        if (n->instr == i)
            return TRUE;
    }
    return FALSE;
}

bool AS_instrSetAdd (AS_instrSet as, AS_instr i) // returns FALSE if i was already in as, TRUE otherwise
{
    for (AS_instrSetNode n = as->first; n; n=n->next)
    {
        if (n->instr == i)
            return FALSE;
    }

    AS_instrSetNode n = AS_InstrSetNode(i);
    n->prev = as->last;

    if (as->last)
        as->last = as->last->next = n;
    else
        as->first = as->last = n;

    return TRUE;
}

void AS_instrSetAddSet (AS_instrSet as, AS_instrSet as2)
{
    for (AS_instrSetNode n = as->first; n; n=n->next)
    {
        AS_instrSetAdd (as, n->instr);
    }
}

bool AS_instrSetSub (AS_instrSet as, AS_instr i) // returns FALSE if i was not in as, TRUE otherwise
{
    for (AS_instrSetNode n = as->first; n; n=n->next)
    {
        if (n->instr == i)
        {
            if (n->prev)
            {
                n->prev->next = n->next;
            }
            else
            {
                as->first = n->next;
                if (n->next)
                    n->next->prev = NULL;
            }

            if (n->next)
            {
                n->next->prev = n->prev;
            }
            else
            {
                as->last = n->prev;
                if (n->prev)
                    n->prev->next = NULL;
            }

            return TRUE;
        }
    }
    return FALSE;
}

enum AS_w AS_tySize(Ty_ty ty)
{
    switch (ty->kind)
    {
        case Ty_bool:
        case Ty_byte:
        case Ty_ubyte:
            return AS_w_B;
        case Ty_integer:
        case Ty_uinteger:
            return AS_w_W;
        case Ty_long:
        case Ty_ulong:
        case Ty_single:
        case Ty_double:
        case Ty_varPtr:
        case Ty_pointer:
        case Ty_string:
        case Ty_forwardPtr:
        case Ty_procPtr:
            return AS_w_L;
        case Ty_sarray:
        case Ty_darray:
        case Ty_record:
        case Ty_void:
        case Ty_toLoad:
        case Ty_prc:
            assert(0);
    }
    return AS_w_L;
}

static Temp_temp nthTemp(Temp_tempList list, int i)
{
    assert(list);
    if (i==0)
        return list->head;
    else
        return nthTemp(list->tail,i-1);
}

static void instrformat(string str, string strTmpl, AS_instr instr, Temp_map m)
{
    int pos = 0;
    int l   = strlen(strTmpl);

    for (int i=0; i<l; i++)
    {
        if (strTmpl[i] == '`')
        {
            i++;
            switch (strTmpl[i])
            {
                case 'w':
                    str[pos] = '.'; pos++;
                    switch (instr->w)
                    {
                        case AS_w_B:
                            str[pos] = 'b'; pos++; break;
                        case AS_w_W:
                            str[pos] = 'w'; pos++; break;
                        case AS_w_L:
                            str[pos] = 'l'; pos++; break;
                        default:
                            str[pos] = '?'; pos++; break;
                    }
                    break;
                case 's':
                {
                    int n = atoi(&strTmpl[++i]);
  	                string s = Temp_look(m, nthTemp(instr->src, n));
  	                strcpy(&str[pos], s);
  	                pos += strlen(s);
                    break;
                }
                case 'd':
                {
                    int n = atoi(&strTmpl[++i]);
  	                string s = Temp_look(m, nthTemp(instr->dst, n));
  	                strcpy(&str[pos], s);
  	                pos += strlen(s);
                    break;
                }
                case 'i':
                {
                    switch (instr->imm->ty->kind)
                    {
                        case Ty_bool:
                            pos += sprintf(&str[pos], "%d", instr->imm->u.b ? -1 : 0);
                            break;
                        case Ty_byte:
                        case Ty_ubyte:
                        case Ty_integer:
                        case Ty_uinteger:
                        case Ty_long:
                        case Ty_ulong:
                        case Ty_string:
                        case Ty_pointer:
                            pos += sprintf(&str[pos], "%d", instr->imm->u.i);
                            break;
                        case Ty_single:
                        case Ty_double:
                            pos += sprintf(&str[pos], "0x%08x /* %f */", encode_ffp(instr->imm->u.f), instr->imm->u.f);
                            break;
                        default:
                            EM_error(0, "*** assem.c:instrformat: internal error");
                            assert(0);
                    }
                    break;
                }
                case 'o':
                {
                    pos += sprintf(&str[pos], "%d", instr->offset);
                    break;
                }
                case 'l':
                {
                    string s = Temp_labelstring(instr->label);
  	                strcpy(&str[pos], s);
  	                pos += strlen(s);
  	                break;
                }
                default:
                    assert(0);
            }
        }
        else
        {
            str[pos] = strTmpl[i];
            pos++;
        }
    }
    str[pos] = 0;
}

void AS_sprint(string str, AS_instr i, Temp_map m)
{
    switch (i->mn)
    {
        case AS_LABEL:           // label:
            sprintf(str, "%s:", Temp_labelstring(i->label));     break;
        case AS_ADD_Dn_Dn:
            instrformat(str, "    add`w    `s0, `d0", i, m);     break;
        case AS_ADD_Imm_Dn:
            instrformat(str, "    add`w    #`i, `d0", i, m);     break;
        case AS_ADD_Imm_sp:
            instrformat(str, "    add`w    #`i, sp", i, m);      break;
        case AS_AND_Dn_Dn:
            instrformat(str, "    and`w    `s0, `d0", i, m);     break;
        case AS_AND_Imm_Dn:
            instrformat(str, "    and`w    #`i, `d0", i, m);     break;
        case AS_ASL_Dn_Dn:
            instrformat(str, "    asl`w    `s0, `d0", i, m);     break;
        case AS_ASL_Imm_Dn:
            instrformat(str, "    asl`w    #`i, `d0", i, m);     break;
        case AS_ASR_Dn_Dn:
            instrformat(str, "    asr`w    `s0, `d0", i, m);     break;
        case AS_ASR_Imm_Dn:
            instrformat(str, "    asr`w    #`i, `d0", i, m);     break;
        case AS_BEQ:
            instrformat(str, "    beq      `l", i, m);           break;
        case AS_BNE:
            instrformat(str, "    bne      `l", i, m);           break;
        case AS_BLT:
            instrformat(str, "    blt      `l", i, m);           break;
        case AS_BGT:
            instrformat(str, "    bgt      `l", i, m);           break;
        case AS_BLE:
            instrformat(str, "    ble      `l", i, m);           break;
        case AS_BGE:
            instrformat(str, "    bge      `l", i, m);           break;
        case AS_BCS:
            instrformat(str, "    bcs      `l", i, m);           break;
        case AS_BHI:
            instrformat(str, "    bhi      `l", i, m);           break;
        case AS_BLS:
            instrformat(str, "    bls      `l", i, m);           break;
        case AS_BCC:
            instrformat(str, "    bcc      `l", i, m);           break;
        case AS_CMP_Dn_Dn:
            instrformat(str, "    cmp`w    `s0, `s1", i, m);     break;
        case AS_DIVS_Dn_Dn:
            instrformat(str, "    divs`w   `s0, `d0", i, m);     break;
        case AS_DIVS_Imm_Dn:
            instrformat(str, "    divs`w   #`i, `d0", i, m);     break;
        case AS_DIVU_Dn_Dn:
            instrformat(str, "    divu`w   `s0, `d0", i, m);     break;
        case AS_DIVU_Imm_Dn:
            instrformat(str, "    divu`w   #`i, `d0", i, m);     break;
        case AS_EOR_Dn_Dn:
            instrformat(str, "    eor`w    `s0, `d0", i, m);     break;
        case AS_EOR_Imm_Dn:
            instrformat(str, "    eor`w    #`i, `d0", i, m);     break;
        case AS_EXT_Dn:
            instrformat(str, "    ext`w    `d0", i, m);          break;
        case AS_LINK_fp:
            instrformat(str, "    link     a5, #`i"    , i, m);  break;
        case AS_LSL_Dn_Dn:
            instrformat(str, "    lsl`w    `s0, `d0", i, m);     break;
        case AS_LSL_Imm_Dn:
            instrformat(str, "    lsl`w    #`i, `d0", i, m);     break;
        case AS_LSR_Dn_Dn:
            instrformat(str, "    lsr`w    `s0, `d0", i, m);     break;
        case AS_LSR_Imm_Dn:
            instrformat(str, "    lsr`w    #`i, `d0", i, m);     break;
        case AS_MOVE_AnDn_AnDn:
            instrformat(str, "    move`w   `s0, `d0"   , i, m);  break;
        case AS_MOVE_fp_AnDn:
            instrformat(str, "    move`w   a5, `d0"   , i, m);   break;
        case AS_MOVE_AnDn_PDsp:
            instrformat(str, "    move`w   `s0, -(sp)",  i, m);  break;
        case AS_MOVE_spPI_AnDn:
            instrformat(str, "    move`w   (sp)+, `d0",  i, m);  break;
        case AS_MOVE_Imm_OAn:
            instrformat(str, "    move`w   #`i, `o(`s0)", i, m); break;
        case AS_MOVE_Imm_RAn:
            instrformat(str, "    move`w   #`i, (`s0)", i, m);   break;
        case AS_MOVE_Imm_PDsp:
            instrformat(str, "    move`w   #`i, -(sp)", i, m);   break;
        case AS_MOVE_AnDn_RAn:
            instrformat(str, "    move`w   `s0, (`s1)", i, m);   break;
        case AS_MOVE_RAn_AnDn:
            instrformat(str, "    move`w   (`s0), `d0", i, m);   break;
        case AS_MOVE_Imm_AnDn:
            instrformat(str, "    move`w   #`i, `d0", i, m);     break;
        case AS_MOVE_Ofp_AnDn:  // move.x  42(a5), d0
            instrformat(str, "    move`w   `o(a5), `d0", i, m);  break;
        case AS_MOVE_AnDn_Ofp:  // move.x  d0, 42(a5)
            instrformat(str, "    move`w   `s0, `o(a5)", i, m);  break;
        case AS_MOVE_Imm_Ofp:   // move.x  #23, 42(a5)
            instrformat(str, "    move`w   #`i, `o(a5)", i, m);  break;
        case AS_MOVE_ILabel_AnDn:
            instrformat(str, "    move`w   #`l, `d0", i, m);     break;
        case AS_MOVE_Label_AnDn:
            instrformat(str, "    move`w    `l, `d0", i, m);     break;
        case AS_MOVE_AnDn_Label:
            instrformat(str, "    move`w    `s0, `l", i, m);     break;
        case AS_MOVE_Imm_Label:
            instrformat(str, "    move`w   #`i, `l", i, m);      break;
        case AS_MULS_Dn_Dn:
            instrformat(str, "    muls`w   `s0, `d0", i, m);     break;
        case AS_MULS_Imm_Dn:
            instrformat(str, "    muls`w   #`i, `d0", i, m);     break;
        case AS_MULU_Dn_Dn:
            instrformat(str, "    mulu`w   `s0, `d0", i, m);     break;
        case AS_MULU_Imm_Dn:
            instrformat(str, "    mulu`w   #`i, `d0", i, m);     break;
        case AS_NEG_Dn:
            instrformat(str, "    neg`w    `d0", i, m);          break;
        case AS_NOT_Dn:
            instrformat(str, "    not`w    `d0", i, m);          break;
        case AS_NOP:
            instrformat(str, "    nop"           , i, m);        break;
        case AS_JMP:
            instrformat(str, "    jmp      `l", i, m);           break;
        case AS_JSR_Label:
            instrformat(str, "    jsr      `l", i, m);           break;
        case AS_JSR_An:
            instrformat(str, "    jsr      (`s0)", i, m);        break;
        case AS_JSR_RAn:
            instrformat(str, "    jsr      `o(`s0)", i, m);      break;
        case AS_OR_Dn_Dn:
            instrformat(str, "    or`w     `s0, `d0", i, m);     break;
        case AS_OR_Imm_Dn:
            instrformat(str, "    or`w     #`i, `d0", i, m);     break;
        case AS_RTS:
            instrformat(str, "    rts"           , i, m);        break;
        case AS_SNE_Dn:
            instrformat(str, "    sne      `d0", i, m);          break;
        case AS_SUB_Dn_Dn:
            instrformat(str, "    sub`w    `s0, `d0", i, m);     break;
        case AS_SUB_Imm_Dn:
            instrformat(str, "    sub`w    #`i, `d0", i, m);     break;
        case AS_SWAP_Dn:
            instrformat(str, "    swap`w   `d0", i, m);          break;
        case AS_TST_Dn:
            instrformat(str, "    tst`w    `s0", i, m);          break;
        case AS_UNLK_fp:
            instrformat(str, "    unlk     a5"   , i, m);        break;
        default:
            printf("***internal error: unknown mn %d\n", i->mn);
            assert(0);
    }
}

void AS_printInstrList (FILE *out, AS_instrList iList, Temp_map m)
{
    for (; iList; iList=iList->tail)
    {
        char buf[255];
        AS_sprint(buf, iList->head, m);
        fprintf(out, "%s\n", buf);
    }
}

AS_proc AS_Proc(string prolog, AS_instrList body, string epilog)
{
    AS_proc proc = checked_malloc(sizeof(*proc));

    proc->prolog = prolog;
    proc->body   = body;
    proc->epilog = epilog;

    return proc;
}

/******************************************************************************
 **
 ** machine code generation
 **
 ******************************************************************************/

static bool isAn (Temp_tempList l)
{
    return F_isAn (l->head);
}

static bool is8BitConst (Ty_const c)
{
    if (c->ty->kind == Ty_bool)
        return TRUE;

    assert (    ( c->ty->kind == Ty_byte     )
             || ( c->ty->kind == Ty_ubyte    )
             || ( c->ty->kind == Ty_integer  )
             || ( c->ty->kind == Ty_uinteger )
             || ( c->ty->kind == Ty_long     )
             || ( c->ty->kind == Ty_ulong    ) );

    return (c->u.i >=-128) && (c->u.i <= 127);
}

static uint32_t instr_size (AS_instr instr)
{
    switch (instr->mn)
    {
        case AS_LABEL:
            return 0;


        case AS_LINK_fp:         // link    a5, #-4
            return 2;

        case AS_MOVE_Imm_AnDn:   //  36 move.x  #23, d0

            if (isAn (instr->dst))  // movea
            {
                switch (instr->w)
                {
                    case AS_w_B:
                        fprintf (stderr, "*** internal error: movea.b does not exist.\n");
                        assert(0);
                        return 0;
                    case AS_w_W:
                        return 2;
                    case AS_w_L:
                        return 3;
                    default:
                        assert(0);
                        return 0;
                }
            }
            else
            {
                switch (instr->w)
                {
                    case AS_w_B:
                    case AS_w_W:
                        return 2;
                    case AS_w_L:
                        if (is8BitConst (instr->imm))       // moveq
                            return 1;
                        return 3;
                    default:
                        assert(0);
                        return 0;
                }
            }
            break;



        case AS_MOVE_AnDn_PDsp:  //  41 move.x  d1, -(sp)
            return 1;

        // case AS_MOVE_Label_AnDn: //  45 move.x  label, d6


        case AS_JSR_Label:       //  61 jsr     label
            return 3;

        default:
            fprintf (stderr, "*** internal error: unknown mn %d!\n", instr->mn);
            assert(0);
    }
}

void AS_assemble (AS_proc proc, Temp_map m)
{
    // step 0: determine size of segment

    uint32_t seg_size = 0;
    for (AS_instrList iList = proc->body; iList; iList=iList->tail)
    {
        AS_instr instr = iList->head;

        char buf[255];
        AS_sprint(buf, instr, m);
        printf("AS_assemble: size of %s\n", buf);

        seg_size += instr_size(instr);

    }



    // for (AS_instrList iList = proc->body; iList; iList=iList->tail)
    // {
    //     AS_instr instr = iList->head;

    //     char buf[255];
    //     AS_sprint(buf, instr, m);
    //     printf("AS_assemble: %s\n", buf);


    //     switch (instr->mn)
    //     {
    //         default:
    //             fprintf (stderr, "*** internal error: unknown mn %d\n", instr->mn);
    //             assert(0);
    //     }
    // }
}

