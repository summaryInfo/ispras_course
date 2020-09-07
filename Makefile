CXXFLAGS += -std=c++17 -Wall -Wextra

ifdef NDEBUG
CXXFLAGS += -DNDEBUG=$(NDEBUG)
else
CXXFLAGS += -g
endif

all: solve

solve: solve.cpp Doxyfile
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@
	doxygen

test: all
	./solve test

clean:
	rm -rf docs
	rm -f solve

.PHONY: all clean test
