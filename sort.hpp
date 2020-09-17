/**
 * @file sort.hpp
 *
 * @brief Sorting algorithms
 *
 * This is an implementation
 * of optimized quick sort templated algorithm
 * that switches to insertion sort for small subarrays
 *
 * @authors summaryInfo
 * @date Sep 15 2020
 */

#pragma once

#include "unit.hpp"

#include <cassert>
#include <functional>
#include <iterator>
#include <random>
#include <type_traits>
#include <utility>

namespace algorithms {

    /** threshold used to deside when sort() switches to small_sort() */
    template <typename It> constexpr inline static typename std::iterator_traits<It>::difference_type threshold = 32;

    /**
     * Sort elements in range small range [first, last).
     *
     * Performs insertion sort.
     * Works faster for small arrays.
     *
     * @param[in] first lower bound iterator
     * @param[in] last higher bound iterator
     * @param[in] cmp comparator (optional)
     *
     * @note Used in sort() implementation
     * @note iterators should be random access
     * @note comparator type should be quivalent to bool(*)(const T&, const T&),
     *       where T is type of iterator element
     */
    template<typename It, typename Compare = std::less<typename std::iterator_traits<It>::value_type>>
    std::enable_if_t<std::is_same_v<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag> &&
                     std::is_convertible_v<decltype(std::declval<Compare>()(*std::declval<It>(), *std::declval<It>())), bool>>
    small_sort(It first, It last, Compare cmp = {}) {
        assert(first <= last);

        if (auto dist = last - first; dist > 2) {
            // Insertion sort for more than 2 elements
            for (auto sorted = first + 1; sorted < last; sorted++) {
                for (auto insert_to = sorted; cmp(insert_to[0], insert_to[-1]); insert_to--) {
                    std::swap(insert_to[-1], insert_to[0]);
                    if (insert_to == first + 1) break;
                }
            }
        } else if (dist == 2) {
            // Just swap two elements
            if (!cmp(first[0], first[1]))
                std::swap(first[0], first[1]);
        } /* else if (dist < 2) -- don't need to sort */
    }

    /**
     * Partition array.
     *
     * Splits array [low, high) on two parts using pivot element, such that:
     *     - pivot element is between lower and higher parts.
     *     - all elements in lower part are less than pivot.
     *     - all elements in higher part are greater than or equal to pivot.
     *
     * @param[in] low lower bound iterator
     * @param[in] high upper bound iterator
     * @param[in] cmp comparator (optional)
     *
     * @return iterator pointing to pivot element
     *
     * @note iterators should be random access
     * @note comparator type should be quivalent to bool(*)(const T&, const T&),
     *       where T is type of iterator element
     */
    template<typename It, typename Compare = std::less<typename std::iterator_traits<It>::value_type>>
    std::enable_if_t<std::is_same_v<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag> &&
                     std::is_convertible_v<decltype(std::declval<Compare>()(*std::declval<It>(), *std::declval<It>())), bool>, It>
    sort_partition(It low, It high, Compare cmp = {}) {
        // In this functions we have at least thresh_hold<It> elements

        // Use last element as a pivot point
        auto &pivot = *--high;

        for (auto it = low; it < high; it++)
            if (cmp(*it, pivot))
                std::swap(*low++, *it);
        std::swap(*low, pivot);

        return low;
    }

    /**
     * Sort elements in range range [first, last).
     *
     * Performs quick sort, switching to small_sort() for small subranges
     * for optimization.
     *
     * @param[in] first lower bound iterator
     * @param[in] last higher bound iterator
     * @param[in] cmp comparator (optional)
     *
     * @note iterators should be random access
     * @note comparator type should be quivalent to bool(*)(const T&, const T&),
     *       where T is type of iterator element
     */
    template<typename It, typename Compare = std::less<typename std::iterator_traits<It>::value_type>>
    std::enable_if_t<std::is_same_v<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag> &&
                     std::is_convertible_v<decltype(std::declval<Compare>()(*std::declval<It>(), *std::declval<It>())), bool>>
    quick_sort(It first, It last, Compare cmp = {}) {
        assert(first <= last);

        // Use quick sort until the array is small enough
        while (last - first >= threshold<It>) {
            auto part_point = sort_partition(first, last, cmp);

            quick_sort(first, part_point, cmp);
            first = part_point + 1;
        }

        // then call small_sort sort which is faster for small arrays
        small_sort(first, last, cmp);
    }
}

#ifndef NDEBUG
namespace tests {

    /**
     * Print integer vertor
     *
     * @param[inout] str stream to print to
     * @param[in] vec vector to print
     *
     * @return stream
     *
     * @note This is required for unit tests
     */
    std::ostream &operator <<(std::ostream &str, const std::vector<int> &vec) {
        str << "vector{ ";
        for (const auto &el : vec)
            str << el << ", ";
        str << "}";
        return str;
    }

    /** Test small_sort template */
    void test_small_sort() {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution dist(1, 1000);

        UNITS_BEGIN;

        for (size_t i = 0; i < 100; i++) {
            std::vector<int> vec, vec2;
            // Generate two copies of random vector
            for (size_t j = 0; j < i; j++)
                vec.emplace_back(dist(rng));
            vec2 = vec;
            std::sort(std::begin(vec), std::end(vec));
            algorithms::small_sort(std::begin(vec2), std::end(vec2));

            UNIT(vec2, vec);
        }

        UNITS_END;
    }

    /** Test sort_partition template */
    void test_sort_partition() {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution dist(1, 1000);

        UNITS_BEGIN;

        for (size_t i = 2; i < 100; i++) {
            std::vector<int> vec;
            // Generate two copies of random vector
            for (size_t j = 0; j < i; j++)
                vec.emplace_back(dist(rng));

            auto pivot = algorithms::sort_partition(std::begin(vec), std::end(vec));

            bool valid = true;

            for (auto it = std::begin(vec); valid && it != pivot; it++)
                valid &= *it < *pivot;
            for (auto it = pivot; valid && it != std::end(vec); it++)
                valid &= *it >= *pivot;

            UNIT(valid, true);
        }

        UNITS_END;
    }

    /** Test quick_sort template */
    void test_quick_sort() {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution dist(1, 1000);

        UNITS_BEGIN;

        for (size_t i = 0; i < 100; i++) {
            std::vector<int> vec, vec2;
            // Generate two copies of random vector
            for (size_t j = 0; j < i; j++)
                vec.emplace_back(dist(rng));
            vec2 = vec;
            std::sort(std::begin(vec), std::end(vec));
            algorithms::quick_sort(std::begin(vec2), std::end(vec2));

            UNIT(vec2, vec);
        }

        UNITS_END;
    }
}
#endif
