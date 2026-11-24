/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Tests of the text tools: words, names and numbers as text. */
#include "check.h"
#include "support.h"

using namespace smgetopt;
using support::Joined;

namespace {

std::string Words(const std::string &line)
{
	return text::Join(text::SplitWords(line), "|");
}

} // namespace

TEST(words_split_at_whitespace)
{
	CHECK_EQ(Words("a b  c"), "a|b|c");
	CHECK_EQ(Words("  a\tb\n"), "a|b");
	CHECK_EQ(Words(""), "");
	CHECK_EQ(Words("   "), "");
}

TEST(quotes_keep_whitespace_and_are_removed)
{
	CHECK_EQ(Words("\"a b\" c"), "a b|c");
	CHECK_EQ(Words("--name=\"a b\""), "--name=a b");
	CHECK_EQ(Words("a\"b c\"d"), "ab cd");
	CHECK_EQ(Words("it's fine"), "it's|fine");
}

TEST(quotes_make_empty_words)
{
	CHECK_EQ(text::SplitWords("--name \"\"").size(), 2u);
	CHECK_EQ(text::SplitWords("--name \"\"")[1], "");
	CHECK_EQ(text::SplitWords("\"\" \"\"").size(), 2u);
}

TEST(an_open_quote_runs_to_the_end)
{
	CHECK_EQ(Words("a \"b c"), "a|b c");
	CHECK_EQ(Words("a \""), "a|");
}

TEST(names)
{
	CHECK(text::IsValidName("owner"));
	CHECK(text::IsValidName("o"));
	CHECK(text::IsValidName("work-dir_2"));
	CHECK(text::IsValidName("V"));
	CHECK(!text::IsValidName(""));
	CHECK(!text::IsValidName("2fast"));
	CHECK(!text::IsValidName("-x"));
	CHECK(!text::IsValidName("a b"));
	CHECK(!text::IsValidName("a=b"));
	CHECK(!text::IsValidName("k\xC3\xB6ln"));
	CHECK_EQ(text::OptionSpelling("o"), "-o");
	CHECK_EQ(text::OptionSpelling("owner"), "--owner");
}

TEST(switch_values)
{
	CHECK(text::IsTrue("1"));
	CHECK(text::IsTrue("yes"));
	CHECK(text::IsTrue("00"));
	CHECK(!text::IsTrue("0"));
	CHECK(!text::IsTrue(""));
}

TEST(width_counts_characters)
{
	CHECK_EQ(text::Width("abc"), 3u);
	CHECK_EQ(text::Width("K\xC3\xB6ln"), 4u);
	CHECK_EQ(text::Width("\xE2\x82\xAC"), 1u);
	CHECK_EQ(text::Width("\xF0\x9F\x98\x80x"), 2u);
	CHECK_EQ(text::Width("\xC3"), 1u);
	CHECK_EQ(text::Width("\xFF\xFF"), 2u);
	CHECK_EQ(std::string(text::FirstCharacter("\xC3\xB6x")), "\xC3\xB6");
	CHECK_EQ(std::string(text::FirstCharacter("")), "");
}

TEST(trim_join_lower)
{
	CHECK_EQ(text::Trim("  a b\t\n"), "a b");
	CHECK_EQ(text::Trim("   "), "");
	CHECK_EQ(text::Join({"a", "b"}, ", "), "a, b");
	CHECK_EQ(text::Join({}, ", "), "");
	CHECK_EQ(text::ToLower("FiLe"), "file");
	CHECK_EQ(text::Join(text::SplitAtWhitespace(" a  \"b c\" "), "|"), "a|\"b|c\"");
	CHECK_EQ(text::Format("%s=%d", "x", 5), "x=5");
	CHECK_EQ(text::Format("%s", std::string(5000, 'x').c_str()).size(), 5000u);
}

