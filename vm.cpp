#include "vm.hpp"
#include "util.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

using op_func = void(*)(vm_state &vm, const uint8_t *&ip);

constexpr static std::size_t stack_size = 1024;
constexpr static double eps = 1e-6;

/* VM opcodes */

void op_wide(vm_state &vm, const uint8_t *&ip) {
    vm.wide = true;
}

template <typename T> void op_add(vm_state &vm, const uint8_t *&ip) {
    T arg = vm.pop<T>();
    vm.push<T>(arg + vm.pop<T>());
}

template <typename T> void op_inc(vm_state &vm, const uint8_t *&ip) {
    //vm.push<T>(vm.pop<T>() + util::read_im<T, uint8_t>(ip, vm.wide));
    vm.push<T>(vm.pop<T>() + 1);
}

template <typename T> void op_dec(vm_state &vm, const uint8_t *&ip) {
    //vm.push<T>(vm.pop<T>() - util::read_im<T, uint8_t>(ip, vm.wide));
    vm.push<T>(vm.pop<T>() - 1);
}

template <typename T> void op_sub(vm_state &vm, const uint8_t *&ip) {
    T arg = vm.pop<T>();
    vm.push<T>(vm.pop<T>() - arg);
}

template <typename T> void op_mul(vm_state &vm, const uint8_t *&ip) {
    T arg = vm.pop<T>();
    vm.push<T>(arg * vm.pop<T>());
}

template <typename T> void op_div(vm_state &vm, const uint8_t *&ip) {
    T a = vm.pop<T>();
    T b = vm.pop<T>();
    // This should work for both integers and floats
    if (a <= T(eps)) throw std::invalid_argument("Divide by zero");
    vm.push<T>(b / a);
}

template <typename T> void op_rem(vm_state &vm, const uint8_t *&ip) {
    T a = vm.pop<T>();
    T b = vm.pop<T>();
    if (a <= T(eps)) throw std::invalid_argument("Divide by zero");
    vm.push<T>(b % a);
}

template <typename T> void op_neg(vm_state &vm, const uint8_t *&ip) {
    vm.push<T>(-vm.pop<T>());
}

template <typename T> void op_and(vm_state &vm, const uint8_t *&ip) {
    (void)ip;
    auto arg = vm.pop<std::make_unsigned_t<T>>();
    vm.push<T>(arg & vm.pop<std::make_unsigned_t<T>>());
}

template <typename T> void op_or(vm_state &vm, const uint8_t *&ip) {
    auto arg = vm.pop<std::make_unsigned_t<T>>();
    vm.push<T>(arg | vm.pop<std::make_unsigned_t<T>>());
}

template <typename T> void op_xor(vm_state &vm, const uint8_t *&ip) {
    auto arg = vm.pop<std::make_unsigned_t<T>>();
    vm.push<T>(arg ^ vm.pop<std::make_unsigned_t<T>>());
}

template <typename T> void op_shr(vm_state &vm, const uint8_t *&ip) {
    auto arg = vm.pop<uint32_t>();
    vm.push<T>(vm.pop<std::make_unsigned_t<T>>() >> arg);
}

template <typename T> void op_shl(vm_state &vm, const uint8_t *&ip) {
    auto arg = vm.pop<uint32_t>();
    vm.push<T>(vm.pop<std::make_unsigned_t<T>>() << arg);
}

template <typename T> void op_sar(vm_state &vm, const uint8_t *&ip) {
    // That is technically UB for until C++20, but works fine in practice
    auto arg = vm.pop<uint32_t>();
    vm.push<T>(vm.pop<std::make_signed_t<T>>() >> arg);
}

template <typename T> void op_not(vm_state &vm, const uint8_t *&ip) {
    vm.push<T>(~vm.pop<T>());
}

template <typename T> void op_jl(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    T arg = vm.pop<T>();
    if (vm.pop<T>() < arg) vm.jump(disp);
}

template <typename T> void op_jg(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    T arg = vm.pop<T>();
    if (vm.pop<T>() > arg) vm.jump(disp);
}

