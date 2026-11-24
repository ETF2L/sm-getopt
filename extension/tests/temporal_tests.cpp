/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Tests of the dates, durations and time zones in temporal.cpp. The expected
 * dates were taken from DateTime::Format::Natural 1.27 for the same "now",
 * except where docs/dates.md says this parser differs. Zone tests read
 * zoneinfo files the test writes itself; tests of real zones skip themselves
 * where /usr/share/zoneinfo is missing. */
#include "check.h"
#include "temporal.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

using namespace smgetopt::temporal;

namespace {

/* 2026-11-14T10:20:30 UTC, a Saturday. */
constexpr int64_t kNow = 1794651630;
/* 2026-11-15T10:20:30 UTC, a Sunday. */
constexpr int64_t kSunday = 1794738030;

const char *const kSystemZoneinfo = "/usr/share/zoneinfo";

struct DateCase {
	const char *text;
	int64_t expected;
};

/* What ParseDate made of text, for readable failures: the epoch or -1. */
int64_t Parsed(const std::string &text, const TimeZone &zone, int64_t now)
{
	std::optional<int64_t> result = ParseDate(text, zone, now);
	return result ? *result : -1;
}

std::string Iso(int64_t epoch, const TimeZone &zone)
{
	return FormatIso(epoch, zone);
}

CivilTime Civil(int year, int month, int day, int hour = 0, int minute = 0, int second = 0)
{
	CivilTime civil;
	civil.year = year;
	civil.month = month;
	civil.day = day;
	civil.hour = hour;
	civil.minute = minute;
	civil.second = second;
	return civil;
}

/* A real zone from the system, or nothing (with a note) without tzdata. */
std::optional<TimeZone> SystemZone(const char *name)
{
	std::optional<TimeZone> zone = TimeZone::Named(name, kSystemZoneinfo);
	if (!zone)
		printf("note: %s/%s not readable, skipping its checks\n", kSystemZoneinfo, name);
	return zone;
}

/* ---- Writing zoneinfo files ---------------------------------------------- */

struct TzifType {
	int32_t utcOffset;
	bool isDst;
};

struct TzifSpec {
	char version = '2';
	std::vector<int64_t> times;
	std::vector<uint8_t> typeIndices;
	std::vector<TzifType> types;
	std::string footer;
	uint32_t leapSeconds = 0;
};

void AppendBigEndian(std::string &out, uint64_t value, int width)
{
	for (int shift = (width - 1) * 8; shift >= 0; shift -= 8)
		out.push_back(static_cast<char>((value >> shift) & 0xff));
}

void AppendTzifBlock(std::string &out, const TzifSpec &spec, int timeSize)
{
	const std::string designations("ZZZ\0", 4);
	out += "TZif";
	out.push_back(spec.version);
	out.append(15, '\0');
	AppendBigEndian(out, 0, 4); // UT indicators
	AppendBigEndian(out, 0, 4); // standard indicators
	AppendBigEndian(out, spec.leapSeconds, 4);
	AppendBigEndian(out, spec.times.size(), 4);
	AppendBigEndian(out, spec.types.size(), 4);
	AppendBigEndian(out, designations.size(), 4);
	for (int64_t time : spec.times)
		AppendBigEndian(out, static_cast<uint64_t>(time), timeSize);
	for (uint8_t index : spec.typeIndices)
		out.push_back(static_cast<char>(index));
	for (const TzifType &type : spec.types) {
		AppendBigEndian(out, static_cast<uint32_t>(type.utcOffset), 4);
		out.push_back(type.isDst ? 1 : 0);
		out.push_back(0);
	}
	out += designations;
	for (uint32_t i = 0; i < spec.leapSeconds; ++i) {
		AppendBigEndian(out, 78796800 + i, timeSize);
		AppendBigEndian(out, i + 1, 4);
	}
}

std::string TzifBytes(const TzifSpec &spec)
{
	std::string out;
	AppendTzifBlock(out, spec, 4);
	if (spec.version == '\0')
		return out;
	AppendTzifBlock(out, spec, 8);
	out += "\n" + spec.footer + "\n";
	return out;
}

/* A directory under the system's temp directory, removed with everything
 * written through it. */
class TempDir {
public:
	TempDir()
	{
		const char *base = getenv("TMPDIR");
		std::string pattern = std::string(base != nullptr && *base != '\0' ? base : "/tmp") + "/temporal-tests-XXXXXX";
		std::vector<char> buffer(pattern.begin(), pattern.end());
		buffer.push_back('\0');
		if (mkdtemp(buffer.data()) != nullptr)
			m_path = buffer.data();
	}

	~TempDir()
	{
		for (auto it = m_created.rbegin(); it != m_created.rend(); ++it)
			remove(it->c_str());
		if (!m_path.empty())
			remove(m_path.c_str());
	}

	TempDir(const TempDir &) = delete;
	TempDir &operator=(const TempDir &) = delete;

	bool IsReady() const { return !m_path.empty(); }
	const std::string &Path() const { return m_path; }

