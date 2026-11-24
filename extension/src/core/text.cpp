/* Copyright 2026 ETF2L Contributors, Dave */
#include "text.h"

#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

namespace smgetopt::text {

namespace {

bool IsSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool IsDigit(char c)
{
	return c >= '0' && c <= '9';
}

bool IsLetter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* The bytes of the UTF-8 character that starts at text[0], 1 when it is
 * not valid UTF-8. */
size_t CharacterLength(std::string_view text)
{
	unsigned char lead = static_cast<unsigned char>(text[0]);
	size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
	if (length > text.size())
		return 1;
	for (size_t i = 1; i < length; i++) {
		if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80)
			return 1;
	}
	return length;
}

/* The sign and the digits of an integer, the digits without leading zeros. */
struct IntegerParts {
	bool negative;
	std::string_view digits;
};

IntegerParts PartsOf(std::string_view integer)
{
	bool negative = integer[0] == '-';
	if (integer[0] == '-' || integer[0] == '+')
		integer.remove_prefix(1);
	while (integer.size() > 1 && integer[0] == '0')
		integer.remove_prefix(1);
	return {negative && integer != "0", integer};
}

int CompareDigits(std::string_view left, std::string_view right)
{
	if (left.size() != right.size())
		return left.size() < right.size() ? -1 : 1;
	return left.compare(right);
}

/* Numbers are read and written with the C locale's decimal point, whatever
 * locale the process this code is loaded into has set. */
class CLocaleScope {
public:
	CLocaleScope() : m_previous(uselocale(CLocale())) {}
	~CLocaleScope() { uselocale(m_previous); }
	CLocaleScope(const CLocaleScope &) = delete;
	CLocaleScope &operator=(const CLocaleScope &) = delete;

private:
	static locale_t CLocale()
	{
		static locale_t locale = newlocale(LC_ALL_MASK, "C", static_cast<locale_t>(0));
		return locale;
	}

	locale_t m_previous;
};

size_t SkipDigits(std::string_view text, size_t at)
{
	while (at < text.size() && IsDigit(text[at]))
		at++;
	return at;
}

} // namespace

std::string Format(const char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	va_list counting;
	va_copy(counting, arguments);
	int length = vsnprintf(nullptr, 0, format, counting);
	va_end(counting);

	std::string result(length > 0 ? static_cast<size_t>(length) : 0, '\0');
	if (length > 0)
		vsnprintf(&result[0], result.size() + 1, format, arguments);
	va_end(arguments);
	return result;
}

std::string Join(const std::vector<std::string> &parts, std::string_view separator)
{
	std::string joined;
	for (size_t i = 0; i < parts.size(); i++) {
		if (i > 0)
			joined += separator;
		joined += parts[i];
	}
	return joined;
}

std::string Trim(std::string_view text)
{
	while (!text.empty() && IsSpace(text.front()))
		text.remove_prefix(1);
	while (!text.empty() && IsSpace(text.back()))
		text.remove_suffix(1);
	return std::string(text);
}

std::string ToLower(std::string_view text)
{
	std::string lower(text);
	for (char &c : lower) {
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	}
	return lower;
}

size_t Width(std::string_view text)
{
	size_t width = 0;
	while (!text.empty()) {
		text.remove_prefix(CharacterLength(text));
		width++;
	}
	return width;
}

std::string_view FirstCharacter(std::string_view text)
{
	return text.empty() ? text : text.substr(0, CharacterLength(text));
}

bool IsValidName(std::string_view name)
{
	if (name.empty() || !IsLetter(name[0]))
		return false;
	for (char c : name) {
		if (!IsLetter(c) && !IsDigit(c) && c != '_' && c != '-')
			return false;
	}
	return true;
}

std::string OptionSpelling(std::string_view name)
{
	return (name.size() == 1 ? "-" : "--") + std::string(name);
}

bool IsTrue(std::string_view value)
{
	return !value.empty() && value != "0";
}

std::vector<std::string> SplitWords(std::string_view line)
{
	std::vector<std::string> words;
	std::string word;
	bool inWord = false;
	bool inQuotes = false;

	for (char c : line) {
		if (c == '"') {
			inQuotes = !inQuotes;
			inWord = true;
			continue;
		}
		if (IsSpace(c) && !inQuotes) {
			if (inWord)
				words.push_back(std::move(word));
			word.clear();
			inWord = false;
			continue;
		}
		word += c;
		inWord = true;
	}
	if (inWord)
		words.push_back(std::move(word));
	return words;
}

