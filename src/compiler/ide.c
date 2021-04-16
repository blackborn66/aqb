#include "ide.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>

#include "util.h"
#include "terminal.h"
#include "scanner.h"
#include "frontend.h"

#define STYLE_NORMAL  0
#define STYLE_KW      1
#define STYLE_STRING  2
#define STYLE_NUMBER  3
#define STYLE_COMMENT 4

#define INFOLINE            "X=   1 Y=   1 #=   0  new file"
#define INFOLINE_CURSOR_X      2
#define INFOLINE_CURSOR_Y      9
#define INFOLINE_NUM_LINES    16
#define INFOLINE_CHANGED      21
#define INFOLINE_FILENAME     22

#define CR  13
#define LF  10
#define TAB  9

#define SCROLL_MARGIN 5

#define INDENT_SPACES 4

static S_symbol S_IF;
static S_symbol S_ELSEIF;
static S_symbol S_ELSE;
static S_symbol S_THEN;
static S_symbol S_END;
static S_symbol S_SUB;
static S_symbol S_FUNCTION;
static S_symbol S_FOR;
static S_symbol S_NEXT;
static S_symbol S_DO;
static S_symbol S_LOOP;
static S_symbol S_WHILE;
static S_symbol S_WEND;

typedef struct IDE_line_     *IDE_line;
typedef struct IDE_editor_   *IDE_editor;

struct IDE_line_
{
	IDE_line  next, prev;
	uint16_t  len;
    char     *buf;
    char     *style;

    int8_t    indent;
    int8_t    pre_indent, post_indent;
};

struct IDE_editor_
{
    IDE_line           line_first, line_last;
	uint16_t           num_lines;
	bool               changed;
	char               infoline[36+PATH_MAX];
    uint16_t           infoline_row;
	char 	           filename[PATH_MAX];

	uint16_t           cursor_col, cursor_row;
    IDE_line           cursor_line;

    // window, scrolling, repaint
	uint16_t           window_width, window_height;
	uint16_t           scrolloff_col, scrolloff_row;
    IDE_line           scrolloff_line;
    uint16_t           scrolloff_line_row;
    bool               up2date_row[TE_MAX_ROWS];
    bool               up2date_il_pos;
    bool               up2date_il_num_lines;
    bool               up2date_il_flags;
    bool               up2date_il_filename;

    // cursor_line currently being edited (i.e. represented in buffer):
    bool               editing;
    char               buf[MAX_LINE_LEN];
    char               style[MAX_LINE_LEN];
    uint16_t           buf_len;
    uint16_t           buf_pos;

    // temporary buffer used in edit operations
    char               buf2[MAX_LINE_LEN];
    char               style2[MAX_LINE_LEN];
    uint16_t           buf2_len;
};

#define DEBUG 0
#define LOG_FILENAME "ide.log"
static FILE *logf=NULL;
#define dprintf(fmt, ...) do { if (DEBUG) fprintf(logf, fmt, __VA_ARGS__); } while (0)

IDE_line newLine(IDE_editor ed, char *buf, char *style, int8_t pre_indent, int8_t post_indent)
{
    int len = strlen(buf);
    IDE_line l = U_poolAlloc (UP_ide, sizeof(*l)+2*(len+1));

	l->next        = NULL;
	l->prev        = NULL;
	l->len         = len;
    l->buf         = (char *) (&l[1]);
    l->style       = l->buf + len + 1;
    l->indent      = 0;
    l->pre_indent  = pre_indent;
    l->post_indent = post_indent;

    memcpy (l->buf, buf, len+1);
    memcpy (l->style, style, len+1);

	return l;
}

static void freeLine (IDE_editor ed, IDE_line l)
{
    // FIXME: implement
    ed->scrolloff_line = NULL;
}

static void insertLineAfter (IDE_editor ed, IDE_line lBefore, IDE_line l)
{
    if (lBefore)
    {
        if (lBefore == ed->line_last)
        {
            ed->line_last = l;
            l->next = NULL;
        }
        else
        {
            l->next = lBefore->next;
            lBefore->next->prev = l;
        }
        l->prev = lBefore;
        lBefore->next = l;
    }
    else
    {
        l->next = ed->line_first;
        if (ed->line_first)
        {
            ed->line_first = ed->line_first->prev = l;
        }
        else
        {
            ed->line_first = ed->line_last = l;
        }
    }
}

