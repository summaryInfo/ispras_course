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
