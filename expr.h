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
    t_if,
    t_while,
    t_statement,
    t_assign,
    t_MAX = t_assign,
};

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
 * Calculate partial derivative of the tree using variable var
 * 
 * @param[in] exp Expression AST to derive
 * @param[in] var Derivative variable
 * @param[in] optimize If set result will be optimized
 * @return new AST
 * 
 * @note you cannot use original tree after calling this
 */
struct expr *derive_tree(struct expr *exp, const char *var);

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

/**
 * Generate assembly code for XSVM
 * 
 * @param[in] exp tree to compile
 * @param[inout] out file to output to
 */
void generate_code(struct expr *exp, FILE *out);

extern bool optimize;

#endif
