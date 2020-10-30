#pragma once

#include "util.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

/* String table index is an offset
 * relative to string table start */
using strtab_index = uint32_t;

/* Object file header */
struct vm_header {
    char signature[4]{};
    uint32_t flags{};
    uint32_t funcs_size{};
    uint32_t funcs_offset{};
    uint32_t globals_size{};
    uint32_t globals_offset{};
    uint32_t strtab_size{};
    uint32_t strtab_offset{};
} __attribute__((packed));

/* Object file global variable description */
struct vm_global {
    strtab_index name{};
    char type{}; /* i, l, f or d */
    uint8_t flags{}; /* enum global_flags */
    uint16_t dummy0{};
    uint64_t init_value{}; /* stored binary representation */

    vm_global(strtab_index name_,char type_, uint8_t flags_) :
        name(name_), type(type_), flags(flags_) {}
    vm_global() {}
} __attribute__((packed));

/* Global variable flags */
enum global_flags {
    gf_init = 1 << 0,
};

/* Object file function description
 * as it is represented on disk */
struct vm_function {
    strtab_index name{};
    strtab_index signature{};
    strtab_index locals{};
    uint32_t code_offset{};
    /* if code_size is 0 this is extern function */
    uint32_t code_size{};
} __attribute__((packed));

/* This is an internal representation of a function
 * used by vm and assembler */
struct function {
    strtab_index name{};
    std::string signature;
    std::string locals;
    uint32_t frame_size{};
    uint32_t args_size{};
    std::vector<uint8_t> code;

    function(strtab_index name_, std::string &&signature_, std::string &&locals_,
             uint32_t frame_size_, uint32_t args_size_, std::vector<uint8_t> &&code_) :
        name(name_), signature(std::move(signature_)), locals(std::move(locals_)),
        frame_size(frame_size_), args_size(args_size_), code(std::move(code_)) {}
    function() {}
};

struct object_file {
    /* String table
     * It is not the same as on-file representation
     * for efficiency reasons */
    std::map<std::string, strtab_index> strtab;
    /* Last index in string table */
    uint32_t strtab_offset{};

    /* Names to globals indexes mapping */
    std::map<strtab_index, uint32_t> global_indices;
    /* Globals themselves */
    std::vector<vm_global> globals;

    /* Names to functions mapping */
    std::map<strtab_index, uint32_t> function_indices;
    /* Functions themselves */
    std::vector<function> functions;

    object_file() { id(""); }

    /** Write object file contents on disk
     *
     * @param[inout] str binary file stream
     */
    void write(std::ostream &str);
    /** Read object file contents from disk
     *
     * @param[inout] str binary file stream
     */
    object_file read(std::istream &str);
    /** Swap two object files
     *
     *  @param[inout] other file to swap with
     */
    void swap(object_file &other);
    /** Add string to string table
     *
     * @param [inout] str
     *
     * @return string table index
     *
     * @note str is moved
     */
    strtab_index id(std::string &&str);

    /** Add global variable description
     *
     *  @param [inout] str variable name
     *  @param [in] value initial value
     *  @param [in] init flags signalizing whether value shuold be used
     *  @param [in] type type id character of the variable
     *
     *  @return true iff insertion was successful
     *
     *  @note str is moved
     */
    template <typename T>
    bool add_global(std::string &&str, T value, bool init, char type) {
        strtab_index idx = id(std::move(str));

        auto res = global_indices.find(idx);
        size_t gi = globals.size();
        if (res == global_indices.end()) {
            /* No variable with such name yet */
            global_indices.emplace(idx, globals.size());
            globals.emplace_back(idx, type, init * gf_init);
        } else {
            if (globals[res->second].type != type) return false;
            gi = res->second;
        }

        if (init) {
            if (globals[gi].flags & gf_init) return false;
            globals.back().flags |= gf_init;
            std::memcpy(&globals.back().init_value, &value, sizeof value);
        }
        return true;
    }
};

/* Code assumes that these types have compatible size */
static_assert(sizeof(double) == sizeof(int64_t));
static_assert(sizeof(float) == sizeof(int32_t));

/* opcodes,
 * _ -- type independent
 * _i -- int32_t
 * _l -- int64_t
 * _d -- double
 * _f -- float
 *
 * ld/st -- load/store global variable
 * lda/sta -- load/store argument or local variable (locals have negative offset)
 *
 * pwide is used to extend argument size from one to two bytes
 * and applies to following insns:
 *    ld/st -- variable index, unsigned
 *    lda/sta -- variable index, signed
 *    ldi -- short constant, signed
 *    call/tcall -- function index unsigned
 *    jmp/jz/jnz/je/jne/jl/jg/jle/jge -- offset, signed
 *
 * At the time of jump IP points to the next insn
 *
 * Undefined instructions are prohibited
 */