	/* Writes relativePath, creating its directories. */
	bool Write(const std::string &relativePath, const std::string &contents)
	{
		for (size_t slash = relativePath.find('/'); slash != std::string::npos; slash = relativePath.find('/', slash + 1)) {
			std::string directory = m_path + "/" + relativePath.substr(0, slash);
			if (mkdir(directory.c_str(), 0700) == 0)
				m_created.push_back(directory);
		}
		std::string path = m_path + "/" + relativePath;
		FILE *file = fopen(path.c_str(), "wb");
		if (file == nullptr)
			return false;
		m_created.push_back(path);
		bool written = fwrite(contents.data(), 1, contents.size(), file) == contents.size();
		return fclose(file) == 0 && written;
	}

private:
	std::string m_path;
	std::vector<std::string> m_created;
};

/* A Berlin-like test zone: +01 (type 0), +02 in summer; two transitions in
 * 2020 and the EU rule in the footer for everything after them. */
TzifSpec TestZoneSpec()
{
	TzifSpec spec;
	spec.times = {1585443600, 1603587600}; // 2020-03-29T01:00Z, 2020-10-25T01:00Z
	spec.typeIndices = {1, 0};
	spec.types = {{3600, false}, {7200, true}};
	spec.footer = "TST-1TDT,M3.5.0,M10.5.0/3";
	return spec;
}

/* A zone of the footer alone, without transitions. */
TzifSpec FooterOnlySpec(const std::string &footer, int32_t utcOffset)
{
	TzifSpec spec;
	spec.types = {{utcOffset, false}};
	spec.footer = footer;
	return spec;
}

/* Loads spec as zone "Test/Zone" from a fresh zoneinfo directory. */
std::optional<TimeZone> LoadSpec(const TzifSpec &spec)
{
	TempDir dir;
	if (!dir.IsReady() || !dir.Write("Test/Zone", TzifBytes(spec)))
		return std::nullopt;
	return TimeZone::Named("Test/Zone", dir.Path());
}

/* unistd.h's access() is not used: its getopt() clashes with the
 * namespace smgetopt. */
bool IsReadable(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (file == nullptr)
		return false;
	fclose(file);
	return true;
}

/* Restores the TZ environment variable at the end of a test. */
class TzVariableGuard {
public:
	TzVariableGuard()
	{
		const char *value = getenv("TZ");
		if (value != nullptr)
			m_saved = value;
	}

	~TzVariableGuard()
	{
		if (m_saved)
			setenv("TZ", m_saved->c_str(), 1);
		else
			unsetenv("TZ");
	}

private:
	std::optional<std::string> m_saved;
};

} // namespace

/* ---- ParseDate in UTC, against the reference ---------------------------- */

TEST(ParseDateMatchesReference)
{
	TimeZone utc = TimeZone::Utc();
	const DateCase cases[] = {
		{"now", 1794651630},
		{"today", 1794614400},
		{"tomorrow", 1794700800},
		{"yesterday", 1794528000},
		{"noon", 1794657600},
		{"midnight", 1794614400},
		{"Today", 1794614400},
		{"NOW", 1794651630},
		{"tomorrow 3pm", 1794754800},
		{"tomorrow at 15:00", 1794754800},
		{"yesterday at noon", 1794571200},
		{"today 9:30am", 1794648600},
		{"tomorrow noon", 1794744000},
		{"today at midnight", 1794614400},
		{"tomorrow  at   3pm", 1794754800},
		{"3pm tomorrow", 1794754800},
		{"15:00 tomorrow", 1794754800},
		{"3:30 pm yesterday", 1794583800},
		{"14:00", 1794664800},
		{"14:00:30", 1794664830},
		{"00:30", 1794616200},
		{"3pm", 1794668400},
		{"3 pm", 1794668400},
		{"3:30 pm", 1794670200},
		{"3:30:15pm", 1794670215},
		{"3am", 1794625200},
		{"3 a.m.", 1794625200},
		{"12am", 1794614400},
		{"12pm", 1794657600},
		{"12:30am", 1794616200},
		{"23:59:59", 1794700799},
		{"2026-10-06", 1791244800},
		{"2026-1-6", 1767657600},
		{"2026/10/06", 1791244800},
		{"2026.10.06", 1791244800},
		{"2026-10-06 14:00", 1791295200},
		{"2026-10-06 14:00:30", 1791295230},
		{"2026-10-06 3pm", 1791298800},
		{"2026-10-06 at noon", 1791288000},
		{"2026-10-06T14:00:00", 1791295200},
		{"2026-10-06T14:00", 1791295200},
		{"10/06/2026", 1781049600},
		{"06.10.2026", 1791244800},
		{"6.10.2026", 1791244800},
		{"06-10-2026", 1791244800},
		{"31/12/2026", 1798675200},
		{"06.10.2026 midnight", 1791244800},
		{"6 october 2026", 1791244800},
		{"october 6 2026", 1791244800},
		{"october 6, 2026", 1791244800},
		{"6 october , 2026", 1791244800},
		{"6 oct", 1791244800},
		{"oct 6", 1791244800},
		{"OCT 6", 1791244800},
		{"6th october", 1791244800},
		{"october 6th", 1791244800},
		{"1st january", 1767225600},
		{"2nd feb", 1769990400},
		{"3rd march 2027", 1804032000},
		{"11th october", 1791676800},
		{"21st october", 1792540800},
		{"22nd october", 1792627200},
		{"23rd april", 1776902400},
		{"sep 6", 1788652800},
		{"6 september", 1788652800},
		{"jun 6", 1780704000},
		{"6 jul", 1783296000},
		{"may 5", 1777939200},
		{"dec 25", 1798156800},
		{"6 october 2026 14:00", 1791295200},
		{"6 october 2026 3pm", 1791298800},
		{"6 october 3pm", 1791298800},
		{"october 6 at 3pm", 1791298800},
		{"3pm 6 october", 1791298800},
		{"3pm october 6th", 1791298800},
		{"29 february 2028", 1835395200},
		{"6 october 0001", -62111577600},
		{"6 october 9999", 253394784000},
		{"monday", 1794182400},
		{"mon", 1794182400},
		{"saturday", 1794614400},
		{"sunday", 1794700800},
		{"friday", 1794528000},
		{"tue", 1794268800},
		{"tues", 1794268800},
		{"wed", 1794355200},
		{"thu", 1794441600},
		{"thur", 1794441600},
		{"thurs", 1794441600},
		{"next monday", 1794787200},
		{"next saturday", 1795219200},
		{"next sunday", 1795305600},
		{"last monday", 1793577600},
		{"last sunday", 1794096000},
		{"last friday", 1793923200},
		{"this monday", 1794182400},
		{"this sunday", 1794700800},
		{"monday 3pm", 1794236400},
		{"monday at 15:00", 1794236400},
		{"3pm monday", 1794236400},
		{"next monday at 3pm", 1794841200},
		{"this friday 9am", 1794560400},
		{"3 days ago", 1794392430},
		{"2 weeks ago", 1793442030},
		{"1 year ago", 1763115630},
		{"3 months ago", 1786702830},
		{"3 hours ago", 1794640830},
		{"10 minutes ago", 1794651030},
		{"1 second ago", 1794651629},
		{"0 days ago", 1794651630},
		{"0 day ago", 1794651630},
		{"03 days ago", 1794392430},
		{"in 2 hours", 1794658830},
		{"in 1 day", 1794738030},
		{"in 2 weeks", 1795861230},
		{"in 1 month", 1797243630},
		{"in 1 year", 1826187630},
		{"in 30 seconds", 1794651660},
		{"IN  2   HOURS", 1794658830},
		{"5 minutes from now", 1794651930},
		{"2 days from now", 1794824430},
		{"1 week from now", 1795256430},
		{"next week", 1795219200},
		{"last week", 1794009600},
		{"this week", 1794614400},
		{"next month", 1796083200},
		{"last month", 1790812800},
		{"this month", 1793491200},
		{"next year", 1798761600},
		{"last year", 1735689600},
		{"this year", 1767225600},
		{"next day", 1794700800},
		{"last day", 1794528000},
		{"next hour", 1794654000},
		{"last hour", 1794646800},
		{"this hour", 1794650400},
		{"next minute", 1794651660},
		{"last minute", 1794651540},
		{"this minute", 1794651600},
		{"next second", 1794651631},
		{"last second", 1794651629},
	};
	for (const DateCase &dateCase : cases)
		CHECK_EQ(Parsed(dateCase.text, utc, kNow), dateCase.expected);
}

