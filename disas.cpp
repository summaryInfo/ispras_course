#include "ofile.hpp"
#include "util.hpp"

#include <iostream>
#include <fstream>

inline static const char *typid_to_type(char id) {
    switch(id) {
    case 'i': return "int";
    case 'l': return "long";
    case 'f': return "float";
    case 'd': return "double";
    default:
        throw std::logic_error("Oops 1");
    }
}

enum insn_class {
    ins_plain,
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
};

static std::vector<opdesc> insns {
    {"hlt", ins_plain, 0},   {"ld.i", ins_local, 'i'},
    {"st.i", ins_local, 'i'},    {"ld.i", ins_global, 'i'},
    {"st.i", ins_global, 'i'},  {"ld.i", ins_const, 'i'},
    {"ld.i", ins_const, 'i'},   {"add.i", ins_plain, 'i'},
    {"sub.i", ins_plain, 'i'},  {"mul.i", ins_plain, 'i'},
    {"div.i", ins_plain, 'i'},   {"neg.i", ins_plain, 'i'},
    {"jl.i", ins_jump, 'i'},   {"jg.i", ins_jump, 'i'},
    {"jlz.i", ins_jump, 'i'},   {"call.i", ins_call, 'i'},
    {"dup.i", ins_plain, 'i'},  {"drop.i", ins_plain, 'i'},
    {"tol.i", ins_plain, 'i'},   {"tof.i", ins_plain, 'i'},
    {"tod.i", ins_plain, 'i'},  {"undef.0", ins_plain, 0},
    {"undef.1", ins_plain, 0}, {"inc.i", ins_plain, 'i'},
    {"rem.i", ins_plain, 'i'},  {"and.i", ins_plain, 'i'},
    {"shr.i", ins_plain, 'i'},   {"xor.i", ins_plain, 'i'},
    {"jle.i", ins_jump, 'i'},  {"je.i", ins_jump, 'i'},
    {"jz.i", ins_jump, 'i'},    {"ret.i", ins_plain, 'i'},
    {"jmp", ins_jump, 0},  {"ld.l", ins_local, 'l'},
    {"st.l", ins_local, 'l'},     {"ld.l", ins_global, 'l'},
    {"st.l", ins_global, 'l'},   {"ld.l", ins_const, 'l'},
    {"ld.l", ins_const, 'l'},   {"add.l", ins_plain, 'l'},
    {"sub.l", ins_plain, 'l'},  {"mul.l", ins_plain, 'l'},
    {"div.l", ins_plain, 'l'},   {"neg.l", ins_plain, 'l'},
    {"jl.l", ins_jump, 'l'},   {"jg.l", ins_jump, 'l'},
    {"jlz.l", ins_jump, 'l'},   {"call.l", ins_call, 'l'},
    {"dup.l", ins_plain, 'l'},  {"drop.l", ins_plain, 'l'},
    {"toi.l", ins_plain, 'l'},   {"tof.l", ins_plain, 'l'},
    {"tod.l", ins_plain, 'l'},  {"undef.2", ins_plain, 0},
    {"undef.3", ins_plain, 0}, {"inc.l", ins_plain, 'l'},
    {"rem.l", ins_plain, 'l'},  {"and.l", ins_plain, 'l'},
    {"shr.l", ins_plain, 'l'},   {"xor.l", ins_plain, 'l'},
    {"jle.l", ins_jump, 'l'},  {"je.l", ins_jump, 'l'},
    {"jz.l", ins_jump, 'l'},    {"ret.l", ins_plain, 'l'},
    {"call.f", ins_call, 'f'}, {"ld.f", ins_local, 'f'},
    {"st.f", ins_local, 'f'},   {"ld.f", ins_global, 'f'},
    {"st.f", ins_global, 'f'},   {"ld.f", ins_const, 'f'},
    {"ret", ins_plain, 0},     {"add.f", ins_plain, 'f'},
    {"sub.f", ins_plain, 'f'},  {"mul.f", ins_plain, 'f'},
    {"div.f", ins_plain, 'f'},   {"neg.f", ins_plain, 'f'},
    {"jl.f", ins_jump, 'f'},   {"jg.f", ins_jump, 'f'},
    {"jgz.i", ins_jump, 'i'},   {"call.d", ins_call, 'd'},
    {"dup2.i", ins_plain, 'i'}, {"drop2.i", ins_plain, 'i'},
    {"tol.f", ins_plain, 'f'},   {"toi.f", ins_plain, 'f'},
    {"tod.f", ins_plain, 'f'},  {"undef.4", ins_plain, 0},
    {"undef.5", ins_plain, 0}, {"dec.i", ins_plain, 'i'},
    {"not.i", ins_plain, 'i'},  {"or.i", ins_plain, 'i'},
    {"shl.i", ins_plain, 'i'},   {"sar.i", ins_plain, 'i'},
    {"jge.i", ins_jump, 'i'},  {"jne.i", ins_jump, 'i'},
    {"jnz.i", ins_jump, 'i'},   {"ret.f", ins_plain, 'f'},
    {"tcall", ins_call, 0}, {"ld.d", ins_local, 'd'},
    {"st.d", ins_local, 'd'},    {"ld.d", ins_global, 'd'},
    {"st.d", ins_global, 'd'},   {"ld.d", ins_const, 'd'},
    {"pwide", ins_wide, 0},   {"add.d", ins_plain, 'd'},
    {"sub.d", ins_plain, 'd'},  {"mul.d", ins_plain, 'd'},
    {"div.d", ins_plain, 'd'},   {"neg.d", ins_plain, 'd'},
    {"jl.d", ins_jump, 'd'},   {"jg.d", ins_jump, 'd'},
    {"jgz.l", ins_jump, 'l'},   {"call", ins_call, 0},
    {"dup2.l", ins_plain, 'l'}, {"drop2.l", ins_plain, 'l'},
    {"tol.d", ins_plain, 'd'},   {"tof.d", ins_plain, 'd'},
    {"toi.d", ins_plain, 'd'},  {"undef.6", ins_plain, 0},
    {"undef.7", ins_plain, 0}, {"dec.l", ins_plain, 'l'},
    {"not.l", ins_plain, 'l'},  {"or.l", ins_plain, 'l'},
    {"shl.l", ins_plain, 'l'},   {"sar.l", ins_plain, 'l'},
    {"jge.l", ins_jump, 'l'},  {"jne.l", ins_jump, 'l'},
    {"jnz.l", ins_jump, 'l'},   {"ret.d", ins_plain, 'd'},

};