enum insn {
    op_hlt_ = 0x00,   op_lda_i = 0x01,   op_sta_i = 0x02,   op_ld_i = 0x03,
    op_st_i = 0x04,   op_ldc_i = 0x05,   op_ldi_i = 0x06,   op_add_i = 0x07,
    op_sub_i = 0x08,  op_mul_i = 0x09,   op_div_i = 0x0A,   op_neg_i = 0x0B,
    op_jl_i = 0x0C,   op_jg_i = 0x0D,    op_jlz_i = 0x0E,   op_call_i = 0x0F,

    op_dup_i = 0x10,  op_drop_i = 0x11,  op_tol_i = 0x12,   op_tof_i = 0x13,
    op_tod_i = 0x14,  op_undef_0 = 0x15, op_undef_1 = 0x16, op_inc_i = 0x17,
    op_rem_i = 0x18,  op_and_i = 0x19,   op_shr_i = 0x1A,   op_xor_i = 0x1B,
    op_jle_i = 0x1C,  op_je_i = 0x1D,    op_jz_i = 0x1E,    op_ret_i = 0x1F,

    op_jmp_ = 0x20,  op_lda_l = 0x21,   op_sta_l = 0x22,   op_ld_l = 0x23,
    op_st_l = 0x24,   op_ldc_l = 0x25,   op_ldi_l = 0x26,   op_add_l = 0x27,
    op_sub_l = 0x28,  op_mul_l = 0x29,   op_div_l = 0x2A,   op_neg_l = 0x2B,
    op_jl_l = 0x2C,   op_jg_l = 0x2D,    op_jlz_l = 0x2E,   op_call_l = 0x2F,

    op_dup_l = 0x30,  op_drop_l = 0x31,  op_toi_l = 0x32,   op_tof_l = 0x33,
    op_tod_l = 0x34,  op_undef_2 = 0x35, op_undef_3 = 0x36, op_inc_l = 0x37,
    op_rem_l = 0x38,  op_and_l = 0x39,   op_shr_l = 0x3A,   op_xor_l = 0x3B,
    op_jle_l = 0x3C,  op_je_l = 0x3D,    op_jz_l = 0x3E,    op_ret_l = 0x3F,

    op_call_f = 0x40, op_lda_f = 0x41,   op_sta_f = 0x42,   op_ld_f = 0x43,
    op_st_f = 0x44,   op_ldc_f = 0x45,   op_ret_  = 0x46,   op_add_f = 0x47,
    op_sub_f = 0x48,  op_mul_f = 0x49,   op_div_f = 0x4A,   op_neg_f = 0x4B,
    op_jl_f = 0x4C,   op_jg_f = 0x4D,    op_jgz_i = 0x4E,   op_call_d = 0x4F,

    op_dup2_i = 0x50, op_drop2_i = 0x51, op_tol_f = 0x52,   op_toi_f = 0x53,
    op_tod_f = 0x54,  op_undef_4 = 0x55, op_undef_5 = 0x56, op_dec_i = 0x57,
    op_not_i = 0x58,  op_or_i = 0x59,    op_shl_i = 0x5A,   op_sar_i = 0x5B,
    op_jge_i = 0x5C,  op_jne_i = 0x5D,   op_jnz_i = 0x5E,   op_ret_f = 0x5F,

    op_tcall_ = 0x60, op_lda_d = 0x61,   op_sta_d = 0x62,   op_ld_d = 0x63,
    op_st_d = 0x64,   op_ldc_d = 0x65,   op_pwide = 0x66,   op_add_d = 0x67,
    op_sub_d = 0x68,  op_mul_d = 0x69,   op_div_d = 0x6A,   op_neg_d = 0x6B,
    op_jl_d = 0x6C,   op_jg_d = 0x6D,    op_jgz_l = 0x6E,   op_call_ = 0x6F,

    op_dup2_l = 0x70, op_drop2_l = 0x71, op_tol_d = 0x72,   op_tof_d = 0x73,
    op_toi_d = 0x74,  op_undef_6 = 0x75, op_undef_7 = 0x76, op_dec_l = 0x77,
    op_not_l = 0x78,  op_or_l = 0x79,    op_shl_l = 0x7A,   op_sar_l = 0x7B,
    op_jge_l = 0x7C,  op_jne_l = 0x7D,   op_jnz_l = 0x7E,   op_ret_d = 0x7F,
};

/** Virtual machine state */
class vm_state {
    std::vector<uint8_t> stack;

    // TODO For now globals array serves the purpose of memory
    /* std::vector<uint8_t> memory; */

    object_file object; /* Object code we evaluating */

    uint8_t *sp{}; /* stack pointer register */
    uint8_t *fp{}; /* frame pointer register */

    function *ip_fun{};
    const uint8_t *ip{}; /* instruction pointer register */
    bool wide_flag; /* wide prefix is active */

public:
    /**
     * Evaluate a function call.
     * Arguments correctness is not checked.
     */
    void eval(const std::string &fun);

