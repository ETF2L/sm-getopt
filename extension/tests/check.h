/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The unit tests' few tools: a test registers itself with TEST and reports
 * with CHECK and CHECK_EQ, which count failures and carry on. */
#ifndef GETOPT_TESTS_CHECK_H
#define GETOPT_TESTS_CHECK_H

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace check {

struct Test {
	const char *name;
	std::function<void()> run;
};

std::vector<Test> &Tests();
int &Failures();

struct Registration {
	Registration(const char *name, std::function<void()> run) { Tests().push_back({name, std::move(run)}); }
};

} // namespace check

#define TEST(name)                                                              \
	static void test_##name();                                                  \
	static check::Registration registration_##name(#name, test_##name);        \
	static void test_##name()

#define CHECK(condition)                                                        \
	do {                                                                        \
		if (!(condition)) {                                                     \
			std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " << #condition << "\n"; \
			check::Failures()++;                                                \
		}                                                                       \
	} while (0)

#define CHECK_EQ(actual, expected)                                              \
	do {                                                                        \
		/* Copies: a reference into a temporary would outlive it. */            \
		auto checkedActual = (actual);                                          \
		auto checkedExpected = (expected);                                      \
		if (!(checkedActual == checkedExpected)) {                              \
			std::cerr << __FILE__ << ":" << __LINE__ << ": " << #actual << "\n"  \
			          << "    expected: " << checkedExpected << "\n"            \
			          << "    got:      " << checkedActual << "\n";             \
			check::Failures()++;                                                \
		}                                                                       \
	} while (0)

#endif // GETOPT_TESTS_CHECK_H
