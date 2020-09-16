
#define _POSIX_C_SOURCE 200809L

#include "file_mapping.hpp"
#include "sort.hpp"
#include "unit.hpp"

#include <algorithm>
#include <clocale>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <langinfo.h>
#include <locale.h>
#include <unistd.h>
#include <utility>
#include <vector>

/** Indication that current encoding is UTF-8 */
static bool utf8;

/**
 * Print usage and exit.
 *
 * @param [in] argv0 program path.
 * @param [in] code exit code.
 */
[[noreturn]] static void usage(char *argv0, int code) {
    assert(argv0);

    std::cerr << "Usage:\n"
              << "    " << argv0 << " infile [outfile1 [outfile2 [outfile3]]]\n\n"
              << "where\toutfile1 is file with lines sorted when compared forward,\n"
              << "\toutfile2 is file with lines sorted when compared backward,\n"
              << "\t\tdefault is 'forward.txt'\n"
              << "\toutfile3 is file with original contents of file\n"
              << "\t\tdefault is 'backward.txt'\n"
              << "\toutfile3 is file with original contents of file\n"
              << "\t\tdefault is 'original.txt'" << std::endl;
#ifndef NDEBUG
    std::cerr << "\nOr alternatively:\n"
              << "    " << argv0 << " test\n\n"
              << "to perform tests." << std::endl;
#endif // !defined(NDEBUG)

    std::exit(code);
}

/**
 * Write lines sorted when comparing starting from the begginning of each lines
 *
 * @param[in] path output file
 * @param[inout] lines vector of parsed lines
 */
void write_forward(const char *path, std::vector<std::pair<const char *, const char *>> &lines) {
    algorithms::quick_sort(std::begin(lines), std::end(lines), [](auto lhs, auto rhs) -> bool {

        // Read characters forward
        auto next_char = [](const char *&str, const char *end) -> wchar_t {
            wchar_t ch = *(unsigned char *)str;
            int len = 1;
            if (utf8)
                len = std::mbtowc(&ch, str, end - str);
            str += std::max(len, 1);
            return ch;
        };

        auto is_alpha = [](wchar_t ch) -> bool {
            // isalpha is defined only for ASCII, so check manually
            // Consider any single byte character that is not ascii to be a letter
            return utf8 ? std::iswalpha(ch) : (ch > 127 || std::isalpha(ch));
        };

        wchar_t lchar{}, rchar{};
        do {
            do lchar = next_char(lhs.first, lhs.second);
            while (lchar && !is_alpha(lchar));
            do rchar = next_char(rhs.first, rhs.second);
            while (rchar && !is_alpha(rchar));
        } while(rchar == lchar && lchar);
        return lchar < rchar;
    });

    std::fstream str(path, std::ios_base::out);
    for (auto &line : lines)
        str << line.first << std::endl;
}

/**
 * Write lines sorted when comparing starting from the end of each lines
 *
 * @param[in] path output file
 * @param[inout] lines vector of parsed lines
 */
void write_backward(const char *path, std::vector<std::pair<const char *, const char *>> &lines) {
    algorithms::quick_sort(std::begin(lines), std::end(lines), [](auto lhs, auto rhs) -> bool {

        // Read characters backwards
        auto next_char = [](const char *start, const char *&str) -> wchar_t {

            // Go back to previous character
            // (for single byte encodings and UTF-8)
            do {
                if (str == start) return 0;
                str--;
            } while(utf8 && (*str & 0xC0U) == 0x80U);

            // And return it
            wchar_t ch = *(unsigned char *)str;
            if (utf8)
                std::mbtowc(&ch, str, MB_CUR_MAX);
            return ch;
        };

        auto is_alpha = [](wchar_t ch) -> bool {
            // isalpha is defined only for ASCII, so check manually
            // Consider any single byte character that is not ascii to be a letter
            return utf8 ? std::iswalpha(ch) : (ch > 127 || std::isalpha(ch));
        };

        wchar_t lchar{}, rchar{};
        do {
            do lchar = next_char(lhs.first, lhs.second);
            while (lchar && !is_alpha(lchar));
            do rchar = next_char(rhs.first, rhs.second);
            while (rchar && !is_alpha(rchar));
        } while(rchar == lchar && lchar);
        return lchar < rchar;
    });

    std::fstream str(path, std::ios_base::out);
    for (auto &line : lines)
        str << line.first << std::endl;
}

/**
 * Write text split to lines as its original representation
 *
 * @param[in] path output file
 * @param[in] data text start
 * @param[in] end text end
 */
void write_original(const char *path, const char *data, const char *end) {
    std::fstream str(path, std::ios_base::out);
    for (bool new_line = true; data < end; data++) {
        if (new_line)
            str << data << std::endl;
        new_line = !*data;
    }
}

/**
 * Split text into lines
 *
 * @param[inout] data text start
 * @param[inout] end text end
 *
 * @return vector of pairs of pointer to lines ends and starts
 */
auto split_lines(char *data, char *end) {
    std::vector<std::pair<const char *, const char *>> lines;

    lines.emplace_back(data, nullptr);
    for (; data < end; data++) {
        if (*data == '\n' || !*data) {
            *data = '\0';
            if (lines.size())
                lines.back().second = data;
            if (data != end - 1)
                lines.emplace_back(data + 1, nullptr);
        }
    }
    if (lines.size())
        lines.back().second = end - !end[-1];

    return lines;
}

