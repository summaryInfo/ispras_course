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
.function int switch
.param int op
    ld.i op
    dup.i
    ld.i $0
    je.i case_0
    dup.i
    ld.i $1
    je.i case_1
    dup.i
    ld.i $2
    je.i case_2
    dup.i
tt: ld.i $3
    je.i case_3
    dup.i
    ld.i $4
    je.i case_4
    dup.i
    ld.i $5
    je.i case_5
    ld.i $6
    ret.i
case_0:
	ld.i $0
    ret.i
case_1:
	ld.i $1
    ret.i
case_2:
	ld.i $2
    ret.i
case_3:
	ld.i $3
    ret.i
case_4:
	ld.i $4
    ret.i
case_5:
	ld.i $5
    ret.i
