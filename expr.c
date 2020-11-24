#define _POSIX_C_SOURCE 200809L

#include "expr.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct tag_info tags[] = {
    [t_constant] =  {NULL, NULL, 0, 0},
    [t_variable] =  {NULL, NULL, 0, 0},
    [t_negate] =  {"-", "-", 1, 1},
    [t_inverse] =  {NULL, "1/", 1, 2},
    [t_multiply] =  {"\\cdot ", "*", -1, 2, t_inverse, "/"},
    [t_add] =  {"+", "+", -1, 3, t_negate, "-"},
    [t_less] =  {"<", "<", 2, 4},
    [t_greater] =  {">", ">", 2, 4},
    [t_lessequal] =  {"<=", "<=", 2, 4},
    [t_greaterequal] =  {">=", ">=", 2, 4},
    [t_equal] =  {"=", "==", 2, 5},
    [t_notequal] =  {"\\ne ", "!=", 2, 5},
    [t_logical_not] =  {"\\lnot ", "!", 1, 6},
    [t_logical_and] =  {"\\land ", "&&", -1, 7},
    [t_logical_or] =  {"\\lor ", "||", -1, 8},
};

#define UNGET_BUF_LEN 16

struct state {
    FILE *in;
    _Bool success;
    const char *expected;
    // Portable way to unget more than one
    // character (this is actually unnecesery in linux)
    size_t unget_pos;
    char unget_buf[UNGET_BUF_LEN];

    // Last line start
    long last_line;
};

inline static char get(struct state *st) {
    if (st->unget_pos) {
        return st->unget_buf[--st->unget_pos];
    } else {
        int ch = fgetc(st->in);
        return ch == EOF ? 0 : ch;
    }
}

inline static void unget(struct state *st, char ch) {
    if (ch && ungetc(ch, st->in) == EOF) {
        if (st->unget_pos + 1 >= UNGET_BUF_LEN) {
            fprintf(stderr, "Unget buffer exhaused\n");
            abort();
        }
        st->unget_buf[st->unget_pos++] = ch;
    }
}

inline static void skip_spaces(struct state *st) {
    for (int ch; (ch = get(st)); ) {
        // Remember new line position
        if (ch == '\n') st->last_line = ftell(st->in) - st->unget_pos + 1;
        if (!isspace((unsigned)ch)) {
            unget(st, ch);
            break;
        }
    }
}

inline static char peek_nospace(struct state *st) {
    char ch = get(st);
    unget(st, ch);
    return ch;
}

inline static char peek_space(struct state *st) {
    skip_spaces(st);
    return peek_nospace(st);
}

inline static double expect_number(struct state *st) {
    double value = 0;
    if (st->unget_pos || fscanf(st->in, "%lg", &value) != 1) {
        st->expected = "<number>";
        st->success = 0;
    }
    return value;
}

inline static _Bool expect(struct state *st, const char *str) {
    if (!st->success) return 0;

    skip_spaces(st);

    const char *str0 = str;
    for (int ch; *str && (ch = get(st)); str++) {
        if (ch != *str) {
            unget(st, ch);
            break;
        }
    }

    if (*str) {
        st->expected = str;
        while (--str >= str0) unget(st, *str);
        return 0;
    }

    return 1;
}

inline static _Bool append_child(struct state *st, struct expr **node, enum tag tag, struct expr *first, struct expr *new) {
    size_t nch = *node ? (*node)->n_child : 1;
    struct expr *tmp = realloc(*node, sizeof(*tmp) + (!!new + nch)*sizeof(tmp));
    if (!tmp) {
        free_tree(new);
        st->expected = NULL;
        st->success = 0;
        return 0;
    }

    if (!*node) {
        tmp->tag = tag;
        tmp->n_child = 1;
        tmp->children[0] = first;
    }

    if (new) {
        tmp->children[nch] = new;
        tmp->n_child = nch + 1;
    }

    return (*node = tmp);
}

#define LIT_CAP_STEP(x) ((x) ? 4*(x)*3 : 16)
#define MAX_NUMBER_LEN 128

static struct expr *exp_8(struct state *st);

static struct expr *exp_0(struct state *st) /* const, var, () */ {
    if (expect(st, "(")) {
        struct expr *node = exp_8(st);
        st->success &= expect(st, ")");
        return node;
    } else if (isdigit((unsigned)peek_space(st))) {
        struct expr *tmp = malloc(sizeof(*tmp));
        if (!tmp) {
            st->success = 0;
            st->expected = NULL;
            return NULL;
        }

        tmp->tag = t_constant;
        tmp->value = expect_number(st);
        return tmp;
    } else if (isalpha((unsigned)peek_nospace(st))) {
        size_t cap = 0, size = 0;
        char *str = NULL;
        for (char ch; (ch = peek_nospace(st)) && isalpha((unsigned)ch);) {
            if (size + 1 >= cap) {
                char *tmp = realloc(str, cap = LIT_CAP_STEP(cap));
                if (!tmp) {
                    st->success = 0;
                    st->expected = NULL;
                    break;
                }
                str = tmp;
            }
            str[size++] = get(st);
        }
        str[size] = 0;

        struct expr *tmp = malloc(sizeof(*tmp));
        if (!tmp) {
            st->success = 0;
            st->expected = NULL;
            return NULL;
        }
        tmp->tag = t_variable;
        tmp->id = str;
        return tmp;
    }
    st->success = 0;
    st->expected = "<number>' or '<variable>";
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
            if (!(append_child(st, &node, t_multiply, first, tmp))) break;
        } else break;
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
        } else break;
    }

    return node ? node : first;
}

static struct expr *exp_4(struct state *st) /* > < >= <= */ {
    struct expr *first = exp_3(st), *node = NULL;

    if (expect(st, tags[t_lessequal].name))
        append_child(st, &node, t_lessequal, first, exp_3(st));
    else if (expect(st, tags[t_less].name))
        append_child(st, &node, t_less, first, exp_3(st));
    else if (expect(st, tags[t_greaterequal].name))
        append_child(st, &node, t_greaterequal, first, exp_3(st));
    else if (expect(st, tags[t_greater].name))
        append_child(st, &node, t_greater, first, exp_3(st));

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

    // TODO Optimize equality and comparisons here

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
        for (size_t i = 0; i < expr->n_child; i++)
            free_tree(expr->children[i]);
    }

    free(expr);
}

struct expr *parse_tree(FILE *in) {
    struct state st = {in, 1};

    struct expr *tree = exp_8(&st);

    st.success &= !peek_space(&st);


    if (!st.success) {
        // Output error in human readable format
        long res = ftell(in);
        fseek(in, 0, SEEK_SET);

        char *str = NULL;
        size_t size = 0;

        if (getline(&str, &size, in) >= 0) {
            fprintf(stderr, "%s\n%*c\n%*c\n", str,
                (int)(res + 1), '^', (int)(res + 1), '|');
        }

        fprintf(stderr, st.expected ?
            "Unexpected character at %zu, expected '%s'\n" :
            "Internal error at %zu\n", res, st.expected);

        free(str);
        free_tree(tree);
        return NULL;
    }

    return tree;
}
