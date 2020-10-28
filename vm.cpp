#include "vm.hpp"
#include "util.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace vm {
    class vmstate {
        std::vector<uint8_t> stack;
        std::vector<uint8_t> memory;

        object_file object;

        uint8_t *sp{}; /* stack pointer register */
        uint8_t *fp{}; /* frame pointer register */

        function *ip_fun{};
        const uint8_t *ip{}; /* instruction pointer register */
        bool wide_flag; /* wide prefix is active */

    public:
        void eval(const std::string &fun);

        vmstate(std::size_t stack_size, std::string path) : stack(stack_size * sizeof(uint32_t)) {
            std::ifstream fstr(path);
            object.read(fstr);
            sp = &*stack.end();
        }
        vmstate(const vmstate &) = delete;
        vmstate &operator=(const vmstate &)= delete;

        void swap(vmstate &other) noexcept {
            std::swap(stack, other.stack);
            std::swap(memory, other.memory);
            std::swap(sp, other.sp);
            std::swap(fp, other.fp);
            std::swap(ip, other.ip);
            std::swap(ip_fun, other.ip_fun);
            std::swap(wide_flag, other.wide_flag);
            object.swap(other.object);
        }

        template<typename T>
        std::enable_if_t<std::is_scalar<T>::value, T> pop() {
            T tmp = util::read_next<T>(ip);
            return tmp;
        }
        template<typename T>
        std::enable_if_t<std::is_scalar<T>::value, T> top() {
            return util::read_at<T>(ip);
        }
        template<typename T>
        std::enable_if_t<std::is_scalar<T>::value> push(T value) {
            util::write_prev(sp, value);
        }

        template<typename T>
        std::enable_if_t<std::is_scalar<T>::value, T> get_local(int32_t n) {
            return util::read_at<T>(&*fp + n*sizeof(int32_t) + (n >= 0 ? 3*sizeof(void*) : -1*sizeof(uint32_t)));
        }
        template<typename T>
        std::enable_if_t<std::is_scalar<T>::value> set_local(std::int32_t n, T value) {
            util::write_at<T>(&*fp + n*sizeof(int32_t) + (n >= 0 ? 3*sizeof(void*) : -1*sizeof(uint32_t)), value);
        }
        template<typename T>
        std::enable_if_t<std::is_scalar<T>::value, T> get_global(uint32_t n) {
            return util::read_at<T>(memory.data() + n*sizeof(uint32_t));
        }
        template<typename T>
        std::enable_if_t<std::is_scalar<T>::value> set_global(uint32_t n, T value) {
            util::write_at<T>(memory.data() + n*sizeof(uint32_t), value);
        }

        void set_wide() {
            wide_flag = true;
        }

        bool get_wide() {
            bool w = wide_flag;
            wide_flag = false;
            return w;
        }

        function &cur_function() {
            return *ip_fun;
        }

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

        void jump(int32_t arg) {
            ip += arg;
        }

        void halt() {
            ip = nullptr;
        }

    };
}

namespace vm {
    using op_func = void(*)(vmstate &vm, const uint8_t *&ip);

    constexpr double eps = 1e-6;

    /* VM opcodes */

