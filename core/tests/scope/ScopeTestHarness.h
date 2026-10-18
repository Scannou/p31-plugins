//
//  ScopeTestHarness.h
//  p31-core-tests
//
//  Maps the check macros of the original foo_jl_vectorscope_mac test suites
//  onto doctest, so the ported tests stay line-for-line comparable. The
//  macros are renamed (EXPECT*) because doctest's own CHECK is variadic and
//  would silently accept CHECK(cond, "message") as a comma expression.
//

#pragma once

#include <doctest/doctest.h>

#include <cmath>
#include <string>

inline std::string g_context;

#define EXPECT(cond, what) CHECK_MESSAGE((cond), g_context << ": " << (what))

#define EXPECT_EQ(actual, expected, what) \
    CHECK_MESSAGE(static_cast<long long>(actual) == static_cast<long long>(expected), g_context << ": " << (what))

#define EXPECT_FLOAT_EQ(actual, expected, what)                                        \
    CHECK_MESSAGE(std::fabs(static_cast<double>(actual) - static_cast<double>(expected)) <= 0.001, \
                  g_context << ": " << (what) << " (got " << (actual) << ", expected " << (expected) << ")")
