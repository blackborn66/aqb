#include "terminal.h"

#include <stdlib.h>
#include <stdarg.h>
#include "logger.h"

#ifdef __amigaos__

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>

#include <devices/conunit.h>

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>

#include <proto/console.h>
#include <pragmas/console_pragmas.h>

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/mathffp_protos.h>
#include <clib/console_protos.h>

#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>
#include <inline/graphics.h>
#include <inline/mathffp.h>

#include <libraries/reqtools.h>
#include <inline/reqtools.h>

extern struct ExecBase      *SysBase;
extern struct DOSBase       *DOSBase;
extern struct DOSBase       *DOSBase;
extern struct IntuitionBase *IntuitionBase;
struct ReqToolsBase         *ReqToolsBase;

#define CSI       "\x9b"

#else

#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#define CSI       "\033["

#endif

#define DEBUG 0

#define BUFSIZE   2048
static char            g_outbuf[BUFSIZE];
static int             g_bpos = 0;
static void            (*g_size_cb)(void) = NULL;

static TE_key_cb       g_key_cb = NULL;
static void           *g_key_cb_user_data = NULL;
static uint16_t        g_scrollStart   = 0;
static uint16_t        g_scrollEnd     = 10;

#ifdef __amigaos__

#define NEWLIST(l) ((l)->lh_Head = (struct Node *)&(l)->lh_Tail, \
                    /*(l)->lh_Tail = NULL,*/ \
                    (l)->lh_TailPred = (struct Node *)&(l)->lh_Head)

static struct NewWindow g_nw =
{
    0, 0, 640, 200,
    -1,-1,                            /* detailpen, blockpen */
    CLOSEWINDOW,                      /* IDCMP */
    WINDOWDEPTH|WINDOWSIZING|
    WINDOWDRAG|WINDOWCLOSE|
    SMART_REFRESH|ACTIVATE,           /* window flags */
    NULL, NULL,
    (uint8_t *) "AQB",
    NULL,
    NULL,
    100,45,                           /* min width, height */
    1024, 768,                        /* max width, height */
    WBENCHSCREEN
};

static struct Window     *g_win           = NULL;
static struct IOStdReq   *g_writeReq      = NULL;
static struct MsgPort    *g_writePort     = NULL;
static struct IOStdReq   *g_readReq       = NULL;
static struct MsgPort    *g_readPort      = NULL;
static bool               g_ConOpened     = FALSE;
static struct FileHandle *g_output        = NULL;
static struct MsgPort    *g_IOport        = NULL;
static int                g_termSignalBit = 0;


static struct MsgPort *create_port(STRPTR name, LONG pri)
{
    struct MsgPort *port = NULL;
    UBYTE portsig;

    if ((BYTE)(portsig=AllocSignal(-1)) >= 0)
    {
        if (!(port=AllocMem(sizeof(*port),MEMF_CLEAR|MEMF_PUBLIC)))
        {
            FreeSignal(portsig);
        }
        else
        {
            port->mp_Node.ln_Type = NT_MSGPORT;
            port->mp_Node.ln_Pri  = pri;
            port->mp_Node.ln_Name = (char *)name;
            /* done via AllocMem
            port->mp_Flags        = PA_SIGNAL;
            */
            port->mp_SigBit       = portsig;
            port->mp_SigTask      = FindTask(NULL);
            NEWLIST(&port->mp_MsgList);
            if (port->mp_Node.ln_Name)
                AddPort(port);
        }
    }
    return port;
}
static void delete_port(struct MsgPort *port)
{
    if (port->mp_Node.ln_Name)
        RemPort(port);
    FreeSignal(port->mp_SigBit);
    FreeMem(port,sizeof(*port));
}
static struct IORequest *create_ext_io(struct MsgPort *port,LONG iosize)
{
    struct IORequest *ioreq = NULL;

