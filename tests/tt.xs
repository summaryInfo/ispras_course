.function void print_i
.param int n
.function void main
.local int ax
   ld.i $1
   st.i ax
next:
   ld.i ax
   dup.i
   mul.i
   call print_i
   ld.i ax
   inc.i
   dup.i
   st.i ax
   ld.i $10
   jle.i next
   hlt
