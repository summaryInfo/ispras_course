#include <cassert>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <type_traits>

namespace equation {

    constexpr static double eps = 1e-4; ///< Precision for equality checks

    /**
     * Checks if argument is close to zero with precision eps.
     *
     * @param [in] v number to check.
     * @return true if argument is close to zero.
     */
    inline static bool is_zero(double v) {
        return std::fabs(v) < eps;
    }

    /**
     * Solution holder class.
     */
    class solution {
        std::array<double, 2> data {}; ///< Exact solution values
    public:
        /**
         * Solution tag.
         *
         * Indicates how many solution are present.
         *
         * @note This is enum for type safety reasons.
         */
        enum class tag {
            none,
            one,
            two,
            infinite,
        } const tag; ///< Readonly solution tag

        /**
         * One exact solution.
         *
         * @param [in] v1 solution.
         */
        solution(double v1) : data{v1}, tag{tag::one} {}

        /**
         * Two exact solutions.
         *
         * @param [in] v1 solution 1.
         * @param [in] v2 solution 2.
         */
        solution(double v1, double v2) : data{v1, v2}, tag{tag::two} {}

        /**
         * Special case solution.
         *
         * Either none or infinite number of solutions.
         *
         * @param [in] any Indicates that any real nubmer is a valid solution otherwise there are no valid solutions.
         */
        solution(bool any) : tag{ any ? tag::infinite : tag::none } {}

        /**
         * First solution getter.
         *
         * @note Aborts program if there is no solutions or infinite number of them.
         * */
        double first() const {
            assert(tag == tag::one || tag == tag::two);
            return data[0];
        }

        /**
         * Second solution getter.
         *
         * @note Aborts program if there is no solutions, one solution or infinite number of them.
         * */
        double second() const {
            assert(tag == tag::two);
            return data[1];
        }

        /**
         * Compare two solutions with precision eps for equality.
         *
         * @param [in] other Solution to compare with.
         * @return true if solutions are equal.
         *
         * @note Aborts program if there is no solutions, one solution or infinite number of them.
         */
        bool operator==(const solution &other) const {
            if (other.tag != tag)
                return false;
            switch(tag) {
            case tag::two:
                if (!is_zero(other.data[1] - data[1]))
                    return false;
                [[fallthrough]];
            case tag::one:
                if (!is_zero(other.data[0] - data[0]))
                    return false;
                [[fallthrough]];
            case tag::none:
            case tag::infinite:
                break;
            }
            return true;
        }


        /**
         * Compare two solutions with precision eps for inequality.
         *
         * @param [in] other Solution to compare with.
         * @return true if solutions are not equal.
         *
         * @note Aborts program if there is no solutions, one solution or infinite number of them.
         */
        bool operator!=(const solution &other) {
            return !(*this == other);
        }
    };

    /**
     * Print solution object to output stream
     *
     * @param [in] str output stream
     * @param [in] val value to output
     */
    auto &operator <<(std::ostream &str, const solution &val) {
        switch(val.tag) {
        case solution::tag::none:
            str << "{}";
            break;
        case solution::tag::two:
            str << "{" << val.first() << ", " << val.second() << "}";
            break;
        case solution::tag::one:
            str << "{" << val.first() << "}";
            break;
        case solution::tag::infinite:
            str << "R";
        }

        return str;
    }

    /**
     * Print solution tag to output stream
     *
     * @param [in] str output stream
     * @param [in] val value to output
     *
     * @note This is used in unit tests
     */
    auto &operator <<(std::ostream &str, enum solution::tag val) {
        switch (val) {
        case solution::tag::none:
            return str << "tag::none";
            break;
        case solution::tag::two:
            return str << "tag::two";
            break;
        case solution::tag::one:
            return str << "tag::one";
            break;
        case solution::tag::infinite:
            return str << "tag::infinite";
            break;
        }
        assert(false);
    }

    /**
     * Solve linear equation k*x + b = 0.
     *
     * @param [in] k fist coeficient of equation.
     * @param [in] b second coeficient of equation.
     * @return solution object.
     *
     * @note Aborts program if either k or b is not finite number.
     */
    solution solve_linear(double k, double b) {
        assert(std::isfinite(k));
        assert(std::isfinite(b));

        if (is_zero(k))
            return {is_zero(b)};

        return {-b/k};
    }

    /**
     * Solve quadratic equation a*x^2 + b*x + c = 0.
     *
     * @param [in] a first coeficient of equation.
     * @param [in] b second coeficient of equation.
     * @param [in] c third coeficient of equation.
     * @return solution object.
     *
     * @note Aborts program if one of a, b or c is not finite number.
     */
    solution solve_quadratic(double a, double b, double c) {
        assert(std::isfinite(a));
        assert(std::isfinite(b));
        assert(std::isfinite(c));

        if (is_zero(a))
            return solve_linear(b, c);

        if (auto det = b*b - 4.*a*c; det >= 0.) {
            auto sqrt_det = std::sqrt(det);
            return {
                (-b - sqrt_det) / (2. * a),
                (-b + sqrt_det) / (2. * a),
            };
        }

        return {false};
    }
}

#ifndef NDEBUG
/** Unit test namespace */
namespace equation::tests {