static void deleteLine (IDE_editor ed, IDE_line l)
{
    if (l->prev)
        l->prev->next = l->next;
    else
        ed->line_first = l->next;
    if (l->next)
        l->next->prev = l->prev;
    else
        ed->line_last = l->prev;
    freeLine (ed, l);
}

void initWindowSize (IDE_editor ed)
{
    int rows, cols;
    if (!TE_getsize (&rows, &cols))
        exit(1);

    ed->window_width  = cols;
    ed->window_height = rows;
    ed->infoline_row  = rows-1;
}

static void _itoa(uint16_t num, char* buf, uint16_t width)
{
    uint16_t i = 0;

    /* Handle 0 explicitely, otherwise empty string is printed for 0 */
    if (num == 0)
    {
        for (uint16_t j=1; j<width; j++)
            buf[i++] = ' ';
        buf[i++] = '0';
        return;
    }

    // Process individual digits
    uint16_t b=1;
    for (uint16_t j=1; j<width; j++)
        b = b * 10;

    for (uint16_t j=0; j<width; j++)
    {
        uint16_t digit = (num / b) % 10;
        b = b / 10;
        buf[i++] = digit + '0';
    }
}

static IDE_line getLine (IDE_editor ed, int linenum)
{
    IDE_line l = ed->line_first;
    int ln = 0;
    while ( l && (ln < linenum) )
    {
        ln++;
        l = l->next;
    }
    return l;
}

static void invalidateAll (IDE_editor ed)
{
    for (uint16_t i=0; i<TE_MAX_ROWS; i++)
        ed->up2date_row[i] = FALSE;
}

static void scroll(IDE_editor ed)
{
    uint16_t cursor_row = ed->cursor_row - ed->scrolloff_row;

    // scroll up ?

    if (cursor_row > ed->window_height - SCROLL_MARGIN)
    {
        uint16_t scrolloff_row_new = ed->cursor_row - ed->window_height + SCROLL_MARGIN;
        uint16_t scrolloff_row_max = ed->num_lines - ed->window_height + 1;

        if (scrolloff_row_new > scrolloff_row_max)
            scrolloff_row_new = scrolloff_row_max;

        uint16_t diff = scrolloff_row_new - ed->scrolloff_row;
        ed->scrolloff_row = scrolloff_row_new;
        switch (diff)
        {
            case 0:
                break;
            case 1:
                TE_scrollUp();
                ed->up2date_row[ed->window_height - 2] = FALSE;
                break;
            default:
                invalidateAll (ed);
        }
    }

    // scroll down ?

    if (cursor_row < SCROLL_MARGIN)
    {
        uint16_t scrolloff_row_new = ed->cursor_row > SCROLL_MARGIN ? ed->cursor_row - SCROLL_MARGIN : 0;

        uint16_t diff = ed->scrolloff_row - scrolloff_row_new;
        ed->scrolloff_row = scrolloff_row_new;
        switch (diff)
        {
            case 0:
                break;
            case 1:
                TE_scrollDown();
                ed->up2date_row[0] = FALSE;
                break;
            default:
                invalidateAll (ed);
        }
    }
    // FIXME: implement horizontal scroll
}

static bool nextch_cb(char *ch, void *user_data)
{
    IDE_editor ed = user_data;
    *ch = ed->buf[ed->buf_pos++];
    return (*ch) != 0;
}

typedef enum { STATE_IDLE, STATE_IF, STATE_ELSEIF, STATE_ELSE, STATE_THEN,
               STATE_ELSEIFTHEN, STATE_LOOP,
               STATE_END, STATE_SUB } state_enum;


