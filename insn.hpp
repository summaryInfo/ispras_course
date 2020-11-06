#pragma once

#include <stdexcept>
#include <vector>

inline static size_t type_size(char typ) {
    switch (typ) {
    case 'i': return sizeof(int32_t);
    case 'l': return sizeof(int64_t);
    case 'f': return sizeof(float);
    case 'd': return sizeof(double);
    default:
        throw std::logic_error("Unknown type in signature");
    }
}

constexpr static uint8_t cmd_type_mask = 0x60;

inline static const char *typid_to_type(char id) {
    switch(id) {
    case 'i': return "int";
    case 'l': return "long";
    case 'f': return "float";
    case 'd': return "double";
    case ')':
    case 0: return "void";
    default:
        throw std::logic_error("Oops 1");
    }
}

enum insn_class {
    ins_plain,
    ins_undef,
    ins_return,
    ins_jump,
    ins_call,
    ins_local,
    ins_global,
    ins_const,
    ins_wide
};

struct opdesc {
    const char *name;
    enum insn_class iclass;
    char type;
    const char *sig;
};

static const std::vector<opdesc> insns {
    {"hlt", ins_undef, 0, "()"},
    {"ld.i", ins_local, 'i', "()i"},
    {"st.i", ins_local, 'i', "(i)"},
    {"ld.i", ins_global, 'i', "()i"},
    {"st.i", ins_global, 'i', "(i)"},
    {"ld.i", ins_const, 'i', "()i"},
    {"ld.i", ins_const, 'i', "()i"},
    {"add.i", ins_plain, 'i', "(ii)i"},
    {"sub.i", ins_plain, 'i', "(ii)i"},
    {"mul.i", ins_plain, 'i', "(ii)i"},
    {"div.i", ins_plain, 'i', "(ii)i"},
    {"neg.i", ins_plain, 'i', "(i)i"},
    {"jl.i", ins_jump, 'i', "(ii)"},
    {"jg.i", ins_jump, 'i', "(ii)"},
    {"jlz.i", ins_jump, 'i', "(i)"},
    {"call.i", ins_call, 'i', nullptr},
    {"dup.i", ins_plain, 'i', "(0)00"},
    {"drop.i", ins_plain, 'i', "(0)"},
    {"tol.i", ins_plain, 'i', "(i)l"},
    {"tof.i", ins_plain, 'i', "(i)f"},
    {"tod.i", ins_plain, 'i', "(i)d"},
    {"swap.i", ins_plain, 'i', "(01)10"},
    {"undef.1", ins_undef, 0, nullptr},
    {"inc.i", ins_plain, 'i', "(i)i"},
    {"rem.i", ins_plain, 'i', "(ii)i"},
    {"and.i", ins_plain, 'i', "(ii)i"},
    {"shr.i", ins_plain, 'i', "(ii)i"},
    {"xor.i", ins_plain, 'i', "(ii)i"},
    {"jle.i", ins_jump, 'i', "(ii)"},
    {"je.i", ins_jump, 'i', "(ii)"},
    {"jz.i", ins_jump, 'i', "(i)"},
    {"ret.i", ins_return, 'i', "(i)"},
    {"jmp", ins_jump, 0, "()"},
    {"ld.l", ins_local, 'l', "()l"},
    {"st.l", ins_local, 'l', "(l)"},
    {"ld.l", ins_global, 'l', "()l"},
    {"st.l", ins_global, 'l', "(l)"},
    {"ld.l", ins_const, 'l', "()l"},
    {"ld.l", ins_const, 'l', "()l"},
    {"add.l", ins_plain, 'l', "(ll)l"},
    {"sub.l", ins_plain, 'l', "(ll)l"},
    {"mul.l", ins_plain, 'l', "(ll)l"},
    {"div.l", ins_plain, 'l', "(ll)l"},
    {"neg.l", ins_plain, 'l', "(l)l"},
    {"jl.l", ins_jump, 'l', "(ll)"},
    {"jg.l", ins_jump, 'l', "(ll)"},
    {"jlz.l", ins_jump, 'l', "(l)"},
    {"call.l", ins_call, 'l', nullptr},
    {"dup.l", ins_plain, 'l', "(5)55"},
    {"drop.l", ins_plain, 'l', "(5)"},
    {"toi.l", ins_plain, 'l', "(l)i"},
    {"tof.l", ins_plain, 'l', "(l)f"},
    {"tod.l", ins_plain, 'l', "(l)d"},
    {"swap.l", ins_plain, 'l', "(56)65"},
    {"undef.3", ins_undef, 0, nullptr},
    {"inc.l", ins_plain, 'l', "(l)l"},
    {"rem.l", ins_plain, 'l', "(ll)l"},
    {"and.l", ins_plain, 'l', "(ll)l"},
    {"shr.l", ins_plain, 'l', "(li)l"},
    {"xor.l", ins_plain, 'l', "(ll)l"},
    {"jle.l", ins_jump, 'l', "(ll)"},
    {"je.l", ins_jump, 'l', "(ll)"},
    {"jz.l", ins_jump, 'l', "(l)"},
    {"ret.l", ins_return, 'l', "(l)"},
    {"call.f", ins_call, 'f', nullptr},
    {"ld.f", ins_local, 'f', "()f"},
    {"st.f", ins_local, 'f', "(f)"},
    {"ld.f", ins_global, 'f', "()f"},
    {"st.f", ins_global, 'f', "(f)"},
    {"ld.f", ins_const, 'f', "()f"},
    {"ret", ins_return, 0, "()"},
    {"add.f", ins_plain, 'f', "(ff)f"},
    {"sub.f", ins_plain, 'f', "(ff)f"},
    {"mul.f", ins_plain, 'f', "(ff)f"},
    {"div.f", ins_plain, 'f', "(ff)f"},
    {"neg.f", ins_plain, 'f', "(ff)f"},
    {"jl.f", ins_jump, 'f', "(ff)"},
    {"jg.f", ins_jump, 'f', "(ff)"},
    {"jgz.i", ins_jump, 'i', "(i)"},
    {"call.d", ins_call, 'd', nullptr},
    {"dup2.i", ins_plain, 'i', "(12)1212"},
    {"drop2.i", ins_plain, 'i', "(12)"},
    {"tol.f", ins_plain, 'f', "(f)l"},
    {"toi.f", ins_plain, 'f', "(f)i"},
    {"tod.f", ins_plain, 'f', "(f)d"},
    {"undef.4", ins_undef, 0, nullptr},
    {"undef.5", ins_undef, 0, nullptr},
    {"dec.i", ins_plain, 'i', "(i)"},
    {"not.i", ins_plain, 'i', "(i)"},
    {"or.i", ins_plain, 'i', "(ii)i"},
    {"shl.i", ins_plain, 'i', "(ii)i"},
    {"sar.i", ins_plain, 'i', "(ii)i"},
    {"jge.i", ins_jump, 'i', "(ii)"},
    {"jne.i", ins_jump, 'i', "(ii)"},
    {"jnz.i", ins_jump, 'i', "(i)"},
    {"ret.f", ins_return, 'f', "(f)"},
    {"tcall", ins_call, 0, nullptr},
    {"ld.d", ins_local, 'd', "()d"},
    {"st.d", ins_local, 'd', "(d)"},
    {"ld.d", ins_global, 'd', "()d"},
    {"st.d", ins_global, 'd', "(d)"},
    {"ld.d", ins_const, 'd', "()d"},
    {"pwide", ins_wide, 0, nullptr},
    {"add.d", ins_plain, 'd', "(dd)d"},
    {"sub.d", ins_plain, 'd', "(dd)d"},
    {"mul.d", ins_plain, 'd', "(dd)d"},
    {"div.d", ins_plain, 'd', "(dd)d"},
    {"neg.d", ins_plain, 'd', "(d)d"},
    {"jl.d", ins_jump, 'd', "(dd)"},
    {"jg.d", ins_jump, 'd', "(dd)"},
    {"jgz.l", ins_jump, 'l', "(l)"},
    {"call", ins_call, 0, nullptr},
    {"dup2.l", ins_plain, 'l', "(56)5656"},
    {"drop2.l", ins_plain, 'l', "(56)"},
    {"tol.d", ins_plain, 'd', "(d)l"},
    {"tof.d", ins_plain, 'd', "(d)f"},
    {"toi.d", ins_plain, 'd', "(d)i"},
    {"undef.6", ins_undef, 0, nullptr},
    {"undef.7", ins_undef, 0, nullptr},
    {"dec.l", ins_plain, 'l', "(l)l"},
    {"not.l", ins_plain, 'l', "(l)l"},
    {"or.l", ins_plain, 'l', "(ll)l"},
    {"shl.l", ins_plain, 'l', "(li)l"},
    {"sar.l", ins_plain, 'l', "(li)l"},
    {"jge.l", ins_jump, 'l', "(ll)"},
    {"jne.l", ins_jump, 'l', "(ll)"},
    {"jnz.l", ins_jump, 'l', "(l)"},
    {"ret.d", ins_return, 'd', "(d)"},
};
