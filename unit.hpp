/**
 * @file unit.hpp
 *
 * @brief Unit testing helper code
 *
 * UNIT testing helper
 * macros + some terminal control
 * sequences wrappers used by those macros.
 *
 * @authors summaryInfo
 * @date Sep 14 2020
 */
#pragma once

#include <iostream>
#include <sstream>

namespace term {

    /** Special value indicating that coordinate should stay unchanged */
    constexpr inline static int coord_unchanged = 1;

    /**
     * Move to absolute coordinates string
     *
     * @param[in] x new column starting from 0
     * @param[in] y new row starting from 0
     *
     * @note use coord_unchanged to keep coordinate the same as it was
     */
    inline static std::string move_to(int x = coord_unchanged, int y = coord_unchanged) {
        std::stringstream ss;
        if (x != coord_unchanged) {
            if (y != coord_unchanged)
                ss << "\033[" << y + 1 << x + 1 << "H";
            else
                ss << "\033[" << x + 1 << "G";
        } else if (y != coord_unchanged) {
            ss << "\033[" << y + 1 << "d";
        }
        return ss.str();
    }

    /**
     * Select bold attribute string
     *
     * @param[in] set Enable/disable flag
     *
     * @note on disable actually also diables faint
     * */
    inline static std::string bold(bool set) {
        return set ? "\033[1m" : "\033[21m";
    }

    /**
     * Terminal colors
     */
    enum class color {
        black,
        red,
        green,
        blue,
        yellow,
        magenta,
        cyan,
        white,
        reset,
    };

    /**
     * Select terminal text color string
     *
     * @param[in] col New text color
     */
    inline static std::string foreground(enum color col) {
        std::stringstream ss;
        ss << "\033[" << static_cast<int>(col) + 30 << "m";
        return ss.str();
    }

    /**
     * Select terminal background color string
     *
     * @param[in] col New text color
     */
    inline static std::string background(enum color col) {
        std::stringstream ss;
        ss << "\033[" << static_cast<int>(col) + 40 << "m";
        return ss.str();
    }

    /**
     * Reset terminal renderition state string
     */
    inline static std::string reset_sgr() {
        return "\033[m";
    }
}

/**
 * Begin unit test group macro
 */
#define UNITS_BEGIN { int unit_count__ = 1; std::cerr << std::endl << "Test " << __func__ << "..." << std::endl

/**
 * End unit test group macro
 */
#define UNITS_END } while(false)

/**
 * Unit test macro
 *
 * @param [in] expr Expression to test
 * @param [in] val Expected value
 *
 * @note expr and val should be evaluated to
 *       objects with overloaded operator<< so they
 *       can be output to the stream
 * @note should be enclosed into UNITS_BEGIN; ... UNITS_END;
 */
#define UNIT(expr, val) do {\
        std::cerr << term::bold(true) << "\tTest " << unit_count__++ << term::reset_sgr() \
                  << ": (" #expr ") == (" #val ")..." << term::move_to(80); \
        if (auto result__ = (expr), expect__ = (val); result__ == expect__)\
            std::cerr << term::foreground(term::color::green) << "passed" << term::reset_sgr() << "." << std::endl;\
        else std::cerr << term::foreground(term::color::red) << "FAILED" << term::reset_sgr() << "." << std::endl\
             << term::foreground(term::color::red) << "\t\tEXPECTED" << term::reset_sgr() <<  ": " << expect__ << std::endl\
             << term::foreground(term::color::red) << "\t\tGOT" << term::reset_sgr()  << ":  " << result__ << std::endl;\
    } while(false)
