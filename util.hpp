#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace util {
    /** Read next unalligned item if type T
     *
     *  @param [in] addr address to read from
     *
     *  @return read value
     */
    template<typename T, typename It>
    std::enable_if_t<std::is_scalar<T>::value, T> read_at(It addr) {
        T tmp{};
        std::memcpy(&tmp, &*addr, sizeof(T));
        return tmp;
    }

    /** Read next unalligned item if type T and increment address
     *
     *  @param [inout] addr address to read from
     *
     *  @return read value
     */
    template<typename T, typename It>
    std::enable_if_t<std::is_scalar<T>::value, T> read_next(It &addr) {
        T tmp = read_at<T>(addr);
        addr += sizeof(T);
        return tmp;
    }

    /** Move to previous item read it
     *
     *  @param [inout] addr address to read from
     *
     *  @return read value
     */
    template<typename T, typename It>
    std::enable_if_t<std::is_scalar<T>::value, T> read_prev(It &addr) {
        addr -= sizeof(T);
        return read_at<T>(addr);
    }

    /** Write next unalligned item if type T and increment address
     *
     *  @param [inout] addr address to write to
     */
    template<typename T, typename It>
    std::enable_if_t<std::is_scalar<T>::value> write_at(It addr, T value) {
        std::memcpy(&*addr, &value, sizeof(T));
    }

    /** Move to previous item write it
     *
     *  @param [inout] addr address to write to
     */
    template<typename T, typename It>
    std::enable_if_t<std::is_scalar<T>::value> write_prev(It &addr, T value) {
        addr -= sizeof(T);
        write_at(addr, value);
    }

    /** Read next either of types T and U
     *
     *  @param [inout] addr address to read from
     *  @param [in] wide width selector
     *
     *  @return read value
     */
    template<typename T, typename U = int8_t, typename It>
    std::enable_if_t<std::is_scalar<T>::value, decltype(T{}+U{})> read_either(It &addr, bool wide) {
        return wide ? read_next<T>(addr) : read_next<U>(addr);
    }

    /** Read operand of types T and U and reset wide flag
     *
     *  @param [inout] addr address to read from
     *  @param [in] wide width selector
     *
     *  @return read value
     */
    template<typename T, typename U = int8_t, typename It>
    std::enable_if_t<std::is_scalar<T>::value, decltype(T{}+U{})> read_im(It &addr, bool &wide) {
        auto res = read_either<T, U>(addr, wide);
        wide = false;
        return res;
    }

    /** Put value in native byte order to code vector
     *
     * @param [inout] vec vector to append
     * @param [in] value data to write
     */
    template<typename T>
    void vec_put_native(std::vector<uint8_t> &vec, T value) {
        vec.resize(vec.size() + sizeof value);
        std::memcpy(&*vec.end() - sizeof value, &value, sizeof value);
    }

    inline static std::string swap_ext(std::string path, const std::string &oldx, const std::string &newx) {
        if (path.size() > oldx.size() && !path.compare(path.size() -
               oldx.size(), oldx.size(), oldx)) {
            path = path.substr(0, path.size() - oldx.size());
        }
        return std::move(path) + newx;
    }
}
