#include "stdio.h"


/**
 * AST node type
 *
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

/**
 * Operation description
 */
struct tag_info {
    const char *tex_name; /** TeX operation name */
    const char *name; /** String/graph name */
    int arity; /** operation arity, -1 for variable */
    int prio; /** operation priority 0 to MAX_PRIO */
    enum tag alt_tag; /** alternate operation symbol child tag */
    const char *alt; /** alternate operation name */
};
extern struct tag_info tags[];

/** MAX priority */
#define MAX_PRIO 15

/**
 * AST node structure
 */
struct expr {
   enum tag tag;
   union {
       /* only for variable */
       char *id;
       /* only for constant value */
       double value;
       /* for all other nodes
        * children are stored
        * in flexible array member */
       size_t n_child;
   };
   struct expr *children[];
};

/**
 * Dump output format
 */
enum format {
    fmt_tex, /* Plain TeX format */
    fmt_graph, /* Graphvis dot format */
    fmt_string, /* Plain text string format */
};

/**
 * Dump AST to out stream in fmt format
 *
 * @param[out] out output file stream
 * @param[in] fmt output format
 * @param[in] expr expression AST to dump
 */
void dump_tree(FILE *out, enum format fmt, struct expr *expr);

/**
 * Build AST from string
 *
 * @param[in] str expression string
 * @return parsed AST
 */
struct expr *parse_tree(FILE *in);

/**
 * Free AST
 *
 * @praram[in] AST to free
 */
void free_tree(struct expr *expr);
