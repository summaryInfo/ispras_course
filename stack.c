#define _GNU_SOURCE

#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

#define STACK_SIZE_STEP(x) ((x)*2)
#define STACK_INIT_SIZE (PAGE_SIZE)

struct generic_stack {
    unsigned long hash;
    long size;
    long caps;
    uint8_t data[];
};

// Use one global mutex for all stacks
// it is more secure than using per-stack mutices
// but leads to worse perforemance
static pthread_rwlock_t mtx = PTHREAD_RWLOCK_INITIALIZER;

// Jump buffer and saved signal handlers for
// SEGV handling
_Thread_local static struct sigaction old_sigbus, old_sigsegv;
static jmp_buf savepoint;

// Round to the power of 2 not less than x
inline static long round2pow(long x) {
    return 1L << (CHAR_BIT * sizeof(long) - __builtin_clzl(x) - 1 + !!(x & (x - 1)));
}

// Calculate crc32/crc64 check sum of the stack (excluding hash field itself)
static unsigned long crc(struct generic_stack *stk) {
    uint8_t *dstart = (uint8_t *)stk + offsetof(struct generic_stack, size);
    uint8_t *dend = (uint8_t *)stk + stk->size - offsetof(struct generic_stack, size);
    unsigned long crc = -1ULL;

    while (dstart < dend) {
        uint8_t byte = *dstart++;
        for(unsigned long j = 0; j < CHAR_BIT; j++, byte >>= 1) {
            _Bool bit = (byte ^ crc) & 1;
            crc >>= 1;
            // Calculate crc32 or crc64-ecma depending on address size
            crc ^= (sizeof(unsigned long) == 8 ? 0xEDB88320 : 0xC96C5795D7870F42ULL) * bit;
        }
    }
    return ~crc;
}

inline static struct generic_stack *stack_ptr(void *stk) {
    // Pointer to data field of stack is passed from user
    // And we want pointer to the start of the generic_stack
    return  (struct generic_stack *)((uint8_t *)stk - sizeof(struct generic_stack));
}

static void handle_fault(int code) {
    longjmp(savepoint, 1);
    (void)code;
}

static int checked_start(void) {
    sigaction(SIGBUS, &(struct sigaction){ .sa_handler = handle_fault }, &old_sigbus);
    sigaction(SIGSEGV, &(struct sigaction){ .sa_handler = handle_fault }, &old_sigsegv);
    return setjmp(savepoint);
}

static void checked_end(void) {
    sigaction(SIGBUS, &old_sigbus, NULL);
    sigaction(SIGSEGV, &old_sigsegv, NULL);
}

static _Bool stack_check(void **stk) {
    // TODO Filter mprotects

    if ((uintptr_t)stk < PAGE_SIZE) return 0;
    if ((uintptr_t)stk % sizeof(void *)) return 0;

    // Check pointer alignment (it is mmaped)
    // (pointer to data is passed here)
    if ((uintptr_t)*stk % PAGE_SIZE != sizeof(struct generic_stack)) return 0;

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
    return 1;
}

long stack_lock_write__(void **stk, long elem_size) {
    if (pthread_rwlock_wrlock(&mtx)) return -1;
    if (checked_start()) goto e_restore_after_fault;
    if (!stack_check(stk)) goto e_restore_after_fault;

    struct generic_stack *stack = stack_ptr(*stk);

    // Unlock memory for writing
    mprotect(stack, stack->caps, PROT_READ | PROT_WRITE);

    // Adjust size if want to insert (elem_size is not 0)
    if (stack->size + elem_size + (long)sizeof *stack > stack->caps) {
        long newsz = STACK_SIZE_STEP(stack->caps);
        if (newsz < 0) goto e_restore;
        void *res = mremap(stack, stack->caps, newsz, PROT_READ | PROT_WRITE);
        if (res == MAP_FAILED) goto e_restore;

        // TODO Add new stack address to ebpf map
    }

    long sz = stack->size - sizeof *stack;
    stack->size += elem_size;

    return sz;

e_restore:
    *stk = NULL;
    munmap(stack_ptr(*stk), stack->caps);

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
    if (checked_start() || !stack_check(stk)) {
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

void *stack_alloc__(long caps) {
    if (caps < 0) return NULL;

    caps += sizeof(struct generic_stack);
    if (caps < STACK_INIT_SIZE) caps = STACK_INIT_SIZE;
    caps = round2pow(caps);

    // Overflow
    if (caps < 0) return NULL;

    if (checked_start()) goto e_stack_early;

    struct generic_stack *stack = mmap(NULL, caps, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
    if (stack == MAP_FAILED) goto e_stack;

    // Initialize stack metadata
    // All memory in stack is zeroed because it is mmaped
    stack->caps = caps;
    stack->size = sizeof *stack;
    stack->hash = crc(stack);

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

long stack_free__(void ** stk) {
    // We can free stack only if we have exclusive
    // access to the stack
    if (!stack_lock_write__(stk, 0)) return -1;

    long res = 0;

    // Intercept signals
    if (!checked_start()) {
        struct generic_stack *stack = stack_ptr(*stk);
        if (munmap(stack, stack->size)) res = -1;
    } else res = -1;

    checked_end();
    pthread_rwlock_unlock(&mtx);
    return res;
}

static __attribute__((constructor)) void init_ebpf(void) {
    // TODO Init seccomp ebpf filter
}