void disas_code(const object_file &obj, std::ostream &ostr, const function &fn, std::vector<uint8_t> &stab) {

    /* First, generate labels */

    std::map<uint32_t, uint32_t> labels;
    bool wide{};
    std::size_t labn{};

    for (auto op = fn.code.begin(); op < fn.code.end(); ) {
        switch(insns[*op++].iclass) {
        case ins_plain:
            break;
        case ins_jump: {
            int16_t disp = util::read_either<int16_t>(op, wide);
            wide = false;
            labels.emplace(op - fn.code.begin() + disp, labn++);
        } break;
        case ins_call:
        case ins_local:
        case ins_global:
            op += 1 + wide;
            wide = false;
            break;
        case ins_const:
            switch(op[-1]) {
            case op_ldi_i:
            case op_ldi_l:
                op += 1 + wide;
                wide = false;
                break;
            case op_ldc_i:
                op += sizeof(int32_t);
                break;
            case op_ldc_l:
                op += sizeof(int64_t);
                break;
            case op_ldc_f:
                op += sizeof(float);
                break;
            case op_ldc_d:
                op += sizeof(double);
                break;
            default:
                throw std::logic_error("Oops 2");
            }
            break;
        case ins_wide:
            wide = true;
        }
    }

    /* Then emit code itself */

    for (auto op = fn.code.begin(); op < fn.code.end(); ) {
        auto &insn = insns[*op++];

        if (insn.iclass == ins_wide) {
            wide = true;
            continue;
        }

        const auto &lab = labels.find(op - fn.code.begin());
        if (lab != labels.end()) {
            /* Emit label if some instruction references current address
             * NOTE: Jumps to the middle of instruction emits invalid labels
             *       and cannot be generated by xsas */
            ostr << "L" << lab->second << ":" << std::endl;
        }
        /* Instruction mnemonic */
        ostr << "\t" << insn.name;

        switch(insn.iclass) {
        case ins_plain:
            /* Don't need to do anything for plain insns */
            break;
        case ins_jump: {
            int16_t disp = util::read_either<int16_t>(op, wide);
            wide = false;
            ostr << " L" << labels[op - fn.code.begin() + disp];
        } break;
        case ins_call: {
            uint16_t disp = util::read_either<uint16_t, uint8_t>(op, wide);
            wide = false;
            ostr << " " << &stab[obj.functions[disp].name];
        } break;
        case ins_local: {
            int16_t disp = util::read_either<int16_t>(op, wide);
            wide = false;
            if (disp < 0) {
                ostr << " loc" << -(1 + disp);
            } else {
                ostr << " par" << disp;
            }
        } break;
        case ins_global: {
            uint16_t disp = util::read_either<uint16_t, uint8_t>(op, wide);
            wide = false;
            ostr << " " << &stab[obj.globals[disp].name];
        } break;
        case ins_const:
            switch(op[-1]) {
            case op_ldi_i:
            case op_ldi_l: {
                int32_t cons = util::read_either<int16_t>(op, wide);
                wide = false;
                ostr << " $" << cons;
            } break;
            case op_ldc_i: {
                int32_t cons = util::read_next<int32_t>(op);
                ostr << " $" << cons;
            } break;
            case op_ldc_l: {
                auto cons = util::read_next<int64_t>(op);
                ostr << " $" << cons;
            } break;
            case op_ldc_f: {
                auto cons = util::read_next<float>(op);
                ostr << " $" << cons;
            } break;
            case op_ldc_d: {
                auto cons = util::read_next<double>(op);
                ostr << " $" << cons;
            } break;
            }
            [[fallthrough]];
        case ins_wide:
            break;
        }

        ostr << std::endl;
    }
}

