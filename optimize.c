#define _POSIX_C_SOURCE 200809L

#include "expr.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *tfile = NULL;
enum format tfmt = fmt_string;

#define EPS 1e-6

inline static bool has_childern(struct expr *exp) {
    return exp->tag != t_constant && exp->tag != t_variable;
}

inline static bool is_const(struct expr *exp) {
    return exp->tag == t_constant;
}

inline static bool is_zero(double x) {
    return fabs(x) < EPS;
}

inline static bool is_eq_const(struct expr *exp, double x) {
    return is_const(exp) && is_zero(exp->value - x);
}

inline static struct expr *node_of_size(enum tag tag, size_t n) {
    struct expr *tmp = calloc(1, sizeof(*tmp) + n*sizeof(tmp));
    tmp->n_child = n;
    tmp->tag = tag;
    return tmp;
}

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


inline static struct expr *const_node(double value) {
    struct expr *tmp = malloc(sizeof(*tmp));
    assert(tmp);

    tmp->value = value;
    tmp->tag = t_constant;

    return tmp;
}

inline static struct expr *var_node(const char *var) {
    struct expr *tmp = malloc(sizeof(*tmp));
    assert(tmp);

    tmp->id = strdup(var);
    tmp->tag = t_variable;

    return tmp;

}

struct expr *deep_copy(struct expr *exp) {
    struct expr *res;
    switch (exp->tag) {
    case t_constant:
        res = const_node(exp->value);
        break;
    case t_variable:
        res = var_node(exp->id);
        break;
    default:
        res = node_of_size(exp->tag, exp->n_child);
        for (size_t i = 0; i < exp->n_child; i++)
            res->children[i] = deep_copy(exp->children[i]);
    }
    return res;
}

static int cmp_tree_rec(const struct expr *aexp, const struct expr *bexp) {
    if (aexp->tag != bexp->tag) return aexp->tag - bexp->tag;
    else switch(aexp->tag) {
    case t_constant:
        if (is_zero(aexp->value - bexp->value)) return 0;
        return aexp->value < bexp->value ? -1 :
               aexp->value > bexp->value;
    case t_variable:
        return strcmp(aexp->id, bexp->id);
    default:
        if (aexp->n_child != bexp->n_child)
            return (ssize_t)aexp->n_child - bexp->n_child;
        for (size_t i = 0; i < aexp->n_child; i++) {
            int res = cmp_tree_rec(aexp->children[i], bexp->children[i]);
            if (res) return res;
        }
        return 0;
    }
}

static int cmp_tree(const void *a, const void *b) {
    const struct expr *aexp = *(const struct expr **)a;
    const struct expr *bexp = *(const struct expr **)b;
    return cmp_tree_rec(aexp, bexp);
}

static void sort_tree(struct expr *exp) {
    if (has_childern(exp)) {
        for (size_t i = 0; i < exp->n_child; i++)
            sort_tree(exp->children[i]);
        switch(exp->tag) {
        case t_less: case t_lessequal:
        case t_greater: case t_greaterequal:
            if (cmp_tree_rec(exp->children[0], exp->children[1]) < 0) {
                struct expr *tmp = exp->children[0];
                exp->children[0] = exp->children[1];
                exp->children[1] = tmp;
                exp->tag = (enum tag[]){
                    [t_less] = t_greater,
                    [t_lessequal] = t_greaterequal,
                    [t_greater] = t_less,
                    [t_greaterequal] = t_lessequal,
                }[exp->tag];
            }
            //fallthrough
        case t_power:
            break;
        default:
            qsort(exp->children, exp->n_child, sizeof(exp), cmp_tree);
        }
    }
}

struct expr *derivate_tree(struct expr *exp, const char *var, bool optimize) {
    static bool nested = 0;
    bool cnested = nested;
    nested = 1;

