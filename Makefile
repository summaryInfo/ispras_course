CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wno-missing-field-initializers
PROG := parse
OBJS := expr.o dump.o main.o

ifdef NDEBUG
CFLAGS += -O2 -flto -DNDEBUG=$(NDEBUG)
else
CFLAGS += -Og
endif

all: $(PROG)

run: all
	./$(PROG)

clean:
	rm -rf *.o $(PROG)
	
$(PROG): $(OBJS)
	$(CC) $(CCFLAGS) $(OBJS) $(LDFLAGS) -o $@

expr.o: expr.h
main.o: expr.h
dump.o: expr.h

.PHONY: all clean run
