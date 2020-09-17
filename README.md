Evgeiny Onegin sorting
=========================

When you need write a small program as if it were a big project...

## Dependencies

* doxygen
* GNU make
* C++17 compatible compiler

## Example

To compile program:

    make

To run with default inputs:

    ./do_sort onegin.txt

Read from `a.txt` and write results to `forward.txt`, `backward.txt` and `original.txt`:

    ./do_sort a.txt forward.txt backward.txt original.txt

Run unit tests:

    ./do_sort test

## Notes

This needs to be compiled in UNIX-like environment.

You can use `make test` to run unit tests.

To compile program without test support use

    NDEBUG=1 make

To start program in single byte mode, change current locale:

    LC_ALL=C ./do_sort cp1251.txt
