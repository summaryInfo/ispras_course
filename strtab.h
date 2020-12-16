#ifndef STRTAB_H_
#define STRTAB_H_ 1

#include <stdint.h>
#include <stddef.h>

typedef uint32_t id_t;

struct strtab {
    size_t size;
    size_t caps;
    char **ids;
    id_t *data;
};

enum static_ids {
    invalid_id_,
    log_,
    if_,
    then_,
    else_,
    while_,
    do_,
    left_brace_,
    right_brace_,
    less_,
    less_eq_,
    greater_,
    greater_eq_,
    equal_,
    not_equal_,
    not_,
    and_,
    or_,
    plus_,
    minus_,
    divide_,
    multiply_,
    power_,
    comma_,
    semi_,
};

#define STRTAB_INIT ((struct strtab){0})

/**
 * Get id corresponding to string
 * 
 * @param[in] stab string table
 * @param[in] str string (gets moved)
 * @return new id
 */
id_t intern(struct strtab *stab, char *str);


/**
 * Get string corresponding to string id
 * 
 * @param[in] stab string table
 * @param[in] id string id
 * @return string
 */
const char *string_of(struct strtab *stab, id_t id);

/**
 * Free string table
 */
void free_strtab(struct strtab *stab);

/**
 * Initialize string table
 */
void init_strtab(struct strtab *stab);

#endif

