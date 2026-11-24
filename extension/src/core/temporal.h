/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Dates and durations in natural language, for the date and duration types:
 * time zones read from the system's zoneinfo files, a date parser and a
 * duration parser. Nothing here reads the clock; the caller passes "now". */
#ifndef GETOPT_CORE_TEMPORAL_H
#define GETOPT_CORE_TEMPORAL_H

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace smgetopt::temporal {

/* A date and time on the calendar, without a zone. */
struct CivilTime {
	int year = 1970;
	int month = 1;   // 1-12
	int day = 1;     // 1-31
	int hour = 0;
	int minute = 0;
	int second = 0;
};

/* A time zone: converts between seconds since the epoch and civil time.
 * "UTC" and "floating" (a date and time without a zone, calculated like
 * UTC) need no files; every other name is read from the zoneinfo
 * directory. Cheap to copy. */
class TimeZone {
public:
	/* The zone called name, or nothing for an unknown name. "local" is the
	 * system's zone (the TZ environment variable, else /etc/localtime), and
	 * floating when that cannot be determined. zoneinfoDir is for tests; empty
	 * means the TZDIR environment variable, else /usr/share/zoneinfo. */
	static std::optional<TimeZone> Named(const std::string &name, const std::string &zoneinfoDir = "");

	static TimeZone Utc();

	/* The name the zone was asked for by, "floating" for a local zone that
	 * fell back. */
	const std::string &Name() const;

	CivilTime ToCivil(int64_t epochSeconds) const;

	/* A civil time that does not exist (skipped by a daylight saving change)
	 * moves forward by the gap; one that exists twice is the earlier one. */
	int64_t ToEpoch(const CivilTime &civil) const;

private:
	struct Data;
	std::shared_ptr<const Data> m_data;
};

/* A length of time as the calendar counts it: months and days have no fixed
 * length in seconds. */
struct Duration {
	int64_t months = 0;
	int64_t days = 0;
	int64_t seconds = 0;
};

/* The point in time text names, counted from now in zone, or nothing when
 * text is no date. docs/dates.md lists what is understood. */
std::optional<int64_t> ParseDate(std::string_view text, const TimeZone &zone, int64_t now);

/* The length text names: one number and one unit, with an optional leading
 * "for" ("90 seconds", "for 2 weeks"), or nothing. */
std::optional<Duration> ParseDuration(std::string_view text);

/* epochSeconds moved by duration on zone's calendar: months first, then
 * days, then seconds. A day of the month that the target month lacks is its
 * last day. */
int64_t AddDuration(int64_t epochSeconds, const Duration &duration, const TimeZone &zone);

/* "2026-10-06T14:00:00", in zone. */
std::string FormatIso(int64_t epochSeconds, const TimeZone &zone);

/* "90 minutes", "1 month": the duration in its largest exact unit. */
std::string FormatDuration(const Duration &duration);

} // namespace smgetopt::temporal

#endif // GETOPT_CORE_TEMPORAL_H
