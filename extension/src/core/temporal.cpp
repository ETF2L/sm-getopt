/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Dates and durations in natural language: the proleptic Gregorian calendar
 * done by hand, a reader for the zoneinfo files of RFC 8536 with the POSIX TZ
 * rule of their footer, and the parsers for dates and durations. The date
 * grammar follows DateTime::Format::Natural (English), which the Perl
 * original uses; docs/dates.md lists what is understood.
 *
 * Nothing here touches the process's time zone state (no setenv, tzset or
 * localtime): this runs inside a game server, where other code may rely on
 * it. */
#include "temporal.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace smgetopt::temporal {

namespace {

constexpr int64_t kSecondsPerMinute = 60;
constexpr int64_t kSecondsPerHour = 60 * kSecondsPerMinute;
constexpr int64_t kSecondsPerDay = 24 * kSecondsPerHour;
constexpr int64_t kDaysPerWeek = 7;
constexpr int64_t kMonthsPerYear = 12;

/* The years a parsed date may fall in. */
constexpr int kMinYear = 1;
constexpr int kMaxYear = 9999;

/* Instants further out than a billion years are clamped before they are
 * turned into civil time, so that the year always fits an int. */
constexpr int64_t kCalendarReachSeconds = 1000000000LL * 366 * kSecondsPerDay;

/* AddDuration clamps durations beyond a million years: no date it is given
 * comes near that, and the clamp keeps the calendar arithmetic in range. */
constexpr int64_t kMaxAddedMonths = 1000000LL * kMonthsPerYear;
constexpr int64_t kMaxAddedDays = 1000000LL * 366;
constexpr int64_t kMaxAddedSeconds = kMaxAddedDays * kSecondsPerDay;

/* ParseDuration accepts at most 10000 years in any unit: longer is no
 * meaningful length for a game server and could not be added to a date in
 * years 1 to 9999 anyway. 3652425 days are 10000 Gregorian years. */
constexpr int64_t kMaxDurationDays = 3652425;
constexpr size_t kMaxCountDigits = 12;

/* RFC 8536 section 3.2: the range of UT offsets a zoneinfo file may use. */
constexpr int64_t kMinUtcOffset = -89999;
constexpr int64_t kMaxUtcOffset = 93599;

/* Every offset lies within this distance of UTC, so the instants that can
 * show a given civil time all lie within it of that civil time. */
constexpr int64_t kOffsetReachSeconds = 26 * kSecondsPerHour;

/* The largest real zoneinfo files are a few kilobytes. */
constexpr size_t kMaxZoneFileSize = 1024 * 1024;

/* POSIX TZ: a transition without a time happens at 02:00 local time. */
constexpr int64_t kDefaultTransitionTime = 2 * kSecondsPerHour;

const char *const kDefaultZoneinfoDir = "/usr/share/zoneinfo";
const char *const kLocalTimeFile = "/etc/localtime";

/* ---- Integer helpers ---------------------------------------------------- */

int64_t FloorDiv(int64_t numerator, int64_t denominator)
{
	int64_t quotient = numerator / denominator;
	bool roundedTowardZero = numerator % denominator != 0 && (numerator < 0) != (denominator < 0);
	return roundedTowardZero ? quotient - 1 : quotient;
}

int64_t FloorMod(int64_t numerator, int64_t denominator)
{
	return numerator - FloorDiv(numerator, denominator) * denominator;
}

int64_t SaturatingAdd(int64_t a, int64_t b)
{
	constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
	constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
	if (b > 0 && a > kMax - b)
		return kMax;
	if (b < 0 && a < kMin - b)
		return kMin;
	return a + b;
}

bool IsAsciiDigit(char c)
{
	return c >= '0' && c <= '9';
}

bool IsAsciiAlpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool IsAsciiSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

char AsciiLower(char c)
{
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool EndsWith(std::string_view text, std::string_view suffix)
{
	return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

/* A number written with 1 to maxDigits ASCII digits and nothing else. */
std::optional<int64_t> WholeNumber(std::string_view text, size_t maxDigits)
{
	if (text.empty() || text.size() > maxDigits)
		return std::nullopt;
	int64_t value = 0;
	for (char c : text) {
		if (!IsAsciiDigit(c))
			return std::nullopt;
		value = value * 10 + (c - '0');
	}
	return value;
}

/* ---- The proleptic Gregorian calendar ----------------------------------- */

struct Date {
	int64_t year;
	int month;
	int day;
};

bool IsLeapYear(int64_t year)
{
	return FloorMod(year, 4) == 0 && (FloorMod(year, 100) != 0 || FloorMod(year, 400) == 0);
}

int DaysInMonth(int64_t year, int month)
{
	static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (month == 2 && IsLeapYear(year))
		return 29;
	return kDays[month - 1];
}

bool IsValidDate(int64_t year, int month, int day)
{
	if (year < kMinYear || year > kMaxYear || month < 1 || month > 12 || day < 1)
		return false;
	return day <= DaysInMonth(year, month);
}

/* Days since 1970-01-01 (Howard Hinnant's days_from_civil). month is 1-12;
 * day may lie outside the month and counts on from its first day. */
int64_t DaysFromCivil(int64_t year, int month, int64_t day)
{
	int64_t marchYear = month <= 2 ? year - 1 : year;
	int64_t era = FloorDiv(marchYear, 400);
	int64_t yearOfEra = marchYear - era * 400;
	int64_t marchMonth = month > 2 ? month - 3 : month + 9;
	int64_t dayOfYear = (153 * marchMonth + 2) / 5 + day - 1;
	int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
	return era * 146097 + dayOfEra - 719468;
}

Date CivilFromDays(int64_t days)
{
	int64_t shifted = days + 719468;
	int64_t era = FloorDiv(shifted, 146097);
	int64_t dayOfEra = shifted - era * 146097;
	int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
	int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
	int64_t marchMonth = (5 * dayOfYear + 2) / 153;
	int day = static_cast<int>(dayOfYear - (153 * marchMonth + 2) / 5 + 1);
	int month = static_cast<int>(marchMonth < 10 ? marchMonth + 3 : marchMonth - 9);
	int64_t year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
	return Date{year, month, day};
}

int64_t DaysFromDate(const Date &date)
{
	return DaysFromCivil(date.year, date.month, date.day);
}

/* 0 is Sunday, as in POSIX TZ rules; 1970-01-01 was a Thursday. */
int WeekdayFromDays(int64_t days)
{
	return static_cast<int>(FloorMod(days + 4, 7));
}

/* Seconds since the epoch as if the civil time were UTC. Fields outside
 * their range carry over, so this is safe for any int values. */
int64_t LocalSecondsFromCivil(const CivilTime &civil)
{
	int64_t monthIndex = static_cast<int64_t>(civil.year) * kMonthsPerYear + (civil.month - 1);
	int64_t year = FloorDiv(monthIndex, kMonthsPerYear);
	int month = static_cast<int>(FloorMod(monthIndex, kMonthsPerYear)) + 1;
	int64_t days = DaysFromCivil(year, month, 1) + (static_cast<int64_t>(civil.day) - 1);
	return days * kSecondsPerDay + static_cast<int64_t>(civil.hour) * kSecondsPerHour
		+ static_cast<int64_t>(civil.minute) * kSecondsPerMinute + civil.second;
}

/* localSeconds is bounded by kCalendarReachSeconds plus an offset, so the
 * year fits an int. */
CivilTime CivilFromLocalSeconds(int64_t localSeconds)
{
	Date date = CivilFromDays(FloorDiv(localSeconds, kSecondsPerDay));
	int64_t secondOfDay = FloorMod(localSeconds, kSecondsPerDay);
	CivilTime civil;
	civil.year = static_cast<int>(date.year);
	civil.month = date.month;
	civil.day = date.day;
	civil.hour = static_cast<int>(secondOfDay / kSecondsPerHour);
	civil.minute = static_cast<int>(secondOfDay % kSecondsPerHour / kSecondsPerMinute);
	civil.second = static_cast<int>(secondOfDay % kSecondsPerMinute);
	return civil;
}

/* ---- POSIX TZ rules (the footer of a zoneinfo file) ---------------------- */

/* A character cursor over a string, for the small grammars below. */
class Cursor {
public:
	explicit Cursor(std::string_view text) : m_text(text) {}

	bool AtEnd() const { return m_position == m_text.size(); }
	char Peek() const { return AtEnd() ? '\0' : m_text[m_position]; }

	bool Take(char expected)
	{
		if (AtEnd() || m_text[m_position] != expected)
			return false;
		++m_position;
		return true;
	}

	/* 1 to maxDigits digits. */
	std::optional<int64_t> Number(size_t maxDigits)
	{
		size_t start = m_position;
		while (!AtEnd() && IsAsciiDigit(Peek()) && m_position - start < maxDigits)
			++m_position;
		if (m_position == start)
			return std::nullopt;
		return WholeNumber(m_text.substr(start, m_position - start), maxDigits);
	}

	template <typename Predicate>
	std::string_view TakeWhile(Predicate matches)
	{
		size_t start = m_position;
		while (!AtEnd() && matches(Peek()))
			++m_position;
		return m_text.substr(start, m_position - start);
	}

private:
	std::string_view m_text;
	size_t m_position = 0;
};

/* The day of the year a transition happens on. */
struct PosixDate {
	enum class Kind {
		JulianSkippingLeapDay, // Jn: 1-365, February 29 is never counted
		ZeroBasedDay,          // n: 0-365, February 29 is counted
		MonthWeekDay,          // Mm.w.d: day d (0 = Sunday) of week w (5 = last) of month m
	};
	Kind kind = Kind::ZeroBasedDay;
	int dayNumber = 0;
	int month = 1;
	int week = 1;
	int weekday = 0;
};

struct PosixTransition {
	PosixDate date;
	int64_t timeOfDay = kDefaultTransitionTime; // local, may be negative or beyond 24 h (RFC 8536)
};

struct DaylightRule {
	int64_t utcOffset = 0;
	PosixTransition start; // given in standard time
	PosixTransition end;   // given in daylight time
};

struct PosixZone {
	int64_t standardOffset = 0;
	std::optional<DaylightRule> daylight;
};

bool IsQuotedZoneNameChar(char c)
{
	return IsAsciiAlpha(c) || IsAsciiDigit(c) || c == '+' || c == '-';
}

/* "CET" or the quoted form "<+03>"; at least three characters. */
bool SkipPosixZoneName(Cursor &cursor)
{
	if (cursor.Take('<')) {
		std::string_view quoted = cursor.TakeWhile(IsQuotedZoneNameChar);
		return quoted.size() >= 3 && cursor.Take('>');
	}
	return cursor.TakeWhile(IsAsciiAlpha).size() >= 3;
}

/* [+-]hh[:mm[:ss]] in seconds, hours up to maxHours. */
std::optional<int64_t> ParsePosixHms(Cursor &cursor, int64_t maxHours)
{
	int64_t sign = 1;
	if (cursor.Take('-'))
		sign = -1;
	else
		cursor.Take('+');

	std::optional<int64_t> hours = cursor.Number(3);
	if (!hours || *hours > maxHours)
		return std::nullopt;
	int64_t minutes = 0;
	int64_t seconds = 0;
	if (cursor.Take(':')) {
		std::optional<int64_t> parsedMinutes = cursor.Number(2);
		if (!parsedMinutes || *parsedMinutes > 59)
			return std::nullopt;
		minutes = *parsedMinutes;
		if (cursor.Take(':')) {
			std::optional<int64_t> parsedSeconds = cursor.Number(2);
			if (!parsedSeconds || *parsedSeconds > 59)
				return std::nullopt;
			seconds = *parsedSeconds;
		}
	}
	return sign * (*hours * kSecondsPerHour + minutes * kSecondsPerMinute + seconds);
}

/* A POSIX offset counts hours west of Greenwich; a UT offset counts east. */
std::optional<int64_t> ParsePosixUtcOffset(Cursor &cursor)
{
	std::optional<int64_t> westOfGreenwich = ParsePosixHms(cursor, 24);
	if (!westOfGreenwich)
		return std::nullopt;
	return -*westOfGreenwich;
}

std::optional<int64_t> ParseBoundedNumber(Cursor &cursor, size_t maxDigits, int64_t min, int64_t max)
{
	std::optional<int64_t> value = cursor.Number(maxDigits);
	if (!value || *value < min || *value > max)
		return std::nullopt;
	return value;
}

std::optional<PosixDate> ParsePosixDate(Cursor &cursor)
{
	PosixDate date;
	if (cursor.Take('J')) {
		std::optional<int64_t> day = ParseBoundedNumber(cursor, 3, 1, 365);
		if (!day)
			return std::nullopt;
		date.kind = PosixDate::Kind::JulianSkippingLeapDay;
		date.dayNumber = static_cast<int>(*day);
		return date;
	}
	if (cursor.Take('M')) {
		std::optional<int64_t> month = ParseBoundedNumber(cursor, 2, 1, 12);
		if (!month || !cursor.Take('.'))
			return std::nullopt;
		std::optional<int64_t> week = ParseBoundedNumber(cursor, 1, 1, 5);
		if (!week || !cursor.Take('.'))
			return std::nullopt;
		std::optional<int64_t> weekday = ParseBoundedNumber(cursor, 1, 0, 6);
		if (!weekday)
			return std::nullopt;
		date.kind = PosixDate::Kind::MonthWeekDay;
		date.month = static_cast<int>(*month);
		date.week = static_cast<int>(*week);
		date.weekday = static_cast<int>(*weekday);
		return date;
	}
	std::optional<int64_t> day = ParseBoundedNumber(cursor, 3, 0, 365);
	if (!day)
		return std::nullopt;
	date.kind = PosixDate::Kind::ZeroBasedDay;
	date.dayNumber = static_cast<int>(*day);
	return date;
}

/* date[/time]; RFC 8536 allows hours from -167 to 167 in the time. */
std::optional<PosixTransition> ParsePosixTransition(Cursor &cursor)
{
	std::optional<PosixDate> date = ParsePosixDate(cursor);
	if (!date)
		return std::nullopt;
	PosixTransition transition;
	transition.date = *date;
	if (!cursor.Take('/'))
		return transition;
	std::optional<int64_t> timeOfDay = ParsePosixHms(cursor, 167);
	if (!timeOfDay)
		return std::nullopt;
	transition.timeOfDay = *timeOfDay;
	return transition;
}

/* std offset [dst [offset] ,start[/time],end[/time]]. A daylight saving name
 * without a rule is rejected: POSIX leaves its default rule to the
 * implementation, and zoneinfo footers always spell the rule out. */
std::optional<PosixZone> ParsePosixZone(std::string_view text)
{
	Cursor cursor(text);
	if (!SkipPosixZoneName(cursor))
		return std::nullopt;
	std::optional<int64_t> standardOffset = ParsePosixUtcOffset(cursor);
	if (!standardOffset)
		return std::nullopt;

	PosixZone zone;
	zone.standardOffset = *standardOffset;
	if (cursor.AtEnd())
		return zone;

	if (!SkipPosixZoneName(cursor))
		return std::nullopt;
	DaylightRule daylight;
	daylight.utcOffset = zone.standardOffset + kSecondsPerHour;
	if (cursor.Peek() != ',') {
		std::optional<int64_t> daylightOffset = ParsePosixUtcOffset(cursor);
		if (!daylightOffset)
			return std::nullopt;
		daylight.utcOffset = *daylightOffset;
	}
	if (!cursor.Take(','))
		return std::nullopt;
	std::optional<PosixTransition> start = ParsePosixTransition(cursor);
	if (!start || !cursor.Take(','))
		return std::nullopt;
	std::optional<PosixTransition> end = ParsePosixTransition(cursor);
	if (!end || !cursor.AtEnd())
		return std::nullopt;

	daylight.start = *start;
	daylight.end = *end;
	zone.daylight = daylight;
	return zone;
}

/* Days since the epoch of the day date names in year. */
int64_t PosixDateDays(int64_t year, const PosixDate &date)
{
	int64_t januaryFirst = DaysFromCivil(year, 1, 1);
	switch (date.kind) {
	case PosixDate::Kind::JulianSkippingLeapDay: {
		bool afterLeapDay = IsLeapYear(year) && date.dayNumber >= 60;
		return januaryFirst + date.dayNumber - 1 + (afterLeapDay ? 1 : 0);
	}
	case PosixDate::Kind::ZeroBasedDay:
		return januaryFirst + date.dayNumber;
	case PosixDate::Kind::MonthWeekDay:
		break;
	}

	int64_t monthFirst = DaysFromCivil(year, date.month, 1);
	int firstMatch = (date.weekday - WeekdayFromDays(monthFirst) + 7) % 7;
	int dayIndex = firstMatch + 7 * (date.week - 1);
	if (dayIndex >= DaysInMonth(year, date.month))
		dayIndex -= 7;
	return monthFirst + dayIndex;
}

int64_t PosixTransitionInstant(int64_t year, const PosixTransition &transition, int64_t offsetBefore)
{
	return PosixDateDays(year, transition.date) * kSecondsPerDay + transition.timeOfDay - offsetBefore;
}

int64_t PosixZoneOffsetAt(const PosixZone &zone, int64_t instant)
{
	if (!zone.daylight)
		return zone.standardOffset;

	const DaylightRule &daylight = *zone.daylight;
	int64_t year = CivilFromDays(FloorDiv(instant + zone.standardOffset, kSecondsPerDay)).year;
	int64_t start = PosixTransitionInstant(year, daylight.start, zone.standardOffset);
	int64_t end = PosixTransitionInstant(year, daylight.end, daylight.utcOffset);
	/* South of the equator daylight saving time spans the turn of the year. */
	bool isDaylight = start < end ? (instant >= start && instant < end) : !(instant >= end && instant < start);
	return isDaylight ? daylight.utcOffset : zone.standardOffset;
}

/* ---- Zone rules and the zoneinfo reader (RFC 8536) ----------------------- */

struct ZoneRules {
	std::vector<int64_t> transitionTimes; // ascending
	std::vector<int64_t> transitionOffsets; // the UT offset from each transition on
	int64_t initialOffset = 0; // before the first transition: time type 0
	std::optional<PosixZone> footer; // after the last transition

	int64_t OffsetAt(int64_t instant) const
	{
		if (transitionTimes.empty())
			return footer ? PosixZoneOffsetAt(*footer, instant) : initialOffset;
		if (instant < transitionTimes.front())
			return initialOffset;
		if (instant > transitionTimes.back() && footer)
			return PosixZoneOffsetAt(*footer, instant);
		auto after = std::upper_bound(transitionTimes.begin(), transitionTimes.end(), instant);
		return transitionOffsets[static_cast<size_t>(after - transitionTimes.begin()) - 1];
	}

	/* The instants that could show localSeconds lie within a day either
	 * way; the offsets in effect there are the candidates. None fits when
	 * the civil time was skipped: it is then read with the offset from
	 * before the gap, which moves it forward by the gap. */
	int64_t UtcFromLocal(int64_t localSeconds) const
	{
		int64_t offsetBefore = OffsetAt(localSeconds - kOffsetReachSeconds);
		int64_t candidateOffsets[] = {
			offsetBefore,
			OffsetAt(localSeconds),
			OffsetAt(localSeconds + kOffsetReachSeconds),
		};
		std::optional<int64_t> earliest;
		for (int64_t offset : candidateOffsets) {
			int64_t instant = localSeconds - offset;
			if (OffsetAt(instant) != offset)
				continue;
			if (!earliest || instant < *earliest)
				earliest = instant;
		}
		return earliest ? *earliest : localSeconds - offsetBefore;
	}
};

ZoneRules UtcRules()
{
	return ZoneRules();
}

/* Big-endian fields of a zoneinfo file. */
class ByteReader {
public:
	explicit ByteReader(std::string_view bytes) : m_bytes(bytes) {}

	uint64_t Remaining() const { return m_bytes.size() - m_position; }

	bool Skip(uint64_t count)
	{
		if (count > Remaining())
			return false;
		m_position += static_cast<size_t>(count);
		return true;
	}

	std::optional<uint64_t> Unsigned(size_t width)
	{
		if (width > Remaining())
			return std::nullopt;
		uint64_t value = 0;
		for (size_t i = 0; i < width; ++i)
			value = (value << 8) | static_cast<uint8_t>(m_bytes[m_position + i]);
		m_position += width;
		return value;
	}

	/* A two's complement field of 4 or 8 bytes. */
	std::optional<int64_t> Signed(size_t width)
	{
		std::optional<uint64_t> raw = Unsigned(width);
		if (!raw)
			return std::nullopt;
		if (width == 8)
			return static_cast<int64_t>(*raw);
		return static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(*raw)));
	}

	std::string_view Take(size_t count)
	{
		std::string_view taken = m_bytes.substr(m_position, count);
		m_position += taken.size();
		return taken;
	}

	std::string_view Rest() const { return m_bytes.substr(m_position); }

private:
	std::string_view m_bytes;
	size_t m_position = 0;
};

struct TzifHeader {
	char version;
	uint64_t utIndicatorCount;
	uint64_t standardIndicatorCount;
	uint64_t leapSecondCount;
	uint64_t transitionCount;
	uint64_t typeCount;
	uint64_t designationBytes;
};

std::optional<TzifHeader> ReadTzifHeader(ByteReader &reader)
{
	if (reader.Take(4) != "TZif")
		return std::nullopt;
	std::optional<uint64_t> version = reader.Unsigned(1);
	if (!version || !reader.Skip(15))
		return std::nullopt;

	uint64_t counts[6];
	for (uint64_t &count : counts) {
		std::optional<uint64_t> value = reader.Unsigned(4);
		if (!value)
			return std::nullopt;
		count = *value;
	}
	TzifHeader header{static_cast<char>(*version), counts[0], counts[1], counts[2], counts[3], counts[4], counts[5]};
	if (header.typeCount == 0)
		return std::nullopt;
	if (header.utIndicatorCount != 0 && header.utIndicatorCount != header.typeCount)
		return std::nullopt;
	if (header.standardIndicatorCount != 0 && header.standardIndicatorCount != header.typeCount)
		return std::nullopt;
	return header;
}

/* The size of a data block; counts are 32-bit, so this cannot overflow. */
uint64_t TzifBlockSize(const TzifHeader &header, uint64_t timeSize)
{
	return header.transitionCount * timeSize + header.transitionCount + header.typeCount * 6
		+ header.designationBytes + header.leapSecondCount * (timeSize + 4)
		+ header.standardIndicatorCount + header.utIndicatorCount;
}

/* Files with leap seconds ("right/" zones) count time differently from
 * every other zone here and are rejected rather than read wrong. */
std::optional<ZoneRules> ReadTzifBlock(ByteReader &reader, const TzifHeader &header, size_t timeSize)
{
	if (header.leapSecondCount != 0 || TzifBlockSize(header, timeSize) > reader.Remaining())
		return std::nullopt;

	ZoneRules rules;
	for (uint64_t i = 0; i < header.transitionCount; ++i) {
		int64_t time = *reader.Signed(timeSize);
		if (!rules.transitionTimes.empty() && time <= rules.transitionTimes.back())
			return std::nullopt;
		rules.transitionTimes.push_back(time);
	}
	std::vector<uint64_t> typeIndices;
	for (uint64_t i = 0; i < header.transitionCount; ++i) {
		uint64_t index = *reader.Unsigned(1);
		if (index >= header.typeCount)
			return std::nullopt;
		typeIndices.push_back(index);
	}
	std::vector<int64_t> typeOffsets;
	for (uint64_t i = 0; i < header.typeCount; ++i) {
		int64_t utcOffset = *reader.Signed(4);
		reader.Skip(2); // is-DST flag and designation index: not needed for offsets
		if (utcOffset < kMinUtcOffset || utcOffset > kMaxUtcOffset)
			return std::nullopt;
		typeOffsets.push_back(utcOffset);
	}
	reader.Skip(header.designationBytes + header.standardIndicatorCount + header.utIndicatorCount);

	rules.initialOffset = typeOffsets[0];
	for (uint64_t index : typeIndices)
		rules.transitionOffsets.push_back(typeOffsets[static_cast<size_t>(index)]);
	return rules;
}

/* The footer of a version 2+ file; an empty TZ string leaves the times
 * after the last transition to that transition. */
struct TzifFooter {
	std::optional<PosixZone> rule;
};

/* "\nTZ string\n" after the version 2+ data. */
std::optional<TzifFooter> ReadTzifFooter(ByteReader &reader)
{
	std::string_view rest = reader.Rest();
	if (rest.empty() || rest.front() != '\n')
		return std::nullopt;
	size_t end = rest.find('\n', 1);
	if (end == std::string_view::npos)
		return std::nullopt;
	std::string_view text = rest.substr(1, end - 1);
	if (text.empty())
		return TzifFooter{};
	std::optional<PosixZone> rule = ParsePosixZone(text);
	if (!rule)
		return std::nullopt;
	return TzifFooter{rule};
}

std::optional<ZoneRules> ParseTzif(std::string_view bytes)
{
	ByteReader reader(bytes);
	std::optional<TzifHeader> header = ReadTzifHeader(reader);
	if (!header)
		return std::nullopt;
	if (header->version == '\0')
		return ReadTzifBlock(reader, *header, 4);

	/* Version 2 and later repeat the data with 64-bit times after the
	 * version 1 block; only that second copy and its footer are read. */
	if (!reader.Skip(TzifBlockSize(*header, 4)))
		return std::nullopt;
	std::optional<TzifHeader> header64 = ReadTzifHeader(reader);
	if (!header64)
		return std::nullopt;
	std::optional<ZoneRules> rules = ReadTzifBlock(reader, *header64, 8);
	if (!rules)
		return std::nullopt;
	std::optional<TzifFooter> footer = ReadTzifFooter(reader);
	if (!footer)
		return std::nullopt;
	rules->footer = footer->rule;
	return rules;
}

struct FileCloser {
	void operator()(FILE *file) const { fclose(file); }
};

std::optional<std::string> ReadSmallFile(const std::string &path)
{
	std::unique_ptr<FILE, FileCloser> file(fopen(path.c_str(), "rb"));
	if (!file)
		return std::nullopt;
	std::string contents;
	char buffer[4096];
	size_t got;
	while ((got = fread(buffer, 1, sizeof buffer, file.get())) > 0) {
		contents.append(buffer, got);
		if (contents.size() > kMaxZoneFileSize)
			return std::nullopt;
	}
	if (ferror(file.get()))
		return std::nullopt;
	return contents;
}

std::optional<ZoneRules> ReadTzifFile(const std::string &path)
{
	std::optional<std::string> bytes = ReadSmallFile(path);
	if (!bytes)
		return std::nullopt;
	return ParseTzif(*bytes);
}

bool IsZoneNameChar(char c)
{
	return IsAsciiAlpha(c) || IsAsciiDigit(c) || c == '_' || c == '+' || c == '-' || c == '.';
}

/* A relative path of plain components, which cannot leave the zoneinfo
 * directory. */
bool IsSafeZoneName(std::string_view name)
{
	if (name.empty() || name.front() == '/')
		return false;
	size_t componentStart = 0;
	while (componentStart <= name.size()) {
		size_t slash = name.find('/', componentStart);
		size_t componentEnd = slash == std::string_view::npos ? name.size() : slash;
		std::string_view component = name.substr(componentStart, componentEnd - componentStart);
		if (component.empty() || component == "." || component == "..")
			return false;
		if (!std::all_of(component.begin(), component.end(), IsZoneNameChar))
			return false;
		componentStart = componentEnd + 1;
	}
	return true;
}

std::string ZoneinfoDirectory(const std::string &requested)
{
	if (!requested.empty())
		return requested;
	const char *fromEnvironment = getenv("TZDIR");
	if (fromEnvironment != nullptr && *fromEnvironment != '\0')
		return fromEnvironment;
	return kDefaultZoneinfoDir;
}

std::optional<ZoneRules> RulesNamed(std::string_view name, const std::string &zoneinfoDir)
{
	if (name == "UTC" || name == "floating")
		return UtcRules();
	if (!IsSafeZoneName(name))
		return std::nullopt;
	return ReadTzifFile(zoneinfoDir + "/" + std::string(name));
}

/* The TZ environment variable: a leading ':' is dropped and an empty value
 * is UTC. Nothing when it is unset or names no zone. */
std::optional<ZoneRules> RulesFromTzVariable(const std::string &zoneinfoDir)
{
	const char *variable = getenv("TZ");
	if (variable == nullptr)
		return std::nullopt;
	std::string_view value(variable);
	if (!value.empty() && value.front() == ':')
		value.remove_prefix(1);
	if (value.empty())
		return UtcRules();
	return RulesNamed(value, zoneinfoDir);
}

} // namespace