static IDE_line buf2line (IDE_editor ed)
{
    static char buf[MAX_LINE_LEN];
    static char style[MAX_LINE_LEN];

    ed->buf_pos = 0;
    ed->buf[ed->buf_len]=0;
    S_init (nextch_cb, ed, /*filter_comments=*/FALSE);

    int pos = 0;
    int8_t pre_indent = 0;
    int8_t post_indent = 0;
    while (TRUE)
    {
        S_tkn tkn = S_nextline();
        if (!tkn)
            break;
        bool first = TRUE;
        S_token lastKind = S_ERRTKN;
        S_tkn lastTkn = NULL;
        state_enum state = STATE_IDLE;
        while (tkn && (pos <MAX_LINE_LEN-1))
        {
            switch (tkn->kind)
            {
                case S_ERRTKN:
                    break;
                case S_EOL:
                    switch (state)
                    {
                        case STATE_THEN:
                            if ((lastKind == S_IDENT) && (lastTkn->u.sym == S_THEN))
                            {
                                post_indent++;
                            }
                            break;
                        case STATE_ELSEIFTHEN:
                            if ((lastKind == S_IDENT) && (lastTkn->u.sym == S_THEN))
                            {
                                pre_indent--;
                                post_indent++;
                            }
                            break;
                        case STATE_ELSE:
                            if ((lastKind == S_IDENT) && (lastTkn->u.sym == S_ELSE))
                            {
                                pre_indent--;
                                post_indent++;
                            }
                            break;
                        case STATE_SUB:
                        case STATE_LOOP:
                            post_indent++;
                            break;
                        case STATE_END:
                            pre_indent--;
                            break;
                        default:
                            break;
                    }
                    break;
                case S_LCOMMENT:
                    buf[pos] = '\'';
                    style[pos++] = STYLE_COMMENT;
                    buf[pos] = ' ';
                    style[pos++] = STYLE_COMMENT;
                    for (char *c=tkn->u.str; *c; c++)
                    {
                        buf[pos] = *c;
                        style[pos++] = STYLE_COMMENT;
                    }
                    break;
                case S_RCOMMENT:
                    buf[pos] = 'R';
                    style[pos++] = STYLE_COMMENT;
                    buf[pos] = 'E';
                    style[pos++] = STYLE_COMMENT;
                    buf[pos] = 'M';
                    style[pos++] = STYLE_COMMENT;
                    buf[pos] = ' ';
                    style[pos++] = STYLE_COMMENT;
                    for (char *c=tkn->u.str; *c; c++)
                    {
                        buf[pos] = *c;
                        style[pos++] = STYLE_COMMENT;
                    }
                    break;
                case S_IDENT:
                {
                    if (!first)
                        buf[pos++] = ' ';
                    bool is_kw = FALSE;
                    for (int i =0; i<FE_num_keywords; i++)
                    {
                        if (FE_keywords[i]==tkn->u.sym)
                        {
                            is_kw = TRUE;
                            break;
                        }
                    }
                    char *s = S_name(tkn->u.sym);
                    int l = strlen(s);
                    if (pos+l >= MAX_LINE_LEN-1)
                        l = MAX_LINE_LEN-1-pos;
                    for (int i =0; i<l; i++)
                    {
                        if (is_kw)
                        {
                            buf[pos] = toupper(s[i]);
                            style[pos++] = STYLE_KW;
                        }
                        else
                        {
                            buf[pos] = s[i];
                            style[pos++] = STYLE_NORMAL;
                        }
                    }

                    // auto-indentation is based on very crude BASIC parsing

                    switch (state)
                    {
                        case STATE_IDLE:
                            if (tkn->u.sym == S_IF)
                            {
                                state = STATE_IF;
                            }
                            else if (tkn->u.sym == S_ELSEIF)
                            {
                                state = STATE_ELSEIF;
                            }
                            else if (tkn->u.sym == S_ELSE)
                            {
                                state = STATE_ELSE;
                            }
                            else if (tkn->u.sym == S_END)
                            {
                                state = STATE_END;
                            }
                            else if (tkn->u.sym == S_SUB)
                            {
                                state = STATE_SUB;
                            }
                            else if (tkn->u.sym == S_FUNCTION)
                            {
                                state = STATE_SUB;
                            }
                            else if (tkn->u.sym == S_FOR)
                            {
                                state = STATE_LOOP;
                            }
                            else if (tkn->u.sym == S_DO)
                            {
                                state = STATE_LOOP;
                            }
                            else if (tkn->u.sym == S_WHILE)
                            {
                                state = STATE_LOOP;
                            }
                            else if (tkn->u.sym == S_NEXT)
                            {
                                state = STATE_END;
                            }
                            else if (tkn->u.sym == S_LOOP)
                            {
                                state = STATE_END;
                            }
                            else if (tkn->u.sym == S_WEND)
                            {
                                state = STATE_END;
                            }
                            break;
                        case STATE_IF:
                            if (tkn->u.sym == S_THEN)
                            {
                                state = STATE_THEN;
                            }
                            break;
                        case STATE_ELSEIF:
                            if (tkn->u.sym == S_THEN)
                            {
                                state = STATE_ELSEIFTHEN;
                            }
                            break;
                        case STATE_THEN:
                        case STATE_ELSEIFTHEN:
                        case STATE_ELSE:
                        case STATE_END:
                        case STATE_SUB:
                        case STATE_LOOP:
                            break;
                        default:
                            assert(FALSE);
                    }
                    break;
                }
                case S_STRING:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '"';
                    style[pos++] = STYLE_STRING;
                    for (char *c=tkn->u.str; *c; c++)
                    {
                        buf[pos] = *c;
                        style[pos++] = STYLE_STRING;
                    }
                    buf[pos] = '"';
                    style[pos++] = STYLE_STRING;
                    break;
                case S_COLON:
                    buf[pos] = ' ';
                    style[pos++] = STYLE_NORMAL;
                    buf[pos] = ':';
                    style[pos++] = STYLE_NORMAL;
                    buf[pos] = ' ';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_SEMICOLON:
                    buf[pos] = ';';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_COMMA:
                    buf[pos] = ',';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_INUM:
                {
                    if (!first && (lastKind != S_MINUS))
                        buf[pos++] = ' ';
                    static char nbuf[64];
                    snprintf (nbuf, 64, "%d", tkn->u.literal.inum);
                    for (char *c=nbuf; *c; c++)
                    {
                        buf[pos] = *c;
                        style[pos++] = STYLE_NUMBER;
                    }
                    break;
                }
                case S_FNUM:
                {
                    if (!first && (lastKind != S_MINUS))
                        buf[pos++] = ' ';
                    static char nbuf[64];
                    snprintf (nbuf, 64, "%g", tkn->u.literal.fnum);
                    for (char *c=nbuf; *c; c++)
                    {
                        buf[pos] = *c;
                        style[pos++] = STYLE_NUMBER;
                    }
                    break;
                }
                case S_MINUS:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '-';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_LPAREN:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '(';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_RPAREN:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = ')';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_EQUALS:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '=';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_EXP:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '^';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_ASTERISK:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '*';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_SLASH:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '/';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_BACKSLASH:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '\\';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_PLUS:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '+';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_GREATER:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '>';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_LESS:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '<';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_NOTEQ:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '<';
                    style[pos++] = STYLE_NORMAL;
                    buf[pos] = '>';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_LESSEQ:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '<';
                    style[pos++] = STYLE_NORMAL;
                    buf[pos] = '=';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_GREATEREQ:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '>';
                    style[pos++] = STYLE_NORMAL;
                    buf[pos] = '=';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_POINTER:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '-';
                    style[pos++] = STYLE_NORMAL;
                    buf[pos] = '>';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_PERIOD:
                    buf[pos] = '.';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_AT:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '@';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_LBRACKET:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '[';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_RBRACKET:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = ']';
                    style[pos++] = STYLE_NORMAL;
                    break;
                case S_TRIPLEDOTS:
                    if (!first)
                        buf[pos++] = ' ';
                    buf[pos] = '.';
                    style[pos++] = STYLE_NORMAL;
                    buf[pos] = '.';
                    style[pos++] = STYLE_NORMAL;
                    buf[pos] = '.';
                    style[pos++] = STYLE_NORMAL;
                    break;
            }

            lastKind = tkn->kind;
            lastTkn = tkn;
            tkn = tkn->next;
            first = FALSE;
        }
    }
    buf[pos] = 0;

    return newLine (ed, buf, style, pre_indent, post_indent);
}