    if (port && (ioreq=AllocMem(iosize,MEMF_CLEAR|MEMF_PUBLIC)))
    {
        ioreq->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
        ioreq->io_Message.mn_ReplyPort    = port;
        ioreq->io_Message.mn_Length       = iosize;
    }
    return ioreq;
}
static void delete_ext_io(struct IORequest *ioreq)
{
    LONG i;

    i = -1;
    ioreq->io_Message.mn_Node.ln_Type = i;
    ioreq->io_Device                  = (struct Device *)i;
    ioreq->io_Unit                    = (struct Unit *)i;
    FreeMem(ioreq,ioreq->io_Message.mn_Length);
}
static void queue_read(struct IOStdReq *readreq, UBYTE *whereto)
{
   readreq->io_Command = CMD_READ;
   readreq->io_Data = (APTR)whereto;
   readreq->io_Length = 1;
   SendIO((struct IORequest *)readreq);
}
static LONG con_may_get_char(struct MsgPort *msgport, UBYTE *whereto)
{
    struct IOStdReq *readreq;

    if (!(readreq = (struct IOStdReq *) GetMsg(msgport)))
		return -1;
    LONG temp = *whereto;                /* get the character */
    queue_read(readreq, whereto);     /* then re-use the request block */
    return temp;
}
static void cleanexit (char *s, uint32_t n)
{
    if (s)
        printf(s);
    exit(n);
}
static BYTE OpenConsole(void)
{
    BYTE error;

    g_writeReq->io_Data = (APTR) g_win;
    g_writeReq->io_Length = sizeof(struct Window);

    error = OpenDevice((uint8_t*)"console.device", 0, (struct IORequest*) g_writeReq, 0);

    g_readReq->io_Device = g_writeReq->io_Device;
    g_readReq->io_Unit   = g_writeReq->io_Unit;

    return error;
}
void TE_flush(void)
{
    if (g_bpos != 0)
    {
        g_writeReq->io_Command = CMD_WRITE;
        g_writeReq->io_Data    = (APTR) g_outbuf;
        g_writeReq->io_Length  = g_bpos;

        DoIO((struct IORequest *) g_writeReq);

        g_bpos = 0;
    }
}

// FIXME: implement size change callback

bool TE_getsize(uint16_t *rows, uint16_t *cols)
{
    struct ConUnit *g_con_unit = (struct ConUnit *) g_writeReq->io_Unit;
    *rows = g_con_unit->cu_YMax + 1;
    *cols = g_con_unit->cu_XMax + 1;

    /* range checks */

    if ( (*rows < 0) || (*rows > 256) )
    {
        *rows = 80;
        *rows = 24;
    }

    if (*cols < TE_MIN_COLUMNS)
        *cols = TE_MIN_COLUMNS;
    if (*cols > TE_MAX_COLUMNS)
        *cols = TE_MAX_COLUMNS;

    if (*rows < TE_MIN_ROWS)
        *rows = TE_MIN_ROWS;
    if (*rows > TE_MAX_ROWS)
        *rows = TE_MAX_ROWS;

    return TRUE;
}

struct FileHandle *TE_output (void)
{
    return g_output;
}

int TE_termSignal (void)
{
    return g_termSignalBit;
}

static void returnpacket(struct DosPacket *packet, long res1, long res2)
{
    struct Message *msg;
    struct MsgPort *replyport;

    packet->dp_Res1  = res1;
    packet->dp_Res2  = res2;
    replyport = packet->dp_Port;
    msg = packet->dp_Link;
    packet->dp_Port = g_IOport;
    msg->mn_Node.ln_Name = (char *)packet;
    msg->mn_Node.ln_Succ = NULL;
    msg->mn_Node.ln_Pred = NULL;

    PutMsg(replyport, msg);
}

static struct DosPacket *getpacket(void)
{
    struct Message *msg;
    msg = GetMsg(g_IOport);
    return ((struct DosPacket *)msg->mn_Node.ln_Name);
}