TEST(ParseDateWeekdaysFromSunday)
{
	TimeZone utc = TimeZone::Utc();
	CHECK_EQ(Parsed("monday", utc, kSunday), 1794182400);
	CHECK_EQ(Parsed("sunday", utc, kSunday), 1794700800);
	CHECK_EQ(Parsed("next monday", utc, kSunday), 1794787200);
	CHECK_EQ(Parsed("last sunday", utc, kSunday), 1794096000);
	CHECK_EQ(Parsed("this week", utc, kSunday), 1794700800);
}

TEST(ParseDateRejectsWhatTheReferenceRejects)
{
	TimeZone utc = TimeZone::Utc();
	const char *const rejected[] = {
		"", "   ", ",", "noon tomorrow", "midnight tomorrow", "noon today", "24:00", "25:00", "13pm", "0pm",
		"15:00 pm", "15:60", "15:59:60", "3:5", "2026-02-31", "2026-13-01", "12/31/2026", "10/06", "06.10.",
		"sept 6", "6 sept", "31 february", "30 february", "29 february 2027", "31 april", "6 october 10000",
		"6 october 1", "6 october 99", "6 october 0", "6 october noon", "6 october at noon", "monday noon",
		"last friday at noon", "at 3pm", "at noon", "at tomorrow", "tomorrow at at 3pm", "tomorrow 3pm 4pm",
		"tomorrow tomorrow", "2026-10-06 2026-10-07", "1 seconds ago", "2 second ago", "1 days from now",
		"in 1 days", "in 2 day", "a day ago", "in a day", "3 days", "in 3 days at 3pm", "3 days ago at noon",
		"next weekend", "next weeks", "last months", "1th october", "2th october", "11st october",
		"32 october", "0 october", "october 0", "6th", "1st", "now 3pm", "now please", "3pm 2026-10-06",
		"3pm 6 october 2026", "at 3pm tomorrow", "2026-10-06T3pm", "october 6,2026", "6. october", "6 oct.",
		"the 6th of october", "6 october at", "tomorrow at",
	};
	for (const char *text : rejected)
		CHECK_EQ(Parsed(text, utc, kNow), -1);
}

/* Forms the reference reads that this parser leaves out on purpose (see
 * docs/dates.md). */
TEST(ParseDateRejectsUnsupportedForms)
{
	TimeZone utc = TimeZone::Utc();
	const char *const unsupported[] = {
		"october", "october 2026", "6", "2026", "6/10/26", "26-10-06", "tomorrow evening", "noon next monday",
		"in 10000 years", "3000 years ago",
	};
	for (const char *text : unsupported)
		CHECK_EQ(Parsed(text, utc, kNow), -1);
}

TEST(ParseDateMonthEnds)
{
	TimeZone utc = TimeZone::Utc();
	const int64_t january31 = 1769853600; // 2026-01-31T10:00:00
	CHECK_EQ(Iso(Parsed("in 1 month", utc, january31), utc), "2026-02-28T10:00:00");
	CHECK_EQ(Iso(Parsed("in 13 months", utc, january31), utc), "2027-02-28T10:00:00");
	CHECK_EQ(Iso(Parsed("1 month ago", utc, january31), utc), "2025-12-31T10:00:00");
	CHECK_EQ(Iso(Parsed("next month", utc, january31), utc), "2026-02-01T00:00:00");
	CHECK_EQ(Iso(Parsed("last month", utc, 1774951200), utc), "2026-02-01T00:00:00"); // from 2026-03-31

	const int64_t leapDay = 1709200800; // 2024-02-29T10:00:00
	CHECK_EQ(Iso(Parsed("1 year ago", utc, leapDay), utc), "2023-02-28T10:00:00");
	CHECK_EQ(Iso(Parsed("in 1 year", utc, leapDay), utc), "2025-02-28T10:00:00");
	CHECK_EQ(Iso(Parsed("in 4 years", utc, leapDay), utc), "2028-02-29T10:00:00");
	CHECK_EQ(Iso(Parsed("29 february", utc, leapDay), utc), "2024-02-29T00:00:00");
	CHECK_EQ(Parsed("29 february", utc, kNow), -1);
	CHECK_EQ(Iso(Parsed("2100-02-28", utc, kNow), utc), "2100-02-28T00:00:00");
	CHECK_EQ(Parsed("2100-02-29", utc, kNow), -1); // 2100 is no leap year
	CHECK_EQ(Iso(Parsed("2000-02-29", utc, kNow), utc), "2000-02-29T00:00:00");
	CHECK_EQ(Iso(Parsed("tomorrow", utc, 1798761599), utc), "2027-01-01T00:00:00"); // from 2026-12-31T23:59:59
}

