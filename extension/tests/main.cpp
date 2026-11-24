/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Runs every registered unit test and exits non-zero when a check failed. */
#include "check.h"

#include <stdio.h>

namespace check {

std::vector<Test> &Tests()
{
	static std::vector<Test> tests;
	return tests;
}

int &Failures()
{
	static int failures = 0;
	return failures;
}

} // namespace check

int main()
{
	for (const check::Test &test : check::Tests()) {
		int before = check::Failures();
		test.run();
		if (check::Failures() != before)
			fprintf(stderr, "FAILED: %s\n", test.name);
	}
	if (check::Failures() != 0) {
		fprintf(stderr, "%d check(s) failed\n", check::Failures());
		return 1;
	}
	printf("%zu tests passed\n", check::Tests().size());
	return 0;
}