    void op_wide(vmstate &vm, const uint8_t *&ip) {
        vm.set_wide();
    }
    template <typename T> void op_add(vmstate &vm, const uint8_t *&ip) {
        T arg = vm.pop<T>();
        vm.push<T>(arg + vm.pop<T>());
    }
    template <typename T> void op_inc(vmstate &vm, const uint8_t *&ip) {
        vm.push<T>(vm.pop<T>() + util::read_either<T, uint8_t>(ip, vm.get_wide()));
    }
    template <typename T> void op_dec(vmstate &vm, const uint8_t *&ip) {
        vm.push<T>(vm.pop<T>() - util::read_either<T, uint8_t>(ip, vm.get_wide()));
    }
    template <typename T> void op_sub(vmstate &vm, const uint8_t *&ip) {
        T arg = vm.pop<T>();
        vm.push<T>(arg - vm.pop<T>());
    }
    template <typename T> void op_mul(vmstate &vm, const uint8_t *&ip) {
        T arg = vm.pop<T>();
        vm.push<T>(arg * vm.pop<T>());
    }
    template <typename T> void op_div(vmstate &vm, const uint8_t *&ip) {
        T a = vm.pop<T>();
        T b = vm.pop<T>();
        // This should work for both integers and floats
        if (b <= T(eps)) throw std::invalid_argument("Divide by zero");
        vm.push<T>(a / b);
    }
    template <typename T> void op_rem(vmstate &vm, const uint8_t *&ip) {
        T a = vm.pop<T>();
        T b = vm.pop<T>();
        if (b <= T(eps)) throw std::invalid_argument("Divide by zero");
        vm.push<T>(a % b);
    }
    template <typename T> void op_neg(vmstate &vm, const uint8_t *&ip) {
        vm.push<T>(-vm.pop<T>());
    }
    template <typename T> void op_and(vmstate &vm, const uint8_t *&ip) {
        (void)ip;
        auto arg = vm.pop<std::make_unsigned_t<T>>();
        vm.push<T>(arg & vm.pop<std::make_unsigned_t<T>>());
    }
    template <typename T> void op_or(vmstate &vm, const uint8_t *&ip) {
        auto arg = vm.pop<std::make_unsigned_t<T>>();
        vm.push<T>(arg | vm.pop<std::make_unsigned_t<T>>());
    }
    template <typename T> void op_xor(vmstate &vm, const uint8_t *&ip) {
        auto arg = vm.pop<std::make_unsigned_t<T>>();
        vm.push<T>(arg ^ vm.pop<std::make_unsigned_t<T>>());
    }
    template <typename T> void op_shr(vmstate &vm, const uint8_t *&ip) {
        auto arg = vm.pop<std::make_unsigned_t<T>>();
        vm.push<T>(arg >> vm.pop<uint32_t>());
    }
    template <typename T> void op_shl(vmstate &vm, const uint8_t *&ip) {
        auto arg = vm.pop<std::make_unsigned_t<T>>();
        vm.push<T>(arg << vm.pop<uint32_t>());
    }
    template <typename T> void op_sar(vmstate &vm, const uint8_t *&ip) {
        // That is technically UB for until C++20, but works fine in practice
        auto arg = vm.pop<std::make_signed_t<T>>();
        vm.push<T>(arg >> vm.pop<int32_t>());
    }
    template <typename T> void op_not(vmstate &vm, const uint8_t *&ip) {
        vm.push<T>(~vm.pop<T>());
    }
    template <typename T> void op_jl(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        T arg = vm.pop<T>();
        if (vm.pop<T>() < arg) vm.jump(disp);
    }
    template <typename T> void op_jg(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        T arg = vm.pop<T>();
        if (vm.pop<T>() > arg) vm.jump(disp);
    }
    template <typename T> void op_jle(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        T arg = vm.pop<T>();
        if (vm.pop<T>() <= arg) vm.jump(disp);
    }
    template <typename T> void op_jge(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        T arg = vm.pop<T>();
        if (vm.pop<T>() >= arg) vm.jump(disp);
    }
    template <typename T> void op_je(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        T arg = vm.pop<T>();
        if (vm.pop<T>() == arg) vm.jump(disp);
    }
    template <typename T> void op_jne(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        T arg = vm.pop<T>();
        if (vm.pop<T>() != arg) vm.jump(disp);
    }
    template <typename T> void op_jz(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        if (!vm.pop<T>()) vm.jump(disp);
    }
    template <typename T> void op_jnz(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        if (vm.pop<T>()) vm.jump(disp);
    }
    template <typename T> void op_jlz(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        if (vm.pop<T>() < T{}) vm.jump(disp);
    }
    template <typename T> void op_jlez(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        if (vm.pop<T>() <= T{}) vm.jump(disp);
    }
    template <typename T> void op_jgz(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        if (vm.pop<T>() > T{}) vm.jump(disp);
    }
    template <typename T> void op_jgez(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        if (vm.pop<T>() >= T{}) vm.jump(disp);
    }
    void op_jmp(vmstate &vm, const uint8_t *&ip) {
        ptrdiff_t disp = util::read_either<int16_t>(ip, vm.get_wide());
        vm.jump(disp);
    }
    template <typename T> void op_toi(vmstate &vm, const uint8_t *&ip) {
        vm.push<int32_t>(vm.pop<T>());
    }
    template <typename T> void op_tol(vmstate &vm, const uint8_t *&ip) {
        vm.push<int64_t>(vm.pop<T>());
    }
    template <typename T> void op_tof(vmstate &vm, const uint8_t *&ip) {
        vm.push<float>(vm.pop<T>());
    }
    template <typename T> void op_tod(vmstate &vm, const uint8_t *&ip) {
        vm.push<double>(vm.pop<T>());
    }
    template <typename T> void op_dup(vmstate &vm, const uint8_t *&ip) {
        vm.push<T>(vm.top<T>());
    }
    template <typename T> void op_dup2(vmstate &vm, const uint8_t *&ip) {
        auto a = vm.pop<T>();
        auto b = vm.top<T>();
        vm.push<T>(a);
        vm.push<T>(b);
        vm.push<T>(a);
    }
    template <typename T> void op_drop(vmstate &vm, const uint8_t *&ip) {
        (void)vm.pop<T>();
    }
    template <typename T> void op_drop2(vmstate &vm, const uint8_t *&ip) {
        (void)vm.pop<T>();
        (void)vm.pop<T>();
    }
    void op_call(vmstate &vm, const uint8_t *&ip) {
        vm.invoke(util::read_either<uint16_t,uint8_t>(ip, vm.get_wide()));
    }
    void op_tcall(vmstate &vm, const uint8_t *&ip) {
        // TODO
        throw std::logic_error("Not implemented");
    }
    template <typename T> void op_ret(vmstate &vm, const uint8_t *&ip) {
        T res = vm.pop<T>();
        vm.ret();
        vm.push<T>(res);
    }
    template <> void op_ret<void>(vmstate &vm, const uint8_t *&ip) {
        vm.ret();
    }
    template <typename T> void op_ld(vmstate &vm, const uint8_t *&ip) {
        vm.push<T>(vm.get_global<T>(util::read_either<uint16_t,uint8_t>(ip, vm.get_wide())));
    }
    template <typename T> void op_lda(vmstate &vm, const uint8_t *&ip) {
        vm.push<T>(vm.get_local<T>(util::read_either<int16_t>(ip, vm.get_wide())));
    }
    template <typename T> void op_st(vmstate &vm, const uint8_t *&ip) {
        vm.set_global(util::read_either<uint16_t,uint8_t>(ip, vm.get_wide()), vm.pop<T>());
    }
    template <typename T> void op_sta(vmstate &vm, const uint8_t *&ip) {
        vm.set_local(util::read_either<int16_t>(ip, vm.get_wide()), vm.pop<T>());
    }
    template <typename T> void op_ldi(vmstate &vm, const uint8_t *&ip) {
        vm.push(util::read_either<int16_t>(ip, vm.get_wide()));
    }
    template <typename T> void op_ldc(vmstate &vm, const uint8_t *&ip) {
        vm.push(util::read_next<T>(ip));
    }
    void op_hlt(vmstate &vm, const uint8_t *&ip) {
        vm.halt();
    }

