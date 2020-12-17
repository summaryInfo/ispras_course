#define _POSIX_C_SOURCE 200809L

#include "expr-impl.h"
#include "strtab.h"

#include <stdio.h>
#include <string.h>

struct funcdef {
    id_t name;
    size_t nargs;
    bool isvoid;
};

struct genstate {
    struct strtab *stab;
    struct funcdef *defs;
    size_t ndefs;
    id_t *globals;
    size_t nglobals;
    FILE *out;
};

static struct funcdef *find_funcdef(struct genstate *st, id_t id) {
    for (size_t i = 0; i < st->ndefs; i++)
        if (id == st->defs[i].name) return st->defs + i;
    return NULL;
}

static void add_global(struct genstate *st, id_t id) {
    for (size_t i = 0; i < st->nglobals; i++)
        if (id == st->globals[i]) return;

    fprintf(st->out, ".global double %s 0\n", string_of(st->stab, id));
    st->globals = realloc(st->globals, (1 + st->nglobals)*sizeof(*st->globals));
    st->globals[st->nglobals++] = id;
}

static void do_codegen(struct expr *exp, struct genstate *st) {
    // Well all this code generation is *very* inefficient
    // due to the lack of proper types (doubles are used for conditionals)
    // and the fact that the only comparison operations for doubles are < and >
    static size_t label_n = 0;
    assert(exp && exp->tag <= t_MAX);

    enum tag tag = exp->tag;

    switch(tag) {
    case t_constant:
        fprintf(st->out, "\tld.d $%lf\n", exp->value);
        break;
    case t_variable:
        fprintf(st->out, "\tld.d %s\n", string_of(st->stab, exp->id));
        break;
    case t_power:
        do_codegen(exp->children[0], st);
        do_codegen(exp->children[1], st);
        fputs("\tcall.d power_d\n", st->out);
        break;
    case t_function: {
        struct funcdef *fn = find_funcdef(st, exp->id);
        if (!fn) {
            fprintf(stderr, "ERROR: No function with name '%s'\n", string_of(st->stab, exp->id));
            abort();
        } else if (fn->nargs != exp->n_child) {
            fprintf(stderr, "ERROR: Function signature missmatch for '%s'\n", string_of(st->stab, exp->id));
            fprintf(stderr, "%s() takes %zd arguments, but %zd was provided\n", string_of(st->stab, exp->id), fn->nargs, exp->n_child);
            abort();
        }
        for (size_t i = 0; i < exp->n_child; i++)
            do_codegen(exp->children[exp->n_child - 1 - i], st);
        if (fn->isvoid)
            fprintf(st->out, "\tcall %s_d\n" "\t\tld.d $0\n", string_of(st->stab, fn->name));
        else fprintf(st->out, "\tcall.d %s_d\n", string_of(st->stab, exp->id));
        break;
    }
    case t_funcdef:
        // Function definition evaluates to 0
        // (Since theres no first class function support in VM)
        fputs("\tld.d $0\n", st->out);
        break;
    case t_negate:
        do_codegen(exp->children[0], st);
        fputs("\tneg.d\n", st->out);
        break;
    case t_assign:
        do_codegen(exp->children[1], st);
        assert(exp->children[0]->tag == t_variable);
        fprintf(st->out, "\tdup.l\n"
                     "\tst.d %s\n", string_of(st->stab, exp->children[0]->id));
        break;
    case t_inverse:
        assert(0);
        break;
    case t_add:
        do_codegen(exp->children[0], st);
        for (size_t i = 1; i < exp->n_child; i++) {
            if (exp->children[i]->tag == t_negate) {
                do_codegen(exp->children[i]->children[0], st);
                fputs("\tsub.d\n", st->out);
            } else {
                do_codegen(exp->children[i], st);
                fputs("\tadd.d\n", st->out);
            }
        }
        break;
    case t_multiply:
        do_codegen(exp->children[0], st);
        for (size_t i = 1; i < exp->n_child; i++) {
            if (exp->children[i]->tag == t_inverse) {
                do_codegen(exp->children[i]->children[0], st);
                fputs("\tdiv.d\n", st->out);
            } else {
                do_codegen(exp->children[i], st);
                fputs("\tmul.d\n", st->out);
            }
        }
        break;
    case t_less:
    case t_greater:
    case t_lessequal:
    case t_greaterequal:
    case t_equal:
    case t_notequal:
    case t_logical_not: {
        size_t l1 = label_n++, l2 = label_n++;
        do_codegen(exp->children[0], st);
        if (tag != t_logical_not) do_codegen(exp->children[1], st);
        switch (tag) {
        case t_less:
            fprintf(st->out, "\tjl.d L%zu\n", l1);
            break;
        case t_greater:
            fprintf(st->out, "\tjg.d L%zu\n", l1);
            break;
        case t_lessequal:
            fprintf(st->out, "\tld.d $%lf\n"
                         "\tadd.d\n"
                         "\tjl.d L%zu\n", EPS, l1);
            break;
        case t_greaterequal:
            fprintf(st->out, "\tld.d $%lf\n"
                         "\tsub.d\n"
                         "\tjg.d L%zu\n", EPS, l1);
            break;
        case t_equal:
            fprintf(st->out, "\tsub.d\n");
            //fallthrough
        case t_logical_not:
            fprintf(st->out, "\tcall.d abs_d\n"
                         "\tld.d $%lf\n"
                         "\tjl.d L%zu\n", EPS, l1);
            break;
        case t_notequal:
            fprintf(st->out, "\tsub.d\n"
                         "\tcall.d abs_d\n"
                         "\tld.d $%lf\n"
                         "\tjg.d L%zu\n", EPS, l1);
            break;
        default:
            assert(0);
        }
        fprintf(st->out, "\tld.d $0\n"
                     "\tjmp L%zu\n"
                     "L%zu:\n"
                     "\tld.d $1\n"
                     "L%zu:\n", l2, l1, l2);
        break;
    }
    case t_logical_and:
    case t_logical_or: {
        size_t lend = label_n++, lend2 = label_n++;
        do_codegen(exp->children[0], st);
        for (size_t i = 1; i < exp->n_child; i++) {
            fprintf(st->out, "\tcall.d abs_d\n"
                         "\tld.d $%lf\n"
                         "\tj%c.d L%zu\n", EPS, tag == t_logical_or ? 'g' : 'l', lend);
            do_codegen(exp->children[0], st);
        }
        if (exp->n_child > 1) {
            fprintf(st->out, "\tjmp L%zu\n"
                         "L%zu:\n"
                         "\tld.d $%d\n"
                         "L%zu:\n", lend2, lend, tag == t_logical_or, lend2);
        }
        break;
    }
    case t_if: {
        size_t lend = label_n++, lelse = label_n++;
        do_codegen(exp->children[0], st);
        fprintf(st->out, "\tcall.d abs_d\n");
        fprintf(st->out, "\tld.d $%lf\n"
                     "\tjl.d L%zu\n", EPS, lelse);
        do_codegen(exp->children[1], st);
        fprintf(st->out, "\tjmp L%zu\n"
                     "L%zu:\n", lend, lelse);
        do_codegen(exp->children[2], st);
        fprintf(st->out, "L%zu:\n", lend);
        break;
    }
    case t_while: {
        size_t lnext = label_n++, lend = label_n++;
        fprintf(st->out, "\tld.d $0\n"
                     "L%zu:\n", lnext);
        do_codegen(exp->children[0], st);
        fprintf(st->out, "\tcall.d abs_d\n"
                     "\tld.d $%lf\n"
                     "\tjl.d L%zu\n", EPS, lend);
        do_codegen(exp->children[1], st);
        fprintf(st->out, "\tswap.l\n"
                     "\tdrop.l\n"
                     "\tjmp L%zu\n"
                     "L%zu:\n", lnext, lend);
        break;
    }
    case t_statement:
        for (size_t i = 0; i < exp->n_child - 1; i++) {
            do_codegen(exp->children[i], st);
            fputs("\tdrop.l\n", st->out);
        }
        do_codegen(exp->children[exp->n_child - 1], st);
    }
}


