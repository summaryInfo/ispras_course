CXXFLAGS += -std=c++17 -Wall -Wextra

all: solve

solve: solve.cpp
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@
	doxygen

test: all
	./solve test

clean:
	rm -rf docs
	rm -f solve

.PHONY: all clean test
