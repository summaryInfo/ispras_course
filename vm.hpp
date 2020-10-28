#pragma once

#include <cstdint>
#include <iostream>
#include <map>
#include <cstring>
#include <vector>

using strtab_index = uint32_t;

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

struct vm_global {
    strtab_index name{};
    char type{}; /* i, l, f or d */
    uint8_t flags{}; /* enum global_flags */
    uint16_t dummy0{};
    uint64_t init_value{}; /* stored binary representation */
    vm_global(strtab_index name_,char type_, uint8_t flags_) :
        name(name_), type(type_), flags(flags_) {}
} __attribute__((packed));

enum global_flags {
    gf_init = 1 << 0,
};

/* This is on-disk representation
 * of a function */
struct vm_function {
    strtab_index name{};
    strtab_index signature{};
    uint32_t code_offset{};
    /* if code_size is 0 this is extern function */
    uint32_t code_size{};
} __attribute__((packed));

/* This is an internal representation of a function
 * used by vm and assembler */
struct function {
    strtab_index name{};
    strtab_index signature{};
    strtab_index locals{};
    uint32_t frame_size{};
    uint32_t args_size{};
    std::vector<uint8_t> code;
};

struct object_file {
    /* string table index */
    std::map<std::string, uint32_t> strtab;
    uint32_t strtab_offset{};

    /* global indexes and names  */
    std::map<uint32_t, uint32_t> global_indices;
    std::vector<vm_global> globals;

    /* all functions */
    std::map<uint32_t, uint32_t> function_indices;
    std::vector<function> functions;

    object_file() { id(""); }

    void validate();
    void write(std::ostream &str);
    object_file read(std::istream &str);
    void swap(object_file &other);
    strtab_index id(std::string &&str);

    template <typename T>
    bool add_global(std::string &&str, T value, bool init, char type) {
        strtab_index idx = id(std::move(str));

        auto res = global_indices.find(idx);
        size_t gi = globals.size();
        if (res == global_indices.end()) {
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

