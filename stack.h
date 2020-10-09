/* NOTE No include guard here */

#undef __assert_fail
#undef assert
#undef caps
#undef create_stack
#undef data
#undef extern
#undef free_stack
#undef hash
#undef init_caps
#undef inline
#undef int
#undef long
#undef return
#undef size
#undef size
#undef sizeof
#undef stack
#undef stack_alloc__
#undef stack_free__
#undef stack_lock__
#undef stack_lock_write__
#undef stack_pop
#undef stack_push
#undef stack_size
#undef stack_top
#undef stack_unlock__
#undef stack_unlock_write__
#undef static
#undef struct
#undef unsigned
#undef value
#undef void
#undef stack_assert_fail__

#include <stdio.h>

/* Stack element type
 * Only one word type names are
 * allowed (e.g. long long would not work, use int64_t instead)
 * To overcome this limitation define STACK_NAME */
#ifndef ELEMENT_TYPE
#   define ELEMENT_TYPE int
#endif

/* Optional stack functions prefix */
#ifndef STACK_NAME
#   define STACK_NAME ELEMENT_TYPE
#endif

#define STACK_CAT__(name, type) name##_##type
#define STACK_TEMPLATE__(name, type) STACK_CAT__(name, type)

// Silent failure mode
#ifdef SILENT_WITH_DEFAULT
                                     /* V no brackets here to allow void */
#define ASSERT__(s, x, r) if (!(x)) return r;
#else
#define SILENT_WITH_DEFAULT
#define ASSERT__(s, x, r) ((void)((x) || (stack_assert_fail__((void**)&s->data, #x, __FILE__, __LINE__, __func__), 0)))
#endif


/* Internal functions implemented elsewhere */
extern long stack_lock_write__(void **, long);
extern long stack_unlock_write__(void **);
extern long stack_lock__(void **);
extern long stack_unlock__(void **);
extern long stack_unlock__(void **);
extern long stack_free__(void **);
extern void *stack_alloc__(long, const char *, const char *, int);
_Noreturn extern void stack_assert_fail__(void **, const char *, const char *, int, const char *);

/**
 * Immutable safe stack
 *
 * @note stack is mmaped
 * @note data is read-only
 * @note long is used instead of size_t in order to prevent overriding
 */
struct STACK_TEMPLATE__(stack, STACK_NAME) {
    ELEMENT_TYPE *data;
};

/**
 * @function struct stack_T create_stack_T(long init_caps)
 *
 * @param[in] init_caps initial capacity of the stack in elements
 * @return new stack
 *
 * @note returns NULL stack on failure
 */
inline static struct STACK_TEMPLATE__(stack, STACK_NAME) STACK_TEMPLATE__(create_stack, STACK_NAME)(long init_caps) {
    return (struct STACK_TEMPLATE__(stack, STACK_NAME)){ stack_alloc__(sizeof(ELEMENT_TYPE)*init_caps, "<unknown>", "<unknown>", 0) };
}

/**
 * Declare new stack variable
 */
#define DECLARE_STACK(name, type, postf, init_caps) struct STACK_TEMPLATE__(stack, postf) name =\
    { stack_alloc__(sizeof(type)*init_caps, "struct stack_" # postf " " # name,  __FILE__, __LINE__) }

/* Free stack */
inline static void STACK_TEMPLATE__(free_stack, STACK_NAME)(struct STACK_TEMPLATE__(stack, STACK_NAME) *stack) {
    ASSERT__(stack, stack_free__((void**)&stack->data) >= 0,);
}

/**
 * @function T stack_top_T(struct stack_T *stack)
 *
 * @return stack top by value
 *
 * @note aborts if inconsistency detected
 */
inline static ELEMENT_TYPE STACK_TEMPLATE__(stack_top, STACK_NAME)(struct STACK_TEMPLATE__(stack, STACK_NAME) *stack) {
    long size = stack_lock__((void**)&stack->data);
    ASSERT__(stack, size > 0, SILENT_WITH_DEFAULT);
    ELEMENT_TYPE value = stack->data[size / sizeof(ELEMENT_TYPE) - 1];
    ASSERT__(stack, stack_unlock__((void**)&stack->data) >= 0, SILENT_WITH_DEFAULT);
    return value;
}

/** @function long stack_size_T(struct stack_T *stack);
 *
 *  @param[in] stack
 *  @return stack size in elements
 *
 *  @note aborts if inconsistency detected
 */
inline static long STACK_TEMPLATE__(stack_size, STACK_NAME)(struct STACK_TEMPLATE__(stack, STACK_NAME) *stack) {
    long size = stack_lock__((void**)&stack->data);
    ASSERT__(stack, size >= 0, -1UL);
    ASSERT__(stack, stack_unlock__((void**)&stack->data) >= 0, -1UL);
    return size / sizeof(ELEMENT_TYPE);
}

/**
 * @function void stack_push_T(struct stack_T *stack, T value)
 *
 * Push value to the stack
 *
 * @param[inout] stack stack object
 * @param[in] value value to push
 *
 * @note Stack can be relocated during push
 * @note aborts if inconsistency detected
 */
inline static void STACK_TEMPLATE__(stack_push, STACK_NAME)(struct STACK_TEMPLATE__(stack, STACK_NAME) *stack, ELEMENT_TYPE *value) {
    long size = stack_lock_write__((void**)&stack->data, sizeof(ELEMENT_TYPE));
    ASSERT__(stack, size >= 0,);
    stack->data[size / sizeof(ELEMENT_TYPE)] = *value;
    ASSERT__(stack, stack_unlock_write__((void **)&stack->data) >= 0,);
}

/**
 * @function void T stack_pop_T(struct stack_T stack)
 *
 * Pop value from the stack
 *
 * @param[inout] stack stack object
 * @return dropped element
 *
 * @note aborts if inconsistency detected
 */
inline static ELEMENT_TYPE STACK_TEMPLATE__(stack_pop, STACK_NAME)(struct STACK_TEMPLATE__(stack, STACK_NAME) *stack) {
    long size = stack_lock_write__((void**)&stack->data, -sizeof(ELEMENT_TYPE));
    ASSERT__(stack, size >= 0, SILENT_WITH_DEFAULT);
    ELEMENT_TYPE value = stack->data[size / sizeof(ELEMENT_TYPE) - 1];
    ASSERT__(stack, stack_unlock_write__((void **)&stack->data) >= 0, SILENT_WITH_DEFAULT);
    return value;
}

/**
 * Set log file
 *
 * @param file log file
 *
 * @note default is stderr
 * @note NULL for silencing
 */

void stack_set_logfile(FILE *file);

#undef SILENT_WITH_DEFAULT
#undef ELEMENT_TYPE
#undef STACK_NAME
#undef ASSERT__
