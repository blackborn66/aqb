/*
 * temp.h
 *
 */

#ifndef TEMP_H
#define TEMP_H

#include "symbol.h"

typedef struct Temp_temp_      *Temp_temp;
typedef S_symbol                Temp_label;
typedef struct Temp_labelList_ *Temp_labelList;

#include "types.h"

Temp_temp       Temp_Temp      (Ty_ty ty);
Temp_temp       Temp_NamedTemp (string name, Ty_ty ty);
Ty_ty           Temp_ty        (Temp_temp t);
int             Temp_num       (Temp_temp t);
void            Temp_printf    (Temp_temp t, FILE *out);
void            Temp_snprintf  (Temp_temp t, string buf, size_t size);
string          Temp_strprint  (Temp_temp t);   // only use in debug code (allocates memory!)

Temp_label      Temp_newlabel(void);
Temp_label      Temp_namedlabel(string name);
string          Temp_labelstring(Temp_label s);

struct Temp_labelList_
{
    Temp_label head;
    Temp_labelList tail;
};
Temp_labelList  Temp_LabelList(Temp_label h, Temp_labelList t);

// Temp_tempSet: mutable set of temps, still represented as a linked list for speed and iteration

typedef struct Temp_tempSet_     *Temp_tempSet;
typedef struct Temp_tempSetNode_ *Temp_tempSetNode;

struct Temp_tempSetNode_
{
    Temp_tempSetNode next, prev;
    Temp_temp        temp;
};

struct Temp_tempSet_
{
    Temp_tempSetNode first, last;
};

Temp_tempSet       Temp_TempSet          (void);
bool               Temp_tempSetContains  (Temp_tempSet ts, Temp_temp t);
bool               Temp_tempSetAdd       (Temp_tempSet ts, Temp_temp t); // returns FALSE if t was already in ts, TRUE otherwise
bool               Temp_tempSetSub       (Temp_tempSet ts, Temp_temp t); // returns FALSE if t was not in ts, TRUE otherwise
string             Temp_tempSetSPrint    (Temp_tempSet ts);
static inline bool Temp_tempSetIsEmpty   (Temp_tempSet ts) { return ts->first == NULL; }
Temp_tempSet       Temp_tempSetUnion     (Temp_tempSet tsA, Temp_tempSet tsB); // return newly allocated TempSet that contains union of nodes from tsA and tsaB
Temp_tempSet       Temp_tempSetCopy      (Temp_tempSet ts); // return newly allocated TempSet that contains the nodes from ts
int                Temp_TempSetCount     (Temp_tempSet ts); // return number of temps in this temp set

#endif
