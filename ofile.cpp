#include "ofile.hpp"
#include "insn.hpp"

#include <limits>
#include <iostream>
#include <memory>

uint32_t object_file::id(std::string &&name) {
    auto idx = strtab.find(name);
    if (idx != strtab.end()) {
        return idx->second;
    } else {
        size_t pos = strtab_offset;
        strtab_offset += name.length() + 1;
        strtab.emplace(std::move(name), pos);
        return pos;
    }
}

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

void object_file::swap(object_file &other) {
    std::swap(strtab, other.strtab);
    std::swap(strtab_offset, other.strtab_offset);
    std::swap(global_indices, other.global_indices);
    std::swap(globals, other.globals);
    std::swap(function_indices, other.function_indices);
    std::swap(functions, other.functions);
}

std::vector<uint8_t> object_file::make_strtab() const {
    std::vector<uint8_t> res;
    std::map<uint32_t, std::string> revmap;
    for (auto &i : strtab) revmap.emplace(i.second, i.first);
    for (auto &str : revmap) std::copy(str.second.data(), str.second.data() + str.second.size() + 1, std::back_inserter(res));
    return res;
}

void object_file::write(std::ostream &st) {

    /* 0. Intern signatures beforeall */

    for (auto &f : functions) {
        id(std::string(f.signature));
        id(std::string(f.locals));
    }

    /* 1. File header */

    vm_header hd = {
        /* magic */ {'X','S','V','M'},
        /* flags */ 0,
        /* functab_size */ static_cast<uint32_t>(sizeof(vm_function)*functions.size()),
        /* functab_offset */ sizeof(vm_header),
        /* globtab_size */ static_cast<uint32_t>(sizeof(vm_global)*globals.size()),
        /* globtab_offset */ static_cast<uint32_t>(sizeof(vm_header) + sizeof(vm_function)*functions.size()),
        /* strtab_size */ static_cast<uint32_t>(strtab_offset),
        /* strtab_offset */ static_cast<uint32_t>(sizeof(vm_header) + sizeof(vm_function)*functions.size() + sizeof(vm_global)*globals.size()),
    };
    size_t file_offset = hd.strtab_offset + hd.strtab_size;

    st.write(reinterpret_cast<char *>(&hd), sizeof(hd));

    /* 2. Function table */
    for (auto &f : functions) {
        vm_function vmf = {
            f.name,
            id(std::string(f.signature)),
            id(std::string(f.locals)),
            static_cast<uint32_t>(file_offset),
            static_cast<uint32_t>(f.code.size())
        };

        st.write(reinterpret_cast<char *>(&vmf), sizeof(vmf));
        file_offset += f.code.size();
    }

    /* 3. Globals table */
    st.write(reinterpret_cast<char *>(globals.data()), globals.size()*sizeof(vm_global));

    /* 4. String table */
    auto stt = make_strtab();
    st.write(reinterpret_cast<char *>(stt.data()), stt.size());
    stt.clear();

    /* 5. Functions code */
    for (auto &f : functions) st.write(reinterpret_cast<char *>(f.code.data()), f.code.size());

    st.flush();
}


struct stack_state {
    std::shared_ptr<stack_state> next;
    char type;
    uint32_t depth;

    /* stack_state comparison is a list traversal */
    bool operator==(stack_state &other) {
        auto oth = &other, thi = this;
        while (oth && thi && oth != thi) {
            if (oth->type != thi->type) return false;

            oth = oth->next.get();
            thi = thi->next.get();
        }

        return oth == thi;
    }

    stack_state(std::shared_ptr<stack_state> next_, char type_) :
        next(std::move(next_)), type(type_), depth(next ? next->depth + 1 : 0) {}
    stack_state() {}
};

std::ostream &operator<<(std::ostream &str, const stack_state &state) {
    auto *stt_ptr = &state;
    str << "stack[";
    while (stt_ptr) {
        str << stt_ptr->type;
        stt_ptr = stt_ptr->next.get();
    }
    str << "]";
    return str;
}

constexpr static uint8_t cmd_type_mask = 0x60;