static void indentLine (IDE_line l)
{
    if (!l->prev)
    {
        l->indent = 0;
        return;
    }
    l->indent = l->prev->indent + l->prev->post_indent + l->pre_indent;
    if (l->indent < 0)
        l->indent = 0;
    dprintf ("identLine: l->prev->indent (%d) + l->prev->post_indent (%d) + l->pre_indent (%d) = %d\n",
             l->prev->indent, l->prev->post_indent, l->pre_indent, l->indent);
}

static void indentSuccLines (IDE_editor ed, IDE_line lp)
{
    IDE_line l = lp->next;
    while (l)
    {
        indentLine (l);
        l = l->next;
    }
    invalidateAll (ed);
}

static IDE_line commitBuf(IDE_editor ed)
{
    IDE_line cl = ed->cursor_line;
    IDE_line l  = buf2line (ed);
    if (cl->prev)
        cl->prev->next = l;
    else
        ed->line_first = l;
    if (cl->next)
        cl->next->prev = l;
    else
        ed->line_last = l;
    l->next = cl->next;
    l->prev = cl->prev;
    ed->editing = FALSE;
    int8_t old_indent = cl->indent;
    int8_t old_post_indent = cl->post_indent;
    freeLine (ed, cl);
    indentLine (l);
    if ( (l->indent == old_indent) && (l->post_indent == old_post_indent) )
        ed->up2date_row[ed->cursor_row - ed->scrolloff_row] = FALSE;
    else
        indentSuccLines (ed, l);
    return l;
}

