#include "vm.hpp"
#include "util.hpp"

#include <limits>
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

void object_file::write(std::ostream &st) {
    /* 1. File header */
    vm_header hd = {
        /* magic */{'X','S','V','M'},
        /* flags */ 0,
        /* functab_offset */ sizeof(vm_header),
        /* functab_size */ static_cast<uint32_t>(sizeof(vm_function)*functions.size()),
        /* globtab_offset */ static_cast<uint32_t>(sizeof(vm_header) + sizeof(vm_function)*functions.size()),
        /* globtab_size */ static_cast<uint32_t>(sizeof(vm_global)*globals.size()),
        /* strtab_offset */ static_cast<uint32_t>(sizeof(vm_header) + sizeof(vm_function)*functions.size() + sizeof(vm_global)*globals.size()),
        /* strtab_size */ static_cast<uint32_t>(strtab_offset)
    };
    size_t file_offset = hd.strtab_offset + hd.strtab_size;

    st.write(reinterpret_cast<char *>(&hd), sizeof(hd));

    /* 2. Function table */
    for (auto &f : functions) {
        vm_function vmf = {
            f.name,
            f.signature,
            id(std::string(f.locals)),
            static_cast<uint32_t>(file_offset),
            static_cast<uint32_t>(f.code.size())
        };
        st.write(reinterpret_cast<char *>(&vmf), sizeof(vmf));
        file_offset += f.code.size();
    }

    /* 3. Globals table */
    st.write(reinterpret_cast<char *>(&globals), globals.size()*sizeof(vm_global));

    /* 4. String table */
    std::map<uint32_t, std::string> revmap;
    for (auto &i : strtab) revmap.emplace(i.second, i.first);
    for (auto &str : revmap) st.write(str.second.data(), str.second.size() + 1);

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
};


