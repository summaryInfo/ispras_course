.function double scan_d

.function double sqrt_d
.param double n

.function void print_d
.param double n

.global double eps 0.0001

.function double abs
.param double x
    ld.d x
    dup.l
    ld.d $0
    jg.d 1
    neg.d
1:  ret.d

.function void main

.local double a
.local double b
.local double c

    call.d scan_d
    st.d a
    call.d scan_d
    st.d b
    call.d scan_d
    st.d c

    ld.d a
    call.d abs
    ld.d eps
    jl.d one_root

# Two roots
    ld.d b
    ld.d b
    mul.d
    ld.d $4
    ld.d a
    mul.d
    ld.d c
    mul.d
    sub.d
    call.d sqrt_d

    dup.l

    ld.d b
    sub.d
    ld.d $2
    ld.d a
    mul.d
    div.d
    call print_d

    ld.d b
    swap.l
    add.d
    neg.d
    ld.d $2
    ld.d a
    mul.d
    div.d
    call print_d
    ret

one_root:
    ld.d c
    neg.d
    ld.d b

    dup.l
    call.d abs
    ld.d eps
    jl.d no_or_inf

    div.d
    call print_d
    ret

no_or_inf:
    drop.l
    call.d abs
    ld.d eps
    jl.d infinite
    ret
infinite:
    # TODO
    ret