static void find_vars(struct expr *exp, id_t **pvars, size_t *pnvars) {
    switch(exp->tag) {
    case t_constant:
    case t_function:
        break;
    case t_variable:
        // TODO Need hashtable for this (or more precisely symbol table)
        for (size_t i = 0; i < *pnvars; i++)
            if (exp->id == (*pvars)[i]) return;
        *pvars = realloc(*pvars, (1 + *pnvars)*sizeof(**pvars));
        assert(*pvars);

        (*pvars)[(*pnvars)++] = exp->id;
        break;
    default:
        for (size_t i = 0; i < exp->n_child; i++)
            find_vars(exp->children[i], pvars, pnvars);
    }
}

static void generate_function(const char *name, bool suff, struct expr *args, struct expr *exp, struct genstate *st) {
    // FIXME For now every variable is double and every function returns double

    struct funcdef *fn = find_funcdef(st, intern(st->stab, strdup(name)));
    assert(fn);

    fprintf(st->out, ".function %s %s%s\n", fn->isvoid ? "void" : "double", name, suff ? "_d" : "");
    id_t *variables = NULL;
    size_t var0 = 0, nvars = 0;

    if (args) {
        nvars =  var0 = args->n_child;
        variables = malloc(args->n_child*sizeof variables);
        assert(variables);
        assert(args->tag == t_statement);
        for (size_t i = 0; i < args->n_child; i++) {
            assert(args->children[i]->tag == t_variable);
            variables[i] = args->children[i]->id;
            fprintf(st->out, ".param double %s\n", string_of(st->stab, variables[i]));
        }
    }

    find_vars(exp, &variables, &nvars);
    // Lets make all variables that are now immediate function arguments global
    for (size_t i = var0; i < nvars; i++) add_global(st, variables[i]);

    free(variables);

    do_codegen(exp, st);
    fputs(fn->isvoid ? "\tret\n" : "\tret.d\n", st->out);
}

