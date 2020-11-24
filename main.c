#define _POSIX_C_SOURCE 200809L

#include "expr.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

enum retcode {
    ERC_WRONG_EXPR = 1,
    ERC_NO_IN_FILE = 2,
    ERC_NO_OUT_FILE = 3,
    ERC_WRONG_PARAM = 4,
};

_Noreturn void usage(const char *argv0) {
    printf("Usage:\n"
           "\t%s [-f <format>] [-o <outfile>] <expr>\n"
           "\t%s [-f <format>] [-o <outfile>] -i <infile>\n"
           "<format> is one of tex, string, graph\n"
           "Default <outfile> is stdout\n", argv0, argv0);
    exit(ERC_WRONG_PARAM);
}

const char *make_input(const char *input, const char *arg, size_t *size) {
    if (!input) return arg;

    char *addr = NULL;
    int fd = open(input, O_RDONLY);

    struct stat stt;
    if (fd >= 0 && fstat(fd, &stt) >= 0) {
        *size = stt.st_size + 1;
        char *addr = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
        if (addr == MAP_FAILED) addr = NULL;
    }

    close(fd);
    return addr;
}

int main(int argc, char **argv) {
    enum format fmt = fmt_string;
    const char *output = NULL, *input = NULL;

    for (int c; (c = getopt(argc, argv, "i:o:f:")) != -1;) {
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
            output = optarg;
            break;
        case 'i':
            input = optarg;
            break;
        default:
            usage(argv[0]);
        }
    }

    if (!input && (argc <= optind || !argv[optind])) usage(argv[0]);

    size_t size;
    const char *in = make_input(input, argv[optind], &size);
    if (!in) return ERC_NO_IN_FILE;

    FILE *out = output ? fopen(output, "w") : stdout;
    if (!out) return ERC_NO_OUT_FILE;

    struct expr *exp = parse_tree(in);
    if (!exp) return ERC_WRONG_EXPR;

    dump_tree(out, fmt, exp);

    if (input) munmap((void *)in, size);

    free_tree(exp);
    return 0;
}
