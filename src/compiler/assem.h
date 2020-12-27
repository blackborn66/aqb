/*
 * assem.h - m68k assembler definitions
 */

#ifndef ASSEM_H
#define ASSEM_H

#include "scanner.h"

enum AS_mn
{                       //     Example
    AS_LABEL,           //   0 label:

    AS_ADD_Dn_Dn,       //   1 add.x   d1, d2
    AS_ADD_Imm_Dn,      //   2 add.x   #42, d2
    AS_ADD_Imm_sp,      //   3 add.x   #42, sp

    AS_AND_Dn_Dn,       //   4 and.x  d1, d2
    AS_AND_Imm_Dn,      //   5 and.x  #23, d3

    AS_ASL_Dn_Dn,       //   6 asl.x   d1, d2
    AS_ASL_Imm_Dn,      //   7 asl.x   #42, d2

    AS_ASR_Dn_Dn,       //   8 asr.x   d1, d2
    AS_ASR_Imm_Dn,      //   9 asr.x   #42, d2

    AS_BEQ,             //  10 beq     label
    AS_BNE,             //  11 bne     label
    AS_BLT,             //  12 blt     label
    AS_BGT,             //  13 bgt     label
    AS_BLE,             //  14 ble     label
    AS_BGE,             //  15 bge     label
    AS_BCS,             //  16 bcs     label
    AS_BHI,             //  17 bhi     label
    AS_BLS,             //  18 bls     label
    AS_BCC,             //  19 bcc     label

    AS_CMP_Dn_Dn,       //  20 cmp.x   d0, d7

    AS_DIVS_Dn_Dn,      //  21 divs.x  d1, d2
    AS_DIVS_Imm_Dn,     //  22 divs.x  #23, d3
    AS_DIVU_Dn_Dn,      //  23 divu.x  d1, d2
    AS_DIVU_Imm_Dn,     //  24 divu.x  #23, d3

    AS_EOR_Dn_Dn,       //  25 eor.x  d1, d2
    AS_EOR_Imm_Dn,      //  26 eor.x  #23, d3

    AS_EXT_Dn,          //  27 ext.x   d1

    AS_LINK_fp,         //  28 link    a5, #-4

    AS_LSL_Dn_Dn,       //  29 lsl.x   d1, d2
    AS_LSL_Imm_Dn,      //  30 lsl.x   #42, d2

    AS_LSR_Dn_Dn,       //  31 lsr.x   d1, d2
    AS_LSR_Imm_Dn,      //  32 lsr.x   #42, d2

    AS_MOVE_AnDn_AnDn,  //  33 move.x  d1, d2
    AS_MOVE_Imm_OAn,    //  34 move.x  #23, 42(a6)
    AS_MOVE_Imm_RAn,    //  35 move.x  #23, (a6)
    AS_MOVE_Imm_AnDn,   //  36 move.x  #23, d0
    AS_MOVE_AnDn_RAn,   //  37 move.x  d1, (a6)
    AS_MOVE_RAn_AnDn,   //  38 move.x  (a5), d1
    AS_MOVE_OAn_AnDn,   //  39 move.x  42(a1), d0
    AS_MOVE_AnDn_OAn,   //  40 move.x  d0, 42(a3)
    AS_MOVE_AnDn_PDsp,  //  41 move.x  d1, -(sp)
    AS_MOVE_Imm_PDsp,   //  42 move.x  #23, -(sp)
    AS_MOVE_spPI_AnDn,  //  43 move.x  (sp)+, d1
    AS_MOVE_ILabel_AnDn,//  44 move.x  #label, d1
    AS_MOVE_Label_AnDn, //  45 move.x  label, d6
    AS_MOVE_AnDn_Label, //  46 move.x  d6, label
    AS_MOVE_Ofp_AnDn,   //  47 move.x  42(a5), d0
    AS_MOVE_AnDn_Ofp,   //  48 move.x  d0, 42(a5)
    AS_MOVE_Imm_Ofp,    //  49 move.x  #42, 42(a5)
    AS_MOVE_Imm_Label,  //  50 move.x  #42, label
    AS_MOVE_fp_AnDn,    //  51 move.x  a5, d0

    AS_MULS_Dn_Dn,      //  52 muls.x  d1, d2
    AS_MULS_Imm_Dn,     //  53 muls.x  #42, d2
    AS_MULU_Dn_Dn,      //  54 mulu.x  d1, d2
    AS_MULU_Imm_Dn,     //  55 mulu.x  #42, d2

    AS_NEG_Dn,          //  56 neg.x   d0

    AS_NOT_Dn,          //  57 not.x   d0

    AS_NOP,             //  58 nop

    AS_OR_Dn_Dn,        //  59 or.x  d1, d2
    AS_OR_Imm_Dn,       //  60 or.x  #42, d2

