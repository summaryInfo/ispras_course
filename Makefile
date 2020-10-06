CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
PROG := teststack
OBJS := stack.o teststack.o

ifdef NDEBUG
CFLAGS += -DNDEBUG=$(NDEBUG)
else
CFLAGS += -g
endif

all: $(PROG)

run: all
	./$(PROG)

clean:
	rm -rf *.o $(PROG)
	
$(PROG): $(OBJS)
	$(CC) $(CCFLAGS) $(OBJS) $(LDFLAGS) -o $@

teststack.o: stack.h

.PHONY: all clean
