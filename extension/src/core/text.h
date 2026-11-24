/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Small text tools the whole library shares: formatting, names, the split
 * of a command's argument string into words, and decimal numbers checked
 * and compared as text, so integers of any size stay exact. */
#ifndef GETOPT_CORE_TEXT_H
#define GETOPT_CORE_TEXT_H

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace smgetopt::text {

/* printf into a string. */
std::string Format(const char *format, ...) __attribute__((format(printf, 1, 2)));

std::string Join(const std::vector<std::string> &parts, std::string_view separator);

/* text without the whitespace at both ends. */
std::string Trim(std::string_view text);

std::string ToLower(std::string_view text);

/* The number of characters of UTF-8 text, as a console shows them. Bytes
 * that are no valid UTF-8 count one each. */
size_t Width(std::string_view text);

/* The first character of UTF-8 text, all of its bytes. */
std::string_view FirstCharacter(std::string_view text);

/* The one shape an option name, an alias, an arg name and a command name
 * share: a letter, then letters, digits, underscores or dashes. */
bool IsValidName(std::string_view name);

/* An option name as it is typed: one dash for a name of one letter, two for
 * a longer one. */
std::string OptionSpelling(std::string_view name);

/* A spec key's value as a switch, with Perl's truth: everything but the
 * empty string and "0" is on. */
bool IsTrue(std::string_view value);

/* The words of a console command's argument string. Whitespace separates
 * words; double quotes keep whitespace in a word, may open anywhere in it
 * (--name="a b") and are removed; a quote left open runs to the end, as in
 * the engine. There is no escape character. */
std::vector<std::string> SplitWords(std::string_view line);

/* The words of text, separated by whitespace. */
std::vector<std::string> SplitAtWhitespace(std::string_view text);

/* lines with every line longer than maxBytes broken into several: at a
 * space where there is one, else between two characters. The pieces after
 * the first keep the indentation of the line. */
std::vector<std::string> FitLines(const std::vector<std::string> &lines, size_t maxBytes);

/* An optional sign and decimal digits, nothing else. */
bool IsInteger(std::string_view text);

/* An integer without a plus sign, leading zeros or a negative zero. */
std::string CanonicalInteger(std::string_view integer);

/* Compares two integers of any size: negative, zero or positive like
 * strcmp. Both must pass IsInteger. */
int CompareIntegers(std::string_view left, std::string_view right);

/* The integer's value when it fits 64 bits. */
std::optional<int64_t> ToInt64(std::string_view integer);

/* A decimal number: an optional sign, digits with an optional fraction or a
 * fraction alone, an optional exponent. No hexadecimal, no whitespace. */
bool IsDecimal(std::string_view text);

/* inf, infinity or nan with an optional sign, in any case. */
bool IsNonFiniteWord(std::string_view text);

/* The value of text when IsDecimal accepts it; infinite when it is too
 * large for a double. */
std::optional<double> ToDouble(std::string_view text);

/* A double as its shortest text that keeps 15 significant digits: 1000 for
 * 1e3, 0.5 for .5. */
std::string FormatDouble(double value);

} // namespace smgetopt::text

#endif // GETOPT_CORE_TEXT_H