void TE_runIO (void)
{
    ULONG iosig   = 1 << g_IOport->mp_SigBit;
	ULONG termsig = 1 << g_termSignalBit;

    BOOL running = TRUE;
    while (running)
    {
        //LOG_printf (LOG_DEBUG, "term: TE_runIO: waiting for signal bits %d, %d ...\n", g_IOport->mp_SigBit, g_termSignalBit);
        ULONG signals = Wait(iosig | termsig);
        //LOG_printf (LOG_DEBUG, "term: TE_runIO: got signals: 0x%08x\n", signals);

        if (signals & iosig)
		{
			struct DosPacket *packet = getpacket();
			LOG_printf (LOG_DEBUG, "term: TE_runIO: got pkg, type=%d\n", packet->dp_Type);

			switch (packet->dp_Type)
			{
				case ACTION_WRITE:
				{
					LONG l = packet->dp_Arg3;
					char *buf = (char *)packet->dp_Arg2;
					//LOG_printf (LOG_DEBUG, "term: TE_runIO: ACTION_WRITE, len=%d\n", l);
					for (int i = 0; i<l; i++)
					{
                        char c = buf[i];
                        if (c=='\n')
                        {
                            uint16_t rows, cols;
                            TE_getsize(&rows, &cols);
                            TE_scrollUp (/*fullscreen=*/TRUE);
                            TE_moveCursor (rows+1, 1);
                            TE_eraseToEOL ();
                        }
                        else
                        {
                            TE_putc(c);
                        }
					}
					TE_flush();

					returnpacket (packet, l, packet->dp_Res2);
					break;
				}
				default:
					//LOG_printf (LOG_DEBUG, "term: TE_runIO: rejecting unknown packet type\n");
					returnpacket (packet, FALSE, ERROR_ACTION_NOT_KNOWN);
			}
		}
        else
        {
            if (signals & termsig)
            {
                running = FALSE;
            }
        }
	}
}