TEST(ParseDateStaysWithinYears1To9999)
{
	TimeZone utc = TimeZone::Utc();
	CHECK_EQ(Parsed("2100-03-01", utc, kNow), 4107542400);
	CHECK_EQ(Parsed("9999-12-31 23:59:59", utc, kNow), 253402300799);
	CHECK_EQ(Parsed("0001-01-01", utc, kNow), -62135596800);
	CHECK_EQ(Parsed("0000-01-01", utc, kNow), -1);

	const int64_t lastDay = 253402214400; // 9999-12-31T00:00:00
	CHECK_EQ(Parsed("tomorrow", utc, lastDay), -1);
	CHECK_EQ(Parsed("in 1 day", utc, lastDay), -1);
	CHECK_EQ(Parsed("next year", utc, lastDay), -1);
	CHECK_EQ(Parsed("next month", utc, lastDay), -1);
	CHECK_EQ(Parsed("this year", utc, lastDay), 253370764800);
	CHECK_EQ(Parsed("1 year ago", utc, -62135596800), -1);
	CHECK_EQ(Parsed("now", utc, 253402300800), -1); // "now" after the year 9999
}

/* ---- ParseDate in real zones ---------------------------------------------- */

TEST(ParseDateAcrossDaylightSavingInBerlin)
{
	std::optional<TimeZone> berlin = SystemZone("Europe/Berlin");
	if (!berlin)
		return;
	const int64_t beforeChange = 1774695600; // 2026-03-28T12:00:00+01:00
	CHECK_EQ(Parsed("today", *berlin, beforeChange), 1774652400);
	CHECK_EQ(Parsed("tomorrow", *berlin, beforeChange), 1774738800);
	CHECK_EQ(Parsed("tomorrow 3pm", *berlin, beforeChange), 1774789200);
	CHECK_EQ(Parsed("in 1 day", *berlin, beforeChange), 1774778400); // same time of day, 23 hours later
	CHECK_EQ(Parsed("in 24 hours", *berlin, beforeChange), 1774782000);
	CHECK_EQ(Parsed("next week", *berlin, beforeChange), 1775253600);
	CHECK_EQ(Parsed("next month", *berlin, beforeChange), 1774994400);

	/* 02:30 does not exist on 2026-03-29 and becomes 03:30; it exists twice
	 * on 2026-10-25 and is the first one. */
	CHECK_EQ(Parsed("2026-03-29 02:30", *berlin, beforeChange), 1774747800);
	CHECK_EQ(Iso(1774747800, *berlin), "2026-03-29T03:30:00");
	CHECK_EQ(Parsed("29 march 2026 2:30am", *berlin, beforeChange), 1774747800);
	CHECK_EQ(Parsed("2026-10-25 02:30", *berlin, beforeChange), 1792888200);

	const int64_t afterFallBack = 1792900000; // 2026-10-25T04:46:40+01:00
	CHECK_EQ(Parsed("3 hours ago", *berlin, afterFallBack), afterFallBack - 3 * 3600);
	CHECK_EQ(Parsed("today", *berlin, afterFallBack), 1792879200);
}

TEST(ParseDateStartsOfHoursInTheRepeatedHour)
{
	/* 2026-10-25T01:30:00Z is the second 02:30 of the day in the test zone,
	 * in winter time, half an hour after the clocks went back. */
	std::optional<TimeZone> zone = LoadSpec(TestZoneSpec());
	CHECK(zone);
	if (!zone)
		return;
	const int64_t secondHalfPast = 1792891800;
	CHECK_EQ(Iso(secondHalfPast, *zone), "2026-10-25T02:30:00");
	CHECK_EQ(Parsed("this minute", *zone, secondHalfPast), secondHalfPast);
	CHECK_EQ(Parsed("next minute", *zone, secondHalfPast), secondHalfPast + 60);
	CHECK_EQ(Parsed("this hour", *zone, secondHalfPast), secondHalfPast - 1800);
	CHECK_EQ(Parsed("next hour", *zone, secondHalfPast), secondHalfPast + 1800);
	CHECK_EQ(Parsed("last hour", *zone, secondHalfPast), secondHalfPast - 5400);
}

TEST(ParseDateInNewYork)
{
	std::optional<TimeZone> newYork = SystemZone("America/New_York");
	if (!newYork)
		return;
	const int64_t beforeChange = 1772906400; // 2026-03-07T13:00:00-05:00
	CHECK_EQ(Parsed("tomorrow", *newYork, beforeChange), 1772946000);
	CHECK_EQ(Parsed("in 1 day", *newYork, beforeChange), 1772989200);
	CHECK_EQ(Parsed("2026-03-08 02:30", *newYork, beforeChange), 1772955000);
	CHECK_EQ(Iso(1772955000, *newYork), "2026-03-08T03:30:00");
	CHECK_EQ(Parsed("2026-11-01 01:30", *newYork, beforeChange), 1793511000);
	CHECK_EQ(Parsed("next month", *newYork, 1793500000), 1793505600); // from 2026-10-31
}

TEST(ParseDateAtTheDateLine)
{
	std::optional<TimeZone> kiritimati = SystemZone("Pacific/Kiritimati");
	std::optional<TimeZone> pagoPago = SystemZone("Pacific/Pago_Pago");
	std::optional<TimeZone> kolkata = SystemZone("Asia/Kolkata");
	if (!kiritimati || !pagoPago || !kolkata)
		return;
	/* The same instant is the 15th at +14 and the 13th at -11. */
	CHECK_EQ(Parsed("today", *kiritimati, kNow), 1794650400);
	CHECK_EQ(Iso(1794650400, *kiritimati), "2026-11-15T00:00:00");
	CHECK_EQ(Parsed("tomorrow 3pm", *kiritimati, kNow), 1794790800);
	CHECK_EQ(Parsed("today", *pagoPago, kNow), 1794567600);
	CHECK_EQ(Iso(1794567600, *pagoPago), "2026-11-13T00:00:00");
	CHECK_EQ(Parsed("next hour", *pagoPago, kNow), 1794654000);
	CHECK_EQ(Parsed("next hour", *kolkata, kNow), 1794652200); // half hour offset
	CHECK_EQ(Parsed("next week", *kolkata, kNow), 1795199400);
}

