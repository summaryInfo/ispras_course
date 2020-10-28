CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Wno-unused-parameter
LDLIBS += -lstdc++

VM := xsvm
VMOBJ := ofile.o vm.o
ASM := xsas
ASMOBJ := ofile.o as.o

ifdef NDEBUG
CXXFLAGS += -DNDEBUG=$(NDEBUG)
else
CXXFLAGS += -Og
endif

all: $(VM) $(ASM)

clean:
	rm -rf *.o $(VM) $(ASM)
	
$(VM): $(VMOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(VMOBJ) $(LDLIBS) -o $@

$(ASM): $(ASMOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(ASMOBJ) $(LDLIBS) -o $@

vm.o: util.hpp vm.hpp
as.o: util.hpp vm.hpp
ofile.o: util.hpp vm.hpp

.PHONY: all clean run
