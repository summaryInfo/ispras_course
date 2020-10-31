# External funtion:
.function void print_i
.param int num
# This is a test
.function int main
.local int a
.global int b 1
    ld.i $0
    jz.i label
	ld.i $2
    ld.i b
    add.i # Comment here
    inc.i
    ld.d $1.234
    drop.l
    dup.i
    st.i a
    dup.i
    call.i test2
    call.i test2
    call print_i
    call.i test2
    ret.i
label:
    ld.i $-3
    call print_i
    ld.i $0
    ret.i
.function int test2
.param int p
    ld.i p
    ret.i
.function int test3
    ld.i $0
    ret.i