/* ---- Real zones ----------------------------------------------------------- */

TEST(SystemZoneOffsets)
{
	std::optional<TimeZone> berlin = SystemZone("Europe/Berlin");
	std::optional<TimeZone> newYork = SystemZone("America/New_York");
	std::optional<TimeZone> sydney = SystemZone("Australia/Sydney");
	if (!berlin || !newYork || !sydney)
		return;
	CHECK_EQ(berlin->Name(), std::string("Europe/Berlin"));
	CHECK_EQ(Iso(1782907200, *berlin), "2026-07-01T14:00:00");
	CHECK_EQ(Iso(1768478400, *berlin), "2026-01-15T13:00:00");
	CHECK_EQ(Iso(1782907200, *newYork), "2026-07-01T08:00:00");
	CHECK_EQ(Iso(1768478400, *newYork), "2026-01-15T07:00:00");
	CHECK_EQ(Iso(1768478400, *sydney), "2026-01-15T23:00:00");
	CHECK_EQ(Iso(1782907200, *sydney), "2026-07-01T22:00:00");
	CHECK_EQ(Iso(-2208988800, *newYork), "1899-12-31T19:00:00"); // 1900: standard time all year
	CHECK_EQ(newYork->ToEpoch(Civil(2026, 11, 1, 1, 30)), 1793511000);
	CHECK_EQ(newYork->ToEpoch(Civil(2026, 3, 8, 2, 30)), 1772955000);
}

/* 2100 lies after the last transition of every zoneinfo file, so these go
 * through the POSIX rule of the file's footer. */
TEST(SystemZonesFarInTheFuture)
{
	std::optional<TimeZone> berlin = SystemZone("Europe/Berlin");
	std::optional<TimeZone> sydney = SystemZone("Australia/Sydney");
	if (!berlin || !sydney)
		return;
	CHECK_EQ(berlin->ToEpoch(Civil(2100, 7, 1, 12)), 4118119200);
	CHECK_EQ(berlin->ToEpoch(Civil(2100, 1, 15, 12)), 4103694000);
	CHECK_EQ(Iso(4109878799, *berlin), "2100-03-28T01:59:59");
	CHECK_EQ(Iso(4109878800, *berlin), "2100-03-28T03:00:00");
	CHECK_EQ(Iso(4128627600, *berlin), "2100-10-31T02:00:00");
	CHECK_EQ(sydney->ToEpoch(Civil(2100, 1, 15, 12)), 4103658000);
	CHECK_EQ(sydney->ToEpoch(Civil(2100, 7, 15, 12)), 4119300000);
	CHECK_EQ(Iso(4126175999, *sydney), "2100-10-03T01:59:59");
	CHECK_EQ(Iso(4126176000, *sydney), "2100-10-03T03:00:00");
}

TEST(UnknownAndUnsafeZoneNames)
{
	CHECK(!TimeZone::Named("Nowhere/Zone", kSystemZoneinfo));
	CHECK(!TimeZone::Named("", kSystemZoneinfo));
	CHECK(!TimeZone::Named("/etc/localtime", kSystemZoneinfo));
	CHECK(!TimeZone::Named("../zoneinfo/Europe/Berlin", kSystemZoneinfo));
	CHECK(!TimeZone::Named("Europe/../Europe/Berlin", kSystemZoneinfo));
	CHECK(!TimeZone::Named("Europe//Berlin", kSystemZoneinfo));
	CHECK(!TimeZone::Named("./Europe/Berlin", kSystemZoneinfo));
	CHECK(!TimeZone::Named("Europe/Berlin/", kSystemZoneinfo));
	CHECK(!TimeZone::Named("Europe", kSystemZoneinfo)); // a directory
	CHECK(!TimeZone::Named("Europe/Berlin", "/nonexistent/zoneinfo"));
}

TEST(BuiltInZones)
{
	std::optional<TimeZone> utc = TimeZone::Named("UTC", "/nonexistent/zoneinfo");
	std::optional<TimeZone> floating = TimeZone::Named("floating", "/nonexistent/zoneinfo");
	CHECK(utc.has_value());
	CHECK(floating.has_value());
	if (!utc || !floating)
		return;
	CHECK_EQ(utc->Name(), std::string("UTC"));
	CHECK_EQ(floating->Name(), std::string("floating"));
	TimeZone builtInUtc = TimeZone::Utc();
	CHECK_EQ(builtInUtc.Name(), std::string("UTC"));
	CHECK_EQ(Iso(0, *floating), "1970-01-01T00:00:00");
	CHECK_EQ(floating->ToEpoch(Civil(2026, 10, 6, 14)), 1791295200);
	CHECK_EQ(Parsed("2026-10-06 14:00", *floating, kNow), 1791295200);
}

/* ---- The zoneinfo reader, on files written here --------------------------- */

TEST(TzifVersion2WithFooter)
{
	std::optional<TimeZone> zone = LoadSpec(TestZoneSpec());
	CHECK(zone.has_value());
	if (!zone)
		return;
	CHECK_EQ(zone->Name(), std::string("Test/Zone"));
	CHECK_EQ(Iso(1561982400, *zone), "2019-07-01T13:00:00"); // before the first transition: type 0
	CHECK_EQ(Iso(1593604800, *zone), "2020-07-01T14:00:00"); // from a transition
	CHECK_EQ(Iso(1782907200, *zone), "2026-07-01T14:00:00"); // from the footer
	CHECK_EQ(Iso(1768478400, *zone), "2026-01-15T13:00:00");
	CHECK_EQ(Iso(1774745999, *zone), "2026-03-29T01:59:59");
	CHECK_EQ(Iso(1774746000, *zone), "2026-03-29T03:00:00");
	CHECK_EQ(zone->ToEpoch(Civil(2026, 3, 29, 2, 30)), 1774747800); // skipped: forward by the gap
	CHECK_EQ(zone->ToEpoch(Civil(2026, 10, 25, 2, 30)), 1792888200); // twice: the earlier
	CHECK_EQ(zone->ToEpoch(Civil(2026, 10, 25, 3, 0)), 1792893600);
	CHECK_EQ(Iso(4109878800, *zone), "2100-03-28T03:00:00");
}