    /* Thats sad that c++ does not support disignated array initializers... */
    op_func opcodes[std::numeric_limits<uint8_t>::max()] {
        /*[0x00] =*/ op_hlt,          op_lda<int32_t>,  op_sta<int32_t>,op_ld<int32_t>,
        /*[0x04] =*/ op_st<int32_t>,  op_ldc<int32_t>,  op_ldi<int32_t>,op_add<int32_t>,
        /*[0x08] =*/ op_sub<int32_t>, op_mul<int32_t>,  op_div<int32_t>,op_neg<int32_t>,
        /*[0x0C] =*/ op_jl<int32_t>,  op_jg<int32_t>,   op_jlz<int32_t>,op_call,
        /*[0x10] =*/ op_dup<int32_t>, op_drop<int32_t>, op_tol<int32_t>,op_tof<int32_t>,
        /*[0x14] =*/ op_tod<int32_t>, op_hlt,           op_hlt,         op_inc<int32_t>,
        /*[0x18] =*/ op_rem<int32_t>, op_and<int32_t>,  op_shr<int32_t>,op_xor<int32_t>,
        /*[0x1C] =*/ op_jle<int32_t>, op_je<int32_t>,   op_jz<int32_t>, op_ret<int32_t>,
        /*[0x20] =*/ op_jmp,          op_lda<int64_t>,  op_sta<int64_t>,op_ld<int64_t>,
        /*[0x24] =*/ op_st<int64_t>,  op_ldc<int64_t>,  op_ldi<int64_t>,op_add<int64_t>,
        /*[0x28] =*/ op_sub<int64_t>, op_mul<int64_t>,  op_div<int64_t>,op_neg<int64_t>,
        /*[0x2C] =*/ op_jl<int64_t>,  op_jg<int64_t>,   op_jlz<int64_t>,op_call,
        /*[0x30] =*/ op_dup<int64_t>, op_drop<int64_t>, op_toi<int64_t>,op_tof<int64_t>,
        /*[0x34] =*/ op_tod<int64_t>, op_hlt,           op_hlt,         op_inc<int64_t>,
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

    void vmstate::eval(const std::string &fun) {
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
}

