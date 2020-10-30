CXXFLAGS += -std=c++14 -Wall -Wextra -Wpedantic -Wno-unused-parameter
LDLIBS += -lstdc++

VM ?= xsvm
VMOBJ ?= ofile.o vm.o
ASM := xsas
ASMOBJ ?= ofile.o as.o
DIS ?= xsdis
DISOBJ := ofile.o disas.o

ifdef NDEBUG
CXXFLAGS += -DNDEBUG=$(NDEBUG) -O2
else
CXXFLAGS += -Og
endif

all: $(VM) $(ASM) $(DIS)

clean:
	rm -rf *.o $(VM) $(ASM) $(DIS)
	
$(VM): $(VMOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(VMOBJ) $(LDLIBS) -o $@
$(ASM): $(ASMOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(ASMOBJ) $(LDLIBS) -o $@
$(DIS): $(DISOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DISOBJ) $(LDLIBS) -o $@

vm.o: util.hpp vm.hpp ofile.hpp
as.o: util.hpp ofile.hpp
disas.o: util.hpp ofile.hpp
ofile.o: util.hpp ofile.hpp

.PHONY: all clean run
