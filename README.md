A Parser Implementataion
========================

## Description

This is a math experesion parser.
Possible output formats are string (plain text), tex, graphviz dot format.

* `-f` specify output format. One of `tex`, `string`, `graph` (default is `string`)
* `-o` output file (default is stdout)

## Dependencies

* GNU make
* C11 compatible compiler

## Example

To compile program:

    make

Examples:

    ./parse -f tex -o out.tex "(1+x)/2"
    ./parse -f graph -o out.dot "x*y*x + --2"
    ./parse  -o out.txt "!(123 < 1 && x / 2 >= y)"
    ./parse "x + 2 < 4*x || x == 2"

Generate PNG file:

    ./parse -f graph "1-2+3" | dot -Tpng >| out.png

Generate PDF file:

## Notes

Use plain format for tex compilation.

To compile stack without advanced debugging use:

    NDEBUG=1 make

This needs to be compiled in UNIX-like environment.
