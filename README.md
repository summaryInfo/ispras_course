A Parser Implementataion
========================

## Description

![alt text](https://github.com/summaryInfo/ispras_course/raw/parser/data/1.png "Generated TeX")

![alt text](https://github.com/summaryInfo/ispras_course/raw/parser/data/2.png "Generated graph image")


This is a math experesion parser.
Possible output formats are string (plain text), tex, graphviz dot format.

* `-f` specify output format. One of `tex`, `string`, `graph` (default is `string`)
* `-o` output file (default is stdout)
* `-i` input file (default is command list argument)
* `-D` trace output file (default is output file)
* `-F` trace format
* `-O` optimize
* `-c` comptile

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
    ./parse -O "(x^x+x*(x+y))'x"
    echo "1+2/3" > file.txt && ./parse -i file.txt

Generate PNG file:

    ./parse -f graph "1-2+3" | dot -Tpng >| out.png

Compile to assembly:

    ./parse -ct.xs "if 1 > 2 then x else log(4)/log(2)"
    ./parse -cg.xs "x=0;while x < 25 do x = x + 1;x^0.5"

Using wrapper:

    ./display tex "(1+x)/2"
    ./display graph "x*y*x + --2"
    ./display string "!(123 < 1 && x / 2 >= y)"
    ./display tex "0+1+2/3^5^3 > x^(1/2)
    ./display optimize-graph "(log (x+1))'x"
    ./display optimize-tex "1+2/3"
    ./display optimize-graph "if (2;4;5) then 6 else 7"
    ./display graph "if (2;4;5) then 6 else 7"
    ./display tex "if 1 then 2 else while x do y;x+1"

## Notes

Use plain format for tex compilation.

To compile stack without advanced debugging use:

    NDEBUG=1 make

This needs to be compiled in UNIX-like environment.
