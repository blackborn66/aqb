'
' ExecList test
'

OPTION EXPLICIT

IMPORT Collections

DIM AS CExecList a = CExecList(NT_USER)

'
' test Add / GetAt
'

FOR i AS INTEGER = 0 TO 9
    a.Add(i)
NEXT i

FOR i AS INTEGER = 0 TO 9
    DIM j AS INTEGER = a.GetAt(i)
    'TRACE j
    ASSERT j = i
NEXT i

' test SetAt/GetAt

FOR i AS INTEGER = 0 TO 9
    a.SetAt(i, i*i)
NEXT i

FOR i AS INTEGER = 0 TO 9
    DIM j AS INTEGER = a.GetAt(i)
    'TRACE j
    ASSERT j = i*i
NEXT i

'' test clone
'
'DIM b AS CArray PTR = CAST(CArray PTR, a.Clone())
'
'FOR i AS INTEGER = 0 TO 9
'    DIM j AS INTEGER = b->GetAt(i)
'    ' TRACE j
'    ASSERT j = i*i
'NEXT i

' test Count

' TRACE a.Count
ASSERT a.Count = 10

' test Contains, IndexOf

ASSERT a.Contains(64)
ASSERT a.Contains(81)
ASSERT NOT a.Contains(65)
ASSERT a.IndexOf(64)=8
ASSERT a.IndexOf(65)=-1

' test IsReadOnly, IsFixedSize

ASSERT NOT a.IsReadOnly
ASSERT NOT a.IsFixedSize

' test Insert

a.Insert (2, 23)
'FOR i AS INTEGER = 0 TO 9
'    DIM j AS INTEGER = a.GetAt(i)
'    TRACE j
'NEXT i
'TRACE

ASSERT a.Contains(64)
ASSERT NOT a.Contains(128)
ASSERT a.Contains(23)
ASSERT a.IndexOf(23)=2
ASSERT a.IndexOf(64)=9
ASSERT a.Count = 11

' test Remove, RemoveAt

a.RemoveAt(9)
'FOR i AS INTEGER = 0 TO 9
'    DIM j AS INTEGER = a.GetAt(i)
'    TRACE j
'NEXT i
'TRACE

ASSERT NOT a.Contains(64)
ASSERT NOT a.Contains(128)
ASSERT a.Contains(23)
ASSERT a.IndexOf(49)=8
ASSERT a.IndexOf(64)=-1
ASSERT a.GetAt(9)=81
ASSERT a.Count = 10

a.Remove(25)
'FOR i AS INTEGER = 0 TO 9
'    DIM j AS INTEGER = a.GetAt(i)
'    TRACE j
'NEXT i
ASSERT NOT a.Contains(64)
ASSERT NOT a.Contains(25)
ASSERT a.Contains(23)
ASSERT a.IndexOf(49)=7
ASSERT a.Count = 9

' test enumeration

DIM e AS IEnumerator PTR

e = a.GetEnumerator()

'TRACE "enumerating..."

DIM AS INTEGER cnt=0, sum=0

WHILE e->MoveNext()
    DIM AS INTEGER i = e->Current
    'TRACE "element: "; i
    cnt=cnt+1
    sum=sum+i
WEND

'TRACE "done. sum=";sum;", cnt=";cnt

ASSERT cnt=9
ASSERT sum=219

'
' test Reset
'

e->Reset()

cnt=0 : sum=0
WHILE e->MoveNext()
    DIM AS INTEGER i = e->Current
    'TRACE "element: "; i
    cnt=cnt+1
    sum=sum+i
WEND

'TRACE "done. sum=";sum;", cnt=";cnt

ASSERT cnt=9
ASSERT sum=219

