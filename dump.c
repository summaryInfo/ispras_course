#include "expr.h"

#include <assert.h>

static int dump_tree_graph(FILE *out, struct expr *expr, int index) {
    assert(expr && expr->tag <= t_MAX);

    enum tag tag = expr->tag;
    struct tag_info *info = &tags[tag];

    if (tag == t_constant) {
        fprintf(out, "\tn%d[label=\"const %lf\"];\n", index, expr->value);
    } else if (tag == t_variable) {
        fprintf(out, "\tn%d[label=\"var %s\"];\n", index, expr->id);
    } else {
        assert((tags[tag].arity < 0 && expr->n_child > 1) || (size_t)tags[tag].arity == expr->n_child);

        struct expr **it = expr->children;
        int node_index = index++, next_index;

        fprintf(out, "\tn%d[label=\"%s\"];\n", node_index, info->name ? info->name : info->alt);

        do {
            next_index = dump_tree_graph(out, *it++, index);
            fprintf(out , "\tn%d -- n%d;\n", node_index, index);
            index = next_index;
        } while (it < &expr->children[expr->n_child]);
    }

    return index;
}

static void dump_tree_tex(FILE *out, struct expr *expr, int outer_prio) {
    assert(expr && expr->tag <= t_MAX);

    enum tag tag = expr->tag;
    struct tag_info *info = &tags[tag];

    assert(tag == t_constant ||
           tag == t_variable ||
           (info->arity < 0 && expr->n_child > 1) ||
           (size_t)info->arity == expr->n_child);

    switch(tag) {
    case t_constant:
        fprintf(out, "%lf", expr->value);
        break;
    case t_variable:
        assert(expr->id);
        fputs(expr->id, out);
        break;
    case t_multiply: {
        // Multiplication and division needs
        // special treatment in tex
        // because of "{a \over b} syntax"

        struct expr **it = expr->children;
        dump_tree_tex(out, *it++, info->prio);

        _Bool has_over = 0;
        for (struct expr **in = it; in < &expr->children[expr->n_child]; in++)
            has_over |= (*in)->tag == t_inverse;

        if (has_over) {
            fputc('{', out);

            int nput = 0;

            while (it < &expr->children[expr->n_child]) {
                struct expr *ch = *it++;
                if (ch->tag != t_inverse) {
                    fputs(info->tex_name, out);
                    dump_tree_tex(out, ch, info->prio);
                    nput++;
                };
            }

            if (!nput) fputc('1', out);

            fputs("\\over ", out);

            while (it < &expr->children[expr->n_child]) {
                struct expr *ch = *it++;
                if (ch->tag == t_inverse) {
                    fputs(info->tex_name, out);
                    dump_tree_tex(out, ch, info->prio);
                };
            }

            fputc('}', out);
        }
    }  //fallthrough
    default:
        if (outer_prio > info->prio) fputs("\\left(", out);

        struct expr **it = expr->children;
        dump_tree_tex(out, *it++, info->prio);

        assert(info->tex_name);

        while (it < &expr->children[expr->n_child]) {
            struct expr *ch = *it++;

            if (info->alt_tag == ch->tag) {
                ch = ch->children[0];
                assert(ch->n_child == (size_t)tags[ch->tag].arity);
                fputs(info->alt, out);
            } else fputs(info->tex_name, out);

            dump_tree_tex(out, ch, info->prio);
        }

        if (outer_prio > info->prio) fputs("\\right)", out);
    }
}


static void dump_tree_string(FILE *out, struct expr *expr, int outer_prio) {
    assert(expr && expr->tag <= t_MAX);

    enum tag tag = expr->tag;
    struct tag_info *info = &tags[tag];

    if (tag == t_constant) {
        fprintf(out, "%lf", expr->value);
    } else if (tag == t_variable) {;
        assert(expr->id);
        fputs(expr->id, out);
    } else {

        if (outer_prio > info->prio) fputc('(', out);

        assert((info->arity < 0 && expr->n_child > 1) || (size_t)info->arity == expr->n_child);
        assert(info->name);

        struct expr **it = expr->children;
        dump_tree_string(out, *it++, info->prio);

        while (it < &expr->children[expr->n_child]) {
            struct expr *ch = *it++;

            if (info->alt_tag == ch->tag) {
                ch = ch->children[0];
                assert(ch->n_child == (size_t)tags[ch->tag].arity);
                fputs(info->alt, out);
            } else fputs(info->name, out);

            dump_tree_string(out, ch, info->prio);
        }

        if (outer_prio > info->prio) fputc(')', out);
    }
}

void dump_tree(FILE *out, enum format fmt, struct expr *expr) {
    switch (fmt) {
    case fmt_graph:
        fputs("graph \"\" {\n\tlabel = \"", out);
        dump_tree_string(out, expr, MAX_PRIO);
        fputs("\"\n", out);
        dump_tree_graph(out, expr, 0);
        fputs("}\n", out);
        break;
    case fmt_string:
        dump_tree_string(out, expr, MAX_PRIO);
        break;
    case fmt_tex:
        fputs("$$\n", out);
        dump_tree_tex(out, expr, MAX_PRIO);
        fputs("\n$$\n\bye\n", out);
        break;
    }
    fflush(out);
}
