#include "expr.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

struct tag_info tags[] = {
	[t_constant] =  {NULL, NULL, 0, 0},
	[t_variable] =  {NULL, NULL, 0, 0},
	[t_negate] =  {"-", "-", 1, 1},
	[t_inverse] =  {NULL, "1/", 1, 2},
	[t_multiply] =  {"\\cdot", "*", -1, 2, t_inverse, "/"},
	[t_add] =  {"+", "+", -1, 3, t_negate, "-"},
	[t_less] =  {"<", "<", 2, 4},
	[t_greater] =  {">", ">", 2, 4},
	[t_lessequal] =  {"<=", "<=", 2, 4},
	[t_greaterequal] =  {">=", ">=", 2, 4},
	[t_equal] =  {"=", "==", 2, 5},
	[t_logical_not] =  {"\\lnot", "!", 1, 6},
	[t_logical_and] =  {"\\land", "&&", -1, 7},
	[t_logical_or] =  {"\\lor", "||", -1, 8},
};

struct state {
    const char *str;
    int depth;
    _Bool success;
    const char *expected;
};

inline static void skip_spaces(struct state *st) {
    while (*st->str && isspace((unsigned)*st->str)) st->str++;
}

inline static char peek_nospace(struct state *st) {
    return *st->str;
}

inline static char peek(struct state *st) {
    skip_spaces(st);
    return peek_nospace(st);
}

inline static char next(struct state *st) {
    // does not skip spaces
    return *st->str++;
}

inline static _Bool expect(struct state *st, const char *str) {
    const char *start = st->str;

    if (!st->success) return 0;

    skip_spaces(st);

    while (*start == *str) start++, str++;

    if (!*str) {
        st->str = start;
        return 1;
    } else {
        st->expected = str;
        return 0;
    }
}

inline static _Bool append_child(struct state *st, struct expr **node, enum tag tag, struct expr *first, struct expr *new) {
    size_t nch = *node ? (*node)->n_child : 1;
    struct expr *tmp = realloc(node, sizeof(*tmp) + (!!new + nch)*sizeof(tmp));
    if (!tmp) {
        free_tree(new);
        st->success = 0;
        return 0;
    }


    if (!*node) {
        tmp->tag = t_logical_or;
        tmp->children[0] = first;
    }

    if (new) {
        tmp->children[nch] = new;
        tmp->n_child = nch + 1;
    }

    return *node = tmp;
}

#define LIT_CAP_STEP(x) ((x) ? 4*(x)*3 : 16)

static struct expr *exp_8(struct state *st);

static struct expr *exp_0(struct state *st) /* const, var, () */ {
    if (expect(st, "(")) {
        struct expr *node = exp_8(st);
        st->success &= expect(st, ")");
        return node;
    } else if (isdigit((unsigned)peek(st))) {
        // TODO Lets ignore base 2, 8 and 16 numbers for now...

        double val = 0;
        char ch;
        while ((ch = next(st)) && isdigit((unsigned)ch))
            val = val * 10 + (ch - '0');

        if (peek_nospace(st) == '.') {
            next(st);
            int dot = 1;
            while ((ch = next(st)) && isdigit((unsigned)ch))
                val = val * 10 + (ch - '0'), dot *= 10;
            val /= dot;
        }

        struct expr *tmp = malloc(sizeof(*tmp));
        if (!tmp) {
            st->success = 0;
            return NULL;
        }
        tmp->tag = t_constant;
        tmp->value = val;
        return tmp;
    } else if (isalpha((unsigned)peek(st))) {
        size_t cap = 0, size = 0;
        char *str = NULL;
        for (char ch; (ch = next(st)) && isalpha((unsigned)ch);) {
            if (size + 1 >= cap) {
                char *tmp = realloc(str, cap = LIT_CAP_STEP(cap));
                if (!tmp) {
                    st->success = 0;
                    break;
                }
                str = tmp;
            }
            str[size++] = ch;
        }
        str[size] = 0;

        struct expr *tmp = malloc(sizeof(*tmp));
        if (!tmp) {
            st->success = 0;
            return NULL;
        }
        tmp->tag = t_variable;
        tmp->id = str;
        return tmp;
    }
    st->success = 0;
    return NULL;
}

