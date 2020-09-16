/**
 * @file sort.hpp
 *
 * @brief File mapping object implementation
 *
 * This is an implementation
 * file mapping wrapper object
 * on top of mmap interface
 *
 * @authors summaryInfo
 * @date Sep 15 2020
 */

#pragma once

#define _POSIX_C_SOURCE 200809L

#include "unit.hpp"

#include <cstring>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <random>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

class file_mapping {
    void *data{MAP_FAILED};
    size_t data_size{0};
public:
    // This is utility class, so lets make
    // every function inline

    /**
     * Swap two file mapping objects.
     *
     * @param[inout] other mapping to swap with
     */
    void swap(file_mapping &other) noexcept {
        std::swap(data, other.data);
        std::swap(data_size, other.data_size);
    }

    // Mapping cannot be copied
    // but it can be moved

    file_mapping(const file_mapping&) = delete;
    file_mapping &operator=(const file_mapping&) = delete;

    /**
     * Move construct file mapping.
     *
     * @param[inout] other mapping to borrow contents
     */
    file_mapping(file_mapping&& other) noexcept {
        this->swap(other);
    }

    /**
     * Move assign file mapping.
     *
     * @param[inout] other mapping to borrow contents
     */
    file_mapping &operator=(file_mapping&& other) noexcept {
        this->swap(other);
        return *this;
    }

    /**
     * Create file mapping from given file path.
     *
     * @param[in] path file to map
     *
     * @note creates invalid file mapping on error
     *       this should be checked before accessing to mapping data.
     */
    file_mapping(const char *path) noexcept {
        int fd = open(path, O_RDONLY);
        if (fd < 0) return;

        struct stat statbuf;
        if (fstat(fd, &statbuf) < 0) return;

        // + 1 needs for NUL byte at the end of file
        data_size = statbuf.st_size + 1;

        // Don't check success, since if we write MAP_FAILED
        // to data, it just would be invalid mapping representation
        data = mmap(nullptr, data_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

        if (data != MAP_FAILED) {
            // Ensure that data is NUL terminated
            static_cast<char *>(data)[data_size - 1] = 0;
        }

        close(fd);
    }

    /**
     * Unmap file contents on file_mapping destruction
     */
    ~file_mapping() noexcept {
        if (data != MAP_FAILED)
            munmap(data, data_size);
    }

    /**
     * Check if mapping is valid
     *
     * @return true iff mapping is valid
     */
    bool is_valid() const noexcept {
        return data != MAP_FAILED;
    }

    /**
     * Get representation of file data as array of type
     *
     * @return data pointer
     *
     * @throw domain_error if mapping is invalid
     */
    template<typename T> T *as() noexcept(false) {
        if (data == MAP_FAILED)
            throw std::domain_error("Cannot access data for invalid file mappings");
        return static_cast<T *>(data);
    }

    /**
     * Get mapping size
     *
     * @return size of mapped region in units of type T
     *
     * @throw domain_error if mapping is invalid
     */
    template<typename T> std::size_t size() noexcept(false) {
        static_assert(sizeof(T));
        if (data == MAP_FAILED)
            throw std::domain_error("Cannot access data for invalid file mappings");
        return (data_size - 1) / sizeof(T);
    }
};

#ifndef NDEBUG
namespace tests {
    /** Test file mapping class */
    void test_file_mapping() {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution dist_data(0, 255);
        std::uniform_int_distribution dist_size(0, 100000);

        UNITS_BEGIN;

        for (size_t i = 0; i < 100; i++) {
            std::vector<char> data;
            size_t sample_size = dist_size(rng);
            {
                data.reserve(sample_size);
                std::fstream str{"test_data_1", std::ios_base::out};
                for (size_t j = 0; j < sample_size; j++) {
                    char res = dist_data(rng);
                    str.put(res);
                    data.push_back(res);
                }
            }

            ::file_mapping mapping("test_data_1");

            UNIT(mapping.is_valid(), true);
            UNIT(mapping.size<char>(), sample_size);
            UNIT(std::memcmp(mapping.as<char>(), data.data(), sample_size), 0);
        }

        UNITS_END;

        unlink("test_data_1");
    }
};
#endif
