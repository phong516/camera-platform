#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

/// Tiny no-framework test helpers. Every test file defines `int main()`, runs
/// CHECK/CHECK_EQ, and ends with TEST_RETURN().
namespace tests
{
inline int &failures()
{
    static int count = 0;
    return count;
}
} // namespace tests

#define CHECK(condition)                                        \
    do                                                          \
    {                                                           \
        if (!(condition))                                       \
        {                                                       \
            ++tests::failures();                                \
            std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
                      << ": " #condition << std::endl;          \
        }                                                       \
    } while (false)

#define CHECK_EQ(actual, expected)                                              \
    do                                                                          \
    {                                                                           \
        const auto &lhs_ = (actual);                                            \
        const auto &rhs_ = (expected);                                          \
        if (!(lhs_ == rhs_))                                                    \
        {                                                                       \
            ++tests::failures();                                                \
            std::cerr << "FAIL " << __FILE__ << ':' << __LINE__                 \
                      << ": " #actual " == " #expected << " (got '" << lhs_     \
                      << "', expected '" << rhs_ << "')" << std::endl;          \
        }                                                                       \
    } while (false)

#define TEST_RETURN() return tests::failures() == 0 ? EXIT_SUCCESS : EXIT_FAILURE
