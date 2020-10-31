#include "ofile.hpp"

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
        next(std::move(next_)), type(type_) {}
    stack_state() {}
};

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
                auto res = ref == state;
                if (!res) std::cerr << "Wrong type interface after jump" << std::endl;
                return res;
            }
            ref = state;
        } while (*op == op_pwide && ++op < env.end);

        uint8_t cmd = *op++;

        auto check_jump = [&]() -> bool {
            if (env.end - op < 1 + wide) {
                std::cerr << "Unterminated instruction" << std::endl;
                return false;
            }
            auto disp = util::read_either<int16_t>(op, wide);
            wide = false;
            if (!(op - env.fun->code.begin() > -disp && op + disp < env.end)) {
                std::cerr << "Jump is out of bounds" << std::endl;
                return false;
            }
            auto x = trace_types(env, state, op + disp);
            return x;
        };

        auto check = [&](const char *sig) -> bool {
            /* Parse signature and compare with stack state */
            auto arg_s = std::strchr(sig, '(');
            auto arg_e = std::strchr(sig, ')');
            if (!arg_e || arg_s >= arg_e) throw std::logic_error("Oops");

            /* Pop new types */
            auto it = arg_e - 1;
            for (; it > arg_s && state; it--, state = state->next)
                if (*it != state->type) return false;

            /* Check stack underflow */
            if (!state && it > arg_s) {
                std::cerr << "Stack undeflow in code" << std::endl;
                return false;
            }

            /* Push new types */
            for (auto it = arg_e + 1; *it; it++)
                state = std::make_shared<stack_state>(state, *it);

            return true;
        };

        auto check_local = [&](char type) -> bool {
            if (env.end - op < 1 + wide) {
                std::cerr << "Unterminated instruction" << std::endl;
                return false;
            }
            int16_t disp = util::read_either<int16_t>(op, wide);
            wide = false;
            if (disp >= 0) /* argument */ {
                auto arg_s = env.fun->signature.find('(');
                auto arg_e = env.fun->signature.find(')');
                if (arg_s == std::string::npos || arg_e == std::string::npos)
                    throw std::logic_error("Oops 1");

                /* We should use linear search and not just
                 * indexing because local have different sizes */

                std::size_t offset{}, i{arg_e};
                while (offset < std::size_t(disp) && --i > arg_s) {
                    switch(env.fun->signature[i]) {
                    case 'l': case 'd':
                        offset++;
                        [[fallthrough]];
                    case 'i': case 'f':
                        offset++;
                        break;
                    default:
                        throw std::logic_error("Oops 2");
                    }
                }
                auto res = i > arg_s && offset == std::size_t(disp) && env.fun->signature[i - 1] == type;
                if (!res) std::cerr << "Parameter type interface violation of " << std::hex << (uint32_t)cmd << std::endl;
                return res;
            } else /* local */ {
                std::size_t offset{}, i{};
                while (offset < std::size_t(-1 - disp) && i < env.fun->locals.size()) {
                    switch(env.fun->locals[i++]) {
                    case 'l': case 'd':
                        offset++;
                        [[fallthrough]];
                    case 'i': case 'f':
                        offset++;
                        break;
                    default:
                        throw std::logic_error("Oops 3");
                    }
                }
                auto res = i < env.fun->locals.size() &&
                           offset == std::size_t(-1 - disp) &&
                           env.fun->locals[i] == type;
                if (!res) std::cerr << "Local variable type interface violation of " << std::hex << (uint32_t)cmd << std::endl;
                return res;
            }
        };

        auto check_global = [&](char type) -> bool {
            if (env.end - op < 1 + wide) {
                std::cerr << "Unterminated instruction" << std::endl;
                return false;
            }
            uint16_t disp = util::read_either<uint16_t, uint8_t>(op, wide);
            wide = false;
            auto res = disp < env.obj->globals.size() &&
                       env.obj->globals[disp].type == type;
            if (!res) std::cerr << "Global variable type interface violation of " << std::hex << (uint32_t)cmd << std::endl;
            return res;
        };

        auto check_call = [&](const char *after) -> bool {
            if (env.end - op < 1 + wide) {
                std::cerr << "Unterminated instruction" << std::endl;
                return false;
            }
            uint16_t disp = util::read_either<uint16_t, uint8_t>(op, wide);
            wide = false;

            auto &sig = env.obj->functions[disp].signature;
            auto res = disp < env.obj->functions.size() &&
                       sig[sig.find(')') + 1] == *after && check(sig.data());
            if (!res) {
                std::cerr << "Function call type interface violation of "
                          << std::hex << (uint32_t)cmd << std::endl;
            }
            return res;
        };


        switch(cmd) {
        case op_je_i: case op_jge_i:
        case op_jle_i: case op_jl_i:
        case op_jg_i: case op_jne_i:
            if (!check("(ii)")) return false;
            if (!check_jump()) return false;
            break;
        case op_add_i: case op_div_i:
        case op_and_i: case op_or_i:
        case op_mul_i: case op_sar_i:
        case op_shl_i: case op_shr_i:
        case op_sub_i: case op_xor_i:
        case op_rem_i:
            if (!check("(ii)i")) return false;
            break;
        case op_je_l: case op_jge_l:
        case op_jle_l: case op_jl_l:
        case op_jg_l: case op_jne_l:
            if (!check_jump()) return false;
            if (!check("(ll)")) return false;
            break;
        case op_add_l: case op_div_l:
        case op_and_l: case op_or_l:
        case op_mul_l: case op_sub_l:
        case op_xor_l: case op_rem_l:
            if (!check("(ll)l")) return false;
            break;
        case op_sar_l: case op_shl_l:
        case op_shr_l:
            if (!check("(li)l")) return false;
            break;
        case op_jg_d: case op_jl_d:
            if (!check("(dd)")) return false;
            if (!check_jump()) return false;
            break;
        case op_add_d: case op_div_d:
        case op_mul_d: case op_sub_d:
            if (!check("(li)l")) return false;
            break;
        case op_jg_f: case op_jl_f:
            if (!check("(ff)")) return false;
            if (!check_jump()) return false;
            [[fallthrough]];
        case op_sub_f: case op_add_f:
        case op_div_f: case op_mul_f:
            if (!check("(ff)f")) return false;
            break;
        case op_jz_l: case op_jlz_l:
        case op_jgz_l: case op_jnz_l:
            if (!check_jump()) return false;
            if (!check("(l)")) return false;
            break;
        case op_inc_l: case op_dec_l:
        case op_neg_l: case op_not_l:
            if (!check("(l)l")) return false;
            break;
        case op_jz_i: case op_jlz_i:
        case op_jgz_i: case op_jnz_i:
            if (!check_jump()) return false;
            if (!check("(i)")) return false;
            break;
        case op_inc_i: case op_dec_i:
        case op_neg_i: case op_not_i:
            if (!check("(i)i")) return false;
            break;
        case op_neg_d:
            if (!check("(d)d")) return false;
            break;
        case op_neg_f:
            if (!check("(f)f")) return false;
            break;
        case op_tod_f:
            if (!check("(f)d")) return false;
            break;
        case op_tod_i:
            if (!check("(i)d")) return false;
            break;
        case op_tod_l:
            if (!check("(l)d")) return false;
            break;
        case op_tof_d:
            if (!check("(d)f")) return false;
            break;
        case op_tof_i:
            if (!check("(i)f")) return false;
            break;
        case op_tof_l:
            if (!check("(l)f")) return false;
            break;
        case op_toi_d:
            if (!check("(d)i")) return false;
            break;
        case op_toi_f:
            if (!check("(f)i")) return false;
            break;
        case op_toi_l:
            if (!check("(l)i")) return false;
            break;
        case op_tol_d:
            if (!check("(d)l")) return false;
            break;
        case op_tol_f:
            if (!check("(f)l")) return false;
            break;
        case op_tol_i:
            if (!check("(i)l")) return false;
            break;
        case op_ldc_l:
            if (!check("()l")) return false;
            op += sizeof(int64_t);
            break;
        case op_ldi_l:
            if (!check("()l")) return false;
            op += 1 + wide;
            break;
        case op_ld_l:
            if (!check("()l")) return false;
            if (!check_global('l')) return false;
            break;
        case op_lda_l:
            if (!check("()l")) return false;
            if (!check_local('l')) return false;
            break;
        case op_ldc_i:
            if (!check("()i")) return false;
            op += sizeof(int32_t);
            break;
        case op_ld_i:
            if (!check("()i")) return false;
            if (!check_global('i')) return false;
            break;
        case op_lda_i:
            if (!check("()i")) return false;
            if (!check_local('i')) return false;
            break;
        case op_ldi_i:
            if (!check("()i")) return false;
            op += 1 + wide;
            break;
        case op_ldc_f:
            if (!check("()f")) return false;
            op += sizeof(float);
            break;
        case op_ld_f:
            if (!check("()f")) return false;
            if (!check_global('f')) return false;
            break;
        case op_lda_f:
            if (!check("()f")) return false;
            if (!check_local('f')) return false;
            break;
        case op_ld_d:
            if (!check("()d")) return false;
            if (!check_global('d')) return false;
            break;
        case op_lda_d:
            if (!check("()d")) return false;
            if (!check_local('d')) return false;
            break;
        case op_ldc_d:
            if (!check("()d")) return false;
            op += sizeof(double);
            break;
        case op_dup_i:
            if (!check("(i)ii") &&
                !check("(f)ff")) return false;
            break;
        case op_dup_l:
            if (!check("(l)ll") &&
                !check("(d)dd")) return false;
            break;
        case op_dup2_i:
            if (!check("(ii)iiii") &&
                !check("(if)ifif") &&
                !check("(fi)fifi") &&
                !check("(ff)ffff")) return false;
            break;
        case op_dup2_l:
            if (!check("(ll)llll") &&
                !check("(ld)ldld") &&
                !check("(dl)dldl") &&
                !check("(dd)dddd")) return false;
            break;
        case op_drop2_i:
            if (!check("(ii)") &&
                !check("(if)") &&
                !check("(fi)") &&
                !check("(ff)")) return false;
            break;
        case op_drop2_l:
            if (!check("(ll)") &&
                !check("(ld)") &&
                !check("(dl)") &&
                !check("(dd)")) return false;
            break;
        case op_drop_i:
            if (!check("(i)") &&
                !check("(f)")) return false;
            break;
        case op_drop_l:
            if (!check("(l)") &&
                !check("(d)")) return false;
            break;
        case op_st_d:
            if (!check("(d)")) return false;
            if (!check_global('d')) return false;
            break;
        case op_sta_d:
            if (!check("(d)")) return false;
            if (!check_local('d')) return false;
            break;
        case op_st_i:
            if (!check("(i)")) return false;
            if (!check_global('i')) return false;
            break;
        case op_sta_i:
            if (!check("(i)")) return false;
            if (!check_local('i')) return false;
            break;
        case op_st_l:
            if (!check("(l)")) return false;
            if (!check_global('l')) return false;
            break;
        case op_sta_l:
            if (!check("(l)")) return false;
            if (!check_local('l')) return false;
            break;
        case op_st_f:
            if (!check("(f)")) return false;
            if (!check_global('f')) return false;
            break;
        case op_sta_f:
            if (!check("(f)")) return false;
            if (!check_local('f')) return false;
            break;
        case op_jmp_:
            return check_jump();
            break;
        case op_call_:
            if (!check_call("")) return false;
            break;
        case op_call_d:
            if (!check_call("d")) return false;
            break;
        case op_call_f:
            if (!check_call("f")) return false;
            break;
        case op_call_i:
            if (!check_call("i")) return false;
            break;
        case op_call_l:
            if (!check_call("l")) return false;
            break;
        case op_tcall_:
            throw std::logic_error("Unimplemented");
        case op_hlt_:
            return true;
            break;
        case op_ret_:
            if (!check("()")) return false;
            return env.fun->signature[env.fun->signature.size() - 1] == ')';
        case op_ret_d:
            if (!check("(d)")) return false;
            return env.fun->signature[env.fun->signature.size() - 1] == 'd';
        case op_ret_f:
            if (!check("(f)")) return false;
            return env.fun->signature[env.fun->signature.size() - 1] == 'f';
        case op_ret_i:
            if (!check("(i)")) return false;
            return env.fun->signature[env.fun->signature.size() - 1] == 'i';
        case op_ret_l :
            if (!check("(l)")) return false;
            return env.fun->signature[env.fun->signature.size() - 1] == 'l';
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
            switch (c) {
            case 'i': fn.frame_size += sizeof(int32_t); break;
            case 'l': fn.frame_size += sizeof(int64_t); break;
            case 'f': fn.frame_size += sizeof(float); break;
            case 'd': fn.frame_size += sizeof(double); break;
            default:
                return "Unknown type in signature";
            }
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
        for (char c : arg_sig) {
            switch (c) {
            case 'i': fn.args_size += sizeof(int32_t); break;
            case 'l': fn.args_size += sizeof(int64_t); break;
            case 'f': fn.args_size += sizeof(float); break;
            case 'd': fn.args_size += sizeof(double); break;
            default:
                return "Unknown type in signature";
            }
        }

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

