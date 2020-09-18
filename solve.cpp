/**
 * @file solve.cpp
 *
 * @brief Equation Solver
 *
 * Simple quadratic equation solver
 * written with support of unit tests
 * and documetation
 *
 * @authors summaryInfo
 * @date Sep 7 2020
 */

#include "unit.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <exception>
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
            none, ///< No solutions
            one, ///< One real solution
            two, ///< Two real solutions
            infinite, ///< Any real number is a solution
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
            if(tag != tag::one && tag != tag::two)
                throw std::length_error("No first root");

            return data[0];
        }

        /**
         * Second solution getter.
         *
         * @note Aborts program if there is no solutions, one solution or infinite number of them.
         * */
        double second() const {
            if(tag != tag::two)
                throw std::length_error("No second root");

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
            return str << "{}";
        case solution::tag::two:
            return str << "{" << val.first() << ", " << val.second() << "}";
        case solution::tag::one:
            return str << "{" << val.first() << "}";
        case solution::tag::infinite:
            return str << "R";
        }
        throw std::invalid_argument("Wrong tag");
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
        case solution::tag::two:
            return str << "tag::two";
        case solution::tag::one:
            return str << "tag::one";
        case solution::tag::infinite:
            return str << "tag::infinite";
        }
        throw std::invalid_argument("Wrong tag");
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
        if (!std::isfinite(k) || !std::isfinite(b))
            throw std::invalid_argument("Non-finite coeficient");

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
        if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c))
            throw std::invalid_argument("Non-finite coeficient");

        if (is_zero(a))
            return solve_linear(b, c);

        b /= a;
        c /= a;

        if (auto det = b*b - 4.*c; det > eps) {
            auto sqrt_det = std::sqrt(det);
            return {
                (-b - sqrt_det) / 2.,
                (-b + sqrt_det) / 2.,
            };
        } else if (is_zero(det)) {
            return {
                -b / 2.,
                -b / 2.,
            };
        }

        return {false};
    }
}

#ifndef NDEBUG
/** Unit test namespace */
namespace equation::tests {
    /** solve_linear() function units tests */
    void test_solve_linear() {
        UNITS_BEGIN;
        UNIT(solve_linear(1., 2.), solution(-2.));
        UNIT(solve_linear(2., 1.), solution(-.5));
        UNIT(solve_linear(2., 0.), solution(0.));
        UNIT(solve_linear(0., 0.), solution(true));
        UNIT(solve_linear(0., 1.), solution(false));
        UNITS_END;
    }

    /** solve_quadratic() function unit tests */
    void test_solve_quadratic() {
        UNITS_BEGIN;
        UNIT(solve_quadratic(0., 1., 2.), solution(-2.));
        UNIT(solve_quadratic(0., 2., 1.), solution(-.5));
        UNIT(solve_quadratic(0., 2., 0.), solution(0.));
        UNIT(solve_quadratic(0., 0., 0.), solution(true));
        UNIT(solve_quadratic(0., 0., 1.), solution(false));
        UNIT(solve_quadratic(2., 0., 0.), solution(0., 0.));
        UNIT(solve_quadratic(1., 0., 2.), solution(false));
        UNIT(solve_quadratic(1., 1., 0), solution(-1., 0.));
        UNIT(solve_quadratic(1., -2., 1), solution(1., 1.));
        UNITS_END;
    }

    /** is_zero() function unit tests */
    void test_is_zero() {
        UNITS_BEGIN;
        UNIT(is_zero(0.), true);
        UNIT(is_zero(eps/2), true);
        UNIT(is_zero(-eps/2), true);
        UNIT(is_zero(-2*eps), false);
        UNIT(is_zero(2*eps), false);
        UNITS_END;
    }

    /** Solution class unit tests */
    void test_solution() {
        UNITS_BEGIN;
        UNIT(solution(false).tag, solution::tag::none);
        UNIT(solution(true).tag, solution::tag::infinite);
        UNIT(solution(2.).tag, solution::tag::one);
        UNIT(is_zero(solution(2.).first() - 2.), true);
        UNIT(solution(2., 2.).tag, solution::tag::two);
        UNIT(is_zero(solution(2., 2.).first() - 2.), true);
        UNIT(is_zero(solution(2., 2.).second() - 2.), true);
        UNIT(solution(2., 2.) == solution(2., 2.), true);
        UNIT(solution(2., 2.) != solution(2., 3.), true);
        UNIT(solution(2., 2.) != solution(3., 2.), true);
        UNIT(solution(2., 2.) != solution(2.), true);
        UNIT(solution(2., 2.) != solution(true), true);
        UNIT(solution(2., 2.) != solution(false), true);
        UNIT(solution(2.) == solution(2.), true);
        UNIT(solution(2.) != solution(3.), true);
        UNIT(solution(2.) != solution(false), true);
        UNIT(solution(2.) != solution(true), true);
        UNIT(solution(true) == solution(true), true);
        UNIT(solution(false) == solution(false), true);
        UNIT(solution(false) != solution(true), true);
        UNITS_END;
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

    try {
        auto a = std::stod(argv[1]);
        auto b = std::stod(argv[2]);
        auto c = std::stod(argv[3]);

        auto res = equation::solve_quadratic(a, b, c);

        std::cout << "# Set of solutions: " << std::endl;
        std::cout << res << std::endl;
    } catch (const std::exception &ex) {
        std::cerr << "Wrong argument: " << ex.what() << std::endl;
        usage(argv[0], EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
