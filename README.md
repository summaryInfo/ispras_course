A Parser Implementataion
========================

## Description

This is a math experesion parser.
Possible output formats are string (plain text), tex, graphviz dot format.

* `-f` specify output format. One of `tex`, `string`, `graph` (default is `string`)
* `-o` output file (default is stdout)
* `-i` input file (default is command list argument)

## Dependencies

* GNU make
* C11 compatible compiler
* tectonic for tex comilation (optional) or texlive (untested)
* zathura as PDF viewer (optional)
* sxiv as image viewer (optional)

## Example

To compile program:

    make

Examples:

    ./parse -f tex -o out.tex "(1+x)/2"
    ./parse -f graph -o out.dot "x*y*x + --2"
    ./parse  -o out.txt "!(123 < 1 && x / 2 >= y)"
    ./parse "x + 2 < 4*x || x == 2"
    echo "1+2/3" > file.txt && ./parse -i file.txt

Generate PNG file:

    ./parse -f graph "1-2+3" | dot -Tpng >| out.png

Using wrapper:

    ./display tex "(1+x)/2"
    ./display graph "x*y*x + --2"
    ./display string "!(123 < 1 && x / 2 >= y)"

## Notes

Use plain format for tex compilation.

To compile stack without advanced debugging use:

    NDEBUG=1 make

This needs to be compiled in UNIX-like environment.