template <typename T> void op_jle(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    T arg = vm.pop<T>();
    if (vm.pop<T>() <= arg) vm.jump(disp);
}

template <typename T> void op_jge(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    T arg = vm.pop<T>();
    if (vm.pop<T>() >= arg) vm.jump(disp);
}

template <typename T> void op_je(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    T arg = vm.pop<T>();
    if (vm.pop<T>() == arg) vm.jump(disp);
}

template <typename T> void op_jne(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    T arg = vm.pop<T>();
    if (vm.pop<T>() != arg) vm.jump(disp);
}

template <typename T> void op_jz(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    if (!vm.pop<T>()) vm.jump(disp);
}

template <typename T> void op_jnz(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    if (vm.pop<T>()) vm.jump(disp);
}

template <typename T> void op_jlz(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    if (vm.pop<T>() < T{}) vm.jump(disp);
}

template <typename T> void op_jlez(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    if (vm.pop<T>() <= T{}) vm.jump(disp);
}

template <typename T> void op_jgz(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    if (vm.pop<T>() > T{}) vm.jump(disp);
}

template <typename T> void op_jgez(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    if (vm.pop<T>() >= T{}) vm.jump(disp);
}

void op_jmp(vm_state &vm, const uint8_t *&ip) {
    ptrdiff_t disp = util::read_im<int16_t>(ip, vm.wide);
    vm.jump(disp);
}

template <typename T> void op_toi(vm_state &vm, const uint8_t *&ip) {
    vm.push<int32_t>(vm.pop<T>());
}

template <typename T> void op_tol(vm_state &vm, const uint8_t *&ip) {
    vm.push<int64_t>(vm.pop<T>());
}

template <typename T> void op_tof(vm_state &vm, const uint8_t *&ip) {
    vm.push<float>(vm.pop<T>());
}

template <typename T> void op_tod(vm_state &vm, const uint8_t *&ip) {
    vm.push<double>(vm.pop<T>());
}

template <typename T> void op_dup(vm_state &vm, const uint8_t *&ip) {
    vm.push<T>(vm.top<T>());
}

template <typename T> void op_dup2(vm_state &vm, const uint8_t *&ip) {
    auto a = vm.pop<T>();
    auto b = vm.top<T>();
    vm.push<T>(a);
    vm.push<T>(b);
    vm.push<T>(a);
}

template <typename T> void op_swap(vm_state &vm, const uint8_t *&ip) {
    auto a = vm.pop<T>();
    auto b = vm.pop<T>();
    vm.push<T>(a);
    vm.push<T>(b);
}

template <typename T> void op_drop(vm_state &vm, const uint8_t *&ip) {
    (void)vm.pop<T>();
}

template <typename T> void op_drop2(vm_state &vm, const uint8_t *&ip) {
    (void)vm.pop<T>();
    (void)vm.pop<T>();
}

void op_call(vm_state &vm, const uint8_t *&ip) {
    vm.invoke(util::read_im<uint16_t,uint8_t>(ip, vm.wide));
}

void op_tcall(vm_state &vm, const uint8_t *&ip) {
    // TODO
    throw std::logic_error("Not implemented");
}

template <typename T> void op_ret(vm_state &vm, const uint8_t *&ip) {
    T res = vm.pop<T>();
    vm.ret();
    vm.push<T>(res);
}

template <> void op_ret<void>(vm_state &vm, const uint8_t *&ip) {
    vm.ret();
}

template <typename T> void op_ld(vm_state &vm, const uint8_t *&ip) {
    vm.push<T>(vm.get_global<T>(util::read_im<uint16_t,uint8_t>(ip, vm.wide)));
}

template <typename T> void op_lda(vm_state &vm, const uint8_t *&ip) {
    vm.push<T>(vm.get_local<T>(util::read_im<int16_t>(ip, vm.wide)));
}

template <typename T> void op_st(vm_state &vm, const uint8_t *&ip) {
    vm.set_global(util::read_im<uint16_t,uint8_t>(ip, vm.wide), vm.pop<T>());
}

