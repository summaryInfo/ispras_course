Quadratic equation solver
=========================

When you need write a small program as if it were a big project...

## Dependencies

* doxygen
* GNU make
* C++17 compatible compiler

## Example

Get solutions for equation `1*x^2 + 2*x + 0`:

	./solve 1 2 0

Run unit tests:

	./solve test

## Notes

You can use `make test` to run unit tests.

To compile program without test support use

    NDEBUG=1 make
