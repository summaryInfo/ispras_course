#include "expr-impl.h"

#include <stdio.h>
#include <string.h>

static void do_codegen(struct expr *exp, FILE *out) {
    // Well all this code generation is *very* inefficient
    // due to the lack of proper types (doubles are used for conditionals)
    // and the fact that the only comparison operations for doubles are < and >
    static size_t label_n = 0;
    assert(exp && exp->tag <= t_MAX);

    enum tag tag = exp->tag;

    switch(tag) {
    case t_constant:
        fprintf(out, "\tld.d $%lf\n", exp->value);
        break;
    case t_variable:
        fprintf(out, "\tld.d %s\n", exp->id);
        break;
    case t_power:
        do_codegen(exp->children[0], out);
        do_codegen(exp->children[1], out);
        fputs("\tcall.d power_d\n", out);
        break;
    case t_log:
        do_codegen(exp->children[0], out);
        fputs("\tcall.d log_d\n", out);
        break;
    case t_negate:
        do_codegen(exp->children[0], out);
        fputs("\tneg.d\n", out);
        break;
    case t_inverse:
        assert(0);
        break;
    case t_add:
        do_codegen(exp->children[0], out);
        for (size_t i = 1; i < exp->n_child; i++) {
            if (exp->children[i]->tag == t_negate) {
                do_codegen(exp->children[i]->children[0], out);
                fprintf(out, "\tsub.d\n");
            } else {
                do_codegen(exp->children[i], out);
                fprintf(out, "\tadd.d\n");
            }
        }
        break;
    case t_multiply:
        do_codegen(exp->children[0], out);
        for (size_t i = 1; i < exp->n_child; i++) {
            if (exp->children[i]->tag == t_inverse) {
                do_codegen(exp->children[i]->children[0], out);
                fprintf(out, "\tdiv.d\n");
            } else {
                do_codegen(exp->children[i], out);
                fprintf(out, "\tmul.d\n");
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
        do_codegen(exp->children[0], out);
        if (tag != t_logical_not) do_codegen(exp->children[1], out);
        switch (tag) {
        case t_less:
            fprintf(out, "\tjl.d L%zu\n", l1);
            break;
        case t_greater:
            fprintf(out, "\tjg.d L%zu\n", l1);
            break;
        case t_lessequal:
            fprintf(out, "\tld.d $%lf\n", EPS);
            fprintf(out, "\tadd.d\n");
            fprintf(out, "\tjl.d L%zu\n", l1);
            break;
        case t_greaterequal:
            fprintf(out, "\tld.d $%lf\n", EPS);
            fprintf(out, "\tsub.d\n");
            fprintf(out, "\tjg.d L%zu\n", l1);
            break;
        case t_equal:
            fprintf(out, "\tsub.d\n");
            //fallthrough
        case t_logical_not:
            fprintf(out, "\tcall.d abs_d\n");
            fprintf(out, "\tld.d $%lf\n", EPS);
            fprintf(out, "\tjl.d L%zu\n", l1);
            break;
        case t_notequal:
            fprintf(out, "\tsub.d\n");
            fprintf(out, "\tcall.d abs_d\n");
            fprintf(out, "\tld.d $%lf\n", EPS);
            fprintf(out, "\tjg.d L%zu\n", l1);
            break;
        default:
            assert(0);
        }
        fprintf(out, "\tld.d $0\n");
        fprintf(out, "\tjmp L%zu\n", l2);
        fprintf(out, "L%zu:\n", l1);
        fprintf(out, "\tld.d $1\n");
        fprintf(out, "L%zu:\n", l2);
        break;
    }
    case t_logical_and:
    case t_logical_or: {
        size_t lend = label_n++, lend2 = label_n++;
        do_codegen(exp->children[0], out);
        for (size_t i = 1; i < exp->n_child; i++) {
            fprintf(out, "\tcall.d abs_d\n");
            fprintf(out, "\tld.d $%lf\n", EPS);
            fprintf(out, "\tj%c.d L%zu\n", tag == t_logical_or ? 'g' : 'l', lend);
            do_codegen(exp->children[0], out);
        }
        if (exp->n_child > 1) {
            fprintf(out, "\tjmp L%zu\n", lend2);
            fprintf(out, "L%zu:\n", lend);
            fprintf(out, "\tld.d $%d\n", tag == t_logical_or);
            fprintf(out, "L%zu:\n", lend2);
        }
        break;
    }
    case t_if: {
        size_t lend = label_n++, lelse = label_n++;
        do_codegen(exp->children[0], out);
        fprintf(out, "\tcall.d abs_d\n");
        fprintf(out, "\tld.d $%lf\n", EPS);
        fprintf(out, "\tjg.d L%zu\n", lelse);
        do_codegen(exp->children[1], out);
        fprintf(out, "\tjmp L%zu\n", lend);
        fprintf(out, "L%zu:\n", lelse);
        do_codegen(exp->children[2], out);
        fprintf(out, "L%zu:\n", lend);
        break;
    }
    case t_statement:
        for (size_t i = 0; i < exp->n_child - 1; i++) {
            do_codegen(exp->children[i], out);
            fputs("\tdrop.d\n", out);
        }
        do_codegen(exp->children[exp->n_child - 1], out);
    }
}


static void find_vars(struct expr *exp, char ***pvars, size_t *pnvars) {
    switch(exp->tag) {
    case t_constant:
        break;
    case t_variable:
        // TODO Need hashtable for this (or more precisely symbol table)
        for (size_t i = 0; i < *pnvars; i++)
            if (!strcmp(exp->id, *pvars[i])) return;

        *pvars = realloc(*pvars, (1 + *pnvars)*sizeof(**pvars));
        assert(*pvars);

        (*pvars)[(*pnvars)++] = exp->id;
        break;
    default:
        for (size_t i = 0; i < exp->n_child; i++)
            find_vars(exp->children[i], pvars, pnvars);
    }
}

static void generate_variables(struct expr *exp, FILE *out) {
    char **variables = NULL;
    size_t nvars = 0;

    find_vars(exp, &variables, &nvars);

    for (size_t i = 0; i < nvars; i++)
        fprintf(out, ".local double %s\n", variables[i]);

    for (size_t i = 0; i < nvars; i++) {
        fprintf(out, "\tcall.d scan_d\n");
        fprintf(out, "\tst.d %s\n", variables[i]);
    }

    free(variables);
}

void generate_code(struct expr *exp, FILE *out) {
    // Declare runtime functions
    fputs(".function double power_d\n"
          ".param double arg\n"
          ".param double pow\n"
          ".function double log_d\n"
          ".param double arg\n"
          ".function double scan_d\n"
          ".function void print_d\n"
          ".param double arg\n"
          ".function double abs_d\n"
          ".param double arg\n"
          "\tld.d arg\n"
          "\tdup.l\n"
          "\tld.d $0\n"
          "\tjg.d 1\n"
          "\tneg.d\n"
          "1:\n"
          "\tret.d\n", out);

    // Declare main
    fputs(".function void main\n", out);

    // Generate and read variables
    // (all of them are locals for now and read automatically upon startup)
    generate_variables(exp, out);

    do_codegen(exp, out);
    fputs("\tcall print_d\n", out);
    fputs("\tret\n", out);
}