template <typename T> void op_sta(vm_state &vm, const uint8_t *&ip) {
    vm.set_local(util::read_im<int16_t>(ip, vm.wide), vm.pop<T>());
}

template <typename T> void op_ldi(vm_state &vm, const uint8_t *&ip) {
    vm.push(util::read_im<int16_t>(ip, vm.wide));
}

template <typename T> void op_ldc(vm_state &vm, const uint8_t *&ip) {
    vm.push(util::read_next<T>(ip));
}

void op_hlt(vm_state &vm, const uint8_t *&ip) {
    vm.halt();
}

/* Thats sad that c++ does not support disignated array initializers... */
op_func opcodes[std::numeric_limits<uint8_t>::max()] {
    /*[0x00] =*/ op_hlt,          op_lda<int32_t>,  op_sta<int32_t>,op_ld<int32_t>,
    /*[0x04] =*/ op_st<int32_t>,  op_ldc<int32_t>,  op_ldi<int32_t>,op_add<int32_t>,
    /*[0x08] =*/ op_sub<int32_t>, op_mul<int32_t>,  op_div<int32_t>,op_neg<int32_t>,
    /*[0x0C] =*/ op_jl<int32_t>,  op_jg<int32_t>,   op_jlz<int32_t>,op_call,
    /*[0x10] =*/ op_dup<int32_t>, op_drop<int32_t>, op_tol<int32_t>,op_tof<int32_t>,
    /*[0x14] =*/ op_tod<int32_t>, op_swap<int32_t>, op_hlt,         op_inc<int32_t>,
    /*[0x18] =*/ op_rem<int32_t>, op_and<int32_t>,  op_shr<int32_t>,op_xor<int32_t>,
    /*[0x1C] =*/ op_jle<int32_t>, op_je<int32_t>,   op_jz<int32_t>, op_ret<int32_t>,
    /*[0x20] =*/ op_jmp,          op_lda<int64_t>,  op_sta<int64_t>,op_ld<int64_t>,
    /*[0x24] =*/ op_st<int64_t>,  op_ldc<int64_t>,  op_ldi<int64_t>,op_add<int64_t>,
    /*[0x28] =*/ op_sub<int64_t>, op_mul<int64_t>,  op_div<int64_t>,op_neg<int64_t>,
    /*[0x2C] =*/ op_jl<int64_t>,  op_jg<int64_t>,   op_jlz<int64_t>,op_call,
    /*[0x30] =*/ op_dup<int64_t>, op_drop<int64_t>, op_toi<int64_t>,op_tof<int64_t>,
    /*[0x34] =*/ op_tod<int64_t>, op_swap<int64_t>, op_hlt,         op_inc<int64_t>,
    /*[0x38] =*/ op_rem<int64_t>, op_and<int64_t>,  op_shr<int64_t>,op_xor<int64_t>,
    /*[0x3C] =*/ op_jle<int64_t>, op_je<int64_t>,   op_jz<int64_t>, op_ret<int64_t>,
    /*[0x40] =*/ op_call,         op_lda<float>,    op_sta<float>,  op_ld<float>,
    /*[0x44] =*/ op_st<float>,    op_ldc<float>,    op_ret<void>,   op_add<float>,
    /*[0x48] =*/ op_sub<float>,   op_mul<float>,    op_div<float>,  op_neg<float>,
    /*[0x4C] =*/ op_jl<float>,    op_jg<float>,     op_jgz<int32_t>,op_call,
    /*[0x50] =*/ op_dup2<int32_t>,op_drop2<int32_t>,op_tol<float>,  op_toi<float>,
    /*[0x54] =*/ op_tod<float>,   op_hlt,           op_hlt,         op_dec<int32_t>,
    /*[0x58] =*/ op_not<int32_t>, op_or<int32_t>,   op_shl<int32_t>,op_sar<int32_t>,
    /*[0x5C] =*/ op_jge<int32_t>, op_jne<int32_t>,  op_jnz<int32_t>,op_ret<float>,
    /*[0x60] =*/ op_tcall,        op_lda<double>,   op_sta<double>, op_ld<double>,
    /*[0x64] =*/ op_st<double>,   op_ldc<double>,   op_wide,        op_add<double>,
    /*[0x68] =*/ op_sub<double>,  op_mul<double>,   op_div<double>, op_neg<double>,
    /*[0x6C] =*/ op_jl<double>,   op_jg<double>,    op_jgz<int64_t>,op_call,
    /*[0x70] =*/ op_dup2<int64_t>,op_drop2<int64_t>,op_tol<double>, op_tof<double>,
    /*[0x74] =*/ op_toi<double>,  op_hlt,           op_hlt,         op_dec<int64_t>,
    /*[0x78] =*/ op_not<int64_t>, op_or<int64_t>,   op_shl<int64_t>,op_sar<int64_t>,
    /*[0x7C] =*/ op_jge<int64_t>, op_jne<int64_t>,  op_jnz<int64_t>,op_ret<double>
};

