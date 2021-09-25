'
' _AQB module interface
'

OPTION EXPLICIT
OPTION PRIVATE

' PRINT statement support:

PUBLIC DECLARE SUB _aio_puts    (BYVAL fno AS UINTEGER, BYVAL s AS STRING  )
PUBLIC DECLARE SUB _aio_puts1   (BYVAL fno AS UINTEGER, BYVAL b AS BYTE    )
PUBLIC DECLARE SUB _aio_puts2   (BYVAL fno AS UINTEGER, BYVAL i AS INTEGER )
PUBLIC DECLARE SUB _aio_puts4   (BYVAL fno AS UINTEGER, BYVAL l AS LONG    )
PUBLIC DECLARE SUB _aio_putu1   (BYVAL fno AS UINTEGER, BYVAL u AS UBYTE   )
PUBLIC DECLARE SUB _aio_putu2   (BYVAL fno AS UINTEGER, BYVAL u AS UINTEGER)
PUBLIC DECLARE SUB _aio_putu4   (BYVAL fno AS UINTEGER, BYVAL u AS ULONG   )
PUBLIC DECLARE SUB _aio_putf    (BYVAL fno AS UINTEGER, BYVAL f AS SINGLE  )
PUBLIC DECLARE SUB _aio_putbool (BYVAL fno AS UINTEGER, BYVAL b AS BOOLEAN )
PUBLIC DECLARE SUB _aio_putnl   (BYVAL fno AS UINTEGER)
PUBLIC DECLARE SUB _aio_puttab  (BYVAL fno AS UINTEGER)

' [ LINE ] INPUT support:

PUBLIC DECLARE SUB _aio_line_input             (BYVAL prompt AS STRING, BYREF s AS STRING, BYVAL do_nl AS BOOLEAN)
PUBLIC DECLARE SUB _aio_console_input          (BYVAL qm AS BOOLEAN, BYVAL prompt AS STRING, BYVAL do_nl AS BOOLEAN)
PUBLIC DECLARE SUB _aio_inputs1                (BYREF v AS BYTE    )
PUBLIC DECLARE SUB _aio_inputu1                (BYREF v AS UBYTE   )
PUBLIC DECLARE SUB _aio_inputs2                (BYREF v AS INTEGER )
PUBLIC DECLARE SUB _aio_inputu2                (BYREF v AS UINTEGER)
PUBLIC DECLARE SUB _aio_inputs4                (BYREF v AS LONG    )
PUBLIC DECLARE SUB _aio_inputu4                (BYREF v AS ULONG   )
PUBLIC DECLARE SUB _aio_inputf                 (BYREF v AS SINGLE  )
PUBLIC DECLARE SUB _aio_inputs                 (BYREF v AS STRING  )
PUBLIC DECLARE SUB _aio_set_dos_cursor_visible (BYVAL visible AS BOOLEAN)


PUBLIC DECLARE SUB      LOCATE  (BYVAL l AS INTEGER=-1, BYVAL c AS INTEGER=-1)
PUBLIC DECLARE FUNCTION CSRLIN  () AS INTEGER
PUBLIC DECLARE FUNCTION POS     (BYVAL dummy AS INTEGER) AS INTEGER

' file i/o

PUBLIC CONST AS UINTEGER FILE_MODE_RANDOM      = 0
PUBLIC CONST AS UINTEGER FILE_MODE_INPUT       = 1
PUBLIC CONST AS UINTEGER FILE_MODE_OUTPUT      = 2
PUBLIC CONST AS UINTEGER FILE_MODE_APPEND      = 3
PUBLIC CONST AS UINTEGER FILE_MODE_BINARY      = 4

PUBLIC CONST AS UINTEGER FILE_ACCESS_READ      = 0
PUBLIC CONST AS UINTEGER FILE_ACCESS_WRITE     = 1
PUBLIC CONST AS UINTEGER FILE_ACCESS_READWRITE = 2

PUBLIC DECLARE SUB _aio_open  (BYVAL fname AS STRING, BYVAL mode AS UINTEGER, BYVAL access AS UINTEGER, BYVAL fno AS UINTEGER, BYVAL recordlen AS UINTEGER)
PUBLIC DECLARE SUB _aio_close (BYVAL fno AS UINTEGER)