    struct expr *res = exp;
    switch (exp->tag) {
    case t_constant:
        free(exp);
        res = const_node(0);
        break;
    case t_log: {
        res = node(t_inverse, 1, deep_copy(exp->children[0]));
        res = node(t_multiply, 2, derivate_tree(exp->children[0], var, optimize), res);
        free(exp);
        break;
    }
    case t_variable: {
        res = const_node(!strcmp(var, exp->id));
        free_tree(exp);
        break;
    }
    case t_power:
        // For power first handle special cases
        if (is_const(exp->children[0])) {
            res = node(t_multiply, 2, exp, const_node(log(exp->children[0]->value)));
        } else if (is_const(exp->children[1])) {
            res = const_node(exp->children[1]->value);
            exp->children[1]->value -= 1;
            res = node(t_multiply, 2, res, exp);
        } else {
            struct expr *res1, *res2;
            // Oof... thats complex
            res1 = node(t_multiply, 2, node(t_log, 1, deep_copy(exp->children[0])),
                        derivate_tree(deep_copy(exp->children[1]), var, optimize));
            res2 = node(t_multiply, 2, deep_copy(exp->children[1]),
                        derivate_tree(node(t_log, 1, deep_copy(exp->children[0])), var, optimize));
            res = node(t_multiply, 2, exp, node(t_add, 2, res1, res2));
        }
        break;
    case t_multiply: {
        res = node_of_size(t_add, exp->n_child);
        for (size_t i = 0; i < exp->n_child; i++) {
            res->children[i] = node_of_size(t_multiply, exp->n_child);
            for (size_t j = 0; j < exp->n_child; j++) {
                res->children[i]->children[j] = (i != j) ? deep_copy(exp->children[j]):
                    derivate_tree(deep_copy(exp->children[j]), var, optimize);
            }
        }
        free_tree(exp);
        break;
    }
    case t_inverse: {
        res = node(t_inverse, 1, node (t_negate, 1, node(t_power, 2, deep_copy(exp->children[0]), const_node(2))));
        res = node(t_multiply, 2, derivate_tree(exp->children[0], var, optimize), res);
        free(exp);
        break;
    }
    case t_add:
    case t_negate:
    case t_less: /* Just ignore comparisons and derivate subexpressions */
    case t_greater:
    case t_lessequal:
    case t_greaterequal:
    case t_equal:
    case t_notequal:
    case t_logical_and:
    case t_logical_or:
    case t_logical_not:
        for (size_t i = 0; i < exp->n_child; i++)
            exp->children[i] = derivate_tree(exp->children[i], var, optimize);
    }

    if (tfile) dump_tree(tfile, tfmt, res, !optimize);
    if (optimize) res = optimize_tree(res);

    nested = cnested;

    if (tfile && !nested && tfmt == fmt_tex) {
        fputs("\\bye\n", tfile);
    }
    return res;

}

inline static bool remove_child(struct expr *exp, struct expr *rem, double c) {
    for (size_t i = 0; i < exp->n_child; i++) {
        if (!cmp_tree_rec(exp->children[i], rem)) {
            free_tree(exp->children[i]);
            exp->children[i] = const_node(c);
            return 1;
        }
    }
    return 0;
}

struct expr *eliminate_common(struct expr *exp) {
    if (has_childern(exp)) {
        // Recursively handle children first
        for (size_t i = 0; i < exp->n_child; i++)
            exp->children[i] = eliminate_common(exp->children[i]);
    }

    struct expr *res = exp;
    enum tag tag = exp->tag;