static void recur

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
bool trace_types(std::vector<std::shared_ptr<stack_state>> &anno, std::shared_ptr<stack_state> &state,
           std::vector<uint8_t>::iterator op, std::vector<uint8_t>::iterator end) {

    bool wide = *op == op_pwide;

    /* Annotate first instruction */
    code_anno[0] = stk;


    while (op < end) {
        // TODO Assign type
        while (*op == op_pwide) {
            op++;
        }

        auto check_jump = [&]() -> bool {
            const uint8_t *tmp = &*op;
            auto disp = util::read_either<int16_t>(tmp, wide);
            wide = 0;
            return op + disp > end || trace_types(anno, state, op + disp, end);
        };

        auto check = [&](const char *before, const char *after) -> bool {
            // TODO
            return false;
        };

        switch(*op++) {
        case op_je_i: case op_jge_i:
        case op_jle_i: case op_jl_i:
        case op_jg_i: case op_jne_i:
            if (!check("ii", "")) return false;
            if (!check_jump()) return false;
            break;
        case op_add_i: case op_div_i:
        case op_and_i: case op_or_i:
        case op_mul_i: case op_sar_i:
        case op_shl_i: case op_shr_i:
        case op_sub_i: case op_xor_i:
        case op_rem_i:
            if (!check("ii", "i")) return false;
            break;
        case op_je_l: case op_jge_l:
        case op_jle_l: case op_jl_l:
        case op_jg_l: case op_jne_l:
            if (!check_jump()) return false;
            if (!check("ll", "")) return false;
            break;
        case op_add_l: case op_div_l:
        case op_and_l: case op_or_l:
        case op_mul_l: case op_sub_l:
        case op_xor_l: case op_rem_l:
            if (!check("ll", "l")) return false;
            break;
        case op_sar_l: case op_shl_l:
        case op_shr_l:
            if (!check("li", "l")) return false;
            break;
        case op_jg_d: case op_jl_d:
            if (!check("dd", "")) return false;
            if (!check_jump()) return false;
            break;
        case op_add_d: case op_div_d:
        case op_mul_d: case op_sub_d:
            if (!check("li", "l")) return false;
            break;
        case op_jg_f: case op_jl_f:
            if (!check("ff", "")) return false;
            if (!check_jump()) return false;
            [[fallthrough]];
        case op_sub_f: case op_add_f:
        case op_div_f: case op_mul_f:
            if (!check("ff", "f")) return false;
            break;
        case op_jz_l: case op_jlz_l:
        case op_jgz_l: case op_jnz_l:
            if (!check_jump()) return false;
            if (!check("l", "")) return false;
            break;
        case op_inc_l: case op_dec_l:
        case op_neg_l: case op_not_l:
            if (!check("l", "l")) return false;
            break;
        case op_jz_i: case op_jlz_i:
        case op_jgz_i: case op_jnz_i:
            if (!check_jump()) return false;
            if (!check("i", "")) return false;
            break;
        case op_inc_i: case op_dec_i:
        case op_neg_i: case op_not_i:
            if (!check("i", "i")) return false;
            break;
        case op_neg_d:
            if (!check("d", "d")) return false;
            break;
        case op_neg_f:
            if (!check("f", "f")) return false;
            break;
        case op_tod_f:
            if (!check("f", "d")) return false;
            break;
        case op_tod_i:
            if (!check("i", "d")) return false;
            break;
        case op_tod_l:
            if (!check("l", "d")) return false;
            break;
        case op_tof_d:
            if (!check("d", "f")) return false;
            break;
        case op_tof_i:
            if (!check("i", "f")) return false;
            break;
        case op_tof_l:
            if (!check("l", "f")) return false;
            break;
        case op_toi_d:
            if (!check("d", "i")) return false;
            break;
        case op_toi_f:
            if (!check("f", "i")) return false;
            break;
        case op_toi_l:
            if (!check("l", "i")) return false;
            break;
        case op_tol_d:
            if (!check("d", "l")) return false;
            break;
        case op_tol_f:
            if (!check("f", "l")) return false;
            break;
        case op_tol_i:
            if (!check("i", "l")) return false;
            break;
        case op_ldc_l:
            // TODO
            break;
        case op_ldi_l:
            // TODO
            break;
        case op_ld_l:
            // TODO
            break;
        case op_lda_l:
            // TODO
            break;
        case op_ldc_i:
            // TODO
            break;
        case op_ld_i:
            // TODO
            break;
        case op_lda_i:
            // TODO
            break;
        case op_ldi_i:
            // TODO
            break;
        case op_ldc_f:
            // TODO
            break;
        case op_ld_f:
            // TODO
            break;
        case op_lda_f:
            // TODO
            break;
        case op_ld_d:
            // TODO
            break;
        case op_lda_d:
            // TODO
            break;
        case op_ldc_d:
            // TODO
            break;
        case op_dup_i: /* (i)ii or (f)ff */
            // TODO
            break;
        case op_dup_l: /* (l)ll or (d)dd */
            // TODO
            break;
        case op_dup2_i:
            // TODO
            break;
        case op_dup2_l:
            // TODO
            break;
        case op_drop2_i:
            // TODO
            break;
        case op_drop2_l:
            // TODO
            break;
        case op_drop_i:
            // TODO
            break;
        case op_drop_l:
            // TODO
            break;
        case op_st_d: /* (d) */
        case op_sta_d:
            // TODO
            op += 1 + wide;
            break;
        case op_st_i: /* (i) */
        case op_sta_i:
            op += 1 + wide;
            // TODO
            break;
        case op_st_l: /* (l) */
        case op_sta_l:
            op += 1 + wide;
            // TODO
            break;
        case op_st_f: /* (f)*/
        case op_sta_f:
            op += 1 + wide;
            // TODO
            break;
        case op_jmp_: /* jump */
            return check_jump();
            break;
        case op_call_: /* (...) */
            // TODO
            op += 1 + wide;
            break;
        case op_call_d: /* (...)d */
            // TODO
            op += 1 + wide;
            break;
        case op_call_f: /* (...)f */
            // TODO
            op += 1 + wide;
            break;
        case op_call_i: /* (...)i */
            // TODO
            op += 1 + wide;
            break;
        case op_call_l: /* (...)l */
            // TODO
            op += 1 + wide;
            break;
        case op_tcall_: /* (...)..., term */
            throw std::logic_error("Unimplemented");
        case op_hlt_: /* (), term */
            return true;
            break;
        case op_ret_: /* (), term */
            break;
        case op_ret_d /* (d), term */:
            break;
        case op_ret_f /* (f), term */:
            break;
        case op_ret_i /* (i), term */:
            break;
        case op_ret_l /* (l), term */:
            break;
        default:
            return false;
        }
    }
    return true;
}