static struct expr *exp_1(struct state *st) /* - + (unary) */ {
    _Bool neg = 0;
    for (;;) {
        if (expect(st, "-")) neg = !neg;
        else if (!expect(st, "+")) break;
    }

    struct expr *first = exp_0(st), *node = NULL;

    if (neg) append_child(st, &node, t_negate, first, NULL);
    
    return node ? node : first;
}

static struct expr *exp_2(struct state *st) /* * / */ {
    struct expr *first = exp_1(st), *node = NULL;

    for (;;) {
        if (expect(st, tags[t_multiply].name)) {
            if (!(append_child(st, &node, t_multiply, first, exp_1(st)))) break;
        } else if (expect(st, tags[t_multiply].alt)) {
            struct expr *tmp = NULL;
            if (!(append_child(st, &tmp, t_inverse, exp_1(st), NULL))) break;
            if (!(append_child(st, &node, t_add, first, tmp))) break;
        }
    }

    return node ? node : first;

}

static struct expr *exp_3(struct state *st) /* + - */ {
    struct expr *first = exp_2(st), *node = NULL;

    for (;;) {
        if (expect(st, tags[t_add].name)) {
            if (!append_child(st, &node, t_add, first, exp_2(st))) break;
        } else if (expect(st, tags[t_add].alt)) {
            struct expr *tmp = NULL;
            if (!append_child(st, &tmp, t_negate, exp_2(st), NULL)) break;
            if (!append_child(st, &node, t_add, first, tmp)) break;
        }
    }

    return node ? node : first;
}

static struct expr *exp_4(struct state *st) /* > < >= <= */ {
    struct expr *first = exp_3(st), *node = NULL;

    if (expect(st, tags[t_less].name))
        append_child(st, &node, t_less, first, exp_3(st));
    else if (expect(st, tags[t_lessequal].name))
        append_child(st, &node, t_lessequal, first, exp_3(st));
    else if (expect(st, tags[t_greater].name))
        append_child(st, &node, t_greater, first, exp_3(st));
    else if (expect(st, tags[t_greaterequal].name))
        append_child(st, &node, t_greaterequal, first, exp_3(st));

    return node ? node : first;
}

static struct expr *exp_5(struct state *st) /* == != */ {
    struct expr *first = exp_4(st), *node = NULL;

    if (expect(st, tags[t_equal].name))
        append_child(st, &node, t_equal, first, exp_4(st));
    else if (expect(st, tags[t_notequal].name))
        append_child(st, &node, t_notequal, first, exp_4(st));

    return node ? node : first;
}

static struct expr *exp_6(struct state *st) /* ! */ {
    _Bool neg = 0;
    while (expect(st, tags[t_logical_not].name)) neg = !neg;

    struct expr *first = exp_5(st), *node = NULL;

    if (neg) append_child(st, &node, t_logical_not, first, NULL);
    
    return node ? node : first;
}

static struct expr *exp_7(struct state *st) /* && */ {
    struct expr *first = exp_6(st), *node = NULL;

    while (expect(st, tags[t_logical_and].name) &&
           append_child(st, &node, t_logical_and, first, exp_6(st)));

    return node ? node : first;
}

static struct expr *exp_8(struct state *st) /* || */ {
    struct expr *first = exp_7(st), *node = NULL;

    while (expect(st, tags[t_logical_or].name) &&
           append_child(st, &node, t_logical_or, first, exp_7(st)));

    return node ? node : first;
}

void free_tree(struct expr *expr) {
    if (!expr) return;

    if (expr->tag == t_variable) free(expr->id);
    else if (expr->tag != t_constant) {
        assert((tags[expr->tag].arity < 0 && expr->n_child > 1) ||
               (size_t)tags[expr->tag].arity == expr->n_child);
        for (size_t i = 0; i < expr->n_child; i++)
            free_tree(expr->children[i]);
    }

    free(expr);
}

struct expr *parse_tree(const char *str) {
    struct state st = {str, 0, 1};

    struct expr *res = exp_8(&st);

    st.success &= !peek(&st);

    if (!st.success) {
        fprintf(stderr, "%*c\n%*c\n",
            (int)(st.str - str + 1), '^',
            (int)(st.str - str + 1), '|');
        fprintf(stderr, st.expected ?
            "Unexpected character at %zu, expected '%s'\n" :
            "Internal error at %zu\n", st.str - str, st.expected);
        return NULL;
    }

    return res;
}


