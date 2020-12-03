#ifndef EXPR_H_
#define EXPR_H_ 1

#include <stdio.h>
#include <stdbool.h>

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
    t_power,
    t_log,
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
 * @param[in] full output should be a separate file
 */
void dump_tree(FILE *out, enum format fmt, struct expr *expr, bool full);

/**
 * Build AST from string
 *
 * @param[in] str expression string
 * @return parsed AST
 */
struct expr *parse_tree(const char *in);

/**
 * Free AST
 *
 * @praram[in] AST to free
 */
void free_tree(struct expr *expr);

/**
 * Calculate partial derivate of the tree using variable var
 * 
 * @param[in] exp Expression AST to derivate
 * @param[in] var Derivative variable
 * @param[in] optimize If set result will be optimized
 * @return new AST
 * 
 * @note you cannot use original tree after calling this
 */
struct expr *derivate_tree(struct expr *exp, const char *var, bool optimize);

/**
 * Optimize given AST
 * 
 * @param[in] exp AST to optimize
 * @return new AST
 * 
 * @note you cannot use original tree after calling this
 */
struct expr *optimize_tree(struct expr *exp);

/**
 * Set trace file and format
 * 
 * @param[in] file Output file
 * @param[in] format Output format
 */
void set_trace(FILE *file, enum format fmt);

#endif