static const char *validate_functions(object_file &obj) {
    for (auto &fn : obj.functions) {
        fn.args_size = 0;
        fn.frame_size = 0;

        /* Initial stack state */
        std::shared_ptr<stack_state> stk = nullptr;

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

        auto it = obj.strtab.begin();
        // TODO: This is inefficient
        for (; it->second != fn.signature; it++) {
            if (it == obj.strtab.end())
                return "Function name is not found";
        }

        if (!it->first.size() || it->first[0] != '(')
            return "Malformed signature";

        auto clo = it->first.find(')', 1);
        if (clo == it->first.npos || clo + 2 != it->first.size())
            return "Malformed signature";

        char return_sig = it->first[clo + 1];
        if (!std::strchr("ilfd", return_sig))
            return "Unknown return type in signature";

        std::string arg_sig(it->first.substr(1, clo));
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
        std::vector<std::shared_ptr<stack_state>> code_anno(fn.code.size());
        if (!trace_types(code_anno, stk, fn.code.begin(), fn.code.end()))
            return "Code is invalid";
    }
}

object_file object_file::read(std::istream &str) {
    object_file in;

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

    std::vector<char> strtab(hd.strtab_size);
    str.seekg(hd.strtab_offset);
    str.read(strtab.data(), hd.strtab_size);

    if (strtab[0] || strtab.back())
        throw std::invalid_argument("String table is not bounded");

    /* 3. Globals table */

    in.globals.resize(hd.globals_size/sizeof(vm_global));
    str.seekg(hd.globals_offset);
    str.read(reinterpret_cast<char *>(in.globals.data()), hd.globals_size);

    /* Validate each global */
    size_t i{};
    for (auto &gl : in.globals) {
        if (gl.name >= strtab.size())
            throw std::invalid_argument("String table index is out of bounds");

        /* Intern global name into a string table */
        auto id = in.id(std::string(&strtab[gl.name]));

        if (!std::strchr("ilfd", gl.type))
            throw std::invalid_argument("Unknown global type");
        if (gl.flags & ~gf_init)
            throw std::invalid_argument("Unsupported global flags");
        if (gl.flags & gf_init && gl.init_value)
            throw std::invalid_argument("Non-zero reserved init value");
        if (gl.dummy0)
            throw std::invalid_argument("Non-zero reserved value");

        in.global_indices.emplace(id, i++);
    }

    /* 4. Functions table */

    std::vector<vm_function> functions(hd.funcs_size/sizeof(vm_function));
    str.seekg(hd.funcs_offset);
    str.read(reinterpret_cast<char *>(functions.data()), hd.funcs_size);

    for (auto &fn : functions) {
        if (fn.name >= strtab.size())
            throw std::invalid_argument("String table index is out of bounds");
        if (fn.signature >= strtab.size())
            throw std::invalid_argument("String table index is out of bounds");
        if (fn.locals >= strtab.size())
            throw std::invalid_argument("String table index is out of bounds");

        if (fn.code_size + fn.code_offset >= pos)
            throw std::invalid_argument("Function body is out of bounds");
        // TODO
        if (!fn.code_size)
            throw std::invalid_argument("Extern functions are not supported yet");
        if (fn.code_size > std::numeric_limits<uint32_t>::max())
            throw std::invalid_argument("Code is too big");


        std::vector<uint8_t> code(fn.code_size);
        str.seekg(fn.code_offset);
        str.read(reinterpret_cast<char *>(code.data()), fn.code_size);

        in.function_indices.emplace(fn.name, in.functions.size());
        in.functions.emplace_back(
            in.id(std::string(&strtab[fn.name])),
            in.id(std::string(&strtab[fn.signature])),
            std::string(&strtab[fn.locals]),
            0, /* frame_size -- calculated later */
            0, /* args_size -- calculated later */
            std::move(code)
        );
    }

    functions.clear();
    strtab.clear();

    /* 5. Validate code */

    const char *erc = validate_functions(in);
    if (erc) throw std::invalid_argument(erc);

    return in;
}

