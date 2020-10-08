#define _GNU_SOURCE

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef NDEBUG
// If this is defined stack will consider it to be poisonous
#    define MAGIC_FAILURE 0xFA11ED66
#endif

#define STACK_SIZE_STEP(x) ((x)*2)
#define STACK_INIT_SIZE (sysconf(_SC_PAGE_SIZE))

// Debug output width
#define BYTES_PER_LINE 8

struct generic_stack {
#ifndef NDEBUG
    const char *stack_decl;
    const char *stack_file;
    int stack_line;
#endif

    unsigned long hash;
    long size;
    long caps;
    uint8_t data[];
};

// Use one global mutex for all stacks
// it is more secure than using per-stack mutices
// but leads to worse perforemance
static pthread_rwlock_t mtx = PTHREAD_RWLOCK_INITIALIZER;

// Jump buffer and saved signal handlers for SEGV handling
// They are thread local because multiple threads can have simultanious
// access to the stack in read-only mode
_Thread_local static struct sigaction old_sigbus, old_sigsegv;
_Thread_local static sigset_t oldsigset;
_Thread_local static jmp_buf savepoint;


// Round to the power of 2 not less than x
inline static long round2pow(long x) {
    return 1L << (CHAR_BIT * sizeof(long) - __builtin_clzl(x) - 1 + !!(x & (x - 1)));
}


// Calculate crc32 or crc64-ecma depending on address size
#define CRC_CONSTANT (sizeof(unsigned long) == 8 ? 0xEDB88320 : 0xC96C5795D7870F42ULL)

// Calculate crc32/crc64 check sum of the stack (excluding hash field itself)
static unsigned long crc(struct generic_stack *stk) {
    uint8_t *dstart = (uint8_t *)stk + offsetof(struct generic_stack, size);
    uint8_t *dend = (uint8_t *)stk + stk->size - offsetof(struct generic_stack, size);
    unsigned long crc = -1ULL;

    while (dstart < dend) {
        for(uint8_t byte = *dstart++, j = 0; j < CHAR_BIT; j++, byte >>= 1)
            crc = (crc >> 1) ^ (CRC_CONSTANT * ((byte ^ crc) & 1));
    }
    return ~crc;
}

inline static struct generic_stack *stack_ptr(void *stk) {
    // Pointer to data field of stack is passed from user
    // And we want pointer to the start of the generic_stack
    return  (struct generic_stack *)((uint8_t *)stk - sizeof(struct generic_stack));
}

static void handle_fault(int code) {
    siglongjmp(savepoint, 1);
    (void)code;
}

static void checked_start(void) {
    // Intercept signals
    sigaction(SIGBUS, &(struct sigaction){ .sa_handler = handle_fault }, &old_sigbus);
    sigaction(SIGSEGV, &(struct sigaction){ .sa_handler = handle_fault }, &old_sigsegv);

    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGSEGV);
    sigaddset(&signals, SIGBUS);
    pthread_sigmask(SIG_UNBLOCK, &signals, &oldsigset);
}

static void checked_end(void) {
    sigaction(SIGBUS, &old_sigbus, NULL);
    sigaction(SIGSEGV, &old_sigsegv, NULL);
    pthread_sigmask(SIG_UNBLOCK, &oldsigset, NULL);
}

static _Bool stack_check(void **stk) {
    // TODO Filter mprotects

    if ((uintptr_t)stk < (uintptr_t)sysconf(_SC_PAGE_SIZE)) return 0;
    if ((uintptr_t)stk % sizeof(void *)) return 0;

    // Check pointer alignment (it is mmaped)
    // (pointer to data is passed here)
    if ((uintptr_t)*stk % sysconf(_SC_PAGE_SIZE) != sizeof(struct generic_stack)) return 0;

    struct generic_stack *stack = stack_ptr(*stk);

    // Size is non-negative
    if (stack->size < (long)sizeof *stack) return 0;
    // Size cannot be more than capacity
    if (stack->size > stack->caps) return 0;
    // Stack capacity cannot be less than initial
    if (stack->caps < STACK_INIT_SIZE) return 0;
    // Stack capacity is a power of 2
    if ((stack->caps - 1) & stack->caps) return 0;

    // Calculate hash of the whole stack, not including
    // hash field itself
    unsigned long hash = crc(stack);

    // Hash cannot be changed
    if (stack->hash != hash) return 0;

#ifdef MAGIC_FAILURE
    // Search for poison
    const uint32_t *end = (uint32_t *)stack->data + (stack->size - sizeof(*stack) + sizeof(uint32_t) - 1)/sizeof(uint32_t);
    for (const uint32_t *p = (uint32_t *)stack->data; p < end; p++) {
        if (*p == MAGIC_FAILURE) return 0;
    }
#endif

    return 1;
}