    switch(tag) {
    case t_power: {
        if (exp->children[0]->tag == t_power) {
            struct expr *tmp = exp->children[0];
            exp->children[1] = node(t_multiply, 2, exp->children[1], tmp->children[1]);
            exp->children[0] = tmp->children[0];
            free(tmp);
        }
        break;
    }
    case t_add: {
        for (size_t i = 0; i < exp->n_child; i++) {
            for (size_t j = 0; j < exp->n_child; j++) {
                if (i == j) continue;
                if (exp->children[j]->tag == t_negate &&
                    !cmp_tree_rec(exp->children[j]->children[0], exp->children[i])) {
                    free_tree(exp->children[i]);
                    free_tree(exp->children[j]);
                    exp->children[i] = const_node(0);
                    exp->children[j] = const_node(0);
                } else if (!cmp_tree_rec(exp->children[i],exp->children[j])) {
                    free_tree(exp->children[j]);
                    exp->children[j] = const_node(0);
                    exp->children[i] = node(t_multiply, 2, const_node(2), exp->children[i]);
                } else if (exp->children[i]->tag == t_multiply &&
                           exp->children[j]->tag == t_multiply) {
                    struct expr *tmp = node_of_size(t_multiply, exp->children[i]->n_child + 1);
                    tmp->n_child = 0;
                    for (size_t k = 0; k < exp->children[i]->n_child; k++) {
                        if (remove_child(exp->children[j], exp->children[i]->children[k], 1)) {
                            tmp->children[tmp->n_child++] = exp->children[i]->children[k];
                            exp->children[i]->children[k] = const_node(1);
                        }
                    }
                    if (tmp->n_child) {
                        tmp->children[tmp->n_child++]  = node(t_add, 2, exp->children[i], exp->children[j]);
                        exp->children[i] = realloc(tmp, sizeof(*tmp) + tmp->n_child*sizeof(tmp));
                        assert(exp->children[i]);
                        exp->children[j] = const_node(0);
                    } else free(tmp);
                } else if (exp->children[i]->tag == t_multiply) {
                    if (remove_child(exp->children[i], exp->children[j], 1)) {
                        exp->children[i] = node(t_multiply, 2, exp->children[j],
                                                node(t_add, 2, const_node(1), exp->children[i]));
                        exp->children[j] = const_node(0);
                    }
                }
                sort_tree(exp);
            }
        }
        sort_tree(exp);
        break;
    }
    case t_multiply: {
        for (size_t i = 0; i < exp->n_child; i++) {
            for (size_t j = 0; j < exp->n_child; j++) {
                if (i == j) continue;
                if (exp->children[j]->tag == t_inverse &&
                    !cmp_tree_rec(exp->children[j]->children[0], exp->children[i])) {
                    free_tree(exp->children[i]);
                    free_tree(exp->children[j]);
                    exp->children[i] = const_node(1);
                    exp->children[j] = const_node(1);
                } else if (!cmp_tree_rec(exp->children[i], exp->children[j])) {
                    exp->children[i] = node(t_power, 2, exp->children[i], const_node(2));
                    free_tree(exp->children[j]);
                    exp->children[j] = const_node(1);
                } else if (exp->children[i]->tag == t_power && exp->children[j]->tag == t_power &&
                           !cmp_tree_rec(exp->children[i]->children[0], exp->children[j]->children[0])) {
                    exp->children[i] = node(t_power, 2, exp->children[i]->children[0],
                                            node(t_add, 2, exp->children[i]->children[1], exp->children[j]->children[1]));
                    free_tree(exp->children[j]->children[0]);
                    free(exp->children[j]);
                    exp->children[j] = const_node(1);
                } else if (exp->children[i]->tag == t_power &&
                           !cmp_tree_rec(exp->children[i]->children[0], exp->children[j])) {
                    exp->children[i]->children[1] = node(t_add, 2, exp->children[i]->children[1], const_node(1));
                    free_tree(exp->children[j]);
                    exp->children[j] = const_node(1);
                }
                sort_tree(exp);
            }
        }
        break;
    }
    case t_less:
    case t_greater:
    case t_lessequal:
    case t_greaterequal:
    case t_equal:
    case t_notequal: {
        // TODO
    }
    //fallthrough
    default:
        break;
    }

    return res;
}

static struct expr *fold_constants(struct expr *exp) {
    if (has_childern(exp)) {
        // Recursively fold children first
        for (size_t i = 0; i < exp->n_child; i++)
            exp->children[i] = fold_constants(exp->children[i]);
    }

