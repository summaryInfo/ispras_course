#include "ofile.hpp"
#include "util.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <limits>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <map>


/* It would be better to use enum class but
 * I am too lazy to write insn_class:: */
enum insn_class {
    ins_plain,
    ins_jump,
    ins_call,
    ins_memory
};

struct opdesc {
    uint8_t code;
    uint8_t insn_class;
    char type;
};

const static std::map<std::string, opdesc> cmds = {
    {"hlt", {op_hlt_, ins_plain, 0}},
    {"jmp", {op_jmp_, ins_jump, 0}},
    {"call", {op_call_, ins_call, 0}},
    {"tcall", {op_tcall_, ins_call, 0}},
     /* These commands are accesses using ld/st mnemonics:
     *    ld.i/ld.l/ld.f/ld.d
     *    st.i/st.l/st.f/st.d
     *    lda.i/lda.l/lda.f/lda.d
     *    sta.i/sta.l/sta.f/sta.d
     *    ldc.i/ldc.l/ldc.f/ldc.d
     *    ldi.i/ldi.l
     * 'wide' is a prefix and never should be used explicitly
     */
    {"ld.i", {op_lda_i, ins_memory, 'i'}},
    {"ld.l", {op_lda_l, ins_memory, 'l'}},
    {"ld.f", {op_lda_f, ins_memory, 'f'}},
    {"ld.d", {op_lda_d, ins_memory, 'd'}},
    {"st.i", {op_sta_i, ins_memory, 'i'}},
    {"st.l", {op_sta_l, ins_memory, 'l'}},
    {"st.f", {op_sta_f, ins_memory, 'f'}},
    {"st.d", {op_sta_d, ins_memory, 'd'}},
    {"add.i", {op_add_i, ins_plain, 'i'}},
    {"add.l", {op_add_l, ins_plain, 'l'}},
    {"add.f", {op_add_f, ins_plain, 'f'}},
    {"add.d", {op_add_d, ins_plain, 'd'}},
    {"sub.i", {op_sub_i, ins_plain, 'i'}},
    {"sub.l", {op_sub_l, ins_plain, 'l'}},
    {"sub.f", {op_sub_f, ins_plain, 'f'}},
    {"sub.d", {op_sub_d, ins_plain, 'd'}},
    {"mul.i", {op_mul_i, ins_plain, 'i'}},
    {"mul.l", {op_mul_l, ins_plain, 'l'}},
    {"mul.f", {op_mul_f, ins_plain, 'f'}},
    {"mul.d", {op_mul_d, ins_plain, 'd'}},
    {"div.i", {op_div_i, ins_plain, 'i'}},
    {"div.l", {op_div_l, ins_plain, 'l'}},
    {"div.f", {op_div_f, ins_plain, 'f'}},
    {"div.d", {op_div_d, ins_plain, 'd'}},
    {"neg.i", {op_neg_i, ins_plain, 'i'}},
    {"neg.l", {op_neg_l, ins_plain, 'l'}},
    {"neg.f", {op_neg_f, ins_plain, 'f'}},
    {"neg.d", {op_neg_d, ins_plain, 'd'}},
    {"jl.i", {op_jl_i, ins_jump, 'i'}},
    {"jl.l", {op_jl_l, ins_jump, 'l'}},
    {"jl.f", {op_jl_f, ins_jump, 'f'}},
    {"jl.d", {op_jl_d, ins_jump, 'd'}},
    {"jg.i", {op_jg_i, ins_jump, 'i'}},
    {"jg.l", {op_jg_l, ins_jump, 'l'}},
    {"jg.f", {op_jg_f, ins_jump, 'f'}},
    {"jg.d", {op_jg_d, ins_jump, 'd'}},
    {"jlz.i", {op_jlz_i, ins_jump, 'i'}},
    {"jlz.l", {op_jlz_l, ins_jump, 'l'}},
    {"jgz.i", {op_jgz_i, ins_jump, 'i'}},
    {"jgz.l", {op_jgz_l, ins_jump, 'l'}},
    {"call.i", {op_call_i, ins_call, 'i'}},
    {"call.l", {op_call_l, ins_call, 'l'}},
    {"call.f", {op_call_f, ins_call, 'f'}},
    {"call.d", {op_call_d, ins_call, 'd'}},
    {"dup.i", {op_dup_i, ins_plain, 'i'}},
    {"dup.l", {op_dup_l, ins_plain, 'l'}},
    {"dup2.i", {op_dup2_i, ins_plain, 'i'}},
    {"dup2.l", {op_dup2_l, ins_plain, 'l'}},
    {"drop.i", {op_drop_i, ins_plain, 'i'}},
    {"drop.l", {op_drop_l, ins_plain, 'l'}},
    {"drop2.i", {op_drop2_i, ins_plain, 'i'}},
    {"drop2.l", {op_drop2_l, ins_plain, 'l'}},
    {"tol.i", {op_tol_i, ins_plain, 'i'}},
    {"toi.l", {op_toi_l, ins_plain, 'l'}},
    {"tol.f", {op_tol_f, ins_plain, 'f'}},
    {"tol.d", {op_tol_d, ins_plain, 'd'}},
    {"tof.i", {op_tof_i, ins_plain, 'i'}},
    {"tof.l", {op_tof_l, ins_plain, 'l'}},
    {"toi.f", {op_toi_f, ins_plain, 'f'}},
    {"tof.d", {op_tof_d, ins_plain, 'd'}},
    {"tod.i", {op_tod_i, ins_plain, 'i'}},
    {"tod.l", {op_tod_l, ins_plain, 'l'}},
    {"tod.f", {op_tod_f, ins_plain, 'f'}},
    {"toi.d", {op_toi_d, ins_plain, 'd'}},
    {"inc.i", {op_inc_i, ins_plain, 'i'}},
    {"inc.l", {op_inc_l, ins_plain, 'l'}},
    {"dec.i", {op_dec_i, ins_plain, 'i'}},
    {"dec.l", {op_dec_l, ins_plain, 'l'}},
    {"rem.i", {op_rem_i, ins_plain, 'i'}},
    {"rem.l", {op_rem_l, ins_plain, 'l'}},
    {"not.i", {op_not_i, ins_plain, 'i'}},
    {"not.l", {op_not_l, ins_plain, 'l'}},
    {"and.i", {op_and_i, ins_plain, 'i'}},
    {"and.l", {op_and_l, ins_plain, 'l'}},
    {"or.i", {op_or_i, ins_plain, 'i'}},
    {"or.l", {op_or_l, ins_plain, 'l'}},
    {"shr.i", {op_shr_i, ins_plain, 'i'}},
    {"shr.l", {op_shr_l, ins_plain, 'l'}},
    {"shl.i", {op_shl_i, ins_plain, 'i'}},
    {"shl.l", {op_shl_l, ins_plain, 'l'}},
    {"xor.i", {op_xor_i, ins_plain, 'i'}},
    {"xor.l", {op_xor_l, ins_plain, 'l'}},
    {"sar.i", {op_sar_i, ins_plain, 'i'}},
    {"sar.l", {op_sar_l, ins_plain, 'l'}},
    {"jle.i", {op_jle_i, ins_jump, 'i'}},
    {"jle.l", {op_jle_l, ins_jump, 'l'}},
    {"jge.i", {op_jge_i, ins_jump, 'i'}},
    {"jge.l", {op_jge_l, ins_jump, 'l'}},
    {"je.i", {op_je_i, ins_jump, 'i'}},
    {"je.l", {op_je_l, ins_jump, 'l'}},
    {"jne.i", {op_jne_i, ins_jump, 'i'}},
    {"jne.l", {op_jne_l, ins_jump, 'l'}},
    {"jz.i", {op_jz_i, ins_jump, 'i'}},
    {"jz.l", {op_jz_l, ins_jump, 'l'}},
    {"jnz.i", {op_jnz_i, ins_jump, 'i'}},
    {"jnz.l", {op_jnz_l, ins_jump, 'l'}},
    {"ret.i", {op_ret_i, ins_plain, 'i'}},
    {"ret.l", {op_ret_l, ins_plain, 'l'}},
    {"ret.f", {op_ret_f, ins_plain, 'f'}},
    {"ret.d", {op_ret_d, ins_plain, 'd'}},
};