    AS_JMP,             //  61 jmp     label
    AS_JSR_Label,       //  62 jsr     label
    AS_JSR_An,          //  63 jsr     (a2)
    AS_JSR_RAn,         //  64 jsr     -36(a6)

    AS_RTS,             //  65 rts

    AS_SNE_Dn,          //  66 sne.b   d1

    AS_SUB_Dn_Dn,       //  67 sub.x   d1, d2
    AS_SUB_Imm_Dn,      //  68 sub.x   #42, d2

    AS_SWAP_Dn,         //  69 swap.x   d4

    AS_TST_Dn,          //  70 tst.x   d0

    AS_UNLK_fp,         //  71 unlink  a5

    AS_NUM_INSTR
};

enum AS_w { AS_w_B, AS_w_W, AS_w_L, AS_w_NONE } ;

typedef struct AS_instr_ *AS_instr;
struct AS_instr_
{
    enum AS_mn     mn;
    enum AS_w      w;
    Temp_label     label;
    Temp_temp      src, dst;
    Ty_const       imm;
    short          offset;

    Temp_tempSet   def, use; // optional, used in flowgraph.c
    S_pos          pos;      // source code reference
};

AS_instr AS_Instr       (S_pos pos, enum AS_mn mn, enum AS_w w, Temp_temp src, Temp_temp dst);
AS_instr AS_InstrEx     (S_pos pos, enum AS_mn mn, enum AS_w w, Temp_temp src, Temp_temp dst, Ty_const imm, long offset, Temp_label label);
AS_instr AS_InstrEx2    (S_pos pos, enum AS_mn mn, enum AS_w w, Temp_temp src, Temp_temp dst, Ty_const imm, long offset, Temp_label label, Temp_tempSet def, Temp_tempSet use);

void     AS_sprint      (string str, AS_instr i);

/*
 * AS_instrInfo: general information about 68k instruction set
 */

typedef struct AS_instrInfo_ AS_instrInfo;

struct AS_instrInfo_
{
    enum AS_mn  mn;
    bool        isJump;
    bool        hasLabel;
    bool        hasImm;
    bool        hasSrc;
    bool        hasDst;
    bool        srcDnOnly, dstDnOnly; // TRUE -> src/dst has to be a data register
    bool        srcAnOnly, dstAnOnly; // TRUE -> src/dst has to be a address register
    bool        dstIsAlsoSrc;         // TRUE -> destination register is also a src (i.e. add.x d1, d2)
    bool        dstIsOnlySrc;         // TRUE -> destination register is only a src (i.e. move.x d1, (a0))
};

extern AS_instrInfo AS_instrInfoA[AS_NUM_INSTR]; // array of AS_instrInfos, indexed by mn above

// AS_instrList: mutable, ordered doubly-linked list of assembly instructions

typedef struct AS_instrList_     *AS_instrList;
typedef struct AS_instrListNode_ *AS_instrListNode;

struct AS_instrListNode_
{
    AS_instrListNode next, prev;
    AS_instr         instr;
};

struct AS_instrList_
{
    AS_instrListNode first, last;
};

AS_instrList AS_InstrList             (void);
void         AS_instrListAppend       (AS_instrList al, AS_instr instr);
void         AS_instrListPrepend      (AS_instrList al, AS_instr instr);
void         AS_instrListInsertBefore (AS_instrList al, AS_instrListNode n, AS_instr instr);
void         AS_instrListInsertAfter  (AS_instrList al, AS_instrListNode n, AS_instr instr);
void         AS_instrListRemove       (AS_instrList al, AS_instrListNode n);

// AS_instrSet: mutable set of instrs, still represented as a linked list for speed and iteration

typedef struct AS_instrSet_     *AS_instrSet;
typedef struct AS_instrSetNode_ *AS_instrSetNode;

struct AS_instrSetNode_
{
    AS_instrSetNode next, prev;
    AS_instr        instr;
};

struct AS_instrSet_
{
    AS_instrSetNode first, last;
};

AS_instrSet        AS_InstrSet         (void);
bool               AS_instrSetContains (AS_instrSet as, AS_instr i);
bool               AS_instrSetAdd      (AS_instrSet as, AS_instr i);      // returns FALSE if i was already in as, TRUE otherwise
void               AS_instrSetAddSet   (AS_instrSet as, AS_instrSet as2); // add all elements from as2 to as
bool               AS_instrSetSub      (AS_instrSet as, AS_instr i);      // returns FALSE if i was not in as, TRUE otherwise
static inline bool AS_instrSetIsEmpty  (AS_instrSet as) { return as->first == NULL; }

enum AS_w          AS_tySize(Ty_ty ty);

void               AS_printInstrList (FILE *out, AS_instrList iList);
void               AS_printInstrSet  (FILE *out, AS_instrSet  is   );

void               AS_assemble (AS_instrList proc);

#endif