TEST(TzifVersion1WithoutFooter)
{
	TzifSpec spec = TestZoneSpec();
	spec.version = '\0';
	std::optional<TimeZone> zone = LoadSpec(spec);
	CHECK(zone.has_value());
	if (!zone)
		return;
	CHECK_EQ(Iso(1593604800, *zone), "2020-07-01T14:00:00");
	/* Without a footer the last transition's type holds on. */
	CHECK_EQ(Iso(1782907200, *zone), "2026-07-01T13:00:00");
}

TEST(TzifEmptyFooterKeepsTheLastType)
{
	TzifSpec spec = TestZoneSpec();
	spec.footer = "";
	std::optional<TimeZone> zone = LoadSpec(spec);
	CHECK(zone.has_value());
	if (zone)
		CHECK_EQ(Iso(1782907200, *zone), "2026-07-01T13:00:00");
}

TEST(TzifRejectsBrokenFiles)
{
	TempDir dir;
	CHECK(dir.IsReady());
	if (!dir.IsReady())
		return;
	std::string good = TzifBytes(TestZoneSpec());
	TzifSpec leapSeconds = TestZoneSpec();
	leapSeconds.leapSeconds = 2;
	TzifSpec badIndex = TestZoneSpec();
	badIndex.typeIndices = {1, 2};
	TzifSpec unordered = TestZoneSpec();
	unordered.times = {1603587600, 1585443600};
	TzifSpec hugeOffset = TestZoneSpec();
	hugeOffset.types[1].utcOffset = 100000;
	TzifSpec noTypes = TestZoneSpec();
	noTypes.times.clear();
	noTypes.typeIndices.clear();
	noTypes.types.clear();

	CHECK(dir.Write("Good", good));
	CHECK(dir.Write("Empty", ""));
	CHECK(dir.Write("BadMagic", "TZiF" + good.substr(4)));
	CHECK(dir.Write("Truncated", good.substr(0, good.size() / 2)));
	CHECK(dir.Write("NoFooterEnd", good.substr(0, good.size() - 1)));
	CHECK(dir.Write("LeapSeconds", TzifBytes(leapSeconds)));
	CHECK(dir.Write("BadIndex", TzifBytes(badIndex)));
	CHECK(dir.Write("Unordered", TzifBytes(unordered)));
	CHECK(dir.Write("HugeOffset", TzifBytes(hugeOffset)));
	CHECK(dir.Write("NoTypes", TzifBytes(noTypes)));
	CHECK(dir.Write("RuleLess", TzifBytes(FooterOnlySpec("CET-1CEST", 3600))));
	CHECK(dir.Write("BadRule", TzifBytes(FooterOnlySpec("CET-1CEST,M13.5.0,M10.5.0/3", 3600))));
	CHECK(dir.Write("ShortName", TzifBytes(FooterOnlySpec("XY-1", 3600))));
	CHECK(dir.Write("Trailing", TzifBytes(FooterOnlySpec("CET-1CEST,M3.5.0,M10.5.0/3x", 3600))));

	CHECK(TimeZone::Named("Good", dir.Path()).has_value());
	const char *const broken[] = {
		"Empty", "BadMagic", "Truncated", "NoFooterEnd", "LeapSeconds", "BadIndex", "Unordered",
		"HugeOffset", "NoTypes", "RuleLess", "BadRule", "ShortName", "Trailing",
	};
	for (const char *name : broken)
		CHECK(!TimeZone::Named(name, dir.Path()).has_value());
}

TEST(PosixRuleFixedNegativeOffset)
{
	std::optional<TimeZone> zone = LoadSpec(FooterOnlySpec("<-03>3", -10800));
	CHECK(zone.has_value());
	if (!zone)
		return;
	CHECK_EQ(Iso(0, *zone), "1969-12-31T21:00:00");
	CHECK_EQ(zone->ToEpoch(Civil(2026, 7, 1, 9)), 1782907200);
}

/* Default transition time 02:00, daylight offset one hour ahead. */
TEST(PosixRuleDefaults)
{
	std::optional<TimeZone> zone = LoadSpec(FooterOnlySpec("EST5EDT,M3.2.0,M11.1.0", -18000));
	CHECK(zone.has_value());
	if (!zone)
		return;
	CHECK_EQ(Iso(1772953199, *zone), "2026-03-08T01:59:59");
	CHECK_EQ(Iso(1772953200, *zone), "2026-03-08T03:00:00");
	CHECK_EQ(Iso(1793512799, *zone), "2026-11-01T01:59:59");
	CHECK_EQ(Iso(1793512800, *zone), "2026-11-01T01:00:00");
}

/* Quoted names, minutes in offsets and a southern rule over the turn of
 * the year (Pacific/Chatham). */
TEST(PosixRuleSouthernWithMinutes)
{
	std::optional<TimeZone> zone = LoadSpec(FooterOnlySpec("<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45", 45900));
	CHECK(zone.has_value());
	if (!zone)
		return;
	CHECK_EQ(Iso(1768478400, *zone), "2026-01-16T01:45:00"); // January: +13:45
	CHECK_EQ(Iso(1782907200, *zone), "2026-07-02T00:45:00"); // July: +12:45
}

/* Negative transition time (America/Nuuk): daylight time starts at -1:00,
 * the Saturday before the last Sunday of March at 23:00 local time. */
TEST(PosixRuleNegativeTransitionTime)
{
	std::optional<TimeZone> zone = LoadSpec(FooterOnlySpec("<-02>2<-01>,M3.5.0/-1,M10.5.0/0", -7200));
	CHECK(zone.has_value());
	if (!zone)
		return;
	CHECK_EQ(Iso(1774745999, *zone), "2026-03-28T22:59:59");
	CHECK_EQ(Iso(1774746000, *zone), "2026-03-29T00:00:00");
}

