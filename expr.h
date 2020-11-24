#include "stdio.h"


/*
 * Division is represented as
 * a combination of t_inverse and t_multiply
 * 
 * Substraction is represented as
 * a combination of t_negate and t_add
 */
enum tag {
	t_constant = 1,
	t_variable,
	t_negate,
	t_inverse,
	t_add,
	t_multiply,
	t_less,
	t_greater,
	t_lessequal,
	t_greaterequal,
	t_equal,
	t_notequal,
	t_logical_and,
	t_logical_or,
	t_logical_not,
	t_MAX = t_logical_not,
};

struct tag_info {
    const char *tex_name;
    const char *name;
    int arity;
    int prio;
    enum tag alt_tag;
    const char *alt;
};
extern struct tag_info tags[];

#define MAX_PRIO 15

struct expr {
   enum tag tag;
   union {
       /* variable */
       char *id;
       /* constant value */
       double value;
       /* all other nodes
        * children are stored
        * in flexible array member */
       size_t n_child;
   };
   struct expr *children[];
};

enum format {
    fmt_tex,
    fmt_graph,
    fmt_string,
};

void dump_tree(FILE *out, enum format fmt, struct expr *expr);
struct expr *parse_tree(FILE *in);
void free_tree(struct expr *expr);