void TE_deinit(void)
{
    TE_flush();

    /* We always have an outstanding queued read request
     * so we must abort it if it hasn't completed,
     * and we must remove it.
     */
    if(!(CheckIO((struct IORequest *)g_readReq)))
		AbortIO((struct IORequest *)g_readReq);
    WaitIO((struct IORequest *)g_readReq);     /* clear it from our replyport */

    if (g_ConOpened)
        CloseDevice((struct IORequest *)g_writeReq);
    if (g_win)
        CloseWindow(g_win);
    if (g_readReq)
        delete_ext_io((struct IORequest *)g_readReq);
    if (g_readPort)
        delete_port(g_readPort);
    if (g_writeReq)
        delete_ext_io((struct IORequest *)g_writeReq);
    if (g_writePort)
        delete_port(g_writePort);
    if (ReqToolsBase)
        CloseLibrary((struct Library *)ReqToolsBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
}

static UBYTE g_ibuf;

bool TE_init (void)
{
    SysBase = *(APTR *)4L;
    if (!(IntuitionBase = (struct IntuitionBase *) OpenLibrary ((STRPTR)"intuition.library",0)))
         cleanexit("Can't open intuition.library\n", RETURN_FAIL);
    if (!(ReqToolsBase = (struct ReqToolsBase *) OpenLibrary ((STRPTR)REQTOOLSNAME, REQTOOLSVERSION)))
         cleanexit("Can't open reqtools.library\n", RETURN_FAIL);
    if (!(g_writePort = create_port((STRPTR)"AQB.console.write",0)))
         cleanexit("Can't create write port\n", RETURN_FAIL);
    if (!(g_writeReq = (struct IOStdReq *) create_ext_io(g_writePort,(LONG)sizeof(struct IOStdReq))))
         cleanexit("Can't create write request\n", RETURN_FAIL);
    if(!(g_readPort = create_port((STRPTR)"AQB.console.read",0)))
         cleanexit("Can't create read port\n", RETURN_FAIL);
    if(!(g_readReq = (struct IOStdReq *) create_ext_io(g_readPort,(LONG)sizeof(struct IOStdReq))))
         cleanexit("Can't create read request\n", RETURN_FAIL);
    if (!(g_win = OpenWindow(&g_nw)))
         cleanexit("Can't open window\n", RETURN_FAIL);

    if (OpenConsole ())
         cleanexit("Can't open console.device\n", RETURN_FAIL);
    g_ConOpened = TRUE;

    TE_putstr(CSI "12{"); /* window resize events activated */
    TE_putstr(CSI ">1l"); /* auto scroll mode deactivated */
    TE_flush();

    queue_read(g_readReq, &g_ibuf); /* send the first console read request */

    /* prepare fake i/o filehandles (for IDE console redirection) */

	if ( !(g_output = AllocMem (sizeof(struct FileHandle), MEMF_CLEAR|MEMF_PUBLIC)) )
	{
		LOG_printf (LOG_ERROR, "run: failed to allocate memory for output file handle!\n");
		exit(24);
	}

	if (!(g_IOport = create_port ((STRPTR) "aqb_io_port", 0)))
	{
		LOG_printf (LOG_ERROR, "run: failed to create i/o port!\n");
		exit(25);
	}

	g_output->fh_Type = g_IOport;
	g_output->fh_Port = 0;
	g_output->fh_Args = (long)g_output;
	g_output->fh_Arg2 = (long)0;

	g_termSignalBit = AllocSignal(-1);
	if (g_termSignalBit == -1)
	{
		LOG_printf (LOG_ERROR, "run: failed to allocate signal bit!\n");
		exit(23);
	}

	return TRUE;
}

// handle CSI sequences: state machine

typedef enum {ESC_idle, ESC_esc1, ESC_csi, ESC_tilde } ESC_state_t;

static inline void report_key (uint16_t key)
{
    if (g_key_cb)
        g_key_cb (key, g_key_cb_user_data);
}

static ESC_state_t g_esc_state = ESC_idle;
static uint16_t nextKey(void)
{
    UBYTE ch;

    ULONG conreadsig = 1 << g_readPort->mp_SigBit;
    ULONG windowsig  = 1 << g_win->UserPort->mp_SigBit;

    BOOL running = TRUE;
    while (running)
    {
        ULONG signals = Wait(conreadsig|windowsig);

        if (signals & conreadsig)
		{
			LONG lch;
            if ((lch = con_may_get_char(g_readPort, &g_ibuf)) != -1)
			{
                ch = lch;
                LOG_printf (LOG_DEBUG, "terminal: *** got ch: 0x%02x, state=%d\n", ch, g_esc_state);
                switch (g_esc_state)
                {
                    case ESC_idle:
                        if ((ch==0x9b)||(ch==0x1b))
                            g_esc_state = ESC_esc1;
                        else
                            return ch;
                        break;
                    case ESC_esc1:
                        switch (ch)
                        {
                            case '[':
                                g_esc_state = ESC_csi;
                                break;
                            case 'A':
                                g_esc_state = ESC_idle;
                                return KEY_CURSOR_UP;
                                break;
                            case 'B':
                                g_esc_state = ESC_idle;
                                return KEY_CURSOR_DOWN;
                                break;
                            case 'C':
                                g_esc_state = ESC_idle;
                                return KEY_CURSOR_RIGHT;
                                break;
                            case 'D':
                                g_esc_state = ESC_idle;
                                return KEY_CURSOR_LEFT;
                                break;
                            case '0':
                                g_esc_state = ESC_tilde;
                                return KEY_F1;
                                break;
                            case '1':
                                g_esc_state = ESC_tilde;
                                return KEY_F2;
                                break;
                            case '2':
                                g_esc_state = ESC_tilde;
                                return KEY_F3;
                                break;
                            case '3':
                                g_esc_state = ESC_tilde;
                                return KEY_F4;
                                break;
                            case '4':
                                g_esc_state = ESC_tilde;
                                return KEY_F5;
                                break;
                            case '5':
                                g_esc_state = ESC_tilde;
                                return KEY_F6;
                                break;
                            case '6':
                                g_esc_state = ESC_tilde;
                                return KEY_F7;
                                break;
                            case '7':
                                g_esc_state = ESC_tilde;
                                return KEY_F8;
                                break;
                            case '8':
                                g_esc_state = ESC_tilde;
                                return KEY_F9;
                                break;
                            case '9':
                                g_esc_state = ESC_tilde;
                                return KEY_F10;
                                break;
                            case '?':
                                g_esc_state = ESC_tilde;
                                return KEY_HELP;
                                break;
                            case 'T':
                                g_esc_state = ESC_idle;
                                return KEY_PAGE_UP;
                                break;
                            case 'S':
                                g_esc_state = ESC_idle;
                                return KEY_PAGE_DOWN;
                                break;
                            default:
                                g_esc_state = ESC_idle;
                                return KEY_UNKNOWN1;
                                break;
                        }
                        break;
                    case ESC_csi:
                        LOG_printf (LOG_DEBUG, "terminal: *** inside CSI sequence: 0x%02x\n", ch);
                        g_esc_state = ESC_idle;
                        break;
                    case ESC_tilde:
                        LOG_printf (LOG_DEBUG, "terminal: *** skipping tilde %c\n", ch);
                        g_esc_state = ESC_idle;
                        break;
                }
			}
		}

        if (signals & windowsig)
		{
			struct IntuiMessage *winmsg;
            while (winmsg = (struct IntuiMessage *)GetMsg(g_win->UserPort))
			{
                switch (winmsg->Class)
				{
                    case CLOSEWINDOW:
						running = FALSE;
						break;
                    default:
						break;
				}
                ReplyMsg((struct Message *)winmsg);
			}
		}
	}
    return KEY_CLOSE;
}

void TE_run (void)
{
    BOOL running = TRUE;
    while (running)
    {
        uint16_t key = nextKey();
        if (key == KEY_CLOSE)
            running = FALSE;
        else
            report_key (key);
	}
}

uint16_t TE_EZRequest (char *body, char *gadgets)
{
	ULONG res = rtEZRequest (body, gadgets, NULL, NULL);
	LOG_printf (LOG_DEBUG, "rtEZRequest result: %ld\n", res);
	return res;
}

void TE_setScrollArea (uint16_t row_start, uint16_t row_end)
{
    g_scrollStart = row_start;
    g_scrollEnd   = row_end;
}

void TE_scrollUp (bool fullscreen)
{
    if (!fullscreen)
    {
        //LOG_printf (LOG_DEBUG, "scroll up, g_scrollEnd=%d\n", g_scrollEnd);
        TE_printf ( CSI "%dt", g_scrollEnd);
    }
    TE_printf ( CSI "S");
    if (!fullscreen)
    {
        //TE_printf ( CSI "y");
        TE_printf ( CSI "t");
    }
}

void TE_scrollDown (void)
{
    //LOG_printf (LOG_DEBUG, "scroll down, g_scrollEnd=%d\n", g_scrollEnd);
    //TE_printf ( CSI "%dy", g_scrollStart+1);
    TE_printf ( CSI "%dt", g_scrollEnd);
    TE_printf ( CSI "T");
    //TE_printf ( CSI "y");
    TE_printf ( CSI "t");
}

uint16_t TE_waitkey (void)
{
    return nextKey();
}

#else // no __amigaos__ -> linux/posix/ansi

void TE_flush  (void)
{
    if (g_bpos != 0)
        write(STDOUT_FILENO, g_outbuf, g_bpos);
    g_bpos = 0;
}

static uint16_t TE_getch (void)
{
    int nread;
    char c, seq[5];
    while ((nread = read(STDIN_FILENO,&c,1)) == 0);
    if (nread == -1)
        exit(1);

    while(1)
    {
        if (c==KEY_ESC) // handle escape sequences
        {
            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): handling escape sequence...\n");
            /* If this is just an ESC, we'll timeout here. */
            if (read(STDIN_FILENO,seq,1) == 0)
            {
                LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 1\n");
                return KEY_ESC;
            }
            if (read(STDIN_FILENO,seq+1,1) == 0)
            {
                LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 2\n");
                return KEY_UNKNOWN1;
            }

            switch (seq[0])
            {
                case '[':       // ESC [ ...

                    switch (seq[1])
                    {
                        case '1':       // ESC [ 1 ...
                            if (read(STDIN_FILENO,seq+2,2) == 0)
                            {
                                LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 3\n");
                                return KEY_UNKNOWN1;
                            }
                            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x] %c [0x%02x] %c [0x%02x]\n", seq[0], seq[0], seq[1], seq[1], seq[2], seq[2], seq[3], seq[3]);
                            switch (seq[2])
                            {
                                case '5': return KEY_F5;
                                case '7': return KEY_F6;
                                case '8': return KEY_F7;
                                case '9': return KEY_F8;
                                case '~': return KEY_HOME;
                                case ';':
                                    if (read(STDIN_FILENO,seq+4,1) == 0)
                                    {
                                        LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 4\n");
                                        return KEY_UNKNOWN1;
                                    }
                                    switch (seq[4])
                                    {
                                        case 'A':
                                            return KEY_PAGE_UP;   // SHIFT + CURSOR_UP
                                        case 'B':
                                            return KEY_PAGE_DOWN; // SHIFT + CURSOR_DOWN
                                        default:
                                            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): unknown escape sequence detected: ESC %c [0x%02x] %c [0x%02x] %c [0x%02x] %c [0x%02x] %c [0x%02x]\n", seq[0], seq[0], seq[1], seq[1], seq[2], seq[2], seq[3], seq[3], seq[4], seq[4]);
                                    }
                                    return KEY_UNKNOWN2;
                            }
                            break;
                        case '2':       // ESC [ 2 ...
                            if (read(STDIN_FILENO,seq+2,1) == 0)
                            {
                                LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 4\n");
                                return KEY_ESC;
                            }
                            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x] %c [0x%02x]\n", seq[0], seq[0], seq[1], seq[1], seq[2], seq[2]);
                            switch (seq[2])
                            {
                                case '0': return KEY_F9;
                                case '1': return KEY_F10;
                                default: return KEY_UNKNOWN1;
                            }
                            break;
                        case '3':       // ESC [ 3 ...
                            if (read(STDIN_FILENO,seq+2,1) == 0)
                            {
                                LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 4\n");
                                return KEY_ESC;
                            }
                            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x] %c [0x%02x]\n", seq[0], seq[0], seq[1], seq[1], seq[2], seq[2]);
                            switch (seq[2])
                            {
                                case '~': return KEY_DEL;
                                default: return KEY_UNKNOWN1;
                            }
                            break;
                        case '4':       // ESC [ 4 ...
                            if (read(STDIN_FILENO,seq+2,1) == 0)
                            {
                                LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 4\n");
                                return KEY_ESC;
                            }
                            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x] %c [0x%02x]\n", seq[0], seq[0], seq[1], seq[1], seq[2], seq[2]);
                            switch (seq[2])
                            {
                                case '~': return KEY_END;
                                default: return KEY_UNKNOWN2;
                            }
                            break;
                        case '5':       // ESC [ 5 ...
                            if (read(STDIN_FILENO,seq+2,1) == 0)
                            {
                                LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 4\n");
                                return KEY_ESC;
                            }
                            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x] %c [0x%02x]\n", seq[0], seq[0], seq[1], seq[1], seq[2], seq[2]);
                            switch (seq[2])
                            {
                                case '~': return KEY_PAGE_UP;
                                default: return KEY_UNKNOWN3;
                            }
                            break;
                        case '6':       // ESC [ 6 ...
                            if (read(STDIN_FILENO,seq+2,1) == 0)
                            {
                                LOG_printf (LOG_DEBUG, "terminal: TE_getch(): ESC timeout 4\n");
                                return KEY_ESC;
                            }
                            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x] %c [0x%02x]\n", seq[0], seq[0], seq[1], seq[1], seq[2], seq[2]);
                            switch (seq[2])
                            {
                                case '~': return KEY_PAGE_DOWN;
                                default: return KEY_UNKNOWN4;
                            }
                            break;
                        case 'A': return KEY_CURSOR_UP;
                        case 'B': return KEY_CURSOR_DOWN;
                        case 'C': return KEY_CURSOR_RIGHT;
                        case 'D': return KEY_CURSOR_LEFT;
                        default:
                            LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x] *** UNKNOWN ***\n", seq[0], seq[0], seq[1], seq[1]);
                            return KEY_UNKNOWN5;
                    }
                    break;
                case 'O':       // ESC O ...
                    LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x]\n", seq[0], seq[0], seq[1], seq[1]);
                    switch(seq[1])
                    {
                        case 'H': return KEY_HOME;
                        case 'F': return KEY_END;
                        case 'P': return KEY_F1;
                        case 'Q': return KEY_F2;
                        case 'R': return KEY_F3;
                        case 'S': return KEY_F4;
                        default: return KEY_UNKNOWN6;
                    }
                    break;
                default:
                    LOG_printf (LOG_DEBUG, "terminal: TE_getch(): escape sequence detected: ESC %c [0x%02x] %c [0x%02x] *** UNKNOWN ***\n", seq[0], seq[0], seq[1], seq[1]);
                    return KEY_UNKNOWN7;

            }
        }
        else
        {
            if (c==127)
                return KEY_BACKSPACE;
            return c;
        }
    }
    return 0;
}