/* Jn never counts February 29, n does. */
TEST(PosixRuleJulianDays)
{
	std::optional<TimeZone> julian = LoadSpec(FooterOnlySpec("AAA0BBB,J60/0,J300/0", 0));
	std::optional<TimeZone> zeroBased = LoadSpec(FooterOnlySpec("AAA0BBB,59/0,J300/0", 0));
	CHECK(julian.has_value());
	CHECK(zeroBased.has_value());
	if (!julian || !zeroBased)
		return;
	const int64_t leapDayNoon = 1835438400;   // 2028-02-29T12:00:00Z
	const int64_t marchFirstNoon = 1803902400; // 2027-03-01T12:00:00Z
	CHECK_EQ(Iso(leapDayNoon, *julian), "2028-02-29T12:00:00"); // J60 is March 1
	CHECK_EQ(Iso(leapDayNoon + 86400, *julian), "2028-03-01T13:00:00");
	CHECK_EQ(Iso(leapDayNoon, *zeroBased), "2028-02-29T13:00:00"); // 59 is February 29
	CHECK_EQ(Iso(marchFirstNoon, *julian), "2027-03-01T13:00:00");
	CHECK_EQ(Iso(marchFirstNoon, *zeroBased), "2027-03-01T13:00:00");
	CHECK_EQ(Iso(marchFirstNoon - 86400, *zeroBased), "2027-02-28T12:00:00");
}

TEST(ZoneNamesCannotLeaveTheZoneinfoDirectory)
{
	TempDir dir;
	CHECK(dir.IsReady());
	if (!dir.IsReady())
		return;
	std::string bytes = TzifBytes(TestZoneSpec());
	CHECK(dir.Write("outside", bytes));
	CHECK(dir.Write("zones/Inside/Zone", bytes));
	std::string zones = dir.Path() + "/zones";
	CHECK(TimeZone::Named("Inside/Zone", zones).has_value());
	CHECK(!TimeZone::Named("../outside", zones).has_value());
	CHECK(!TimeZone::Named("Inside/../../outside", zones).has_value());
	CHECK(!TimeZone::Named(dir.Path() + "/outside", zones).has_value());
}

/* ---- The local zone ------------------------------------------------------- */

TEST(LocalZoneFromTzVariable)
{
	TempDir dir;
	CHECK(dir.IsReady());
	if (!dir.IsReady() || !dir.Write("Test/Zone", TzifBytes(TestZoneSpec())))
		return;
	TzVariableGuard guard;

	setenv("TZ", ":Test/Zone", 1);
	std::optional<TimeZone> named = TimeZone::Named("local", dir.Path());
	CHECK(named.has_value());
	if (named) {
		CHECK_EQ(named->Name(), std::string("local"));
		CHECK_EQ(Iso(1782907200, *named), "2026-07-01T14:00:00");
	}

	setenv("TZ", "Test/Zone", 1);
	std::optional<TimeZone> withoutColon = TimeZone::Named("local", dir.Path());
	if (withoutColon)
		CHECK_EQ(Iso(1782907200, *withoutColon), "2026-07-01T14:00:00");

	setenv("TZ", "", 1);
	std::optional<TimeZone> empty = TimeZone::Named("local", dir.Path());
	CHECK(empty.has_value());
	if (empty) {
		CHECK_EQ(empty->Name(), std::string("local"));
		CHECK_EQ(Iso(1782907200, *empty), "2026-07-01T12:00:00");
	}
}

/* With a TZ that names no zone, /etc/localtime decides; without a readable
 * one the zone falls back to floating. */
TEST(LocalZoneFallback)
{
	TzVariableGuard guard;
	setenv("TZ", "Nowhere/Zone", 1);
	std::optional<TimeZone> local = TimeZone::Named("local", "/nonexistent/zoneinfo");
	CHECK(local.has_value());
	if (!local)
		return;
	if (IsReadable("/etc/localtime")) {
		CHECK_EQ(local->Name(), std::string("local"));
		return;
	}
	CHECK_EQ(local->Name(), std::string("floating"));
	CHECK_EQ(Iso(1782907200, *local), "2026-07-01T12:00:00");
}

/* ---- Durations ------------------------------------------------------------ */

bool operator==(const Duration &a, const Duration &b)
{
	return a.months == b.months && a.days == b.days && a.seconds == b.seconds;
}

std::ostream &operator<<(std::ostream &out, const Duration &duration)
{
	return out << "{months " << duration.months << ", days " << duration.days << ", seconds " << duration.seconds << "}";
}

Duration ParsedDuration(const char *text)
{
	std::optional<Duration> duration = ParseDuration(text);
	return duration ? *duration : Duration{-1, -1, -1};
}

TEST(ParseDurationForms)
{
	CHECK_EQ(ParsedDuration("90 seconds"), (Duration{0, 0, 90}));
	CHECK_EQ(ParsedDuration("for 90 seconds"), (Duration{0, 0, 90}));
	CHECK_EQ(ParsedDuration("1 second"), (Duration{0, 0, 1}));
	CHECK_EQ(ParsedDuration("1 minute"), (Duration{0, 0, 60}));
	CHECK_EQ(ParsedDuration("for 3 hours"), (Duration{0, 0, 10800}));
	CHECK_EQ(ParsedDuration("2 days"), (Duration{0, 2, 0}));
	CHECK_EQ(ParsedDuration("2 weeks"), (Duration{0, 14, 0}));
	CHECK_EQ(ParsedDuration("1 month"), (Duration{1, 0, 0}));
	CHECK_EQ(ParsedDuration("2 years"), (Duration{24, 0, 0}));
	CHECK_EQ(ParsedDuration("0 days"), (Duration{0, 0, 0}));
	CHECK_EQ(ParsedDuration("0 day"), (Duration{0, 0, 0}));
	CHECK_EQ(ParsedDuration("03 days"), (Duration{0, 3, 0}));
	CHECK_EQ(ParsedDuration("01 day"), (Duration{0, 1, 0}));
	CHECK_EQ(ParsedDuration("  FOR   3\tHours "), (Duration{0, 0, 10800}));
	CHECK_EQ(ParsedDuration("for 10000 years"), (Duration{120000, 0, 0}));
	CHECK_EQ(ParsedDuration("3652425 days"), (Duration{0, 3652425, 0}));
	CHECK_EQ(ParsedDuration("315569520000 seconds"), (Duration{0, 0, 315569520000}));
}

