Xtra small virtual machine
==========================

## Description

This an implementation of simple virtual machine with some infrastructure (assembler and disassembler).

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
