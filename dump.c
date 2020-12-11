#include "expr.h"
#include "expr-impl.h"

#include <assert.h>

static int dump_tree_graph(FILE *out, struct expr *expr, int index) {
    assert(expr && expr->tag <= t_MAX);

    enum tag tag = expr->tag;
    struct tag_info *info = &tags[tag];

    if (tag == t_constant) {
        fprintf(out, "\tn%d[label=\"const %lg\", shape=box, fillcolor=lightgrey, style=filled];\n", index, expr->value);
    } else if (tag == t_variable) {
        fprintf(out, "\tn%d[label=\"var %s\", shape=box];\n", index, expr->id);
    } else {
        assert((tags[tag].arity < 0 && expr->n_child > 0) || (size_t)tags[tag].arity == expr->n_child);

        struct expr **it = expr->children;
        int node_index = index++, next_index;

        fprintf(out, "\tn%d[label=\"%s\", shape=triangle, color=lightblue, style=filled];\n", node_index, info->name ? info->name : info->alt);

        do {
            next_index = dump_tree_graph(out, *it++, index);
            fprintf(out , "\tn%d -- n%d;\n", node_index, index);
            index = next_index;
        } while (it < &expr->children[expr->n_child]);
    }

    return index + 1;
}

static void dump_tree_tex(FILE *out, struct expr *expr, int outer_prio) {
    assert(expr && expr->tag <= t_MAX);

    enum tag tag = expr->tag;
    struct tag_info *info = &tags[tag];

    assert(tag == t_constant ||
           tag == t_variable ||
           (info->arity < 0 && expr->n_child > 0) ||
           (size_t)info->arity == expr->n_child);

    switch(tag) {
    case t_constant:
        fprintf(out, "%lg", expr->value);
        break;
    case t_variable:
        assert(expr->id);
        fputs(expr->id, out);
        break;
    case t_while:
    case t_if:
        if (outer_prio < info->prio) fputs("\\left(", out);
        fputs(tag == t_if ? "{\rm if}" : "{\rm while}", out);
        dump_tree_tex(out, expr->children[0], info->prio);
        fputs(tag == t_if ? "{\rm then}" : "{\rm do}", out);
        dump_tree_tex(out, expr->children[1], info->prio);
        if (tag == t_while && !is_eq_const(expr->children[2], 0)) {
            fputs("{\rm else}", out);
            dump_tree_tex(out, expr->children[2], info->prio);
        }
        if (outer_prio < info->prio) fputs("\\right)", out);
        break;
    case t_multiply: {
        // Multiplication and division needs
        // special treatment in tex
        // because of "{a \over b} syntax"

        _Bool has_over = 0;
        for (struct expr **in = expr->children; in < &expr->children[expr->n_child]; in++)
            has_over |= (*in)->tag == t_inverse;

        if (has_over) {
            fputc('{', out);

            int nput = 0;
            struct expr **it = expr->children;
            while (it < &expr->children[expr->n_child]) {
                struct expr *ch = *it++;
                if (ch->tag != t_inverse) {
                    if (nput) fputs(info->tex_name, out);
                    dump_tree_tex(out, ch, MAX_PRIO);
                    nput++;
                }
            }

            if (!nput) fputc('1', out);

            fputs("\\over ", out);

            nput = 0;
            it = expr->children;
            while (it < &expr->children[expr->n_child]) {
                struct expr *ch = *it++;
                if (ch->tag == t_inverse) {
                    if (nput) fputs(info->tex_name, out);
                    dump_tree_tex(out, ch->children[0], MAX_PRIO);
                    nput++;
                }
            }

            fputc('}', out);
            break;
        }
    }  //fallthrough
    default:
        if (outer_prio < info->prio) fputs("\\left(", out);

        struct expr **it = expr->children;
        if (expr->n_child == 1) fputs(info->tex_name, out);
        fputc('{', out);
        dump_tree_tex(out, *it++, info->prio);
        fputc('}', out);

        assert(info->tex_name);


        while (it < &expr->children[expr->n_child]) {
            struct expr *ch = *it++;

            if (info->alt_tag == ch->tag) {
                assert(ch->n_child == (size_t)tags[ch->tag].arity);
                ch = ch->children[0];
                fputs(info->alt, out);
            } else if (tag != t_add ||
                       ch->tag != t_constant ||
                       ch->value >= 0) {
                fputs(info->tex_name, out);
            }

            fputc('{', out);
            dump_tree_tex(out, ch, expr->tag == t_power ? MAX_PRIO : info->prio);
            fputc('}', out);
        }


        if (outer_prio < info->prio) fputs("\\right)", out);
    }
}


static void dump_tree_string(FILE *out, struct expr *expr, int outer_prio) {
    assert(expr && expr->tag <= t_MAX);

    enum tag tag = expr->tag;
    struct tag_info *info = &tags[tag];

    if (tag == t_constant) {
        fprintf(out, "%lg", expr->value);
    } else if (tag == t_variable) {;
        assert(expr->id);
        fputs(expr->id, out);
    } else if (tag == t_if || tag == t_while) {
        if (outer_prio < info->prio) fputc('(', out);
        fputs(tag == t_if ? "if " : "while ", out);
        dump_tree_string(out, expr->children[0], info->prio);
        fputs(tag == t_if ? " then " : " do ", out);
        dump_tree_string(out, expr->children[1], info->prio);
        if (tag == t_if && !is_eq_const(expr->children[2], 0)) {
            fputs(" else ", out);
            dump_tree_string(out, expr->children[2], info->prio);
        }
        if (outer_prio < info->prio) fputc(')', out);
    } else {

        if (outer_prio < info->prio ||
            (outer_prio == info->prio && tag == t_power)) fputc('(', out);

        assert((info->arity < 0 && expr->n_child > 0) || (size_t)info->arity == expr->n_child);
        assert(info->name);

        struct expr **it = expr->children;
        if (expr->n_child == 1 && info->arity == 1) fputs(info->name, out);
        dump_tree_string(out, *it++, info->prio);

        while (it < &expr->children[expr->n_child]) {
            struct expr *ch = *it++;
            assert(ch);

            if (info->alt_tag == ch->tag) {
                assert(ch->n_child == (size_t)tags[ch->tag].arity);
                ch = ch->children[0];
                fputs(info->alt, out);
            } else if (tag != t_add ||
                       ch->tag != t_constant ||
                       ch->value >= 0) {
                fputs(info->name, out);
            }

            dump_tree_string(out, ch, info->prio);
        }

        if (outer_prio < info->prio ||
            (outer_prio == info->prio && tag == t_power)) fputc(')', out);
    }
}

void dump_tree(FILE *out, enum format fmt, struct expr *expr, bool full) {
    switch (fmt) {
    case fmt_graph:
        fputs("graph \"\" {\n\tlabel = \"", out);
        dump_tree_string(out, expr, MAX_PRIO);
        fputs("\";\n", out);
        dump_tree_graph(out, expr, 0);
        fputs("}\n", out);
        break;
    case fmt_string:
        dump_tree_string(out, expr, MAX_PRIO);
        fputc('\n', out);
        break;
    case fmt_tex:
        fputs("$$\n", out);
        dump_tree_tex(out, expr, MAX_PRIO);
        fputs("\n$$\n", out);
        if (full) fputs("\\bye\n", out);
        break;
    }
    fflush(out);
}