void disas_object(const object_file &obj, const char *file, std::ostream &ostr) {
    auto stab = obj.make_strtab();

    /* 1. Globals */

    for (const auto &gl : obj.globals) {
        ostr << ".global " << typid_to_type(gl.type) << " " << &stab[gl.name];
        if (gl.flags & gf_init) {
            const auto ptr = reinterpret_cast<const uint8_t*>(&gl.init_value);
            switch(gl.type) {
            case 'i': ostr << " " << util::read_at<int32_t>(ptr); break;
            case 'l': ostr << " " << util::read_at<int64_t>(ptr); break;
            case 'f': ostr << " " << util::read_at<float>(ptr); break;
            case 'd': ostr << " " << util::read_at<double>(ptr); break;
            default:
                throw std::logic_error("Oops 3");
            }
        }
        ostr << std::endl;
    }

    /* 2. Functions */

    for (const auto &fn : obj.functions) {
        /* Function itself */
        ostr << ".function " << typid_to_type(fn.signature.back()) << " " << &stab[fn.name] << std::endl;
        auto p0 = fn.signature.find('(');
        auto p1 = fn.signature.find(')');
        if (p0 == fn.signature.npos || p1 == fn.signature.npos || p0 >= p1)
            throw std::logic_error("Oops 4");

        /* Parameters */
        for(auto pos = p1; --pos > p0;) {
            ostr << ".param "
                      << typid_to_type(fn.signature[pos])
                      << " par"
                      << p1 - pos - 1
                      << std::endl;
        }
        /* Local variables */
        std::size_t i = 0;
        for (auto c : fn.locals) {
            ostr << ".local "
                      << typid_to_type(c)
                      << " loc"
                      << i++
                      << std::endl;
        }

        /* Code */
        disas_code(obj, ostr, fn, stab);
    }
}

int main(int argc, char *argv[]) {

    if ((argc > 1 && !std::strcmp("-h", argv[1])) || argc < 2) {
        std::cout << "Usage:\n" << std::endl;
        std::cout << "\t" << argv[0] << " <infile> [<outfile>]" << std::endl;
        return 0;
    }

    std::ifstream str(argv[1], std::ios::binary);
    object_file obj;
    obj.read(str);

    if (argc > 2) {
        std::ofstream fstr(argv[2]);
        disas_object(obj, argv[2], fstr);
    } else {
        disas_object(obj, "<stdout>", std::cout);
    }

    return 0;
}