void vm_state::eval(const std::string &fun) {
    extern op_func opcodes[std::numeric_limits<uint8_t>::max()];

    strtab_index id = object.id(std::string(fun));
    auto it = object.function_indices.find(id);
    if (it == object.function_indices.end())
        throw std::invalid_argument("Function not exists");

    invoke(it->second);
    while (ip) {
        uint8_t op = *ip++;
        opcodes[op](*this, ip);
    }
}

static void print_i(vm_state &vm) {
    std::cout << vm.pop<int32_t>() << std::endl;
}

static void scan_i(vm_state &vm) {
    int32_t i{};
    std::cin >> i;
    vm.push(i);
}

static void print_d(vm_state &vm) {
    std::cout << vm.pop<double>() << std::endl;
}

static void scan_d(vm_state &vm) {
    double i{};
    std::cin >> i;
    vm.push(i);
}

static void sqrt_d(vm_state &vm) {
    double sq = std::sqrt(vm.pop<double>());
    vm.push(sq);
}

static void log_d(vm_state &vm) {
    double sq = std::sqrt(vm.pop<double>());
    vm.push(sq);
}
static void power_d(vm_state &vm) {
    double p = vm.pop<double>();
    double sq = std::pow(vm.pop<double>(), p);
    vm.push(sq);
}

vm_state::vm_state(std::size_t stack_size, std::string path) {
    std::ifstream fstr(path);
    if (!fstr.is_open()) {
        std::cerr << "Cannot open file " << path << std::endl;
        throw std::invalid_argument("Cannot open input file");
    }

    stack.resize(stack_size*sizeof(uint32_t));
    object.read(fstr);
    sp = &*stack.end();

    auto defnative = [&](native_function f, const char *sig, const char *str) {
        auto pidx = object.function_indices.find(object.id(str));
        if (pidx != object.function_indices.end()) {
            if (object.functions[pidx->second].signature != sig)
                throw std::logic_error("Native function interface violation");
            object.functions[pidx->second].native_function = reinterpret_cast<void *>(f);
        }
    };

    defnative(print_i, "(i)", "print_i");
    defnative(scan_i, "()i", "scan_i");
    defnative(print_d, "(d)", "print_d");
    defnative(scan_d, "()d", "scan_d");
    defnative(sqrt_d, "(d)d", "sqrt_d");
    defnative(log_d, "(d)d", "log_d");
    defnative(power_d, "(dd)d", "power_d");

    for (function &fn : object.functions)
        if (!fn.code.size())
            throw std::logic_error("Unbound native function");
}

int main(int argc, char **argv) {
    if (argc < 2 || (argc > 1 && !std::strcmp(argv[1], "-h"))) {
        std::cout << "Usage:\n" << std::endl;
        std::cout << "\t" << argv[0] << " <object>" << std::endl;
        return 0;
    }

    // TODO Specify stack size via option
    vm_state vm(stack_size, argv[1]);

    /* By default 'main' function in file is called */
    vm.eval("main");

    return 0;
}