' --------------------------------------------------------------------------------------------------------
' --
' -- AmigaBASIC like screens, windows and graphics
' --
' --------------------------------------------------------------------------------------------------------

' error codes

PUBLIC CONST AS INTEGER ERR_WIN_OPEN          = 101
PUBLIC CONST AS INTEGER ERR_SCREEN_OPEN       = 102
PUBLIC CONST AS INTEGER ERR_PALETTE           = 103
PUBLIC CONST AS INTEGER ERR_COLOR             = 104
PUBLIC CONST AS INTEGER ERR_AREA              = 105
PUBLIC CONST AS INTEGER ERR_PATTERN           = 106
PUBLIC CONST AS INTEGER ERR_WIN_CLOSE         = 107
PUBLIC CONST AS INTEGER ERR_WIN_OUTPUT        = 108
PUBLIC CONST AS INTEGER ERR_SCREEN_CLOSE      = 109
PUBLIC CONST AS INTEGER ERR_PAINT             = 110
PUBLIC CONST AS INTEGER ERR_LINE              = 111
PUBLIC CONST AS INTEGER ERR_PSET              = 112
PUBLIC CONST AS INTEGER ERR_INPUT_OUT_OF_DATA = 113
PUBLIC CONST AS INTEGER AE_ON_TIMER_CALL      = 114
PUBLIC CONST AS INTEGER AE_TIMER_ON           = 115
PUBLIC CONST AS INTEGER AE_TIMER_OFF          = 116
PUBLIC CONST AS INTEGER AE_OPEN               = 117
PUBLIC CONST AS INTEGER AE_OUTPUT             = 118
PUBLIC CONST AS INTEGER AE_CLOSE              = 119
PUBLIC CONST AS INTEGER AE_MOUSE              = 120

' screen modes

PUBLIC CONST AS INTEGER AS_MODE_LORES                = 1
PUBLIC CONST AS INTEGER AS_MODE_HIRES                = 2
PUBLIC CONST AS INTEGER AS_MODE_LORES_LACED          = 3
PUBLIC CONST AS INTEGER AS_MODE_HIRES_LACED          = 4
PUBLIC CONST AS INTEGER AS_MODE_HAM                  = 5
PUBLIC CONST AS INTEGER AS_MODE_EXTRAHALFBRITE       = 6
PUBLIC CONST AS INTEGER AS_MODE_HAM_LACED            = 7
PUBLIC CONST AS INTEGER AS_MODE_EXTRAHALFBRITE_LACED = 8

PUBLIC DECLARE SUB SCREEN (BYVAL id AS INTEGER, BYVAL width AS INTEGER, BYVAL height AS INTEGER, BYVAL depth AS INTEGER, BYVAL mode AS INTEGER, BYVAL title AS STRING=NULL)
PUBLIC DECLARE SUB SCREEN CLOSE (BYVAL id AS INTEGER)


' window flags

PUBLIC CONST AS INTEGER AW_FLAG_SIZE       =  1
PUBLIC CONST AS INTEGER AW_FLAG_DRAG       =  2
PUBLIC CONST AS INTEGER AW_FLAG_DEPTH      =  4
PUBLIC CONST AS INTEGER AW_FLAG_CLOSE      =  8
PUBLIC CONST AS INTEGER AW_FLAG_REFRESH    = 16
PUBLIC CONST AS INTEGER AW_FLAG_BACKDROP   = 32
PUBLIC CONST AS INTEGER AW_FLAG_BORDERLESS = 64

' WINDOW id [, [Title] [, [(x1,y1)-(x2,y2)] [, [Flags] [, Screen] ] ]
PUBLIC DECLARE SUB WINDOW (BYVAL id AS INTEGER, BYVAL title AS STRING = NULL, _
                           _COORD2(BYVAL s1 AS BOOLEAN=FALSE, BYVAL x1 AS INTEGER=-1, BYVAL y1 AS INTEGER=-1, BYVAL s2 AS BOOLEAN=FALSE, BYVAL x2 AS INTEGER=-1, BYVAL y2 AS INTEGER=-1), _
                           BYVAL flags AS INTEGER=15, BYVAL scrid AS INTEGER = 0)