static bool cursorUp(IDE_editor ed)
{
    IDE_line pl = ed->cursor_line->prev;
    if (!pl)
        return FALSE;

    if (ed->editing)
        commitBuf (ed);

    ed->cursor_line = pl;
    ed->cursor_row--;
    if (ed->cursor_col > pl->len)
    {
        ed->cursor_col = pl->len;
    }
    ed->up2date_il_pos = FALSE;

    return TRUE;
}

static bool cursorDown(IDE_editor ed)
{
    IDE_line nl = ed->cursor_line->next;
    if (!nl)
        return FALSE;

    if (ed->editing)
        commitBuf (ed);

    ed->cursor_line = nl;
    ed->cursor_row++;
    if (ed->cursor_col > nl->len)
        ed->cursor_col = nl->len;
    ed->up2date_il_pos = FALSE;

    return TRUE;
}

static bool cursorLeft(IDE_editor ed)
{
    if (ed->cursor_col == 0)
        return FALSE;

    ed->cursor_col--;
    ed->up2date_il_pos = FALSE;

    return TRUE;
}

static bool cursorRight(IDE_editor ed)
{
    int len = ed->editing ? ed->buf_len : ed->cursor_line->len + INDENT_SPACES * ed->cursor_line->indent;
    if (ed->cursor_col >= len)
        return FALSE;

    ed->cursor_col++;
    ed->up2date_il_pos = FALSE;

    return TRUE;
}

static void line2buf (IDE_editor ed, IDE_line l)
{
    uint16_t off = 0;
    for (int8_t i = 0; i<l->indent; i++)
    {
        for (int8_t j = 0; j<INDENT_SPACES; j++)
        {
            ed->buf[off] = ' ';
            ed->style[off++] = STYLE_NORMAL;
        }
    }

    memcpy (ed->buf+off,   l->buf  , l->len+1);
    memcpy (ed->style+off, l->style, l->len+1);
    ed->editing  = TRUE;
    if (!ed->changed)
    {
        ed->changed  = TRUE;
        ed->up2date_il_flags = FALSE;
    }
    ed->buf_len  = l->len+off;
}

static void repaintLine (IDE_editor ed, char *buf, char *style, uint16_t len, uint16_t row, uint16_t indent)
{
    // FIXME: horizontal scroll

    TE_moveCursor (row, 1);

    char s=STYLE_NORMAL;
    TE_setTextStyle (TE_STYLE_NORMAL);

    for (uint16_t i=0; i<indent; i++)
    {
        TE_putstr ("    ");
    }

    for (uint16_t i=0; i<len; i++)
    {
        char s2 = style[i];
        if (s2 != s)
        {
            switch (s2)
            {
                case STYLE_NORMAL:
                    TE_setTextStyle (TE_STYLE_NORMAL);
                    break;
                case STYLE_KW:
                    TE_setTextStyle (TE_STYLE_BOLD);
                    TE_setTextStyle (TE_STYLE_YELLOW);
                    break;
                case STYLE_STRING:
                    TE_setTextStyle (TE_STYLE_BOLD);
                    TE_setTextStyle (TE_STYLE_MAGENTA);
                    break;
                case STYLE_NUMBER:
                    TE_setTextStyle (TE_STYLE_BOLD);
                    TE_setTextStyle (TE_STYLE_MAGENTA);
                    break;
                case STYLE_COMMENT:
                    TE_setTextStyle (TE_STYLE_BOLD);
                    TE_setTextStyle (TE_STYLE_BLUE);
                    break;
                default:
                    assert(FALSE);
            }
            s = s2;
        }
        TE_putc (buf[i]);
    }
    TE_eraseToEOL();
}