/* ---- TimeZone ------------------------------------------------------------ */

struct TimeZone::Data {
	std::string name;
	ZoneRules rules;
};

std::optional<TimeZone> TimeZone::Named(const std::string &name, const std::string &zoneinfoDir)
{
	std::string directory = ZoneinfoDirectory(zoneinfoDir);
	TimeZone zone;
	if (name == "local") {
		std::optional<ZoneRules> rules = RulesFromTzVariable(directory);
		if (!rules)
			rules = ReadTzifFile(kLocalTimeFile);
		if (rules)
			zone.m_data = std::make_shared<const Data>(Data{name, std::move(*rules)});
		else
			zone.m_data = std::make_shared<const Data>(Data{"floating", UtcRules()});
		return zone;
	}

	std::optional<ZoneRules> rules = RulesNamed(name, directory);
	if (!rules)
		return std::nullopt;
	zone.m_data = std::make_shared<const Data>(Data{name, std::move(*rules)});
	return zone;
}

TimeZone TimeZone::Utc()
{
	TimeZone zone;
	zone.m_data = std::make_shared<const Data>(Data{"UTC", UtcRules()});
	return zone;
}

/* A default-constructed TimeZone has no data and behaves as UTC. */
const std::string &TimeZone::Name() const
{
	static const std::string kDefaultName = "UTC";
	return m_data ? m_data->name : kDefaultName;
}