PUBLIC DECLARE SUB WINDOW CLOSE         (BYVAL id AS INTEGER = 0)
PUBLIC DECLARE SUB WINDOW OUTPUT        (BYVAL id AS INTEGER = 0)
PUBLIC DECLARE SUB ON WINDOW CALL       (BYVAL p AS SUB)
PUBLIC DECLARE FUNCTION WINDOW          (BYVAL n AS INTEGER) AS ULONG
PUBLIC DECLARE SUB MOUSE ON
PUBLIC DECLARE SUB MOUSE OFF
PUBLIC DECLARE SUB ON MOUSE CALL        (BYVAL p AS SUB)
PUBLIC DECLARE FUNCTION MOUSE           (BYVAL n AS INTEGER) AS INTEGER
PUBLIC DECLARE SUB MOUSE MOTION ON
PUBLIC DECLARE SUB MOUSE MOTION OFF
PUBLIC DECLARE SUB ON MOUSE MOTION CALL (BYVAL p AS SUB)
PUBLIC DECLARE SUB SLEEP
PUBLIC DECLARE SUB SLEEP FOR            (BYVAL s AS SINGLE)

PUBLIC DECLARE SUB CLS
PUBLIC DECLARE SUB LINE                 (_COORD2(BYVAL s1 AS BOOLEAN=FALSE, BYVAL x1 AS INTEGER=-1, BYVAL y1 AS INTEGER=-1, BYVAL s2 AS BOOLEAN=FALSE, BYVAL x2 AS INTEGER=-1, BYVAL y2 AS INTEGER=-1), _
                                         BYVAL c AS INTEGER=-1, _LINEBF(BYVAL bf AS INTEGER=0) )
PUBLIC DECLARE SUB PSET                 (_COORD(BYVAL s AS BOOLEAN=FALSE, BYVAL x1 AS INTEGER, BYVAL y AS INTEGER), BYVAL c AS INTEGER=-1)
PUBLIC DECLARE SUB PALETTE              (BYVAL cid AS INTEGER, BYVAL red AS SINGLE, BYVAL green AS SINGLE, BYVAL blue AS SINGLE)
PUBLIC DECLARE SUB COLOR                (BYVAL fg AS INTEGER=-1, BYVAL bg AS INTEGER=-1, BYVAL o AS INTEGER=-1)
PUBLIC DECLARE SUB PAINT                (_COORD(BYVAL s AS BOOLEAN=FALSE, BYVAL x AS INTEGER, BYVAL y AS INTEGER), BYVAL pc AS INTEGER=-1, BYVAL bc AS INTEGER=-1)
PUBLIC DECLARE SUB AREA                 (_COORD(BYVAL s AS BOOLEAN=FALSE, BYVAL x1 AS INTEGER, BYVAL y AS INTEGER))
PUBLIC DECLARE SUB AREAFILL             (BYVAL mode AS INTEGER=0)
PUBLIC DECLARE SUB AREA OUTLINE         (BYVAL enabled AS BOOLEAN)

PUBLIC DECLARE SUB PATTERN              (BYVAL lineptrn AS UINTEGER = &HFFFF, BYREF areaptrn() AS INTEGER = NULL)
PUBLIC DECLARE SUB PATTERN RESTORE

PUBLIC DECLARE FUNCTION BITMAP          (BYVAL width AS INTEGER, BYVAL height AS INTEGER, BYVAL depth AS INTEGER) AS VOID PTR
PUBLIC DECLARE FUNCTION GET             (_COORD2(BYVAL s1 AS BOOLEAN=FALSE, BYVAL x1 AS INTEGER=-1, BYVAL y1 AS INTEGER=-1, BYVAL s2 AS BOOLEAN=FALSE, BYVAL x2 AS INTEGER=-1, BYVAL y2 AS INTEGER=-1)) AS VOID PTR

PUBLIC DECLARE FUNCTION INKEY$          ()

' ON TIMER support

PUBLIC DECLARE SUB ON TIMER CALL        (BYVAL id AS INTEGER, BYVAL seconds AS SINGLE, BYVAL p AS SUB)
PUBLIC DECLARE SUB TIMER ON             (BYVAL id AS INTEGER)
PUBLIC DECLARE SUB TIMER OFF            (BYVAL id AS INTEGER)

