#define _POSIX_C_SOURCE 200809L

#include "expr.h"
#include "expr-impl.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


/*
 * A grammar corresponding to the parser:
 *
 * E12 := E11 (";" E12)
 * E11 := E10 | "if" E11 "then" E11 ("else" E11) | "while" E11 "do" E11
 * E10 := E10 "||" E9 | E9
 * E9 := E9 "&&" E8 | E8
 * E8 := E7 |  "!" E8
 * E7 := E6 "==" E7 | E6 "!=" E7 | E6
 * E6 := E5 ">" E6 | E5 "<" E6 | E5 "<=" E5 | E5 ">=" E6 | E5
 * E5 := "log" E5 | E4
 * E4 := E4 "+" E3 | E4 "-" E3 | E3
 * E3 := E3 "*" E2 | E3 "/" E2 | E2
 * E2 := E1 "^" E2 | E1
 * E1 := "+" E1 | "-" E1 | E0
 * E0 := VAR | NUM | "(" E12 ")"
 * NUM := <c double constant>
 * LETTER = "a" | ... | "z" | "A" | ... | "Z"
 * VAR := LETTER VAR | LETTER
 *
 */

struct tag_info tags[] = {
    [t_constant] = {NULL, NULL, 0, 0},
    [t_variable] = {NULL, NULL, 0, 0},
    [t_log] =  {"\\log ", "log", 1, 0},
    [t_power] = {"^", "^", 2, 1},
    [t_negate] =  {"-", "-", 1, 2},
    [t_inverse] =  {NULL, "1/", 1, 3},
    [t_multiply] =  {"\\cdot ", "*", -1, 3, t_inverse, "/"},
    [t_add] =  {"+", "+", -1, 4, t_negate, "-"},
    [t_less] =  {"<", "<", 2, 6},
    [t_greater] =  {">", ">", 2, 6},
    [t_lessequal] =  {"<=", "<=", 2, 6},
    [t_greaterequal] =  {">=", ">=", 2, 6},
    [t_equal] =  {"=", "==", 2, 7},
    [t_notequal] =  {"\\ne ", "!=", 2, 7},
    [t_logical_not] =  {"\\lnot ", "!", 1, 8},
    [t_logical_and] =  {"\\land ", "&&", -1, 9},
    [t_logical_or] =  {"\\lor ", "||", -1, 10},
    [t_assign] = {":=", "=", 2, 11},
    [t_if] = {"{\rm if}", "if", 3, 12},
    [t_statement] = {";", ";", -1, 13},
};

#define LIT_CAP_STEP(x) ((x) ? 4*(x)/3 : 16)
#define UNGET_BUF_LEN 16

struct state {
    // Input buffer
    const char *inbuf;
    // Last encountered line start
    const char *last_line;
    // Next expected token
    const char *expected;
    _Bool success;
};

inline static void set_fail(struct state *st, const char *expected) {
    st->expected = expected;
    st->success = 0;
}

inline static unsigned char get(struct state *st) {
    return *st->inbuf ? *st->inbuf++ : 0;
}

inline static void skip_spaces(struct state *st) {
    const char *last = NULL;
    while (*st->inbuf) {
        if (!isspace((unsigned)*st->inbuf)) {
            if (last) st->last_line = last;
            break;
        }
        // Remember new line position
        if (*st->inbuf++ == '\n')
            last = st->inbuf;
    }
}

inline static unsigned char peek(struct state *st) {
    return *st->inbuf;
}

inline static unsigned char peek_space(struct state *st) {
    skip_spaces(st);
    return peek(st);
}

inline static double expect_number(struct state *st) {
    double value = 0;
    ssize_t len = 0;
    if (sscanf(st->inbuf, "%lg%zn", &value, &len) != 1) {
        set_fail(st, "<number>");
    } else st->inbuf += len;
    return value;
}

inline static _Bool expect(struct state *st, const char *str) {
    if (!st->success) return 0;

    skip_spaces(st);

    const char *str0 = st->inbuf;
    while (*str && *str0 == *str) str0++, str++;

    if (*str) st->expected = str;
    else st->inbuf = str0;

    return !*str;
}

