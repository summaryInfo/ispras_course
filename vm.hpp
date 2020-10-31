#pragma once

#include "util.hpp"
#include "ofile.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

using native_function = void(*)(class vm_state&);

/** Virtual machine state */
class vm_state {
    std::vector<uint8_t> stack;
    std::map<uint32_t, native_function> nfunc;

    // TODO For now globals array serves the purpose of memory
    /* std::vector<uint8_t> memory; */

    object_file object; /* Object code we evaluating */

    uint8_t *sp{}; /* stack pointer register */
    uint8_t *fp{}; /* frame pointer register */

    function *ip_fun{};
    const uint8_t *ip{}; /* instruction pointer register */
    bool wide_flag{}; /* wide prefix is active */


public:
    /**
     * Evaluate a function call.
     * Arguments correctness is not checked.
     */
    void eval(const std::string &fun);

    vm_state(std::size_t stack_size, std::string path);

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
        T tmp = util::read_next<T>(sp);
        std::cout << "->" << tmp << std::endl;
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
        return util::read_at<T>(sp);
    }

    /**
     * Push a value to the stack
     *
     * @param[in] value pushed value
     */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value> push(T value) {
        std::cout << "<-" << value << std::endl;
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
        if (object.functions[idx].code.size()) {
            push<function *>(ip_fun);
            push<const uint8_t *>(ip);
            push<uint8_t *>(*&fp);

            fp = sp;
            ip_fun = &object.functions[idx];
            ip = object.functions[idx].code.data();
            /* Allocate space on stack for locals */
            auto fr_size = object.functions[idx].frame_size;
            sp -= fr_size;
            /* Frame should be initially zeroed*/
            std::memset(sp, 0, fr_size);
        } else {
            /* This is native function */
            auto nf = nfunc.find(idx);
            if (nf == nfunc.end())
                throw std::logic_error("Undefined native function");
            nf->second(*this);
        }
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
        ip_fun = pop<function *>();

        /* Arguments are popped from stack
         * by caller like in stdcall
         * (and no support for vargargs) */
        sp += args;
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

