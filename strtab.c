#define _POSIX_C_SOURCE 200809L

#include "strtab.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t hash(char *str) {
    // Murmur...
    uint64_t h = 525201411107845655ULL;
    while (*str) {
        h ^= *str++;
        h *= 0x5BD1E9955BD1E995;
        h ^= h >> 47;
    }
    if (sizeof (size_t) == sizeof(uint64_t)) return h;
    else return h ^ (h >> 32);
}

static id_t insert(struct strtab *stab, id_t id, char *str) {
    size_t ha = hash(str) % stab->caps;

    while (stab->data[ha]) {
        if (!strcmp(stab->ids[stab->data[ha] - 1], str)) {
            free(str);
            return stab->data[ha];
        }
        ha = (ha + 1) % stab->caps;
    }

    if (!id) {
        stab->ids[stab->size] = str;
        id = ++stab->size;
    }

    stab->data[ha] = id;
    return id;
}

id_t intern(struct strtab *stab, char *str) {
    if (!str) return invalid_id_;

    if (stab->size + 1 > 3*stab->caps/4) {
        
        size_t oldc = stab->caps;
        id_t *old = stab->data;

        stab->caps = 5*oldc/4 + 2;
        stab->data = calloc(stab->caps, sizeof(*old));
        stab->ids = realloc(stab->ids, stab->caps*sizeof(*stab->ids));
        assert(stab->data);
        assert(stab->ids);

        for (size_t i = 0; i < oldc; i++)
            if (old[i]) insert(stab, old[i], stab->ids[old[i]-1]);
        free(old);
    }

    return insert(stab, 0, str);
}

const char *string_of(struct strtab *stab, id_t id) {
    if (!id) *((volatile char*)0) = 0;
    assert(id);
    return stab->ids[id - 1];
}

void free_strtab(struct strtab *stab) {
    free(stab->data);
    free(stab->ids);
}

void init_strtab(struct strtab *stab) {
    *stab = STRTAB_INIT;
    id_t res;

    res = intern(stab, strdup("log")); assert(res == log_);
    assert(!strcmp("log", string_of(stab, res)));
    assert(intern(stab, strdup("log")) == log_);
    res = intern(stab, strdup("if")); assert(res == if_);
    res = intern(stab, strdup("then")); assert(res == then_);
    res = intern(stab, strdup("else")); assert(res == else_);
    res = intern(stab, strdup("while")); assert(res == while_);
    res = intern(stab, strdup("do")); assert(res == do_);
    res = intern(stab, strdup("(")); assert(res == left_brace_);
    res = intern(stab, strdup(")")); assert(res == right_brace_);
    res = intern(stab, strdup("<")); assert(res == less_);
    res = intern(stab, strdup("<=")); assert(res == less_eq_);
    res = intern(stab, strdup(">")); assert(res == greater_);
    res = intern(stab, strdup(">=")); assert(res == greater_eq_);
    res = intern(stab, strdup("==")); assert(res == equal_);
    res = intern(stab, strdup("!=")); assert(res == not_equal_);
    res = intern(stab, strdup("!")); assert(res == not_);
    res = intern(stab, strdup("&&")); assert(res == and_);
    res = intern(stab, strdup("||")); assert(res == or_);
    res = intern(stab, strdup("+")); assert(res == plus_);
    res = intern(stab, strdup("-")); assert(res == minus_);
    res = intern(stab, strdup("/")); assert(res == divide_);
    res = intern(stab, strdup("*")); assert(res == multiply_);
    res = intern(stab, strdup("^")); assert(res == power_);
    res = intern(stab, strdup(",")); assert(res == comma_);
    res = intern(stab, strdup(";")); assert(res == semi_);
}