TEST(long_lines_are_broken)
{
	CHECK_EQ(Joined(text::FitLines({"short", ""}, 10)), "short\n");
	CHECK_EQ(Joined(text::FitLines({"aaa bbb ccc ddd"}, 8)), "aaa bbb\nccc ddd");
	CHECK_EQ(Joined(text::FitLines({"  aaa bbb ccc ddd"}, 9)), "  aaa bbb\n  ccc ddd");
	CHECK_EQ(Joined(text::FitLines({"abcdefghij"}, 4)), "abcd\nefgh\nij");
	CHECK_EQ(Joined(text::FitLines({"  abcdefghij"}, 6)), "  abcd\n  efgh\n  ij");
	/* A character is never cut in two. */
	CHECK_EQ(Joined(text::FitLines({"a\xC3\xB6\xC3\xB6\xC3\xB6"}, 4)), "a\xC3\xB6\n\xC3\xB6\xC3\xB6");
	/* An indentation that leaves no room is given up. */
	CHECK_EQ(Joined(text::FitLines({"        abcdefgh ij"}, 8)), "       \nabcdefgh\nij");
	for (const std::string &line : text::FitLines({std::string(40, ' ') + std::string(3000, 'x') + " y " + std::string(700, 'z')}, 500))
		CHECK(line.size() <= 500);
}

TEST(integers_as_text)
{
	CHECK(text::IsInteger("0"));
	CHECK(text::IsInteger("-7"));
	CHECK(text::IsInteger("+007"));
	CHECK(!text::IsInteger(""));
	CHECK(!text::IsInteger("-"));
	CHECK(!text::IsInteger("1.0"));
	CHECK(!text::IsInteger("5\n"));
	CHECK(!text::IsInteger(" 5"));
	CHECK(!text::IsInteger("\xD9\xA3"));

	CHECK_EQ(text::CanonicalInteger("+007"), "7");
	CHECK_EQ(text::CanonicalInteger("-007"), "-7");
	CHECK_EQ(text::CanonicalInteger("-0"), "0");
	CHECK_EQ(text::CanonicalInteger("000"), "0");

	CHECK(text::CompareIntegers("2", "10") < 0);
	CHECK(text::CompareIntegers("-2", "-10") > 0);
	CHECK(text::CompareIntegers("-1", "1") < 0);
	CHECK(text::CompareIntegers("+05", "5") == 0);
	CHECK(text::CompareIntegers("-0", "0") == 0);
	CHECK(text::CompareIntegers("100000000000000000000", "100000000000000000001") < 0);

	CHECK_EQ(*text::ToInt64("9223372036854775807"), INT64_MAX);
	CHECK_EQ(*text::ToInt64("-9223372036854775808"), INT64_MIN);
	CHECK(!text::ToInt64("9223372036854775808"));
	CHECK(!text::ToInt64("-9223372036854775809"));
}

TEST(decimals_as_text)
{
	for (const char *number : {"1.5", "-2", ".5", "5.", "1e3", "1E-3", "+0.25e+2"})
		CHECK(text::IsDecimal(number));
	for (const char *other : {"", ".", "-", "e3", "1e", "1e+", "0x10", "1.2.3", " 1", "1 ", "1,5", "inf", "nan"})
		CHECK(!text::IsDecimal(other));

	CHECK(text::IsNonFiniteWord("inf"));
	CHECK(text::IsNonFiniteWord("-Infinity"));
	CHECK(text::IsNonFiniteWord("+NaN"));
	CHECK(!text::IsNonFiniteWord("info"));

	CHECK_EQ(*text::ToDouble("1e3"), 1000.0);
	CHECK_EQ(*text::ToDouble(".5"), 0.5);
	CHECK(!text::ToDouble("abc"));
	CHECK_EQ(text::FormatDouble(1000.0), "1000");
	CHECK_EQ(text::FormatDouble(0.5), "0.5");
	CHECK_EQ(text::FormatDouble(-2.25), "-2.25");
	CHECK_EQ(text::FormatDouble(0.1), "0.1");
}