    struct expr *res = exp;
    enum tag tag = exp->tag;

    switch(tag) {
    case t_constant:
    case t_variable:
        break;
    case t_log:
        assert(exp->n_child == 1);
        if (is_const(exp->children[0])) {
            res = const_node(log(exp->children[0]->value));
            free_tree(exp);
        }
        break;
    case t_power:
        assert(exp->n_child == 2);
        if (is_const(exp->children[0]) && is_const(exp->children[1])) {
            res = const_node(pow(exp->children[0]->value, exp->children[1]->value));
            free_tree(exp);
        } else if (is_eq_const(exp->children[1], 1)) {
            res = exp->children[0];
            free_tree(exp->children[1]);
            free(exp);
        } else if (is_eq_const(exp->children[0], 1)) {
            free_tree(exp);
            res = const_node(1);
        } else if (is_eq_const(exp->children[1], -1)) {
            res = node(t_multiply, 1, node(t_inverse, 1, exp->children[0]));
            free_tree(exp->children[1]);
            free(exp);
        }
        break;
    case t_negate:
        assert(exp->n_child == 1);
        if (exp->children[0]->tag == t_constant) {
            res = const_node(-exp->children[0]->value);
            free_tree(exp);
        }
        break;
    case t_inverse:
        assert(exp->n_child == 1);
        if (is_const(exp->children[0])) {
            res = const_node(1/exp->children[0]->value);
            free_tree(exp);
        }
        break;
    case t_add: {
        double acc = 0;
        size_t newn = 0;
        for (size_t i = 0; i < exp->n_child; i++) {
            if (is_const(exp->children[i])) {
                acc += exp->children[i]->value;
                free(exp->children[i]);
            } else {
                exp->children[newn++] = exp->children[i];
            }
        }
        if (!is_zero(acc)) {
            exp->children[newn++] = const_node(acc);
        }
        if (newn == 1) {
            res = exp->children[0];
            free(exp);
        } else {
            res = realloc(exp, sizeof(*exp) + newn*sizeof(exp));
            assert(res);
            res->n_child = newn;
        }
        break;
    }
    case t_multiply: {
        double acc = 1;
        size_t newn = 0;
        for (size_t i = 0; i < exp->n_child; i++) {
            if (is_const(exp->children[i])) {
                acc *= exp->children[i]->value;
                free(exp->children[i]);
            } else if (exp->children[i]->tag == t_inverse &&
                       is_const(exp->children[i]->children[0])){
                acc /= exp->children[i]->children[0]->value;
                free_tree(exp->children[i]);
            } else {
                exp->children[newn++] = exp->children[i];
            }
        }
        if (!is_zero(acc - 1)) {
            exp->children[newn++] = const_node(acc);
        }
        exp->n_child = newn;
        if (is_zero(acc)) {
            res = const_node(0);
            free_tree(exp);
        } else if (newn == 1 && exp->children[0]->tag != t_inverse) {
            res = exp->children[0];
            free(exp);
        } else {
            res = realloc(exp, sizeof(*exp) + newn*sizeof(exp));
            assert(res);
            res->n_child = newn;
        }
        break;
    }
    // Well, these rules are not strictly math, but its ok
    case t_less:
        if (is_const(exp->children[0]) && is_const(exp->children[1])) {
            res = const_node(exp->children[0]->value < exp->children[1]->value);
            free_tree(exp);
        }
        break;
    case t_greater:
        if (is_const(exp->children[0]) && is_const(exp->children[1])) {
            res = const_node(exp->children[0]->value > exp->children[1]->value);
            free_tree(exp);
        }
        break;
    case t_lessequal:
        if (is_const(exp->children[0]) && is_const(exp->children[1])) {
            res = const_node(exp->children[0]->value < exp->children[1]->value + EPS);
            free_tree(exp);
        }
        break;
    case t_greaterequal:
        if (is_const(exp->children[0]) && is_const(exp->children[1])) {
            res = const_node(exp->children[0]->value + EPS > exp->children[1]->value);
            free_tree(exp);
        }
        break;
    case t_equal:
        if (is_const(exp->children[0]) && is_const(exp->children[1])) {
            res = const_node(is_zero(exp->children[0]->value - exp->children[1]->value));
            free_tree(exp);
        }
        break;
    case t_notequal:
        if (is_const(exp->children[0]) && is_const(exp->children[1])) {
            res = const_node(!is_zero(exp->children[0]->value - exp->children[1]->value));
            free_tree(exp);
        }
        break;
    case t_logical_or:
    case t_logical_and: {
        size_t newn = 0;
        bool res_short = 0;
        for (size_t i = 0; i < exp->n_child; i++) {
            if (is_const(exp->children[i])) {
                if ((tag == t_logical_and) == is_zero(exp->children[i]->value)) res_short = 1;
                free(exp->children[i]);
            } else {
                exp->children[newn++] = exp->children[i];
            }
        }
        res->n_child = newn;
        if (res_short) {
            free_tree(exp);
            res = const_node(tag != t_logical_and);
        } else if (newn == 1) {
            res = exp->children[0];
            free(exp);
        } else {
            res = realloc(exp, sizeof(*exp) + newn*sizeof(exp));
            assert(res);
        }
        break;
    }
    case t_logical_not:
        if (is_const(exp->children[0])) {
            res = exp->children[0];
            res->value = !is_zero(res->value);
            free(exp);
        }
        break;
    }

