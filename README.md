Super safe stack implementation
===============================

## Description

This is an implementation of safe error-checking stack using:

* crc32 check sums
* Memory protection
* Opaque implementation
* Thread safety
* Memory issues control

## Dependencies

* GNU make
* C11 compatible compiler

## Example

To compile program:

    make

To run (interactive):

    make run

## Notes

Linux seccomp filter with eBPF needs to be implemented to prevent
unlocking memory for writing from outside of internal functions.

No tests this time, since it would simplify attempts to break the stack.

To compile stack without advanced debugginh use:

    NDEBUG=1 make

This needs to be compiled in UNIX-like environment.