uint16_t TE_waitkey (void)
{
    return TE_getch();
}

static void handleSigWinCh(int unused __attribute__((unused)))
{
    if (g_size_cb)
        g_size_cb();
}

static void TE_setAlternateScreen (bool enabled)
{
    if (enabled)
        TE_printf (CSI "?1049h");
    else
        TE_printf (CSI "?1049l");
}

static struct termios g_orig_termios;
void TE_deinit (void)
{
    TE_setTextStyle (TE_STYLE_NORMAL);
    TE_moveCursor(0, 0);
    TE_eraseDisplay();
    TE_setAlternateScreen(FALSE);
    TE_flush();
	// disable raw mode
	tcsetattr(STDOUT_FILENO, TCSAFLUSH, &g_orig_termios);
}

bool TE_init (void)
{
    signal(SIGWINCH, handleSigWinCh);

	// enable raw mode
    struct termios raw;

    if (!isatty(STDIN_FILENO))
		goto fatal;
    if (tcgetattr(STDOUT_FILENO, &g_orig_termios) == -1)
		goto fatal;

    raw = g_orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw) < 0)
		goto fatal;
    return TRUE;

fatal:
    errno = ENOTTY;
    return FALSE;
}

static bool getCursorPosition(uint16_t *rows, uint16_t *cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
		return FALSE;

    while (i < sizeof(buf)-1)
	{
        if (read(STDIN_FILENO,buf+i,1) != 1)
			break;
        if (buf[i] == 'R')
			break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != KEY_ESC || buf[1] != '[')
		return FALSE;
    if (sscanf(buf+2,"%hd;%hd",rows,cols) != 2)
		return FALSE;
    return TRUE;
}


