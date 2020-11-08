.function void print_i
.param int n
.function int fib
.param int n
    ld.i n
    dup.i
    ld.i $2
    jle.i one
    dec.i
    dup.i
    call.i fib
    swap.i
    dec.i
    call.i fib
    add.i
    ret.i
one:
    ld.i $1
    ret.i
.function void main
.local int ax
   ld.i $1
   st.i ax
next:
   ld.i ax
   dup.i
   call.i fib
   call print_i
   inc.i
   dup.i
   st.i ax
   ld.i $20
   jle.i next
   hlt