static void repaint (IDE_editor ed)
{
    TE_setCursorVisible (FALSE);

    // cache first visible line for speed
    if (!ed->scrolloff_line || (ed->scrolloff_line_row != ed->scrolloff_row))
    {
        ed->scrolloff_line = getLine (ed, ed->scrolloff_row);
        ed->scrolloff_line_row = ed->scrolloff_row;
    }

    IDE_line l = ed->scrolloff_line;

    uint16_t linenum_end = ed->scrolloff_row + ed->window_height - 2;
    uint16_t linenum = ed->scrolloff_row;
    uint16_t row = 0;
    while (l && (linenum <= linenum_end))
    {
        if (!ed->up2date_row[row])
        {

            if (ed->editing && (linenum == ed->cursor_row))
                repaintLine (ed, ed->buf, ed->style, ed->buf_len, row + 1, 0);
            else
                repaintLine (ed, l->buf, l->style, l->len, row + 1, l->indent);
            ed->up2date_row[row] = TRUE;
        }
        linenum++;
        l = l->next;
        row++;
    }

    // infoline

    bool update_infoline = FALSE;
    if (!ed->up2date_il_pos)
    {
        _itoa (ed->cursor_col+1, ed->infoline + INFOLINE_CURSOR_X, 4);
        _itoa (ed->cursor_row+1, ed->infoline + INFOLINE_CURSOR_Y, 4);
        update_infoline = TRUE;
        ed->up2date_il_pos = TRUE;
    }
    if (!ed->up2date_il_num_lines)
    {
        _itoa (ed->num_lines, ed->infoline + INFOLINE_NUM_LINES, 4);
        update_infoline = TRUE;
        ed->up2date_il_num_lines = TRUE;
    }
    if (!ed->up2date_il_flags)
    {
        char *fs = ed->infoline + INFOLINE_CHANGED;
        if (ed->changed)
            *fs = '*';
        else
            *fs = ' ';
        update_infoline = TRUE;
        ed->up2date_il_flags = TRUE;
    }
    if (!ed->up2date_il_filename)
    {
        char *fs = ed->infoline + INFOLINE_FILENAME;
        for (char *c = ed->filename; *c; c++)
        {
            *fs++ = *c;
        }
        *fs = 0;
        update_infoline = TRUE;
        ed->up2date_il_filename = TRUE;
    }

    if (update_infoline)
    {
        dprintf ("outputInfoLine: row=%d, txt=%s\n", ed->infoline_row, ed->infoline);
        TE_setTextStyle (TE_STYLE_NORMAL);
        TE_setTextStyle (TE_STYLE_INVERSE);
        TE_moveCursor   (ed->infoline_row+1, 1);
        char *c = ed->infoline;
        int col = 1;
        while (*c && col < ed->window_width)
        {
            TE_putc (*c++);
            col++;
        }
        while (col < ed->window_width)
        {
            TE_putc (' ');
            col++;
        }
        TE_setTextStyle (TE_STYLE_NORMAL);
    }

    TE_moveCursor (ed->cursor_row-ed->scrolloff_row+1, ed->cursor_col-ed->scrolloff_col+1);
    TE_setCursorVisible (TRUE);
}

static void enterKey (IDE_editor ed)
{
    // split line ?
    uint16_t l = ed->editing ? ed->buf_len : ed->cursor_line->len;
    if (ed->cursor_col < l-1)
    {
        if (!ed->editing)
            line2buf (ed, ed->cursor_line);

        memcpy (ed->buf2,   ed->buf+ed->cursor_col  , l - ed->cursor_col);
        memcpy (ed->style2, ed->style+ed->cursor_col, l - ed->cursor_col);

        ed->buf_len = ed->cursor_col;
        ed->cursor_line = commitBuf (ed);

        l -= ed->cursor_col;

        memcpy (ed->buf,   ed->buf2  , l);
        memcpy (ed->style, ed->style2, l);
        ed->buf_len = l;
    }
    else
    {
        if (ed->editing)
            ed->cursor_line = commitBuf (ed);
        ed->buf_len = 0;
    }

    IDE_line line = buf2line (ed);
    insertLineAfter (ed, ed->cursor_line, line);
    ed->cursor_col = 0;
    ed->cursor_row++;
    ed->cursor_line = line;
    ed->num_lines++;
    ed->up2date_il_num_lines = FALSE;
    invalidateAll(ed);
    ed->up2date_il_pos = FALSE;
}