struct check_env {
    object_file *obj;
    function *fun;
    std::vector<std::shared_ptr<stack_state>> anno;
    std::vector<uint8_t>::iterator end;
};

/* Iterate through code,
 * recusively handling control flow splits
 * on every conditional branch.
 *
 * If insn is annotated with nullptr
 * it is not seen yet and a new state can be assigned to it
 *
 * If insn is annotated the stack state
 * should be the same as already seen
 * (handled with overloaded operator= of stack state)
 *
 * Jump should be in bounds of functions
 *
 * Overlapping instructions are allowed for now
 */
bool trace_types(check_env &env, std::shared_ptr<stack_state> state, std::vector<uint8_t>::iterator op) {

    bool wide{};
    while (op < env.end) {
        wide |= *op == op_pwide;
        do {
            auto &ref = env.anno[op - env.fun->code.begin()];
            if (ref) {
                auto res = *ref == *state;
                if (!res) {
                    std::cerr << "Wrong type interface after jump" << std::endl;
                    std::cerr << "\t" << *ref << std::endl;
                    std::cerr << "\t" << *state << std::endl;
                }
                return res;
            }
            ref = state;
        } while (*op == op_pwide && ++op < env.end);

        uint8_t cmd = *op++;

        auto check = [&](const char *sig) -> bool {
            /* Parse signature and compare with stack state */
            auto arg_s = std::strchr(sig, '(');
            auto arg_e = std::strchr(sig, ')');
            if (!arg_e || arg_s >= arg_e) throw std::logic_error("Oops");

            /* Pop new types */
            auto it = arg_e - 1;
            char poly[10] {};
            for (; it > arg_s && state; it--, state = state->next) {
                if (std::isdigit(*it)) {
                    /* Polymorphic parameters */
                    if (type_size(state->type) != (1 + (*it >= '5'))*sizeof(int32_t)) return false;
                    poly[*it - '0'] = state->type;
                } else if (*it != state->type) return false;
                /* Disallow poping locals */
                if (state->depth < env.fun->locals.size()) {
                    std::cerr << "Stack undeflow in code" << std::endl;
                    return false;
                }
            }

            /* Check stack underflow */
            if (!state && it > arg_s) {
                std::cerr << "Stack undeflow in code" << std::endl;
                return false;
            }

            /* Push new types */
            for (auto it = arg_e + 1; *it; it++) {
                char tp = std::isdigit(*it) ? poly[*it - '0'] : *it;
                state = std::make_shared<stack_state>(state, tp);
            }

            return true;
        };

        auto arg_im_check = [&]() -> bool {
            if (env.end - op < 1 + wide) {
                std::cerr << "Unterminated instruction" << std::endl;
                return false;
            }
            return true;
        };

        switch(cmd < insns.size() ? insns[cmd].iclass : ins_undef) {
        case ins_jump: {
            if (!check(insns[cmd].sig) || !arg_im_check()) return false;
            auto disp = util::read_im<int16_t>(op, wide);
            if ((disp < 0 && op + disp < env.fun->code.begin()) || (disp > 0 && op + disp >= env.end)) {
                std::cerr << "Jump is out of bounds" << std::endl;
                return false;
            }
            if (!trace_types(env, state, op + disp)) return false;
        } break;
        case ins_plain:
            if (!check(insns[cmd].sig)) return false;
            break;
        case ins_local: {
            if (!check(insns[cmd].sig) || !arg_im_check()) return false;
            int16_t disp = util::read_im<int16_t>(op, wide);
            if (disp >= 0) /* argument */ {
                auto arg_s = env.fun->signature.find('(');
                auto arg_e = env.fun->signature.find(')');
                if (arg_s == std::string::npos || arg_e == std::string::npos)
                    throw std::logic_error("Oops 1");

                /* We should use linear search and not just
                 * indexing because local have different sizes */

                std::size_t offset{}, i{arg_s + 1};
                while (offset < std::size_t(disp) && i < arg_e)
                    offset += type_size(env.fun->signature[i++]) / sizeof(int32_t);

                if (i > arg_e || offset != std::size_t(disp) ||
                    env.fun->signature[i] != insns[cmd].type) {

                    std::cerr << "Parameter type interface violation of " << std::hex << (uint32_t)cmd << std::endl;
                    return false;
                }
            } else /* local */ {
                std::size_t offset{}, i{};
                while (offset < std::size_t(-1 - disp) && i < env.fun->locals.size())
                    offset += type_size(env.fun->locals[i]) / sizeof(int32_t);

                if (i >= env.fun->locals.size() ||
                    offset != std::size_t(-1 - disp) ||
                    env.fun->locals[i] != insns[cmd].type) {

                    std::cerr << "Local variable type interface violation of " << std::hex << (uint32_t)cmd << std::endl;
                    return false;
                }
            }
        } break;
        case ins_global: {
            if (!check(insns[cmd].sig) || !arg_im_check()) return false;
            uint16_t disp = util::read_im<uint16_t, uint8_t>(op, wide);
            auto res = disp < env.obj->globals.size() &&
                       env.obj->globals[disp].type == insns[cmd].type;
            if (!res) {
                std::cerr << "Global variable type interface violation of " << std::hex << (uint32_t)cmd << std::endl;
                return false;
            }
        } break;
        case ins_const: {
            if (!check(insns[cmd].sig)) return false;
            switch (cmd & ~cmd_type_mask) {
            case op_ldi_i:
                util::read_im<int16_t>(op, wide);
                break;
            case op_ldc_i:
                op += type_size(insns[cmd].type);
                break;
            default:
                throw std::logic_error("Oops 5");
            }
        } break;
        case ins_call: {
            if (cmd == op_tcall_)
                throw std::logic_error("Unimplemented");

            if (!arg_im_check()) return false;
            uint16_t disp = util::read_im<uint16_t, uint8_t>(op, wide);

            auto &sig = env.obj->functions[disp].signature;
            auto res = disp < env.obj->functions.size() &&
                       sig[sig.find(')') + 1] == insns[cmd].type && check(sig.data());
            if (!res) {
                std::cerr << "Function call type interface violation of "
                          << std::hex << (uint32_t)cmd << std::endl;
            }
            return res;

        } break;
        case ins_return: {
            if (!check(insns[cmd].sig)) return false;
            char tp = env.fun->signature[env.fun->signature.size() - 1];
            return (tp == ')' ? 0 : tp) == insns[cmd].type;
        }
        case ins_undef:
            if (cmd == op_hlt_) return true;
            [[fallthrough]];
        default:
            std::cerr << "Unknown opcode " << std::hex << (uint32_t)cmd << std::endl;
            return false;
        }
    }
    if (env.fun->code.size()) {
        std::cout << "Code is unterminated" << std::endl;
        return false;
    } else {
        /* Native function, checked in runtime */
        return true;
    }
}