#ifndef NDEBUG
namespace tests {
    /**
     * Test line splitting and file mapping for given contents
     *
     * @param[in] contents
     *
     * @return zero if passed or else index of failed check
     * */
    inline static int do_test(const char *contents) {
        {
            std::fstream str("test_data_1", std::ios_base::out);
            str << contents;
        }

        ::file_mapping mapping("test_data_1");
        if (!mapping.is_valid()) return 1;

        auto start = mapping.as<char>();
        auto end = start + mapping.size<char>();
        auto lines = split_lines(start, end);

        write_original("test_data_2", start, end);

        {
            std::fstream str("test_data_3", std::ios_base::out);
            for (auto &line : lines) {
                if (!line.first) return 2;
                if (!line.second) return 3;
                if (strlen(line.first) + line.first != line.second) return 4;

                str << line.first << std::endl;
            }
        }

        ::file_mapping data_1("test_data_1");
        if (!data_1.is_valid()) return 5;

        ::file_mapping data_2("test_data_2");
        if (!data_2.is_valid()) return 6;

        ::file_mapping data_3("test_data_3");
        if (!data_3.is_valid()) return 7;

        if (data_1.size<char>() != data_2.size<char>()) return 8;
        if (data_1.size<char>() != data_3.size<char>()) return 9;

        if (std::memcmp(data_1.as<void>(), data_2.as<void>(), data_1.size<char>())) return 10;
        if (std::memcmp(data_1.as<void>(), data_3.as<void>(), data_1.size<char>())) return 11;

        return 0;
    }

    /**
     * Test main program logic:
     *    - file mapping
     *    - line splitting
     *    - original contens writing
     */
    void test_comparators() {
        UNITS_BEGIN;

        UNIT(do_test("a\nb\nc\nd\n"), 0);
        UNIT(do_test("\nazzz\nааа\nббб\n"), 0);
        UNIT(do_test("\nавп\nааа\nббб\nавпы\nfgdfg\nкне авыаы\n"), 0);

        const char *str =
            "jx\nnp\nni\naw\nei\nqi\ntb\nzy\npc\nmg\nac\nyh\nir\nio\nnc\n"
            "qp\ndz\nrw\nlr\nja\nnt\nxo\nxb\nbb\nlc\nef\npm\nif\noy\ntn\n"
            "xd\nsy\nfm\nwn\npg\ncb\nzm\nqo\npi\nhl\naa\nul\nvw\nrk\nmu\n"
            "re\nrr\ncz\ndf\nyd\nkc\nbx\not\ncx\nfe\nto\ndq\nsj\nlh\ngl\n"
            "nv\nos\nke\nev\nop\ntx\nse\nuq\nvh\nnq\nrm\nkv\ntj\nms\ntr\n"
            "wk\nrb\ntk\nay\nbi\nwt\nau\nat\nfj\ngr\nld\nob\njp\ngs\nkb\n"
            "zq\nzw\nkp\nqa\nbk\ncv\nzx\nmb\nua\ncd\n";
        UNIT(do_test(str), 0);

        UNITS_END;


        unlink("test_data_1");
        unlink("test_data_2");
        unlink("test_data_3");
    }
}
#endif

/** Main function */
int main(int argc, char *argv[]) {

    // Ensure that wchar_t is Unicode
    static_assert(__STDC_ISO_10646__);

    // Set locale from environment
    std::setlocale(LC_CTYPE, "");

    // Check if current locale encoding is UTF-8
    // We need to compare charset that way since
    // charset representation is not standardized
    // So accept "UTF-8" "UTF_8" "UTF8" in any case
    const char *charset = nl_langinfo(CODESET);
    utf8 = charset &&
          (std::tolower(charset[0]) == 'u') &&
          (std::tolower(charset[1]) == 't') &&
          (std::tolower(charset[2]) == 'f') &&
          (charset[3] == '8' || charset[4] == '8');

    if (!utf8 && MB_CUR_MAX > 1) {
        std::cerr << "Non-UTF-8 multibyte encodings are not supported" << std::endl;
        return EXIT_FAILURE;
    }

#ifndef NDEBUG
    // If the only argument of program is 'test', perform tests
    if (argc == 2 && !std::strcmp(argv[1], "test")) {
        tests::test_file_mapping();
        tests::test_small_sort();
        tests::test_sort_partition();
        tests::test_quick_sort();
        tests::test_comparators();
        return EXIT_SUCCESS;
    }
#endif // !defined(NDEBUG)

    if (argc < 2)
        usage(argv[0], EXIT_FAILURE);

    file_mapping mapping(argv[1]);
    if (!mapping.is_valid()) {
        std::cerr << "Invalid file: " << argv[1] << std::endl;
        usage(argv[0], EXIT_FAILURE);
    }

    char *data = mapping.as<char>();
    char *end = data + mapping.size<char>();
    auto lines = split_lines(data, end);

    write_forward(argc > 2 ? argv[2] : "forward.txt", lines);
    write_backward(argc > 3 ? argv[3] : "backward.txt", lines);
    write_original(argc > 4 ? argv[4] : "original.txt", data, end);

    return EXIT_SUCCESS;
}