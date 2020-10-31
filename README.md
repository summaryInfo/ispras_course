Xtra small virtual machine
==========================

## Description

This an implementation of simple virtual machine with some infrastructure (assembler and disassembler).

## Features

* Functions
* Strict typing (int, long, float and double types)
* Builtin function support
* Code validation
* Local and global variables

## Dependencies

* GNU make
* C++14 compatible compiler

## Example

To compile programs:

    make

Usage:

    ./xsas test.xso test.xs
    ./xsdis test.xso test2.xs
    ./xsvm test.xso

## Assembly language

### Directives

    .function <type> <name>
    .local <type> <name>
    .global <type> <name>
    .param <type> <name>

`<type>` is one of `int`,`long`,`float`,`double` (and `void` for functions).
`<name>` is a name of entity (alphanumeric + `'_'` + `'.'`).

`.global` has an alternate syntax with initial value:

    .global <type> <name> <init>

### Commands

* `add.[dfli]` -- add two top elements
* `and.[il]` -- bitwise and
* `call` -- call void function
* `call.[dfli]` -- call non-void function
* `dec.[il]` -- decrement by 1
* `div.[dfli]` -- divide two top elements
* `drop.[il]` -- drop top element (polymorphic for types of same size)
* `drop2.[li]` -- drop two top elements (polymorphic for types of same size)
* `dup2.[li]` -- duplicate two top elements (polymorphic for types of same size)
* `hlt` -- stop execution
* `inc.[il]` -- increment by 1
* `jgz.[il]` -- compare and branch if greater than zero
* `jlz.[il]` -- compare and branch if less than zero
* `jl.[dfli]` --  compate and branch if less
* `jg.[dfli]` --  compate and branch if greater
* `jle.[dfli]` --  compate and branch if less or equal
* `jge.[dfli]` --  compate and branch if greater or equal
* `jmp <label>` -- unconditional branch
* `jnz.[il] <label>` -- compare and branch if not zero
* `jz.[il] <label>` -- compare branch if zero
* `je.[il] <label>` -- compate and branch if equal
* `jne.[il] <label>` -- compate and branch if not equal
* `ld.[ilfd] <label>` or `ld.[ilfd] $<const>` -- load local or global variable, parameter or immediate value
* `mul.[dfli`] -- multiply two top elements
* `neg.[dfli]` -- change sign
* `not.[il]` -- bitwise not
* `or.[il]` -- bitwise or
* `rem.[il]` -- division reminder (same semantics as in C++)
* `ret.[dfil]` -- return from non-void function
* `ret` -- return from void function
* `sar.[il]` -- arithmetic bitwise shift right
* `shl.[il]` -- logic bitwise shift left
* `shr.[il]` -- logic bitwise shift right
* `st.[dfil] <label>`  -- store value to local or global variable or parameter 
* `sub.[dfil]` -- substact two top elements
* `swap.i` -- swap two elements (polymorphic for float and int)
* `swap.l` -- swap two elements (polymorphic for double and long)
* `tcall` -- tail call (unimplemented)
* `tod.[fil]` -- convert to double
* `tof.[dil]` -- convert to float
* `toi.[dfl]` -- convert to int
* `tol.[dfi]` -- convert to long
* `xor.[il]` -- exclusive bitwise or
