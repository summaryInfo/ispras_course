#include <stdio.h>
#include <stdlib.h>

#define ELEMENT_TYPE int
#include "stack.h"

static void print_help(void) {
    printf("Supported commands:\n"
           "\t q  --  quit\n"
           "\t d  -- duplicate top element\n"
           "\t p  -- pop top element\n"
           "\t s  -- print stack size\n"
           "\t t  -- print stack top element\n"
           "\t<N> -- push <N> (in any C notation) onto the stack\n");
}

int main(void) {
    struct stack_int stk = create_stack_int(0);
    if (!stk.data) return EXIT_FAILURE;

	for (;;) {
    	char cmd = 0;
    	int data = 0;
        if (scanf(" %i", &data) == 1) {
        	stack_push_int(&stk, &data);
    	} else if (scanf(" %c", &cmd) == 1) {
        	switch(cmd) {
            case 'q' /* quit */:
            	return 0;
            case 'd' /* dup */:
                data = stack_top_int(&stk);
            	stack_push_int(&stk, &data);
            	break;
            case 'p' /* pop */:
                printf("dropped = %d\n", stack_pop_int(&stk));
                break;
            case 's' /* size */:
                printf("size = %ld\n", stack_size_int(&stk));
                break;
            case 't' /* top */:
                printf("top = %d\n", stack_top_int(&stk));
                break;
            default:
                printf("Unknown command\n");
                /* fallthrough */
            case 'h':
                print_help();
        	}
    	}
	}

	return EXIT_SUCCESS;
}