long stack_lock_write__(void **stk, long elem_size) {
    if (pthread_rwlock_wrlock(&mtx)) return -1;
    if ((checked_start(), sigsetjmp(savepoint, 1))  || !stack_check(stk)) goto e_restore_after_fault;

    struct generic_stack *stack = stack_ptr(*stk);

    // Unlock memory for writing
    mprotect(stack, stack->caps, PROT_READ | PROT_WRITE);

    // Adjust size if want to insert (elem_size is not 0)
    if (stack->size + elem_size + (long)sizeof *stack > stack->caps) {
        long newsz = STACK_SIZE_STEP(stack->caps);
        if (newsz < 0) goto e_restore;
        void *res = mremap(stack, stack->caps, newsz, PROT_READ | PROT_WRITE);
        if (res == MAP_FAILED) goto e_restore;

        stack = res;
        stack->caps = newsz;

#ifdef MAGIC_FAILURE
        long i = (stack->size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
        for (; i < newsz/(long)sizeof(uint32_t); i++) {
            ((uint32_t*)stack)[i] = MAGIC_FAILURE;
        }
#endif
        // TODO Add new stack address to ebpf map
    }

    long sz = stack->size - sizeof *stack;
    stack->size += elem_size;

    return sz;

e_restore:
    mprotect(stack_ptr(*stk), stack->caps, PROT_READ);

e_restore_after_fault:
    checked_end();
    pthread_rwlock_unlock(&mtx);
    return -1;
}

long stack_unlock_write__(void **stk) {
    struct generic_stack *stack = stack_ptr(*stk);

    // Rehash
    stack->hash = crc(stack);

    long res = 0;

    // Make memory read-only
    if (!mprotect(stack, stack->caps, PROT_READ)) {
        res = -!stack_check(stk);
    } else res = -1;

    checked_end();
    if (pthread_rwlock_unlock(&mtx)) return -1;
    return res;
}

long stack_lock__(void **stk) {
    if (pthread_rwlock_rdlock(&mtx)) return -1;
    if ((checked_start(), sigsetjmp(savepoint, 1)) || !stack_check(stk)) {
        checked_end();
        pthread_rwlock_unlock(&mtx);
        return -1;
    }
    return stack_ptr(*stk)->size - sizeof(struct generic_stack);
}

long stack_unlock__(void **stk) {
    long res = -!stack_check(stk);

    checked_end();
    if (pthread_rwlock_unlock(&mtx)) return -1;

    return res;
}

void *stack_alloc__(long caps, const char *decl, const char *file, int line) {
    if (caps < 0) return NULL;

    caps += sizeof(struct generic_stack);
    if (caps < STACK_INIT_SIZE) caps = STACK_INIT_SIZE;
    caps = round2pow(caps);

    // Overflow
    if (caps < 0) return NULL;

    if (checked_start(), sigsetjmp(savepoint, 1)) goto e_stack_early;

    struct generic_stack *stack = mmap(NULL, caps, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
    if (stack == MAP_FAILED) goto e_stack;

#ifdef MAGIC_FAILURE
     for (long i = 0; i < caps/4; i++) {
         ((uint32_t*)stack)[i] = MAGIC_FAILURE;
     }
#endif

    // Initialize stack metadata
    // All memory in stack is zeroed because it is mmaped
    stack->caps = caps;
    stack->size = sizeof *stack;
    stack->hash = crc(stack);

#ifndef NDEBUG
    stack->stack_decl = decl;
    stack->stack_file = file;
    stack->stack_line = line;
#else
    (void)decl;
    (void)file;
    (void)line;
#endif

    // Make stack memory read-only
    if (mprotect(stack, stack->caps, PROT_READ)) goto e_stack;

    void *data = stack->data;
    if (!stack_check(&data)) goto e_stack;

    // TODO Add new stack address to ebpf map

    checked_end();
    return  (uint8_t *)stack + sizeof(*stack);

e_stack:
    if (stack != MAP_FAILED) munmap(stack, stack->caps);
e_stack_early:
    checked_end();
    return NULL;
}

long stack_free__(void **stk) {
    // We can free stack only if we have exclusive
    // access to the stack
    if (stack_lock_write__(stk, 0) < 0) return -1;

    long res = 0;

    struct generic_stack *stack = stack_ptr(*stk);
    if (munmap(stack, stack->size)) res = -1;

    checked_end();
    pthread_rwlock_unlock(&mtx);
    return res;
}

static FILE *logfile;

void stack_set_logfile(FILE *file) {
    if (logfile != file) {
        if (logfile != stderr && logfile) fclose(logfile);
        logfile = file;
        setvbuf(logfile, NULL, _IONBF, BUFSIZ);
    }
}

_Noreturn void stack_assert_fail__(void **stk, const char * expr, const char *file, int line, const char *func) {
    if (checked_start(), sigsetjmp(savepoint, 1)) {
        checked_end();
        if (logfile) fprintf(logfile, "\nStack dump failed with SIGSEGV\n");
        __assert_fail(expr, file, line, func);
    }

    if (logfile) {

        // At this point we don't even know element size
        // so just hexdump

        struct generic_stack *stack = stack_ptr(*stk);
#ifndef NDEBUG
        fprintf(logfile, "Stack defined at %s:%d as\n%s = {\n", stack->stack_file, stack->stack_line, stack->stack_decl);
#else
        fprintf(logfile, "struct stack stk = {\n");
#endif
        fprintf(logfile, "\thash = 0x%016lX\n", stack->hash);
        fprintf(logfile, "\tsize = %ld\n", stack->size);
        fprintf(logfile, "\tcaps = %ld\n", stack->caps);
        fprintf(logfile, "\tdata = (uint8_t[]){\n");
        for (long i = (long)sizeof(*stack); i < stack->size;) {
            fprintf(logfile, "\t\t[0x%08lX] = ", i - sizeof(*stack));
            for (long j = 0; i < stack->size && j < BYTES_PER_LINE; j++, i++) {
                fprintf(logfile, "0x%02X, ", stack->data[i - sizeof(*stack)]);
            }
            fprintf(logfile, "\n");
        }
        fprintf(logfile, "\t}\n}\n");
    }

    checked_end();
    __assert_fail(expr, file, line, func);
}

static __attribute__((constructor)) void init_stack(void) {
    logfile = stderr;

    // TODO Init seccomp ebpf filter
}