    /* Copying is prohibited */
    vm_state(std::size_t stack_size, std::string path) : stack(stack_size * sizeof(uint32_t)) {
        std::ifstream fstr(path);
        object.read(fstr);
        sp = &*stack.end();
    }
    /* Copying is prohibited */
    vm_state(const vm_state &) = delete;
    vm_state &operator=(const vm_state &)= delete;

    /**
     * Swap two VM states
     *
     * @param[inout] other state to swap with
     */
    void swap(vm_state &other) noexcept {
        std::swap(stack, other.stack);
        // std::swap(memory, other.memory);
        std::swap(sp, other.sp);
        std::swap(fp, other.fp);
        std::swap(ip, other.ip);
        std::swap(ip_fun, other.ip_fun);
        std::swap(wide_flag, other.wide_flag);
        object.swap(other.object);
    }

    /**
     * Move-constuct VM state
     *
     * @param[inout] vm state to move from
     */
    vm_state(vm_state && vm) noexcept {
        swap(vm);
    }

    /**
     * Move-assign VM state
     *
     * @param[inout] vm state to move from
     *
     * @return this object
     */
    vm_state &operator=(vm_state && vm) noexcept {
        swap(vm);
        return *this;
    }

    /**
     * Pop value from the stack
     *
     * @param[in] value pushed value
     */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value, T> pop() {
        T tmp = util::read_next<T>(ip);
        return tmp;
    }

    /**
     * Return the top of the stack
     * (it is not a reference to prevent strict alising violation)
     *
     * @param[in] value pushed value
     */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value, T> top() {
        return util::read_at<T>(ip);
    }

    /**
     * Push a value to the stack
     *
     * @param[in] value pushed value
     */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value> push(T value) {
        util::write_prev(sp, value);
    }

    /**
     * Get local variable
     *
     * @param[in] n local variable index
     *
     * @return value
     */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value, T> get_local(int32_t n) {
        return util::read_at<T>(&*fp + n*sizeof(int32_t) + (n >= 0 ? 3*sizeof(void*) : -1*sizeof(uint32_t)));
    }

    /**
     * Set local variable
     *
     * @param[in] n local variable index
     * @param[in] value new value
     */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value> set_local(std::int32_t n, T value) {
        util::write_at<T>(&*fp + n*sizeof(int32_t) + (n >= 0 ? 3*sizeof(void*) : -1*sizeof(uint32_t)), value);
    }

    /**
     * Get global variable
     *
     * @param[in] n global variable index
     *
     * @return value
     */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value, T> get_global(uint32_t n) {
        // TODO Use memory instead of globals description init_value as a variable storage
        // return util::read_at<T>(memory.data() + n*sizeof(uint32_t));

        return util::read_at<T>(reinterpret_cast<uint8_t*>(&object.globals[n].init_value));
    }

    /**
     * Set global variable
     *
     * @param[in] n global variable index
     * @param[in] value new value
     */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value> set_global(uint32_t n, T value) {
        // TODO Use memory instead of globals description init_value as a variable storage
        // util::write_at<T>(memory.data() + n*sizeof(uint32_t), value);

        return util::write_at<T>(reinterpret_cast<uint8_t*>(&object.globals[n].init_value), value);
    }

    /**
     * Set wide flag
     */
    void set_wide() {
        wide_flag = true;
    }

    /**
     * Read and reset wide flag
     *
     * @return wide flag
     */
    bool get_wide() {
        bool w = wide_flag;
        wide_flag = false;
        return w;
    }

    /**
     * Get current function
     *
     * @return function reference
     */
    function &cur_function() {
        return *ip_fun;
    }

    /**
     * Call a function
     *
     * @param[in] idx functions array index
     */
    void invoke(uint32_t idx) {
        push(ip_fun);
        push(ip);
        push(*&fp);

        fp = sp;
        ip_fun = &object.functions[idx];
        ip = object.functions[idx].code.data();

        /* Allocate space on stack for locals */
        auto fr_size = object.functions[idx].frame_size;
        sp -= fr_size;

        /* Frame should be initially zeroed*/
        std::memset(sp, 0, fr_size);
    }

    /**
     * Return from a function
     */
    void ret() {
        /* Get arguments size for current function */
        size_t args = ip_fun->args_size;

        sp = fp;

        fp = pop<uint8_t *>();
        ip = pop<const uint8_t *>();
        ip_fun = pop<function*>();

        /* Arguments are popped from stack
         * by caller like in stdcall
         * (and no support for vargargs) */
        ip += args;
    }

    /**
     * Perform a jump
     *
     * @param[in] arg signed displacement
     */
    void jump(int32_t arg) {
        ip += arg;
    }

    /**
     * Stop VM
     */
    void halt() {
        ip = nullptr;
    }

};

