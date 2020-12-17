#ifndef EXPR_H_
#define EXPR_H_ 1

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>


struct strtab;
typedef uint32_t id_t;

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
    t_function,
    t_funcdef, // Ex. [x,y|x]f ===> funcdef "f"{statement{variable "x",variable "y"},variable "x"}
    t_MAX = t_funcdef,
};

/**
 * AST node structure
 */
struct expr {
   enum tag tag;
   union {
       /* only for variable or function */
       id_t id;
       /* only for constant value */
       double value;
   };
   /* children are stored
    * in flexible array member */
   size_t n_child;
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
void dump_tree(FILE *out, enum format fmt, struct expr *expr, struct strtab *stab, bool full);

/**
 * Build AST from string
 *
 * @param[inout] stab global string table
 * @param[in] str expression string
 * @return parsed AST
 */
struct expr *parse_tree(struct strtab *stab, const char *in);

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
 * @param[in] stab Global string table
 * @param[in] var Derivative variable
 * @param[in] optimize If set result will be optimized
 * @return new AST
 * 
 * @note you cannot use original tree after calling this
 */
struct expr *derive_tree(struct expr *exp, struct strtab *stab, id_t var);

/**
 * Optimize given AST
 * 
 * @param[in] exp AST to optimize
 * @praram[in] stab global string table
 * @return new AST
 * 
 * @note you cannot use original tree after calling this
 */
struct expr *optimize_tree(struct expr *exp, struct strtab *stab);

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
void generate_code(struct expr *exp, struct strtab *stab, FILE *out);

extern bool optimize;

#endif