CivilTime TimeZone::ToCivil(int64_t epochSeconds) const
{
	int64_t instant = std::clamp(epochSeconds, -kCalendarReachSeconds, kCalendarReachSeconds);
	int64_t offset = m_data ? m_data->rules.OffsetAt(instant) : 0;
	return CivilFromLocalSeconds(instant + offset);
}

int64_t TimeZone::ToEpoch(const CivilTime &civil) const
{
	int64_t localSeconds = LocalSecondsFromCivil(civil);
	return m_data ? m_data->rules.UtcFromLocal(localSeconds) : localSeconds;
}

namespace {

/* ---- Durations ----------------------------------------------------------- */

enum class Unit { Second, Minute, Hour, Day, Week, Month, Year };

struct UnitName {
	std::string_view singular;
	std::string_view plural;
	Unit unit;
};

const UnitName kUnitNames[] = {
	{"second", "seconds", Unit::Second},
	{"minute", "minutes", Unit::Minute},
	{"hour", "hours", Unit::Hour},
	{"day", "days", Unit::Day},
	{"week", "weeks", Unit::Week},
	{"month", "months", Unit::Month},
	{"year", "years", Unit::Year},
};

std::optional<Unit> SingularUnit(std::string_view word)
{
	for (const UnitName &name : kUnitNames) {
		if (word == name.singular)
			return name.unit;
	}
	return std::nullopt;
}

/* The reference grammar wants the unit to agree with the count: "1 day",
 * "2 days", and for zero either; "1 days" and "2 day" are no durations. */
std::optional<Unit> UnitAgreeingWith(int64_t count, std::string_view word)
{
	for (const UnitName &name : kUnitNames) {
		bool singularFits = count <= 1 && word == name.singular;
		bool pluralFits = count != 1 && word == name.plural;
		if (singularFits || pluralFits)
			return name.unit;
	}
	return std::nullopt;
}

int64_t MaxCount(Unit unit)
{
	switch (unit) {
	case Unit::Second: return kMaxDurationDays * kSecondsPerDay;
	case Unit::Minute: return kMaxDurationDays * kSecondsPerDay / kSecondsPerMinute;
	case Unit::Hour:   return kMaxDurationDays * kSecondsPerDay / kSecondsPerHour;
	case Unit::Day:    return kMaxDurationDays;
	case Unit::Week:   return kMaxDurationDays / kDaysPerWeek;
	case Unit::Month:  return 10000 * kMonthsPerYear;
	case Unit::Year:   return 10000;
	}
	return 0;
}

Duration DurationOf(int64_t count, Unit unit)
{
	Duration duration;
	switch (unit) {
	case Unit::Second: duration.seconds = count; break;
	case Unit::Minute: duration.seconds = count * kSecondsPerMinute; break;
	case Unit::Hour:   duration.seconds = count * kSecondsPerHour; break;
	case Unit::Day:    duration.days = count; break;
	case Unit::Week:   duration.days = count * kDaysPerWeek; break;
	case Unit::Month:  duration.months = count; break;
	case Unit::Year:   duration.months = count * kMonthsPerYear; break;
	}
	return duration;
}

Duration Negated(const Duration &duration)
{
	return Duration{-duration.months, -duration.days, -duration.seconds};
}

/* "90" "seconds", "1" "month". */
std::optional<Duration> DurationFromWords(std::string_view countWord, std::string_view unitWord)
{
	std::optional<int64_t> count = WholeNumber(countWord, kMaxCountDigits);
	if (!count)
		return std::nullopt;
	std::optional<Unit> unit = UnitAgreeingWith(*count, unitWord);
	if (!unit || *count > MaxCount(*unit))
		return std::nullopt;
	return DurationOf(*count, *unit);
}

/* Lowercased words split at ASCII whitespace. */
std::vector<std::string> SplitWords(std::string_view text)
{
	std::vector<std::string> words;
	std::string current;
	for (char c : text) {
		if (IsAsciiSpace(c)) {
			if (!current.empty())
				words.push_back(std::move(current));
			current.clear();
			continue;
		}
		current.push_back(AsciiLower(c));
	}
	if (!current.empty())
		words.push_back(std::move(current));
	return words;
}

std::string CountOf(int64_t count, std::string_view singular, std::string_view plural)
{
	bool isOne = count == 1 || count == -1;
	return std::to_string(static_cast<long long>(count)) + " " + std::string(isOne ? singular : plural);
}

std::string FormatMonths(int64_t months)
{
	if (months % kMonthsPerYear == 0)
		return CountOf(months / kMonthsPerYear, "year", "years");
	return CountOf(months, "month", "months");
}

std::string FormatDays(int64_t days)
{
	if (days % kDaysPerWeek == 0)
		return CountOf(days / kDaysPerWeek, "week", "weeks");
	return CountOf(days, "day", "days");
}

std::string FormatSeconds(int64_t seconds)
{
	if (seconds % kSecondsPerHour == 0)
		return CountOf(seconds / kSecondsPerHour, "hour", "hours");
	if (seconds % kSecondsPerMinute == 0)
		return CountOf(seconds / kSecondsPerMinute, "minute", "minutes");
	return CountOf(seconds, "second", "seconds");
}

/* ---- Words of the date grammar ------------------------------------------- */

struct NamedNumber {
	std::string_view name;
	int value;
};

const NamedNumber kMonthNames[] = {
	{"january", 1}, {"jan", 1}, {"february", 2}, {"feb", 2}, {"march", 3}, {"mar", 3},
	{"april", 4}, {"apr", 4}, {"may", 5}, {"june", 6}, {"jun", 6}, {"july", 7}, {"jul", 7},
	{"august", 8}, {"aug", 8}, {"september", 9}, {"sep", 9}, {"october", 10}, {"oct", 10},
	{"november", 11}, {"nov", 11}, {"december", 12}, {"dec", 12},
};

/* 0 is Monday: weeks run from Monday to Sunday, as in the reference. */
const NamedNumber kWeekdayNames[] = {
	{"monday", 0}, {"mon", 0}, {"tuesday", 1}, {"tue", 1}, {"tues", 1},
	{"wednesday", 2}, {"wed", 2}, {"thursday", 3}, {"thu", 3}, {"thur", 3}, {"thurs", 3},
	{"friday", 4}, {"fri", 4}, {"saturday", 5}, {"sat", 5}, {"sunday", 6}, {"sun", 6},
};

template <size_t Count>
std::optional<int> LookUp(const NamedNumber (&table)[Count], std::string_view word)
{
	for (const NamedNumber &entry : table) {
		if (entry.name == word)
			return entry.value;
	}
	return std::nullopt;
}

std::optional<int> MonthNamed(std::string_view word)
{
	return LookUp(kMonthNames, word);
}

std::optional<int> WeekdayNamed(std::string_view word)
{
	return LookUp(kWeekdayNames, word);
}

/* next = 1, last = -1, this = 0. */
std::optional<int> Direction(std::string_view word)
{
	if (word == "next")
		return 1;
	if (word == "last")
		return -1;
	if (word == "this")
		return 0;
	return std::nullopt;
}

std::string_view OrdinalSuffix(int day)
{
	if (day % 100 >= 11 && day % 100 <= 13)
		return "th";
	switch (day % 10) {
	case 1:  return "st";
	case 2:  return "nd";
	case 3:  return "rd";
	default: return "th";
	}
}

/* "6", "06" or "6th" (the suffix must be the right one: "1st", not "1th"). */
std::optional<int> DayOfMonthWord(std::string_view word)
{
	size_t digits = 0;
	while (digits < word.size() && IsAsciiDigit(word[digits]))
		++digits;
	std::optional<int64_t> day = WholeNumber(word.substr(0, digits), 2);
	if (!day || *day < 1 || *day > 31)
		return std::nullopt;
	std::string_view suffix = word.substr(digits);
	if (!suffix.empty() && suffix != OrdinalSuffix(static_cast<int>(*day)))
		return std::nullopt;
	return static_cast<int>(*day);
}

/* Exactly four digits, 0001 to 9999. */
std::optional<int> YearWord(std::string_view word)
{
	if (word.size() != 4)
		return std::nullopt;
	std::optional<int64_t> year = WholeNumber(word, 4);
	if (!year || *year < kMinYear)
		return std::nullopt;
	return static_cast<int>(*year);
}

/* ---- Times of day -------------------------------------------------------- */

struct ClockTime {
	int hour = 0;
	int minute = 0;
	int second = 0;
};

/* "H", "H:MM" or "H:MM:SS": the hour unchecked, minutes and seconds two
 * digits each. */
struct ClockDigits {
	int64_t hour;
	int minute;
	int second;
	bool hasMinutes;
};

std::optional<ClockDigits> ParseClockDigits(std::string_view text)
{
	Cursor cursor(text);
	std::optional<int64_t> hour = cursor.Number(2);
	if (!hour)
		return std::nullopt;
	ClockDigits clock{*hour, 0, 0, false};
	if (cursor.Take(':')) {
		std::string_view minutes = cursor.TakeWhile(IsAsciiDigit);
		std::optional<int64_t> minute = minutes.size() == 2 ? WholeNumber(minutes, 2) : std::nullopt;
		if (!minute || *minute > 59)
			return std::nullopt;
		clock.minute = static_cast<int>(*minute);
		clock.hasMinutes = true;
		if (cursor.Take(':')) {
			std::string_view seconds = cursor.TakeWhile(IsAsciiDigit);
			std::optional<int64_t> second = seconds.size() == 2 ? WholeNumber(seconds, 2) : std::nullopt;
			if (!second || *second > 59)
				return std::nullopt;
			clock.second = static_cast<int>(*second);
		}
	}
	if (!cursor.AtEnd())
		return std::nullopt;
	return clock;
}

/* "14:00", "14:00:30"; a bare hour is no time on a 24 hour clock. */
std::optional<ClockTime> TwentyFourHourClock(std::string_view word)
{
	std::optional<ClockDigits> clock = ParseClockDigits(word);
	if (!clock || !clock->hasMinutes || clock->hour > 23)
		return std::nullopt;
	return ClockTime{static_cast<int>(clock->hour), clock->minute, clock->second};
}

enum class Meridiem { Am, Pm };

const std::pair<std::string_view, Meridiem> kMeridiemWords[] = {
	{"am", Meridiem::Am}, {"pm", Meridiem::Pm}, {"a.m.", Meridiem::Am}, {"p.m.", Meridiem::Pm},
};

std::optional<Meridiem> MeridiemWord(std::string_view word)
{
	for (const auto &[name, meridiem] : kMeridiemWords) {
		if (word == name)
			return meridiem;
	}
	return std::nullopt;
}

/* "3", "3:30", "3:30:15" with am or pm: hours 1 to 12, 12am is midnight. */
std::optional<ClockTime> TwelveHourClock(std::string_view digits, Meridiem meridiem)
{
	std::optional<ClockDigits> clock = ParseClockDigits(digits);
	if (!clock || clock->hour < 1 || clock->hour > 12)
		return std::nullopt;
	int hour = static_cast<int>(clock->hour % 12) + (meridiem == Meridiem::Pm ? 12 : 0);
	return ClockTime{hour, clock->minute, clock->second};
}

/* "3pm", "3:30a.m." in one word. */
std::optional<ClockTime> TwelveHourClockWord(std::string_view word)
{
	for (const auto &[name, meridiem] : kMeridiemWords) {
		if (EndsWith(word, name) && word.size() > name.size())
			return TwelveHourClock(word.substr(0, word.size() - name.size()), meridiem);
	}
	return std::nullopt;
}

/* noon and midnight combine with fewer dates than clock times do. */
enum class TimeKind { Clock, NoonOrMidnight };

struct TimePhrase {
	ClockTime time;
	TimeKind kind;
	size_t end; // index of the first word after the phrase
};

using Words = std::vector<std::string>;

std::optional<TimePhrase> ParseTimePhrase(const Words &words, size_t at)
{
	if (at >= words.size())
		return std::nullopt;
	const std::string &word = words[at];
	if (word == "noon")
		return TimePhrase{ClockTime{12, 0, 0}, TimeKind::NoonOrMidnight, at + 1};
	if (word == "midnight")
		return TimePhrase{ClockTime{0, 0, 0}, TimeKind::NoonOrMidnight, at + 1};
	if (std::optional<ClockTime> time = TwelveHourClockWord(word))
		return TimePhrase{*time, TimeKind::Clock, at + 1};
	if (at + 1 < words.size()) {
		if (std::optional<Meridiem> meridiem = MeridiemWord(words[at + 1])) {
			std::optional<ClockTime> time = TwelveHourClock(word, *meridiem);
			if (!time)
				return std::nullopt;
			return TimePhrase{*time, TimeKind::Clock, at + 2};
		}
	}
	if (std::optional<ClockTime> time = TwentyFourHourClock(word))
		return TimePhrase{*time, TimeKind::Clock, at + 1};
	return std::nullopt;
}

/* ---- Dates --------------------------------------------------------------- */

/* What a date phrase is decides which times it combines with. */
enum class DateKind {
	RelativeDay,         // today, tomorrow, yesterday
	Numeric,             // 2026-10-06, 06.10.2026, 10/06/2026
	MonthNameWithoutYear, // 6 october, october 6th
	MonthNameWithYear,   // 6 october 2026
	Weekday,             // monday, next monday
};

struct DatePhrase {
	Date date;
	DateKind kind;
	size_t end; // index of the first word after the phrase
};

/* What ParseDate is relative to. */
struct Reference {
	const TimeZone &zone;
	int64_t now;
	CivilTime civilNow;
	Date today;
};

Date AddDays(const Date &date, int64_t days)
{
	return CivilFromDays(DaysFromDate(date) + days);
}

std::optional<Date> ValidDate(int64_t year, int month, int day)
{
	if (!IsValidDate(year, month, day))
		return std::nullopt;
	return Date{year, month, day};
}

std::optional<DatePhrase> RelativeDayPhrase(const Words &words, size_t at, const Reference &reference)
{
	const std::string &word = words[at];
	int64_t shift;
	if (word == "today")
		shift = 0;
	else if (word == "tomorrow")
		shift = 1;
	else if (word == "yesterday")
		shift = -1;
	else
		return std::nullopt;
	return DatePhrase{AddDays(reference.today, shift), DateKind::RelativeDay, at + 1};
}

/* Splits "a?b?c" at a separator sep into three parts. */
std::optional<std::vector<std::string_view>> ThreeParts(std::string_view text, char separator)
{
	std::vector<std::string_view> parts;
	size_t start = 0;
	for (;;) {
		size_t found = text.find(separator, start);
		parts.push_back(text.substr(start, found == std::string_view::npos ? std::string_view::npos : found - start));
		if (found == std::string_view::npos)
			break;
		start = found + 1;
	}
	if (parts.size() != 3)
		return std::nullopt;
	return parts;
}

/* Year, month and day as 2026-10-06 (also 2026-1-6, 2026/10/06,
 * 2026.10.06), or day, month and year as 06.10.2026, 10/06/2026 or
 * 06-10-2026: the reference reads numeric dates as d/m/y unless they start
 * with a four digit year. */
std::optional<Date> NumericDate(std::string_view word)
{
	size_t separatorAt = word.find_first_of("-/.");
	if (separatorAt == std::string_view::npos)
		return std::nullopt;
	char separator = word[separatorAt];
	std::optional<std::vector<std::string_view>> parts = ThreeParts(word, separator);
	if (!parts)
		return std::nullopt;

	bool yearFirst = (*parts)[0].size() == 4;
	std::string_view yearPart = yearFirst ? (*parts)[0] : (*parts)[2];
	std::string_view monthPart = (*parts)[1];
	std::string_view dayPart = yearFirst ? (*parts)[2] : (*parts)[0];

	std::optional<int> year = YearWord(yearPart);
	std::optional<int64_t> month = WholeNumber(monthPart, 2);
	std::optional<int64_t> day = WholeNumber(dayPart, 2);
	if (!year || !month || !day || *month > 12 || *day > 31)
		return std::nullopt;
	return ValidDate(*year, static_cast<int>(*month), static_cast<int>(*day));
}

std::optional<DatePhrase> NumericDatePhrase(const Words &words, size_t at)
{
	std::optional<Date> date = NumericDate(words[at]);
	if (!date)
		return std::nullopt;
	return DatePhrase{*date, DateKind::Numeric, at + 1};
}

/* [next|last|this] weekday: the day in this week (Monday to Sunday), the
 * next or the last one. */
std::optional<DatePhrase> WeekdayPhrase(const Words &words, size_t at, const Reference &reference)
{
	int weekShift = 0;
	size_t nameAt = at;
	if (std::optional<int> direction = Direction(words[at])) {
		weekShift = *direction;
		nameAt = at + 1;
	}
	if (nameAt >= words.size())
		return std::nullopt;
	std::optional<int> weekday = WeekdayNamed(words[nameAt]);
	if (!weekday)
		return std::nullopt;

	int64_t todayDays = DaysFromDate(reference.today);
	int64_t mondayThisWeek = todayDays - FloorMod(WeekdayFromDays(todayDays) - 1, 7);
	Date date = CivilFromDays(mondayThisWeek + *weekday + kDaysPerWeek * weekShift);
	return DatePhrase{date, DateKind::Weekday, nameAt + 1};
}

/* The day and month, then an optional year; without one, this year. */
std::optional<DatePhrase> FinishMonthNamePhrase(const Words &words, size_t yearAt, int month, int day,
	const Reference &reference)
{
	std::optional<int> year = yearAt < words.size() ? YearWord(words[yearAt]) : std::nullopt;
	int64_t dateYear = year ? *year : reference.today.year;
	std::optional<Date> date = ValidDate(dateYear, month, day);
	if (!date)
		return std::nullopt;
	if (year)
		return DatePhrase{*date, DateKind::MonthNameWithYear, yearAt + 1};
	return DatePhrase{*date, DateKind::MonthNameWithoutYear, yearAt};
}

/* "6 october [2026]", "6th oct". */
std::optional<DatePhrase> DayMonthPhrase(const Words &words, size_t at, const Reference &reference)
{
	if (at + 1 >= words.size())
		return std::nullopt;
	std::optional<int> day = DayOfMonthWord(words[at]);
	std::optional<int> month = MonthNamed(words[at + 1]);
	if (!day || !month)
		return std::nullopt;
	return FinishMonthNamePhrase(words, at + 2, *month, *day, reference);
}

/* "october 6 [2026]", "oct 6th". */
std::optional<DatePhrase> MonthDayPhrase(const Words &words, size_t at, const Reference &reference)
{
	if (at + 1 >= words.size())
		return std::nullopt;
	std::optional<int> month = MonthNamed(words[at]);
	std::optional<int> day = DayOfMonthWord(words[at + 1]);
	if (!month || !day)
		return std::nullopt;
	return FinishMonthNamePhrase(words, at + 2, *month, *day, reference);
}

std::optional<DatePhrase> ParseDatePhrase(const Words &words, size_t at, const Reference &reference)
{
	if (at >= words.size())
		return std::nullopt;
	if (std::optional<DatePhrase> phrase = RelativeDayPhrase(words, at, reference))
		return phrase;
	if (std::optional<DatePhrase> phrase = NumericDatePhrase(words, at))
		return phrase;
	if (std::optional<DatePhrase> phrase = WeekdayPhrase(words, at, reference))
		return phrase;
	if (std::optional<DatePhrase> phrase = DayMonthPhrase(words, at, reference))
		return phrase;
	return MonthDayPhrase(words, at, reference);
}

/* ---- Whole date expressions ---------------------------------------------- */

int64_t EpochAt(const Date &date, const ClockTime &time, const TimeZone &zone)
{
	CivilTime civil;
	civil.year = static_cast<int>(date.year);
	civil.month = date.month;
	civil.day = date.day;
	civil.hour = time.hour;
	civil.minute = time.minute;
	civil.second = time.second;
	return zone.ToEpoch(civil);
}

/* Dates are within years 1 to 9999 in every parser below, so the casts to
 * int in EpochAt are safe; a check of the final result keeps relative
 * expressions inside that range too. */
bool IsWithinCalendar(int64_t epochSeconds, const TimeZone &zone)
{
	int year = zone.ToCivil(epochSeconds).year;
	return year >= kMinYear && year <= kMaxYear;
}

std::optional<int64_t> NowExpression(const Words &words, const Reference &reference)
{
	if (words.size() != 1 || words[0] != "now")
		return std::nullopt;
	return reference.now;
}

/* "2026-10-06T14:00" and "2026-10-06T14:00:30", alone. */
std::optional<int64_t> IsoDateTimeExpression(const Words &words, const Reference &reference)
{
	if (words.size() != 1)
		return std::nullopt;
	size_t separatorAt = words[0].find('t');
	if (separatorAt == std::string::npos)
		return std::nullopt;
	std::string_view word = words[0];
	std::string_view datePart = word.substr(0, separatorAt);
	if (datePart.size() < 5 || datePart[4] != '-')
		return std::nullopt;
	std::optional<Date> date = NumericDate(datePart);
	std::optional<ClockTime> time = TwentyFourHourClock(word.substr(separatorAt + 1));
	if (!date || !time)
		return std::nullopt;
	return EpochAt(*date, *time, reference.zone);
}

/* "3 days ago", "in 2 hours", "5 minutes from now": days, weeks, months and
 * years keep the time of day on the calendar, hours, minutes and seconds
 * count elapsed time. */
std::optional<int64_t> RelativeOffsetExpression(const Words &words, const Reference &reference)
{
	std::optional<Duration> duration;
	if (words.size() == 3 && words[2] == "ago") {
		duration = DurationFromWords(words[0], words[1]);
		if (duration)
			duration = Negated(*duration);
	} else if (words.size() == 3 && words[0] == "in") {
		duration = DurationFromWords(words[1], words[2]);
	} else if (words.size() == 4 && words[2] == "from" && words[3] == "now") {
		duration = DurationFromWords(words[0], words[1]);
	}
	if (!duration)
		return std::nullopt;
	return AddDuration(reference.now, *duration, reference.zone);
}

/* The instant the clock last showed a whole multiple of unit seconds,
 * such as the start of the current hour: now less what the clock shows
 * beyond it, at the offset in effect now. Exact in the hour a daylight
 * saving change repeats, and for offsets of half an hour. */
int64_t StartOfClockUnit(const Reference &reference, int64_t unitSeconds)
{
	return reference.now - FloorMod(LocalSecondsFromCivil(reference.civilNow), unitSeconds);
}

/* "next week", "last month", "this year": the start of the unit, moved by
 * one; weeks and days start at today's midnight, hours and minutes are
 * counted in elapsed time from the start of the current one. */
std::optional<int64_t> RelativeUnitExpression(const Words &words, const Reference &reference)
{
	if (words.size() != 2)
		return std::nullopt;
	std::optional<int> direction = Direction(words[0]);
	std::optional<Unit> unit = SingularUnit(words[1]);
	if (!direction || !unit)
		return std::nullopt;

	const TimeZone &zone = reference.zone;
	const Date &today = reference.today;
	int shift = *direction;
	switch (*unit) {
	case Unit::Second:
		return reference.now + shift;
	case Unit::Minute:
		return StartOfClockUnit(reference, kSecondsPerMinute) + shift * kSecondsPerMinute;
	case Unit::Hour:
		return StartOfClockUnit(reference, kSecondsPerHour) + shift * kSecondsPerHour;
	case Unit::Day:
		return EpochAt(AddDays(today, shift), ClockTime{}, zone);
	case Unit::Week:
		return EpochAt(AddDays(today, shift * kDaysPerWeek), ClockTime{}, zone);
	case Unit::Month: {
		int64_t monthIndex = today.year * kMonthsPerYear + (today.month - 1) + shift;
		Date first{FloorDiv(monthIndex, kMonthsPerYear), static_cast<int>(FloorMod(monthIndex, kMonthsPerYear)) + 1, 1};
		if (first.year < kMinYear || first.year > kMaxYear)
			return std::nullopt;
		return EpochAt(first, ClockTime{}, zone);
	}
	case Unit::Year: {
		int64_t year = today.year + shift;
		if (year < kMinYear || year > kMaxYear)
			return std::nullopt;
		return EpochAt(Date{year, 1, 1}, ClockTime{}, zone);
	}
	}
	return std::nullopt;
}

bool CombinesWithNoonOrMidnight(DateKind kind)
{
	return kind == DateKind::RelativeDay || kind == DateKind::Numeric;
}

bool CombinesWithLeadingTime(DateKind kind)
{
	return kind == DateKind::RelativeDay || kind == DateKind::MonthNameWithoutYear || kind == DateKind::Weekday;
}

/* A date, then optionally "at" and a time: "tomorrow at 3pm". */
std::optional<int64_t> DateThenTimeExpression(const Words &words, const Reference &reference)
{
	std::optional<DatePhrase> date = ParseDatePhrase(words, 0, reference);
	if (!date)
		return std::nullopt;
	if (date->end == words.size())
		return EpochAt(date->date, ClockTime{}, reference.zone);

	size_t timeAt = words[date->end] == "at" ? date->end + 1 : date->end;
	std::optional<TimePhrase> time = ParseTimePhrase(words, timeAt);
	if (!time || time->end != words.size())
		return std::nullopt;
	if (time->kind == TimeKind::NoonOrMidnight && !CombinesWithNoonOrMidnight(date->kind))
		return std::nullopt;
	return EpochAt(date->date, time->time, reference.zone);
}

/* A clock time, then a date: "3pm tomorrow". */
std::optional<int64_t> TimeThenDateExpression(const Words &words, const Reference &reference)
{
	std::optional<TimePhrase> time = ParseTimePhrase(words, 0);
	if (!time || time->kind != TimeKind::Clock)
		return std::nullopt;
	std::optional<DatePhrase> date = ParseDatePhrase(words, time->end, reference);
	if (!date || date->end != words.size() || !CombinesWithLeadingTime(date->kind))
		return std::nullopt;
	return EpochAt(date->date, time->time, reference.zone);
}

/* A time alone is today. */
std::optional<int64_t> TimeAloneExpression(const Words &words, const Reference &reference)
{
	std::optional<TimePhrase> time = ParseTimePhrase(words, 0);
	if (!time || time->end != words.size())
		return std::nullopt;
	return EpochAt(reference.today, time->time, reference.zone);
}

/* The reference ignores commas at the end of words, and words that are
 * only commas: "october 6, 2026", "6 october , 2026". */
Words WithoutTrailingCommas(const Words &words)
{
	Words stripped;
	for (std::string word : words) {
		while (!word.empty() && word.back() == ',')
			word.pop_back();
		if (!word.empty())
			stripped.push_back(std::move(word));
	}
	return stripped;
}

using Expression = std::optional<int64_t> (*)(const Words &, const Reference &);

const Expression kDateExpressions[] = {
	NowExpression,
	IsoDateTimeExpression,
	RelativeOffsetExpression,
	RelativeUnitExpression,
	DateThenTimeExpression,
	TimeThenDateExpression,
	TimeAloneExpression,
};

} // namespace