bool TE_getsize(uint16_t *rows, uint16_t *cols)
{
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
        uint16_t orig_row, orig_col;

        if (!getCursorPosition(&orig_row, &orig_col))
			goto failed;

        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			goto failed;
        if (!getCursorPosition(rows, cols))
			goto failed;


        char seq[32];
        snprintf(seq, 32, "\x1b[%d;%dH", orig_row, orig_col);
        write(STDOUT_FILENO, seq, strlen(seq));
    }
	else
	{
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    }
	return TRUE;

failed:
    return FALSE;
}

void TE_run(void)
{
    bool running = TRUE;
    while (running)
    {
        uint16_t ch = TE_getch();
        LOG_printf (LOG_DEBUG, "terminal: TE_getch() returned %d\n", ch);
        if (g_key_cb)
            g_key_cb (ch, g_key_cb_user_data);
    }
}

uint16_t TE_EZRequest (char *body, char *gadgets)
{
    uint16_t rows, cols;

    TE_getsize (&rows, &cols);

    TE_scrollUp(/*fullscreen=*/TRUE);
    TE_scrollUp(/*fullscreen=*/TRUE);

    TE_moveCursor (rows, 1);

    TE_setTextStyle (TE_STYLE_NORMAL);

    for (char *p = body; *p; p++)
    {
        if (*p != '\n')
        {
            TE_putc(*p);
        }
        else
        {
            TE_scrollUp(/*fullscreen=*/TRUE);
            TE_moveCursor (rows, 1);
        }
    }
    TE_scrollUp(/*fullscreen=*/TRUE);
    TE_moveCursor (rows, 1);

    uint16_t cnt;
    char *c = gadgets;
    static char buf[256];
    char *s = buf;
    while (*c)
    {
        if (*c=='|')
        {
            cnt++;
            *s = 0;
            TE_printf ("%s", buf);
            TE_printf ("[%d] ", cnt);
            c++;
            s = buf;
        }
        else
        {
            *s++ = *c++;
        }
    }
    *s = 0;
    TE_printf ("%s", buf);
    TE_printf ("[0]", cnt);
    cnt++;

    TE_flush();

    uint16_t res = 0;
    bool running = TRUE;
    while (running)
    {
        uint16_t ch = TE_getch();

        switch(ch)
        {
			case 13:
                res = 1;
                running = FALSE;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                res = ch - '0';
                running = FALSE;
                break;
        }
    }

    TE_eraseDisplay ();
    return res;
}