static const char *validate_functions(object_file &obj) {
    for (auto &fn : obj.functions) {
        fn.args_size = 0;
        fn.frame_size = 0;

        /* Initial stack state */
        std::shared_ptr<stack_state> stk;

        for (char c : fn.locals) {
            fn.frame_size += type_size(c);
            /* Build initial stack state */
            stk = std::make_shared<stack_state>(stk, c);
        }

        if (!fn.signature.size() || fn.signature[0] != '(')
            return "Malformed signature";

        auto clo = fn.signature.find(')', 1);
        if (clo == fn.signature.npos || fn.signature.size() - clo > 2)
            return "Malformed signature";

        char return_sig = fn.signature[clo + 1];
        if (!std::strchr("ilfd", return_sig) && return_sig)
            return "Unknown return type in signature";

        std::string arg_sig(fn.signature.substr(1, clo - 1));
        for (char c : arg_sig) fn.args_size += type_size(c);

        /* Code type annotation */
        check_env env = {&obj, &fn, {}, fn.code.end()};
        env.anno.resize(fn.code.size());
        if (!trace_types(env, stk, fn.code.begin()))
            return "Code is invalid";
    }

    return nullptr;
}

void object_file::read(std::istream &str) {
    /* 1. Header */

    struct vm_header hd;
    str.seekg(0, std::ios_base::end);
    auto pos = str.tellg();
    str.seekg(0, std::ios_base::beg);
    str.read(reinterpret_cast<char *>(&hd), sizeof hd);
    if (hd.signature[0] != 'X' || hd.signature[1] != 'S' ||
            hd.signature[2] != 'V' || hd.signature[3] != 'M')
        throw std::invalid_argument("Wrong magic");
    if (hd.flags)
        throw std::invalid_argument("Flags are not supported");
    if (hd.funcs_offset + hd.funcs_size >= pos)
        throw std::invalid_argument("Function table extends beyond file size");
    if (hd.funcs_size % sizeof(vm_function))
        throw  std::invalid_argument("Function table size in not a multiple of entry size");
    if (hd.strtab_offset + hd.strtab_size >= pos)
        throw std::invalid_argument("String table extends beyond file size");
    if (hd.globals_offset + hd.globals_size >= pos)
        throw std::invalid_argument("Globals table extends beyond file size");
    if (hd.globals_size % sizeof(vm_global))
        throw  std::invalid_argument("Globals table size in not a multiple of entry size");

    /* 2. String table */

    std::vector<char> vstrtab(hd.strtab_size);
    str.seekg(hd.strtab_offset);
    str.read(vstrtab.data(), hd.strtab_size);

    if (vstrtab[0] || vstrtab.back())
        throw std::invalid_argument("String table is not bounded");

    /* 3. Globals table */

    globals.resize(hd.globals_size/sizeof(vm_global));
    str.seekg(hd.globals_offset);
    str.read(reinterpret_cast<char *>(globals.data()), hd.globals_size);

    /* Validate each global */
    size_t i{};
    for (auto &gl : globals) {
        if (gl.name >= vstrtab.size())
            throw std::invalid_argument("String table index is out of bounds");

        /* Intern global name into a string table */
        gl.name = id(std::string(&vstrtab[gl.name]));

        if (!std::strchr("ilfd", gl.type))
            throw std::invalid_argument("Unknown global type");
        if (gl.flags & ~gf_init)
            throw std::invalid_argument("Unsupported global flags");
        if (!(gl.flags & gf_init) && gl.init_value)
            throw std::invalid_argument("Non-zero reserved init value");
        if (gl.dummy0)
            throw std::invalid_argument("Non-zero reserved value");

        global_indices.emplace((uint32_t)gl.name, i++);
    }

    /* 4. Functions table */

    std::vector<vm_function> vfunctions(hd.funcs_size/sizeof(vm_function));
    str.seekg(hd.funcs_offset);
    str.read(reinterpret_cast<char *>(vfunctions.data()), hd.funcs_size);


    for (auto &fn : vfunctions) {
        if (fn.name >= vstrtab.size())
            throw std::invalid_argument("String table index is out of bounds");
        if (fn.signature >= vstrtab.size())
            throw std::invalid_argument("String table index is out of bounds");
        if (fn.locals >= vstrtab.size())
            throw std::invalid_argument("String table index is out of bounds");

        if (fn.code_size + fn.code_offset > pos)
            throw std::invalid_argument("Function body is out of bounds");
        if (fn.code_size > std::numeric_limits<uint32_t>::max())
            throw std::invalid_argument("Code is too big");

        std::vector<uint8_t> code(fn.code_size);
        str.seekg(fn.code_offset);
        str.read(reinterpret_cast<char *>(code.data()), fn.code_size);

        fn.name = id(std::string(&vstrtab[fn.name]));

        function_indices.emplace(static_cast<strtab_index>(fn.name), functions.size());
        functions.emplace_back(
            (uint32_t)fn.name,
            std::string(&vstrtab[fn.signature]),
            std::string(&vstrtab[fn.locals]),
            0, /* frame_size -- calculated later */
            0, /* args_size -- calculated later */
            std::move(code)
        );
    }

    vfunctions.clear();
    vstrtab.clear();

    /* 5. Validate code */

    const char *erc = validate_functions(*this);
    if (erc) throw std::invalid_argument(erc);

}