static void backspaceKey (IDE_editor ed)
{
    // join lines ?
    if (ed->cursor_col == 0)
    {
        if (!ed->cursor_line->prev)
            return;
        IDE_line cl = ed->cursor_line;
        if (ed->editing)
            cl = commitBuf (ed);
        IDE_line pl = cl->prev;
        line2buf (ed, pl);
        ed->cursor_col = ed->buf_len;
        ed->cursor_row--;
        ed->cursor_line = pl;
        ed->buf[ed->buf_len] = ' ';
        ed->style[ed->buf_len] = STYLE_NORMAL;
        ed->buf_len++;
        memcpy (ed->buf+ed->buf_len,   cl->buf  , cl->len);
        memcpy (ed->style+ed->buf_len, cl->style, cl->len);
        ed->buf_len += cl->len;
        ed->editing = TRUE;
        deleteLine (ed, cl);
        ed->cursor_line = commitBuf (ed);
        invalidateAll(ed);
        ed->up2date_il_pos = FALSE;
        ed->num_lines--;
        ed->up2date_il_num_lines = FALSE;
    }
    else
    {
        if (!ed->editing)
            line2buf (ed, ed->cursor_line);

        for (uint16_t i=ed->cursor_col; i<ed->buf_len; i++)
        {
            ed->buf[i-1]   = ed->buf[i];
            ed->style[i-1] = ed->style[i];
        }
        ed->buf_len--;
        ed->up2date_row[ed->cursor_row - ed->scrolloff_row] = FALSE;
        cursorLeft(ed);
    }
}

static bool printableAsciiChar (uint16_t c)
{
    return (c >= 32) && (c <= 126);
}

static bool insertChar (IDE_editor ed, uint16_t c)
{
    if (!printableAsciiChar(c))
        return FALSE;

    if (!ed->editing)
    {
        line2buf (ed, ed->cursor_line);
    }

    uint16_t cp = ed->scrolloff_col + ed->cursor_col;
    for (uint16_t i=ed->buf_len; i>cp; i--)
    {
        ed->buf[i]   = ed->buf[i-1];
        ed->style[i] = ed->style[i-1];
    }
    ed->buf[cp]   = c;
    ed->style[cp] = STYLE_NORMAL;
    ed->buf_len++;

    ed->up2date_row[ed->cursor_row - ed->scrolloff_row] = FALSE;

    cursorRight(ed);

    return TRUE;
}

static void IDE_save (IDE_editor ed)
{
    if (ed->editing)
        commitBuf (ed);

    FILE *sourcef = fopen(ed->filename, "w");
    if (!sourcef)
    {
        fprintf(stderr, "failed to write %s: %s\n\n", ed->filename, strerror(errno));
        exit(2);
    }

    static char *indent_str = "    ";
    static char *lf = "\n";

    for (IDE_line l = ed->line_first; l; l=l->next)
    {
        for (int8_t i = 0; i<l->indent; i++)
            fwrite (indent_str, 4, 1, sourcef);
        fwrite (l->buf, l->len, 1, sourcef);
        fwrite (lf, 1, 1, sourcef);
    }

    fclose (sourcef);

    ed->changed = FALSE;
    ed->up2date_il_flags = FALSE;
}

static void IDE_exit (IDE_editor ed)
{
    exit(0);
}