    return res;
}

static struct expr *fold_ops(struct expr *exp);

inline static void push_neg_mul(struct expr *exp) {
    if (exp->children[0]->tag == t_inverse) {
        exp->children[0]->children[0] =
            fold_ops(node(t_negate, 1, exp->children[0]->children[0]));
    } else {
        exp->children[0] =
            fold_ops(node(t_negate, 1, exp->children[0]));
    }
}

inline static void push_node(struct expr *exp, enum tag tag) {
    for (size_t i = 0; i < exp->n_child; i++)
        exp->children[i] = fold_ops(node(tag, 1, exp->children[i]));
}

// Fold same operations and push unary down to the tree
static struct expr *fold_ops(struct expr *exp) {
    if (exp->tag != t_constant && exp->tag != t_variable) {
        // Recursively fold children first
        for (size_t i = 0; i < exp->n_child; i++)
            exp->children[i] = fold_ops(exp->children[i]);
    }

    struct expr *res = exp;
    enum tag tag = exp->tag;

    switch (tag) {
    case t_multiply: {
        // Factor out negations
        bool negate = 0;
        for (size_t i = 0; i < exp->n_child; i++) {
            assert(exp->children[i]);
            if (exp->children[i]->tag == t_negate) {
                struct expr *tmp = exp->children[i];
                exp->children[i] = exp->children[i]->children[0];
                free(tmp);
                negate ^= 1;
            } else if (exp->children[i]->tag == t_inverse &&
                       exp->children[i]->children[0]->tag == t_negate) {
                struct expr *tmp = exp->children[i]->children[0];
                exp->children[i]->children[0] = exp->children[i]->children[0]->children[0];
                free(tmp);
                negate ^= 1;
            }
        }
        if (negate) push_neg_mul(exp);
    }
        // fallthrough
    case t_add:
    case t_logical_and:
    case t_logical_or: {
        size_t ntotal = exp->n_child;
        for (size_t i = 0; i < exp->n_child; i++) {
            assert(exp->children[i]);
            if (exp->children[i]->tag == tag)
                ntotal += exp->children[i]->n_child - 1;
        }
        res = node_of_size(tag, ntotal);
        for (size_t i = 0, k = 0; i < exp->n_child; i++) {
            if (exp->children[i]->tag == tag) {
                for (size_t j = 0; j < exp->children[i]->n_child; j++)
                    res->children[k++] = exp->children[i]->children[j];
                free(exp->children[i]);
            } else res->children[k++] = exp->children[i];
            assert(k <= ntotal);
        }
        free(exp);
        break;
    }
    default: break;
    }

