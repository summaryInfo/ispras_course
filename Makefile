CXXFLAGS += -std=c++17 -Wall -Wextra

ifdef NDEBUG
CXXFLAGS += -DNDEBUG=$(NDEBUG)
else
CXXFLAGS += -g
endif

all: do_sort

do_sort: do_sort.cpp Doxyfile
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@
	doxygen

do_sort: sort.hpp file_mapping.hpp unit.hpp

test: all
	./do_sort test

clean:
	rm -rf docs
	rm -f do_sort

.PHONY: all clean test