static void key_cb (uint16_t key, void *user_data)
{
	IDE_editor ed = (IDE_editor) user_data;

    switch (key)
    {
        case KEY_ESC:
        case KEY_CTRL_C:
        case KEY_CTRL_Q:
            IDE_exit(ed);

        case KEY_CURSOR_UP:
            cursorUp(ed);
            break;

        case KEY_CURSOR_DOWN:
            cursorDown(ed);
            break;

        case KEY_CURSOR_LEFT:
            cursorLeft(ed);
            break;

        case KEY_CURSOR_RIGHT:
            cursorRight(ed);
            break;

        case KEY_ENTER:
            enterKey(ed);
            break;

        case KEY_BACKSPACE:
            backspaceKey(ed);
            break;

        case KEY_CTRL_S:
            IDE_save(ed);
            break;

        default:
            if (!insertChar(ed, (uint8_t) key))
                TE_bell();
            break;

    }

    scroll(ed);
    repaint(ed);
    TE_flush();
}

IDE_editor openEditor(void)
{
	IDE_editor ed = U_poolAlloc (UP_ide, sizeof (*ed));

    strncpy (ed->infoline, INFOLINE, sizeof(ed->infoline));
    ed->infoline_row     = 0;
    ed->filename[0]      = 0;
    ed->line_first       = NULL;
    ed->line_last        = NULL;
    ed->num_lines	     = 0;
    ed->scrolloff_col    = 0;
    ed->scrolloff_row    = 0;
    ed->scrolloff_line   = NULL;
    ed->cursor_col		 = 0;
    ed->cursor_row		 = 0;
    ed->cursor_line      = NULL;
    ed->changed		     = FALSE;
    ed->editing          = FALSE;

    invalidateAll (ed);
    ed->up2date_il_pos       = FALSE;
    ed->up2date_il_num_lines = FALSE;
    ed->up2date_il_flags     = FALSE;
    ed->up2date_il_filename  = FALSE;

    initWindowSize(ed);

	return ed;
}

static void IDE_load (IDE_editor ed, char *sourcefn)
{
	FILE *sourcef = fopen(sourcefn, "r");
	if (!sourcef)
	{
		fprintf(stderr, "failed to read %s: %s\n\n", sourcefn, strerror(errno));
		exit(2);
	}

    ed->buf_len = 0;
    bool eof = FALSE;
    bool eol = FALSE;
    int linenum = 0;
    IDE_line lastLine = NULL;
    while (!eof)
    {
        char ch;
        int n = fread (&ch, 1, 1, sourcef);
        if (n==1)
        {
            ed->buf[ed->buf_len++] = ch;
            if ( (ed->buf_len==(MAX_LINE_LEN-1)) || (ch==10)  )
                eol = TRUE;
        }
        else
        {
            eof = TRUE;
            eol = TRUE;
        }

        if (eol)
        {
            ed->buf[ed->buf_len] = 0;
            IDE_line line = buf2line (ed);
            insertLineAfter (ed, lastLine, line);
            lastLine = line;
            linenum = (linenum+1) % ed->window_height;
            eol=FALSE;
            ed->num_lines++;
            ed->buf_len = 0;
        }
    }

    fclose(sourcef);

    strcpy (ed->filename, sourcefn);
    ed->cursor_col		 = 0;
    ed->cursor_row		 = 0;
    ed->cursor_line      = ed->line_first;

    indentSuccLines (ed, ed->line_first);
}

void IDE_open(char *fn)
{
    TE_init();

    // indentation support
    S_IF       = S_Symbol ("if", FALSE);
    S_ELSEIF   = S_Symbol ("elseif", FALSE);
    S_ELSE     = S_Symbol ("else", FALSE);
    S_THEN     = S_Symbol ("then", FALSE);
    S_END      = S_Symbol ("end", FALSE);
    S_SUB      = S_Symbol ("sub", FALSE);
    S_FUNCTION = S_Symbol ("function", FALSE);
    S_FOR      = S_Symbol ("for", FALSE);
    S_NEXT     = S_Symbol ("next", FALSE);
    S_DO       = S_Symbol ("do", FALSE);
    S_LOOP     = S_Symbol ("loop", FALSE);
    S_WHILE    = S_Symbol ("while", FALSE);
    S_WEND     = S_Symbol ("wend", FALSE);

#if DEBUG
    logf = fopen (LOG_FILENAME, "a");
#endif

	IDE_editor ed = openEditor();

    if (fn)
        IDE_load (ed, fn);

    TE_setCursorVisible (FALSE);
    TE_moveCursor (0, 0);
    TE_eraseDisplay();
    repaint(ed);
    TE_flush();

	TE_onKeyCall(key_cb, ed);

	TE_run();
}