    /**
     * Unit test macro
     *
     * @param [in] n Test index
     * @param [in] expr Expression to test
     * @param [in] val Expected value
     *
     * @note expr and val should be evaluated to
     * objects with overloaded operator<< so they
     * can be output to the stream
     */
#define UNIT(n, expr, val) {\
    std::cerr << "\tTest " #n ": (" #expr ") == (" #val ")..." << std::endl;\
    if (auto result__ = (expr), expect__ = (val); result__ == expect__)\
        std::cerr << "\t\t...passed." << std::endl;\
    else std::cerr << "\t\t...FAILED." << std::endl\
         << "\tEXPECTED: " << expect__ << std::endl\
         << "\tGOT: " << result__ << std::endl; }


    /** solve_linear() function units tests */
    void test_solve_linear() {
        std::cerr << "Testing solve_linear()..." << std::endl;

        UNIT(1, solve_linear(1., 2.), solution(-2.));
        UNIT(2, solve_linear(2., 1.), solution(-.5));
        UNIT(3, solve_linear(2., 0.), solution(0.));
        UNIT(4, solve_linear(0., 0.), solution(true));
        UNIT(5, solve_linear(0., 1.), solution(false));
    }

    /** solve_quadratic() function unit tests */
    void test_solve_quadratic() {
        std::cerr << "Testing solve_quadratic()..." << std::endl;

        UNIT(1, solve_quadratic(0., 1., 2.), solution(-2.));
        UNIT(2, solve_quadratic(0., 2., 1.), solution(-.5));
        UNIT(3, solve_quadratic(0., 2., 0.), solution(0.));
        UNIT(4, solve_quadratic(0., 0., 0.), solution(true));
        UNIT(5, solve_quadratic(0., 0., 1.), solution(false));
        UNIT(6, solve_quadratic(2., 0., 0.), solution(0., 0.));
        UNIT(7, solve_quadratic(1., 0., 2.), solution(false));
        UNIT(8, solve_quadratic(1., 1., 0), solution(-1., 0.));
        UNIT(9, solve_quadratic(1., -2., 1), solution(1., 1.));
    }

    /** is_zero() function unit tests */
    void test_is_zero() {
        std::cerr << "Testing is_zero()..." << std::endl;

        UNIT(1, is_zero(0.), true);
        UNIT(2, is_zero(eps/2), true);
        UNIT(3, is_zero(-eps/2), true);
        UNIT(4, is_zero(-2*eps), false);
        UNIT(5, is_zero(2*eps), false);
    }

    /** Solution class unit tests */
    void test_solution() {
        std::cerr << "Testing class solution..." << std::endl;

        UNIT(1, solution(false).tag, solution::tag::none);
        UNIT(2, solution(true).tag, solution::tag::infinite);
        UNIT(3, solution(2.).tag, solution::tag::one);
        UNIT(4, is_zero(solution(2.).first() - 2.), true);
        UNIT(5, solution(2., 2.).tag, solution::tag::two);
        UNIT(6, is_zero(solution(2., 2.).first() - 2.), true);
        UNIT(7, is_zero(solution(2., 2.).second() - 2.), true);
        UNIT(8, solution(2., 2.) == solution(2., 2.), true);
        UNIT(9, solution(2., 2.) != solution(2., 3.), true);
        UNIT(10, solution(2., 2.) != solution(3., 2.), true);
        UNIT(11, solution(2., 2.) != solution(2.), true);
        UNIT(12, solution(2., 2.) != solution(true), true);
        UNIT(13, solution(2., 2.) != solution(false), true);
        UNIT(14, solution(2.) == solution(2.), true);
        UNIT(15, solution(2.) != solution(3.), true);
        UNIT(16, solution(2.) != solution(false), true);
        UNIT(17, solution(2.) != solution(true), true);
        UNIT(18, solution(true) == solution(true), true);
        UNIT(19, solution(false) == solution(false), true);
        UNIT(20, solution(false) != solution(true), true);
    }
#undef UNIT
}

#endif // !defined(NDEBUG)

/**
 * Print usage and exit.
 *
 * @param [in] argv0 program path.
 * @param [in] code exit code.
 */
[[noreturn]] static void usage(char *argv0, int code) {
    assert(argv0);

    std::cerr << "Usage:\n"
              << "    " << argv0 << " a b c\n\n"
              << "where a, b, c are coeficients of\n"
              << "    a*x^2 + b*x + c = 0" << std::endl;
#ifndef NDEBUG
    std::cerr << "\nOr alternatively:\n"
              << "    " << argv0 << " test\n\n"
              << "to perform tests." << std::endl;
#endif // !defined(NDEBUG)

    std::exit(code);
}

/** Main function */
int main(int argc, char *argv[]) {
#ifndef NDEBUG
    // If the only argument of program is 'test', perform tests
    if (argc == 2 && !std::strcmp(argv[1], "test")) {
        equation::tests::test_is_zero();
        equation::tests::test_solution();
        equation::tests::test_solve_linear();
        equation::tests::test_solve_quadratic();
        return EXIT_SUCCESS;
    }
#endif // !defined(NDEBUG)

    if (argc < 4)
        usage(argv[0], EXIT_FAILURE);

    double a{}, b{}, c{};
    try {
        a = std::stod(argv[1]);
        b = std::stod(argv[2]);
        c = std::stod(argv[3]);
    } catch (const std::exception &ex) {
        std::cerr << "Wrong argument: " << ex.what() << std::endl;
        usage(argv[0], EXIT_FAILURE);
    }

    std::cout << "# Set of solutions: " << std::endl;
    std::cout << equation::solve_quadratic(a, b, c) << std::endl;

    return EXIT_SUCCESS;
}
