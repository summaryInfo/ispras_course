#pragma once

#include <type_traits>
#include <cstdint>
#include <cstring>
#include <vector>

namespace util {
    /* Read next unalligned item if type T */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value, T> read_at(const uint8_t *addr) {
        T tmp{};
        std::memcpy(&tmp, addr, sizeof(T));
        return tmp;
    }

    /* Read next unalligned item if type T and increment address */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value, T> read_next(const uint8_t *&addr) {
        T tmp = read_at<T>(addr);
        addr += sizeof(T);
        return tmp;
    }

    /* Move to previous item read it */
    template<typename T>
    std::enable_if_t<std::is_scalar<T>::value, T> read_prev(const uint8_t *&addr) {
        addr -= sizeof(T);
        return read_at<T>(addr);
    }

    /* Write next unalligned item if type T and increment address */
    template<typename T, typename It>
    std::enable_if_t<std::is_scalar<T>::value> write_at(It addr, T value) {
        std::memcpy(&*addr, &value, sizeof(T));
    }

    /* Move to previous item write it */
    template<typename T, typename It>
    std::enable_if_t<std::is_scalar<T>::value> write_prev(It &addr, T value) {
        write_at(addr, value);
        addr += sizeof(T);
    }

    /* Read next either of types T and U */
    template<typename T, typename U = int8_t>
    std::enable_if_t<std::is_scalar<T>::value, T> read_either(const uint8_t *&addr, bool wide) {
        return wide ? read_next<T>(addr) : read_next<U>(addr);
    }

    /* Put value in native byte order to code vector */
    template<typename T>
    static void vec_put_native(std::vector<uint8_t> &vec, T value) {
        vec.resize(vec.size() + sizeof value);
        std::memcpy(&*vec.end() - sizeof value, &value, sizeof value);
    }
}