/* ---- Public functions ---------------------------------------------------- */

std::optional<int64_t> ParseDate(std::string_view text, const TimeZone &zone, int64_t now)
{
	Words words = WithoutTrailingCommas(SplitWords(text));
	if (words.empty())
		return std::nullopt;

	CivilTime civilNow = zone.ToCivil(now);
	if (civilNow.year < kMinYear || civilNow.year > kMaxYear)
		return std::nullopt;
	Reference reference{zone, now, civilNow, Date{civilNow.year, civilNow.month, civilNow.day}};

	for (Expression expression : kDateExpressions) {
		std::optional<int64_t> result = expression(words, reference);
		if (!result)
			continue;
		if (!IsWithinCalendar(*result, zone))
			return std::nullopt;
		return result;
	}
	return std::nullopt;
}

std::optional<Duration> ParseDuration(std::string_view text)
{
	std::vector<std::string> words = SplitWords(text);
	size_t countAt = !words.empty() && words[0] == "for" ? 1 : 0;
	if (words.size() != countAt + 2)
		return std::nullopt;
	return DurationFromWords(words[countAt], words[countAt + 1]);
}

int64_t AddDuration(int64_t epochSeconds, const Duration &duration, const TimeZone &zone)
{
	int64_t months = std::clamp(duration.months, -kMaxAddedMonths, kMaxAddedMonths);
	int64_t days = std::clamp(duration.days, -kMaxAddedDays, kMaxAddedDays);
	int64_t seconds = std::clamp(duration.seconds, -kMaxAddedSeconds, kMaxAddedSeconds);
	if (months == 0 && days == 0)
		return SaturatingAdd(epochSeconds, seconds);

	CivilTime civil = zone.ToCivil(epochSeconds);
	int64_t monthIndex = static_cast<int64_t>(civil.year) * kMonthsPerYear + (civil.month - 1) + months;
	int64_t year = FloorDiv(monthIndex, kMonthsPerYear);
	int month = static_cast<int>(FloorMod(monthIndex, kMonthsPerYear)) + 1;
	int day = std::min(civil.day, DaysInMonth(year, month));
	Date moved = CivilFromDays(DaysFromCivil(year, month, day) + days);

	CivilTime target = civil;
	target.year = static_cast<int>(moved.year);
	target.month = moved.month;
	target.day = moved.day;
	return SaturatingAdd(zone.ToEpoch(target), seconds);
}

std::string FormatIso(int64_t epochSeconds, const TimeZone &zone)
{
	CivilTime civil = zone.ToCivil(epochSeconds);
	long long year = civil.year;
	char buffer[64];
	snprintf(buffer, sizeof buffer, "%s%04lld-%02d-%02dT%02d:%02d:%02d", year < 0 ? "-" : "",
		year < 0 ? -year : year, civil.month, civil.day, civil.hour, civil.minute, civil.second);
	return buffer;
}

/* Each part in its largest exact unit; parts that cannot be merged (months,
 * days and seconds have no fixed ratio) are joined with spaces. */
std::string FormatDuration(const Duration &duration)
{
	std::vector<std::string> parts;
	if (duration.months != 0)
		parts.push_back(FormatMonths(duration.months));
	if (duration.days != 0)
		parts.push_back(FormatDays(duration.days));
	if (duration.seconds != 0)
		parts.push_back(FormatSeconds(duration.seconds));
	if (parts.empty())
		return "0 seconds";

	std::string text = parts[0];
	for (size_t i = 1; i < parts.size(); ++i)
		text += " " + parts[i];
	return text;
}

} // namespace smgetopt::temporal