static void generate_rec_functions(struct expr *exp, struct genstate *st) {
    for (size_t i = 0; i < exp->n_child; i++)
        generate_rec_functions(exp->children[i], st);

    if (exp->tag == t_funcdef)
        generate_function(string_of(st->stab, exp->id), 1, exp->children[0], exp->children[1], st);
}

static void generate_prototypes(struct expr *exp, struct genstate *st) {
    for (size_t i = 0; i < exp->n_child; i++)
        generate_prototypes(exp->children[i], st);
    if (exp->tag == t_funcdef) {
        st->defs = realloc(st->defs, (1 + st->ndefs)*sizeof(*st->defs));
        st->defs[st->ndefs++] = (struct funcdef){exp->id, exp->children[0]->n_child};
        fprintf(st->out, ".function double %s_d\n", string_of(st->stab, exp->id));
        assert(exp->children[0]->tag == t_statement);
        for (size_t i = 0; i < exp->children[0]->n_child; i++) {
            assert(exp->children[0]->children[i]->tag == t_variable);
            fprintf(st->out, ".param double %s\n", string_of(st->stab, exp->children[0]->children[i]->id));
        }
    }
}

void declare(const char *name, size_t narg, struct genstate *st, bool isvoid, bool suff) {
    fprintf(st->out, ".function %s %s%s\n", isvoid ? "void" : "double", name, suff ? "_d" : "");
    for (size_t i = 0; i < narg; i++)
        fprintf(st->out, ".param double n%zd\n", i);
    st->defs = realloc(st->defs, (1 + st->ndefs)*sizeof(*st->defs));
    st->defs[st->ndefs++] = (struct funcdef){intern(st->stab, strdup(name)), narg, isvoid};
}

void generate_code(struct expr *exp, struct strtab *stab, FILE *out) {
    struct genstate st = {
        .stab = stab,
        .out = out,
    };

    declare("main", 0, &st, 1, 0);
    declare("power", 2, &st, 0, 1);
    declare("log", 1, &st, 0, 1);
    declare("scan", 0, &st, 0, 1);
    declare("print", 1, &st, 1, 1);
    declare("abs", 1, &st, 0, 1);
    fputs("\tld.d n0\n"
          "\tdup.l\n"
          "\tld.d $0\n"
          "\tjg.d 1\n"
          "\tneg.d\n"
          "1:\n"
          "\tret.d\n", out);

    generate_prototypes(exp, &st);

    // Generate and read variables
    // (all of them are locals for now and read automatically upon startup)
    generate_function("main", 0, NULL, exp, &st);
    generate_rec_functions(exp, &st);

    free(st.globals);
    free(st.defs);
}