std::vector<std::string> SplitAtWhitespace(std::string_view text)
{
	std::vector<std::string> words;
	size_t at = 0;
	while (at < text.size()) {
		while (at < text.size() && IsSpace(text[at]))
			at++;
		size_t start = at;
		while (at < text.size() && !IsSpace(text[at]))
			at++;
		if (at > start)
			words.emplace_back(text.substr(start, at - start));
	}
	return words;
}

namespace {

/* Where to cut text so that the piece before the cut has at most maxBytes:
 * after the last space in reach that is past the first skip bytes, else at
 * the last character boundary. */
size_t CutOf(std::string_view text, size_t skip, size_t maxBytes)
{
	size_t space = text.rfind(' ', maxBytes);
	if (space != std::string_view::npos && space > skip)
		return space;

	size_t cut = maxBytes;
	while (cut > skip + 1 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
		cut--;
	return cut;
}

} // namespace

std::vector<std::string> FitLines(const std::vector<std::string> &lines, size_t maxBytes)
{
	std::vector<std::string> fitted;
	for (const std::string &line : lines) {
		size_t indentLength = line.find_first_not_of(' ');
		std::string indent = indentLength == std::string::npos || indentLength >= maxBytes / 2 ? "" : line.substr(0, indentLength);

		std::string rest = line;
		while (rest.size() > maxBytes) {
			size_t cut = CutOf(rest, indent.size(), maxBytes);
			fitted.push_back(rest.substr(0, cut));
			rest = indent + rest.substr(rest[cut] == ' ' ? cut + 1 : cut);
		}
		fitted.push_back(rest);
	}
	return fitted;
}

bool IsInteger(std::string_view text)
{
	if (!text.empty() && (text[0] == '+' || text[0] == '-'))
		text.remove_prefix(1);
	return !text.empty() && SkipDigits(text, 0) == text.size();
}

std::string CanonicalInteger(std::string_view integer)
{
	IntegerParts parts = PartsOf(integer);
	return (parts.negative ? "-" : "") + std::string(parts.digits);
}

int CompareIntegers(std::string_view left, std::string_view right)
{
	IntegerParts a = PartsOf(left);
	IntegerParts b = PartsOf(right);
	if (a.negative != b.negative)
		return a.negative ? -1 : 1;
	return a.negative ? CompareDigits(b.digits, a.digits) : CompareDigits(a.digits, b.digits);
}

std::optional<int64_t> ToInt64(std::string_view integer)
{
	if (CompareIntegers(integer, "-9223372036854775808") < 0 || CompareIntegers(integer, "9223372036854775807") > 0)
		return std::nullopt;
	return strtoll(CanonicalInteger(integer).c_str(), nullptr, 10);
}

bool IsDecimal(std::string_view text)
{
	size_t at = !text.empty() && (text[0] == '+' || text[0] == '-') ? 1 : 0;
	size_t wholeEnd = SkipDigits(text, at);
	size_t fractionEnd = wholeEnd;
	if (wholeEnd < text.size() && text[wholeEnd] == '.')
		fractionEnd = SkipDigits(text, wholeEnd + 1);

	bool hasDigits = wholeEnd > at || fractionEnd > wholeEnd + 1;
	if (!hasDigits)
		return false;
	if (fractionEnd == text.size())
		return true;

	if (text[fractionEnd] != 'e' && text[fractionEnd] != 'E')
		return false;
	size_t exponent = fractionEnd + 1;
	if (exponent < text.size() && (text[exponent] == '+' || text[exponent] == '-'))
		exponent++;
	return exponent < text.size() && SkipDigits(text, exponent) == text.size();
}

bool IsNonFiniteWord(std::string_view text)
{
	if (!text.empty() && (text[0] == '+' || text[0] == '-'))
		text.remove_prefix(1);
	std::string word = ToLower(text);
	return word == "inf" || word == "infinity" || word == "nan";
}

std::optional<double> ToDouble(std::string_view text)
{
	if (!IsDecimal(text))
		return std::nullopt;
	CLocaleScope cLocale;
	return strtod(std::string(text).c_str(), nullptr);
}

std::string FormatDouble(double value)
{
	CLocaleScope cLocale;
	return Format("%.15g", value);
}

} // namespace smgetopt::text