TEST(ParseDurationRejections)
{
	const char *const rejected[] = {
		"", "for", "for for 1 day", "days", "for days", "1 days", "2 day", "1 minutes", "01 days", "1h", "1 hour 30 minutes",
		"1.5 hours", "-1 days", "+1 days", "a day", "one day", "1 fortnight", "1 hr", "1 day ago", "in 1 day",
		"for 10001 years", "120001 months", "3652426 days", "315569520001 seconds", "999999999999999999999 days",
		"1000000000000 seconds", "1 day,",
	};
	for (const char *text : rejected)
		CHECK(!ParseDuration(text).has_value());
}

TEST(AddDurationOnTheCalendar)
{
	TimeZone utc = TimeZone::Utc();
	const int64_t january31 = 1769853600; // 2026-01-31T10:00:00
	CHECK_EQ(Iso(AddDuration(january31, Duration{1, 0, 0}, utc), utc), "2026-02-28T10:00:00");
	CHECK_EQ(Iso(AddDuration(january31, Duration{1, 1, 0}, utc), utc), "2026-03-01T10:00:00"); // months first
	CHECK_EQ(Iso(AddDuration(january31, Duration{-2, 0, 0}, utc), utc), "2025-11-30T10:00:00");
	CHECK_EQ(Iso(AddDuration(january31, Duration{0, 0, 90}, utc), utc), "2026-01-31T10:01:30");
	CHECK_EQ(Iso(AddDuration(january31, Duration{0, -31, -3600}, utc), utc), "2025-12-31T09:00:00");
	CHECK_EQ(AddDuration(january31, Duration{}, utc), january31);
	const int64_t leapDay = 1709200800; // 2024-02-29T10:00:00
	CHECK_EQ(Iso(AddDuration(leapDay, Duration{12, 0, 0}, utc), utc), "2025-02-28T10:00:00");
	CHECK_EQ(Iso(AddDuration(leapDay, Duration{48, 0, 0}, utc), utc), "2028-02-29T10:00:00");
	CHECK_EQ(Iso(AddDuration(4107542400, Duration{0, 365, 0}, utc), utc), "2101-03-01T00:00:00"); // from 2100-03-01
}

TEST(AddDurationAcrossDaylightSaving)
{
	std::optional<TimeZone> zone = LoadSpec(TestZoneSpec());
	if (!zone)
		return;
	const int64_t saturdayNoon = 1774695600; // 2026-03-28T12:00:00+01:00
	CHECK_EQ(Iso(AddDuration(saturdayNoon, Duration{0, 1, 0}, *zone), *zone), "2026-03-29T12:00:00");
	CHECK_EQ(Iso(AddDuration(saturdayNoon, Duration{0, 0, 86400}, *zone), *zone), "2026-03-29T13:00:00");
	/* Landing in the gap moves forward by it. */
	const int64_t saturdayHalfPastTwo = 1774661400; // 2026-03-28T02:30:00+01:00
	CHECK_EQ(Iso(AddDuration(saturdayHalfPastTwo, Duration{0, 1, 0}, *zone), *zone), "2026-03-29T03:30:00");
}

TEST(AddDurationDoesNotOverflow)
{
	TimeZone utc = TimeZone::Utc();
	const int64_t max = std::numeric_limits<int64_t>::max();
	const int64_t min = std::numeric_limits<int64_t>::min();
	CHECK_EQ(AddDuration(max, Duration{0, 0, max}, utc), max);
	CHECK_EQ(AddDuration(min, Duration{0, 0, min}, utc), min);
	CHECK(AddDuration(kNow, Duration{max, max, max}, utc) > kNow);
	CHECK(AddDuration(kNow, Duration{min, min, min}, utc) < kNow);
}

TEST(FormatDurationInTheLargestExactUnit)
{
	CHECK_EQ(FormatDuration(Duration{0, 0, 90}), "90 seconds");
	CHECK_EQ(FormatDuration(Duration{0, 0, 1}), "1 second");
	CHECK_EQ(FormatDuration(Duration{0, 0, 60}), "1 minute");
	CHECK_EQ(FormatDuration(Duration{0, 0, 5400}), "90 minutes");
	CHECK_EQ(FormatDuration(Duration{0, 0, 7200}), "2 hours");
	CHECK_EQ(FormatDuration(Duration{0, 1, 0}), "1 day");
	CHECK_EQ(FormatDuration(Duration{0, 10, 0}), "10 days");
	CHECK_EQ(FormatDuration(Duration{0, 14, 0}), "2 weeks");
	CHECK_EQ(FormatDuration(Duration{1, 0, 0}), "1 month");
	CHECK_EQ(FormatDuration(Duration{13, 0, 0}), "13 months");
	CHECK_EQ(FormatDuration(Duration{24, 0, 0}), "2 years");
	CHECK_EQ(FormatDuration(Duration{}), "0 seconds");
	CHECK_EQ(FormatDuration(Duration{1, 2, 3600}), "1 month 2 days 1 hour");
	CHECK_EQ(FormatDuration(*ParseDuration("for 2 weeks")), "2 weeks");
}

TEST(FormatIsoPadsTheYear)
{
	TimeZone utc = TimeZone::Utc();
	CHECK_EQ(FormatIso(0, utc), "1970-01-01T00:00:00");
	CHECK_EQ(FormatIso(-62135596800, utc), "0001-01-01T00:00:00");
	CHECK_EQ(FormatIso(253402300799, utc), "9999-12-31T23:59:59");
	CHECK_EQ(FormatIso(-1, utc), "1969-12-31T23:59:59");
}