constexpr static uint32_t no_function = -1U;
constexpr static int32_t long_jump_len = 4;
constexpr static int32_t short_jump_len = 2;
constexpr static int32_t global_load_op_offset = 2;

static void print_line_error(const char *msg, const char *file, size_t line_n, const std::string &line, std::string::iterator it) {
    std::cerr
        << msg << " at file " << file << ":" << line_n << std::endl
        << std::setw(it - line.begin()) << "|\n"
        << std::setw(it - line.begin()) << "V\n"
        << line << std::endl;
}

object_file compile_functions(const char *file, std::istream &istr) {
    /* unresolved jump instructions for current function */
    std::map<std::string, uint32_t> labels;
    std::multimap<std::string, uint32_t> jumps;

    /* Signature parts */
    std::string locals_sig;
    std::string args_sig;
    char return_sig{};

    /* local variables for current function */
    std::map<std::string, uint32_t> locals;
    /* currently generated function */
    uint32_t cfun = no_function;

    /* current line */
    std::string line;
    size_t line_n{1};

    /* object file representation we generate */
    object_file out;

    /* Emit call instruction */
    auto compile_call =  [&out](uint32_t cfun, uint8_t op, std::string id) -> const char * {
        uint32_t idx = out.id(std::move(id));
        auto fun_idx = out.function_indices.find(idx);
        /* If function is not defined yet we should insert dummy
         * function with extern flag (builtin functions work like this) */
        uint32_t func_i{};
        if (fun_idx == out.function_indices.end()) {
            func_i = out.functions.size();
            out.function_indices.emplace(idx, func_i);
            out.functions.emplace_back();
        } else func_i = fun_idx->second;

        auto &code = out.functions[cfun].code;

        /* generate as short instruction as we can */
        if (func_i < std::numeric_limits<uint8_t>::max()) {
            code.push_back(op);
            code.push_back(func_i);
        } else if (func_i < std::numeric_limits<uint16_t>::max()) {
            code.push_back(op_pwide);
            code.push_back(op);
            code.push_back(func_i);
        } else {
            return "Too many functions";
        }
        return nullptr;
    };

    /* Emit jump instruction */
    auto compile_jump = [&out, &labels, &jumps](uint32_t cfun, uint8_t op, std::string id) -> const char * {
        auto lab = labels.find(id);
        auto &code = out.functions[cfun].code;
        if (lab != labels.end()) {
            int32_t disp = lab->second - code.size();
            if (disp - short_jump_len <= std::numeric_limits<int8_t>::max() &&
                disp - short_jump_len >= std::numeric_limits<int8_t>::min()) {
                 /* Can generate short jump insn */
                code.push_back(op);
                code.push_back(disp);
            } else if (disp - long_jump_len <= std::numeric_limits<int16_t>::max() &&
                       disp - long_jump_len >= std::numeric_limits<int16_t>::min()) {
                 /* Can generate long jump insn */
                code.push_back(op_pwide);
                code.push_back(op);
                util::vec_put_native<int16_t>(code, disp);
            } else return "Jump is out of range";
        } else {
            /* Store jump as one of unresolved jumps */
            jumps.emplace(std::move(id), code.size());

            /* If we does not know where we jump
             * for sake of simplicity generate long jump */
            code.push_back(op_pwide);
            code.push_back(op);
            util::vec_put_native<int16_t>(code, 0);
        }
        return nullptr;
    };

    /* Emit load/store instruction */
    auto compile_load = [&out, &locals](std::vector<uint8_t> &code, std::string &&label, uint8_t op) -> const char *{
        auto loc = locals.find(label);
        if (loc != locals.end()) {
            /* local variable/parameter load */

            int32_t disp = loc->second;

            if (disp <= std::numeric_limits<int8_t>::max() &&
                disp >= std::numeric_limits<int8_t>::min()) {
                code.push_back(op);
                code.push_back(disp);
            } else if (disp <= std::numeric_limits<int16_t>::max() &&
                       disp >= std::numeric_limits<int16_t>::min()) {
                code.push_back(op_pwide);
                code.push_back(op);
                util::vec_put_native<int16_t>(code, disp);
            } else return "Too many locals/arguments";
        } else {
            /* Otherwise consider local to be a load of global variable */
            uint32_t idx = out.id(std::move(label));
            auto glob = out.global_indices.find(idx);
            if (glob != out.global_indices.end()) {
                auto disp = glob->second;

                if (disp <= std::numeric_limits<uint8_t>::max() &&
                    disp >= std::numeric_limits<uint8_t>::min()) {
                    code.push_back(op + global_load_op_offset);
                    code.push_back(disp);
                } else if (disp <= std::numeric_limits<uint16_t>::max() &&
                           disp >= std::numeric_limits<uint16_t>::min()) {
                    code.push_back(op_pwide);
                    code.push_back(op + global_load_op_offset);
                    util::vec_put_native<uint16_t>(code, disp);
                } else return "Too many globals";
            } else return "Undefined variable";
        }
        return nullptr;
    };

    /* Emit constant load */
    auto compile_const = [](std::vector<uint8_t> &code, std::string &&label, uint8_t op, std::string::iterator &it, char type) -> const char *{
        if ((op & ~0x60) != op_lda_i) return "Constant store";
        char *end;
        errno = 0;
        switch(type) {
        case 'i': {
            long res = std::strtol(&*it, &end, 0);
            if (res <= std::numeric_limits<int8_t>::max() ||
                res >= std::numeric_limits<int8_t>::min()) {
                /* Generate ldi.i */
                code.push_back(op_ldi_i);
                util::vec_put_native(code, (int8_t)res);
            } else if (res <= std::numeric_limits<int16_t>::max() ||
                       res >= std::numeric_limits<int16_t>::min()) {
                /* Generate wide ldi.i */
                code.push_back(op_pwide);
                code.push_back(op_ldi_i);
                util::vec_put_native(code, (int16_t)res);
            } else {
                /* Generate wide ldc.i */
                code.push_back(op_ldc_i);
                if (res > std::numeric_limits<int32_t>::max() ||
                    res < std::numeric_limits<int32_t>::min()) errno = ERANGE;
                util::vec_put_native(code, (int32_t)res);
            }
        } break;
        case 'l': {
            long long res = std::strtoll(&*it, &end, 0);
            if (res <= std::numeric_limits<int8_t>::max() ||
                res >= std::numeric_limits<int8_t>::min()) {
                /* Generate ldi.i */
                code.push_back(op_ldi_l);
                util::vec_put_native(code, (int8_t)res);
            } else if (res <= std::numeric_limits<int16_t>::max() ||
                       res >= std::numeric_limits<int16_t>::min()) {
                /* Generate wide ldi.i */
                code.push_back(op_pwide);
                code.push_back(op_ldi_l);
                util::vec_put_native(code, (int16_t)res);
            } else {
                /* Generate wide ldc.i */
                code.push_back(op_ldc_l);
                if (res > std::numeric_limits<int64_t>::max() ||
                    res < std::numeric_limits<int64_t>::min()) errno = ERANGE;
                util::vec_put_native(code, (int64_t)res);
            }
        } break;
        case 'f': {
            float res = std::strtof(&*it, &end);
            code.push_back(op_ldc_f);
            util::vec_put_native(code, res);
        } break;
        case 'd': {
            code.push_back(op_ldc_d);
            double res = std::strtod(&*it, &end);
            util::vec_put_native(code, res);
        } break;
        default:
            throw std::logic_error("Unreachable");
        }
        if (errno || end == &*it) return ("Malformed constant");
        it += end - &*it;
        return nullptr;
    };

    /* Define new label */
    auto add_label = [&jumps, &labels](function &fun, std::string &&label) -> const char * {
        auto range = jumps.equal_range(label);
        uint32_t off = fun.code.size();
        /* define new label */
        labels.emplace(std::move(label), off);
        if (range.first != jumps.end()) {
            /* and resolve jumps */
            for (auto itr = range.first; itr != range.second; itr++) {
                int32_t disp = off - itr->second - 4;
                if (disp <= std::numeric_limits<int16_t>::max() &&
                    disp >= std::numeric_limits<int16_t>::min()) {
                    int16_t d16 = disp;
                    std::memcpy(&fun.code[itr->second + 2], &d16, sizeof d16);
                } else {
                    /* It is a little confusing to generate
                     * this mesage in place of label definition and
                     * not jump insn itself */
                    return "Jump is out of range";
                }
            }
            /* remove resolved jumps */
            jumps.erase(range.first, range.second);
        }
        return nullptr;
    };

    /* Emit finished function*/
    auto emit_function = [&](uint32_t cfun) -> const char * {
        if (!jumps.empty()) return "Unresolved jumps";
        out.functions[cfun].signature = '(' + args_sig + ')' + return_sig;
        out.functions[cfun].locals = std::move(locals_sig);

        /* Clear all temporary info */
        locals.clear();
        locals_sig.clear();
        args_sig.clear();
        labels.clear();

        return nullptr;
    };

    while (getline(istr, line)) {
        /* Skip whitespaces */
        auto skip_spaces = [](std::string::iterator &it) {
            while(*it && std::isspace(*it)) it++;
        };
        /* Read identifier */
        auto consume_id = [&](std::string &id, std::string::iterator &it, const char *additional_term) {
            id.clear();
            while (*it && (std::isalnum(*it) || *it == '.')) id.push_back(*it++);
            if (!id.length() || !(!*it || std::isspace(*it) || std::strchr(additional_term,*it))) {
                print_line_error("Malformed identifier", file, line_n, line, it);
                throw std::logic_error("Malformed id");
            }
            skip_spaces(it);
        };

        auto it = line.begin();

        skip_spaces(it);

        if (!*it) continue;
        else if (*it == '.') /* directive */ {
            /* All of supported directives apparently have the same format:
             * .<directive> <type> <name> */

             /* skip dot */
             it++;

            /* read directive name */
            std::string id;
            consume_id(id, it, "");

            /* read type */
            std::string typid;
            consume_id(typid, it, "");
            if (typid != "int" && typid != "long" && typid != "float" && typid != "double") {
                print_line_error("Unknown type", file, line_n, line, it);
                throw std::logic_error("Wrong directive");
            }

            /* read variable/function identifier */
            std::string name;
            consume_id(name, it, "#");

            if (id == "global") {

                /* Initial value */
                char *end = nullptr;
                errno = 0;
                bool success = true, init{};
                if (typid == "int") {
                    int32_t value{};
                    long res = std::strtol(&*it, &end, 0);
                    if (res > std::numeric_limits<int32_t>::max() ||
                        res < std::numeric_limits<int32_t>::min()) errno = ERANGE;
                    else if (!end || errno) print_line_error("Wrong constant", file, line_n, line, it);
                    else if (end != &*it) value = res, init = true;
                    success = out.add_global(std::move(name), value, init, typid[0]);
                } else if (typid == "long") {
                    int64_t value{};
                    long long res = std::strtoll(&*it, &end, 0);
                    if (res > std::numeric_limits<int64_t>::max() ||
                        res < std::numeric_limits<int64_t>::min()) errno = ERANGE;
                    if (!end || errno) print_line_error("Wrong constant", file, line_n, line, it);
                    else if (end != &*it) value = res, init = true;
                    success = out.add_global(std::move(name), value, init, typid[0]);
                } else if (typid == "float") {
                    float value = std::strtof(&*it, &end);
                    if (!end || errno)  print_line_error("Wrong constant", file, line_n, line, it);
                    else if (end == &*it) value = {};
                    success = out.add_global(std::move(name), value, init, typid[0]);
                } else if (typid == "double") {
                    double value = std::strtod(&*it, &end);
                    if (!end || errno)  print_line_error("Wrong constant", file, line_n, line, it);
                    else if (end == &*it) value = {};
                    success = out.add_global(std::move(name), value, init, typid[0]);
                }
                if (!success) {
                    print_line_error("Redefinition of global with different type", file, line_n, line, it);
                    throw std::logic_error("Global redefinition");
                }
                if (end) it += end - &*it;
            } else if (id == "local") {
                /* Local variables have negative indices for lda/sta */
                if (cfun == no_function) {
                    print_line_error("Locals can only be defined inside a function", file, line_n, line, it);
                    throw std::logic_error("Out of scope");
                }
                locals_sig.push_back(typid[0]);
                locals.emplace(std::move(name), -locals_sig.size());
            } else if (id == "param") {
                /* Prameters have positive indices for lda/sta */
                if (cfun == no_function) {
                    print_line_error("Parameters can only be defined inside a function", file, line_n, line, it);
                    throw std::logic_error("Out of scope");
                }
                args_sig.push_back(typid[0]);
                locals.emplace(std::move(name), args_sig.size());
            } else if (id == "function") {
                if (cfun != no_function) {
                    auto erc = emit_function(cfun);
                    if (erc) {
                        print_line_error(erc, file, line_n, line, it);
                        throw std::logic_error(erc);
                    }
                }

                return_sig = typid[0];

                auto id = out.id(std::move(name));
                auto res = out.function_indices.find(id);
                if (res == out.function_indices.end()) {
                    /* new function */
                    cfun = out.functions.size();
                    out.functions.emplace_back();
                    out.functions.back().name = id;
                } else if (!out.functions[res->second].code.size()) {
                    /* forward declared function */
                    cfun = res->second;
                } else {
                    print_line_error("Function redefinition", file, line_n, line, it);
                    throw std::logic_error("Redefinition");
                }
            } else {
                print_line_error("Unknown directive", file, line_n, line, it);
                throw std::logic_error("Unknown directive");
            }

        } else if (*it != '#') /* command or label */ {
            if (cfun == no_function) {
                print_line_error("Instruction are only allowed inside a function", file, line_n, line, it);
                throw std::logic_error("Out of scope");
            }
            std::string id, label;
            consume_id(id, it, ":#");

            if (*it == ':') /* label */ {
                it++;
                label = std::move(id);

                auto erc = add_label(out.functions[cfun], std::move(label));
                if (erc) {
                    print_line_error(erc, file, line_n, line, it);
                    throw std::out_of_range(erc);
                }

                skip_spaces(it);
                if (!*it) continue;
                consume_id(id, it, "#");
            }

            auto insn = cmds.find(id);
            if (insn == cmds.end()) {
                print_line_error("Unknown instruction", file, line_n, line, it);
                throw std::logic_error("Unknown instruction");
            }

            switch(insn->second.insn_class) {
            case ins_call: {
                consume_id(id, it, "#");
                auto *erc = compile_call(cfun, insn->second.code, id);
                if (erc) {
                    print_line_error(erc, file, line_n, line, it);
                    throw std::out_of_range(erc);
                }
            } break;
            case ins_jump: {
                consume_id(id, it, "#");
                auto *erc = compile_jump(cfun, insn->second.code, id);
                if (erc) {
                    print_line_error(erc, file, line_n, line, it);
                    throw std::out_of_range(erc);
                }
            } break;
            case ins_plain: {
                /* Just opcode */
                out.functions[cfun].code.push_back(insn->second.code);
            } break;
            case ins_memory: {
                const char *erc{};
                if (*it == '$') /* immediate */ {
                    skip_spaces(++it);
                    erc = compile_const(out.functions[cfun].code,
                                        std::move(label), insn->second.code,
                                        it, insn->second.type);
                } else {
                    consume_id(label, it, "#");
                    erc = compile_load(out.functions[cfun].code, std::move(label), insn->second.code);
                }
                if (erc) {
                    print_line_error(erc, file, line_n, line, it);
                    throw std::logic_error(erc);
                }
            } break;
            }
        }

        skip_spaces(it);

        /* skip comment */
        if (*it && *it != '#') {
            print_line_error("Unexpected character at the end of line", file, line_n, line, it);
            throw std::logic_error("Unexpected char at EOL");
        }

        line_n++;
    }

    auto erc = emit_function(cfun);
    if (erc) {
        print_line_error(erc, file, line_n, line, line.end() - 1);
        throw std::logic_error(erc);
    }

    return out;
}

int main(int argc, char *argv[]) {

    if ((argc > 1 && !std::strcmp("-h", argv[1])) || argc < 2) {
        std::cout << "Usage:\n" << std::endl;
        std::cout << "\t" << argv[0] << " <outfile> [<infile>]" << std::endl;
        std::cout << "Default value of <infile> is stdin" << std::endl;
        return 0;
    }

    object_file obj;
    if (argc > 2) {
        std::ifstream fstr(argv[2]);
        obj = compile_functions(argv[2], fstr);
    } else {
        obj = compile_functions("<stdoin>", std::cin);
    }

    std::ofstream outstr(argv[1]);
    obj.write(outstr);

    return 0;
}