inline static _Bool expect_not_followed(struct state *st, const char *str, char c) {
    if (!st->success) return 0;

    skip_spaces(st);

    const char *str0 = st->inbuf;
    while (*str && *str0 == *str) str0++, str++;

    if (*str || *str0 == c) st->expected = str;
    else st->inbuf = str0;

    return !*str && *str0 != c;
}

inline static _Bool append_child(struct state *st, struct expr **node, enum tag tag, struct expr *first, struct expr *new) {
    size_t nch = *node ? (*node)->n_child : 1;
    struct expr *tmp = realloc(*node, sizeof(*tmp) + (!!new + nch)*sizeof(tmp));
    if (!tmp) {
        free_tree(new);
        set_fail(st, NULL);
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

static struct expr *exp_13(struct state *st);

static struct expr *exp_0(struct state *st) /* const, var, (), log */ {
    if (expect(st, tags[t_log].name)) {
        struct expr *node = NULL;
        append_child(st, &node, t_log, exp_0(st), NULL);
        return node;
    } else if (expect(st, "(")) {;
        struct expr *node = exp_13(st);
        st->success &= expect(st, ")");
        return node;
    } else if (isdigit(peek_space(st))) {
        struct expr *tmp = malloc(sizeof(*tmp));
        if (!tmp) {
            st->success = 0;
            set_fail(st, NULL);
        }

        tmp->tag = t_constant;
        tmp->value = expect_number(st);
        return tmp;
    } else if (isalpha(peek(st))) {
        size_t cap = 0, size = 0;
        char *str = NULL;
        for (char ch; (ch = peek(st)) && isalpha(ch);) {
            if (size + 1 >= cap) {
                char *tmp = realloc(str, cap = LIT_CAP_STEP(cap));
                if (!tmp) {
                    set_fail(st, NULL);
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
    set_fail(st, "<number>' or '<variable>");
    return NULL;
}

static struct expr *exp_1(struct state *st) /* ^ */ {
    // ^ is binary, right associative

    struct expr *first = exp_0(st), *node = NULL;

    if (expect(st, tags[t_power].name))
        append_child(st, &node, t_power, first, exp_1(st));

    return node ? node : first;
}

static struct expr *exp_2(struct state *st) /* - + (unary) */ {
    // + and - are unary, prefix, right associative
    // These operation are optimized such that + is treated as nop
    // and at most 1 '-' can be generated

    _Bool neg = 0;
    for (;;) {
        if (expect(st, "-")) neg = !neg;
        else if (!expect(st, "+")) break;
    }

    struct expr *first = exp_1(st), *node = NULL;

    if (neg) append_child(st, &node, t_negate, first, NULL);

    return node ? node : first;
}

static struct expr *exp_3(struct state *st) /* * / */ {
    // * and / are n-ary, left associative

    struct expr *first = exp_2(st), *node = NULL;

    for (;;) {
        if (expect(st, tags[t_multiply].name)) {
            if (!(append_child(st, &node, t_multiply, first, exp_2(st)))) break;
        } else if (expect(st, tags[t_multiply].alt)) {
            struct expr *tmp = NULL;
            if (!(append_child(st, &tmp, t_inverse, exp_2(st), NULL))) break;
            if (!(append_child(st, &node, t_multiply, first, tmp))) break;
        } else break;
    }

    return node ? node : first;

}

static struct expr *exp_4(struct state *st) /* + - */ {
    // + and - are n-ary, left associative

    struct expr *first = exp_3(st), *node = NULL;

    for (;;) {
        if (expect(st, tags[t_add].name)) {
            if (!append_child(st, &node, t_add, first, exp_3(st))) break;
        } else if (expect(st, tags[t_add].alt)) {
            struct expr *tmp = NULL;
            if (!append_child(st, &tmp, t_negate, exp_3(st), NULL)) break;
            if (!append_child(st, &node, t_add, first, tmp)) break;
        } else break;
    }

    return node ? node : first;
}


static struct expr *exp_6(struct state *st) /* > < >= <= */ {
    // <, >, <=, >= are binary, right associative

    struct expr *first = exp_4(st), *node = NULL;

    if (expect(st, tags[t_lessequal].name))
        append_child(st, &node, t_lessequal, first, exp_6(st));
    else if (expect(st, tags[t_less].name))
        append_child(st, &node, t_less, first, exp_6(st));
    else if (expect(st, tags[t_greaterequal].name))
        append_child(st, &node, t_greaterequal, first, exp_6(st));
    else if (expect(st, tags[t_greater].name))
        append_child(st, &node, t_greater, first, exp_6(st));

    return node ? node : first;
}

static struct expr *exp_7(struct state *st) /* == != */ {
    // ==, != are binary, right associative

    struct expr *first = exp_6(st), *node = NULL;

    if (expect(st, tags[t_equal].name))
        append_child(st, &node, t_equal, first, exp_7(st));
    else if (expect(st, tags[t_notequal].name))
        append_child(st, &node, t_notequal, first, exp_7(st));

    return node ? node : first;
}

static struct expr *exp_8(struct state *st) /* ! */ {
    // ! is unary, prefix, right associative
    // At most 1 '!' can be generated

    _Bool neg = 0;
    while (expect(st, tags[t_logical_not].name)) neg = !neg;

    struct expr *first = exp_7(st), *node = NULL;

    // TODO Optimize equality and comparisons here

    if (neg) append_child(st, &node, t_logical_not, first, NULL);

    return node ? node : first;
}

static struct expr *exp_9(struct state *st) /* && */ {
    // && is binary, left associative

    struct expr *first = exp_8(st), *node = NULL;

    while (expect(st, tags[t_logical_and].name) &&
           append_child(st, &node, t_logical_and, first, exp_8(st)));

    return node ? node : first;
}

static struct expr *exp_10(struct state *st) /* || */ {
    // || is binary, left associative

    struct expr *first = exp_9(st), *node = NULL;

    while (expect(st, tags[t_logical_or].name) &&
           append_child(st, &node, t_logical_or, first, exp_9(st)));

    return node ? node : first;
}

static struct expr *exp_11(struct state *st) /* = */ {
    // assigment is right associative

    struct expr *first = exp_10(st), *node = NULL;

    if (expect_not_followed(st, tags[t_assign].name, '=')) {
        if (first->tag != t_variable) set_fail(st, "<variable>");
        append_child(st, &node, t_assign, first, exp_11(st));
    }

    return node ? node : first;
}

static struct expr *exp_12(struct state *st) /* if-then-else */ {
    // conditionals are expressions

    if (expect(st, "if")) {
        struct expr *first = exp_11(st), *node = NULL;
        if (expect(st, "then") &&
                append_child(st, &node, t_if, first, exp_11(st))) {
            append_child(st, &node, t_if, NULL,
                         expect(st, "else") ? exp_11(st) : const_node(0));
        }
        return node ? node : first;
    } else return exp_11(st);
}

static struct expr *exp_13(struct state *st) /* ; */ {
    // semicolon works like comma in C

    struct expr *first = exp_12(st), *node = NULL;

    while (expect(st, tags[t_statement].name) &&
           append_child(st, &node, t_statement, first, exp_12(st)));

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

struct expr *parse_tree(const char *in) {
    struct state st = {
        .inbuf = in,
        .success = 1,
        .last_line = in
    };

    struct expr *tree = exp_13(&st);

    st.success &= !peek_space(&st);

    if (!st.success) {
        // Output error in human readable format
        int res = in - st.last_line;
        size_t slen = strlen(st.last_line);

        const char *end = memchr(st.last_line, '\n', slen);
        int len = end ? end - st.last_line : (int)slen;

        fprintf(stderr, "%*s\n%*c\n%*c\n", len, st.last_line,
            (int)(res + 1), '^', (int)(res + 1), '|');

        fprintf(stderr, st.expected ?
            "Unexpected character at %d, expected '%s'\n" :
            "Internal error at %d\n", res, st.expected);

        free_tree(tree);
        return NULL;
    }

    return tree;
}
