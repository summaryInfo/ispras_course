#ifndef EXPR_IMPL_H_
#define EXPR_IMPL_H_

#include "expr.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#define EPS 1e-6

/**
 * Check if node can have children
 *
 * @param exp node to check
 * @return 1 if node can have children
 */
inline static bool has_childern(struct expr *exp) {
    return exp->tag != t_constant && exp->tag != t_variable;
}

/**
 * Check if node is constant
 *
 * @param exp node to check
 * @return 1 if node is constant
 */
inline static bool is_const(struct expr *exp) {
    return exp->tag == t_constant;
}

/**
 * Compare with 0
 *
 * @param x value to compare
 * @return 1 if close to 0
 */
inline static bool is_zero(double x) {
//TODO its better to detect epsilon dynamically or even better to support types
    return fabs(x) < EPS;
}

/**
 * Compare with constant
 *
 * @param[in] exp expression to compare
 * @param[in] x constant value to compare with
 * @return 1 if exp is constant and has value x
 */
inline static bool is_eq_const(struct expr *exp, double x) {
    return is_const(exp) && is_zero(exp->value - x);
}

/**
 * Create node with n children
 *
 * @param[in] tag node type
 * @param[in] n number of children
 * @return new node
 */
inline static struct expr *node_of_size(enum tag tag, size_t n) {
    struct expr *tmp = calloc(1, sizeof(*tmp) + n*sizeof(tmp));
    tmp->n_child = n;
    tmp->tag = tag;
    return tmp;
}

/**
 * Create variable node
 *
 * @param[in] var variable name
 * @return new node
 */
 inline static struct expr *var_node(char *var) {
    struct expr *tmp = malloc(sizeof(*tmp));
    assert(tmp);

    tmp->id = var;
    tmp->tag = t_variable;

    return tmp;

}

/**
 * Craete node with given children
 *
 * @param[in] tag node type
 * @param[in] nch number of children
 * @param[in] ... children
 * @return new node
 */
inline static struct expr *node(enum tag tag, size_t nch, ...) {
    struct expr *tmp = node_of_size(tag, nch);
    assert(tmp);

    va_list va;
    va_start(va, nch);

    for (size_t i = 0; i < nch; i++)
        tmp->children[i] = va_arg(va, struct expr *);

    va_end(va);

    return tmp;
}



/**
 * Create constant node
 *
 * @param[in] value constant
 * @return new node
 */
inline static struct expr *const_node(double value) {
    struct expr *tmp = malloc(sizeof(*tmp));
    assert(tmp);

    tmp->value = value;
    tmp->tag = t_constant;

    return tmp;
}

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

#endif

