#ifndef HAVE_SCANNER_H
#define HAVE_SCANNER_H

#include <stdio.h>

#include "symbol.h"

// when enabled, scanner will keep a copy of the source code lines in RAM
// so tree (IR) and assembly output can be annotated with source code excerpts

#define S_KEEP_SOURCE

typedef enum {
 S_ERRTKN, S_EOL,
 S_IDENT, S_STRING, S_COLON, S_SEMICOLON, S_COMMA, S_INUM, S_FNUM, S_MINUS, S_LPAREN, S_RPAREN,
 S_EQUALS, S_EXP, S_ASTERISK, S_SLASH, S_BACKSLASH, S_PLUS, S_GREATER, S_LESS, S_NOTEQ, S_LESSEQ,
 S_GREATEREQ, S_POINTER, S_PERIOD, S_AT, S_LBRACKET, S_RBRACKET, S_TRIPLEDOTS } S_token;

typedef uint32_t  S_pos; // 32 bits: cccc cccc llll llll

void  S_init(FILE *fin);

int   S_getline(S_pos pos);
int   S_getcol(S_pos pos);
char *S_getcurline(void);
int   S_getcurlinenum(void);

typedef struct S_tkn_   *S_tkn;

typedef enum { S_thNone, S_thSingle, S_thDouble, S_thInteger, S_thLong, S_thUInteger, S_thULong } S_typeHint;

struct S_tkn_
{
    S_tkn   next;
    S_pos   pos;
    S_token kind;
    union
    {
        struct { 
            int32_t    inum; 
            double     fnum;
            S_typeHint typeHint;
        }                             literal;
        S_symbol                      sym;
        char                         *str;
    } u;
};

S_tkn S_nextline(void);

#ifdef S_KEEP_SOURCE
string  S_getSourceLine (int line);
#endif

#endif