    return res;
}

static struct expr *push_ops(struct expr *exp) {
    if (exp->tag != t_constant && exp->tag != t_variable) {
        // Recursively fold children first
        for (size_t i = 0; i < exp->n_child; i++)
            exp->children[i] = push_ops(exp->children[i]);
    }

    struct expr *res = exp;
    enum tag ctag, tag = exp->tag;

    switch (tag) {
    case t_add:
        if (!exp->n_child) {
            free(exp);
            res = const_node(0);
        }
        break;
    case t_multiply:
        if (!exp->n_child) {
            free(exp);
            res = const_node(1);
        }
        break;
    case t_inverse:
        assert(exp->n_child == 1);
        if (exp->children[0]->tag == t_negate &&
                exp->children[0]->children[0]->tag == t_multiply) {
            res = exp->children[0];
            push_node(res->children[0], t_inverse);
            free(exp);
        }
        break;
    case t_negate:
        assert(exp->n_child == 1);
        switch(ctag = exp->children[0]->tag) {
        case t_negate:
            res = exp->children[0]->children[0];
            free(exp->children[0]);
            free(exp);
            break;
        case t_multiply:
            res = exp->children[0];
            push_neg_mul(res);
            free(exp);
            break;
        case t_add:
            res = exp->children[0];
            push_node(res, t_negate);
            free(exp);
            //fallthrough
        default: break;
        }
        break;
    case t_logical_not: {
        assert(exp->n_child == 1);
        switch(ctag = exp->children[0]->tag) {
        case t_equal: case t_notequal:
        case t_less: case t_greater:
        case t_lessequal: case t_greaterequal:
            res = exp->children[0];
            free(exp);
            res->tag = (enum tag[]){
                [t_equal] = t_notequal,
                [t_notequal] = t_equal,
                [t_less] = t_greaterequal,
                [t_greaterequal] = t_less,
                [t_greater] = t_lessequal,
                [t_lessequal] = t_greater,
                [t_logical_and] = t_logical_or,
                [t_logical_or] = t_logical_and,
            }[ctag];
            if (ctag == t_logical_and ||
                    ctag == t_logical_or) {
                push_node(res, t_logical_not);
            }
            //fallthrough
        default: break;
        }
        break;
    }
    default: break;
    }

    return res;
}


#define MAX_OPT 10

struct expr *optimize_tree(struct expr *exp) {
    for (size_t i = 0; i < MAX_OPT; i++) {
        if (tfile && tfmt != fmt_graph) {
            fputs("\nFolding operations; tree\n", tfile);
            dump_tree(tfile, tfmt, exp, 0);
            fputs("Becomes\n", tfile);
        }
        exp = fold_ops(exp);
        if (tfile) dump_tree(tfile, tfmt, exp, 0);
        if (tfile && tfmt != fmt_graph) {
            fputs("\nFolding constants; tree\n", tfile);
            dump_tree(tfile, tfmt, exp, 0);
            fputs("Becomes\n", tfile);
        }
        exp = fold_constants(exp);
        if (tfile) dump_tree(tfile, tfmt, exp, 0);
        if (tfile && tfmt != fmt_graph) {
            fputs("\nEliminating common; tree\n", tfile);
            dump_tree(tfile, tfmt, exp, 0);
            fputs("Becomes\n", tfile);
        }
        sort_tree(exp);
        exp = eliminate_common(exp);
        if (tfile) dump_tree(tfile, tfmt, exp, 0);
        exp = push_ops(exp);
    }
    return exp;
}

void set_trace(FILE *file, enum format fmt) {
    tfile = file;
    tfmt = fmt;
}

