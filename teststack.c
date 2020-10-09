#include <stdio.h>
#include <stdlib.h>

#define ELEMENT_TYPE int
#define STACK_NAME i
//#define SILENT_WITH_DEFAULT 0
#include "stack.h"

// Element type is undefined in stack.h
// for security reasons
#define ELEMENT_TYPE int
#define ELEMENT_TYPE_DEFAULT 0
#define PRI_STACK "i"
#define SCN_STACK "i"

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
    DECLARE_STACK(stk, int, i, 0);
    if (!stk.data) return EXIT_FAILURE;

    ELEMENT_TYPE data = ELEMENT_TYPE_DEFAULT;
    for (int res; (res = scanf(" %"SCN_STACK, &data)) != EOF;) {
        char cmd = 0;

        if (res == 1) {
            stack_push_i(&stk, &data);
        } else if (scanf(" %c", &cmd) == 1) {
            switch(cmd) {
            case 'q' /* quit */:
                goto free_and_exit;
            case 'd' /* dup */:
                data = stack_top_i(&stk);
                stack_push_i(&stk, &data);
                break;
            case 'p' /* pop */:
                printf("dropped = %"PRI_STACK"\n", stack_pop_i(&stk));
                break;
            case 's' /* size */:
                printf("size = %ld\n", stack_size_i(&stk));
                break;
            case 't' /* top */:
                printf("top = %"PRI_STACK"\n", stack_top_i(&stk));
                break;
            default:
                printf("Unknown command\n");
                /* fallthrough */
            case 'h':
                print_help();
            }
        }
    }

free_and_exit:
    free_stack_i(&stk);
    return EXIT_SUCCESS;
}