void TE_setScrollArea (uint16_t row_start, uint16_t row_end)
{
    g_scrollStart = row_start;
    g_scrollEnd   = row_end;
    TE_printf (CSI "%d;%dt", row_start, row_end);
}

void TE_scrollUp (bool fullscreen)
{
    if (fullscreen)
        TE_printf (CSI "t");

    TE_printf ( CSI "S");

    if (fullscreen)
        TE_printf (CSI "%d;%dt", g_scrollStart, g_scrollEnd);
}

void TE_scrollDown (void)
{
    TE_printf ( CSI "T");
}

#endif

void TE_putc(char c)
{
    g_outbuf[g_bpos++] = c;
    if (g_bpos >= BUFSIZE)
        TE_flush();
}

void TE_putstr(char *s)
{
    while (*s)
    {
        g_outbuf[g_bpos++] = *s++;
        if (g_bpos >= BUFSIZE)
            TE_flush();
    }
}

void TE_printf (char* format, ...)
{
    va_list args;
    va_start(args, format);
    TE_vprintf (format, args);
    va_end(args);
}

void TE_vprintf (char* format, va_list args)
{
    static char buf[BUFSIZE];
    vsnprintf (buf, BUFSIZE, format, args);
    TE_putstr(buf);
}

void TE_onSizeChangeCall (void (*cb)(void))
{
    g_size_cb = cb;
}

void TE_moveCursor (int row, int col)
{
    TE_printf (CSI "%d;%d;H", row, col);
}

void TE_eraseToEOL (void)
{
    TE_putstr (CSI "K");
}

void TE_eraseDisplay (void)
{
    TE_moveCursor (1, 1);
    TE_putstr (CSI "J");
}

void TE_bell (void)
{
    TE_putstr ("\007");
}

void TE_setCursorVisible (bool visible)
{
#ifdef __amiga__
    if (visible)
        TE_putstr ( CSI " p");
    else
        TE_putstr ( CSI "0 p");
#else
    if (visible)
        TE_putstr ( CSI "?25h");
    else
        TE_putstr ( CSI "?25l");
#endif
}

void TE_setTextStyle (int style)
{
    TE_printf ( CSI "%dm", style);
}

void TE_onKeyCall (TE_key_cb cb, void *user_data)
{
    g_key_cb           = cb;
    g_key_cb_user_data = user_data;
}

