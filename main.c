#define _POSIX_C_SOURCE 200809L

#include "expr.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Noreturn void usage(const char *argv0) {
    printf("Usage:\n"
           "\t%s [-f <format>] [-o <file>] <expr>\n"
           "<format> is one of tex, string, graph\n"
           "Default <file> is stdout\n", argv0);
    exit(0);
}

int main(int argc, char **argv) {
    enum format fmt = fmt_string;
    const char *file = NULL;

    for (int c; (c = getopt(argc, argv, "o:f:")) != -1;) {
        if (c == 'f') {

        } else usage(argv[0]);
        switch (c) {
        case 'f':
            if (!strcmp(optarg, "tex"))
                fmt = fmt_tex;
            else if (!strcmp(optarg, "string"))
                fmt = fmt_string;
            else if (!strcmp(optarg, "graph"))
                fmt = fmt_graph;
            else usage(argv[0]);
            break;
        case 'o':
            file = optarg;
            break;
        default:
            usage(argv[0]);
        }
    }

    if (argc <= optind || !argv[optind]) usage(argv[0]);

    char *buf = argv[optind];
    FILE *in = fmemopen(buf, strlen(argv[optind]), "r");
    if (!in) return 1;

    FILE *out = file ? fopen(file, "w") : stdout;
    if (!out) return 2;

    struct expr *exp = parse_tree(in);
    if (!exp) return 1;

    dump_tree(out, fmt, exp);

    free_tree(exp);
    return 0;
}
