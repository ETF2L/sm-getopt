/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The built-in types and the registry. */
#include "types.h"

#include "text.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <set>
#include <stdexcept>

namespace smgetopt {

namespace {

using text::Format;

std::optional<std::string> KeyOf(const TypeKeys &keys, const char *name)
{
	auto found = keys.find(name);
	if (found == keys.end())
		return std::nullopt;
	return found->second;
}

Scalar IntegerScalar(int64_t value)
{
	return Scalar{std::to_string(value), value, static_cast<double>(value), {}, {}};
}

/* --name: 1 when given. A default may also say 0. */
class FlagType : public Type {
public:
	TypeKind Kind() const override { return TypeKind::Flag; }

	Problem Check(const std::string &value) const override
	{
		if (value.empty() || value == "0" || value == "1")
			return std::nullopt;
		return Format("'%s' is not a boolean (use 1 or 0)", value.c_str());
	}

	Scalar Coerce(const std::string &value) const override { return IntegerScalar(value == "1" ? 1 : 0); }
};

class BoolType : public FlagType {
public:
	TypeKind Kind() const override { return TypeKind::Bool; }
};

class CounterType : public Type {
public:
	TypeKind Kind() const override { return TypeKind::Counter; }

	Problem Check(const std::string &value) const override
	{
		bool isCount = !value.empty() && value[0] != '+' && value[0] != '-' && text::IsInteger(value);
		if (isCount && text::ToInt64(value))
			return std::nullopt;
		return Format("'%s' is not a count", value.c_str());
	}

	Scalar Coerce(const std::string &value) const override { return IntegerScalar(*text::ToInt64(value)); }
};

class StringType : public Type {
public:
	TypeKind Kind() const override { return TypeKind::Value; }
};

/* The bounds of the number types, as the spec wrote them. */
struct Bounds {
	std::optional<std::string> min;
	std::optional<std::string> max;
};

Bounds BoundsOf(const TypeKeys &keys)
{
	return Bounds{KeyOf(keys, "min"), KeyOf(keys, "max")};
}

/* Two numbers compared exactly when both are integers, as doubles
 * otherwise. Both must be integers or decimals. */
int CompareNumbers(const std::string &left, const std::string &right)
{
	if (text::IsInteger(left) && text::IsInteger(right))
		return text::CompareIntegers(left, right);
	double a = *text::ToDouble(left);
	double b = *text::ToDouble(right);
	return a < b ? -1 : a > b ? 1 : 0;
}

Problem CheckBounds(const Bounds &bounds, const std::string &value)
{
	if (bounds.min && CompareNumbers(value, *bounds.min) < 0)
		return Format("%s is smaller than the minimum of %s", value.c_str(), bounds.min->c_str());
	if (bounds.max && CompareNumbers(value, *bounds.max) > 0)
		return Format("%s is larger than the maximum of %s", value.c_str(), bounds.max->c_str());
	return std::nullopt;
}

/* A bound that is no number, or an empty range, is a mistake in the spec. */
Problem CheckBoundKeys(const TypeKeys &keys)
{
	Bounds bounds = BoundsOf(keys);
	if (bounds.min && !text::IsDecimal(*bounds.min))
		return Format("min must be a number, not '%s'", bounds.min->c_str());
	if (bounds.max && !text::IsDecimal(*bounds.max))
		return Format("max must be a number, not '%s'", bounds.max->c_str());
	if (bounds.min && bounds.max && CompareNumbers(*bounds.min, *bounds.max) > 0)
		return Format("min %s is larger than max %s", bounds.min->c_str(), bounds.max->c_str());
	return std::nullopt;
}

bool IsBigInt(const TypeKeys &keys)
{
	return text::IsTrue(KeyOf(keys, "bigint").value_or(""));
}

/* The bounds of a bigint option are compared exactly, so they must be
 * integers themselves. */
Problem CheckIntKeys(const TypeKeys &keys)
{
	Problem problem = CheckBoundKeys(keys);
	if (problem || !IsBigInt(keys))
		return problem;

	Bounds bounds = BoundsOf(keys);
	if (bounds.min && !text::IsInteger(*bounds.min))
		return Format("min must be an integer with bigint, not '%s'", bounds.min->c_str());
	if (bounds.max && !text::IsInteger(*bounds.max))
		return Format("max must be an integer with bigint, not '%s'", bounds.max->c_str());
	return std::nullopt;
}

/* An integer that fits a cell, the 32-bit number of a plugin; with bigint
 * an integer of any size, exact in its text. */
class IntType : public Type {
public:
	explicit IntType(const TypeKeys &keys) : m_bounds(BoundsOf(keys)), m_bigint(IsBigInt(keys)) {}

	TypeKind Kind() const override { return TypeKind::Value; }

	Problem Check(const std::string &value) const override
	{
		if (!text::IsInteger(value))
			return Format("'%s' is not an integer", value.c_str());
		bool fitsCell = text::CompareIntegers(value, "-2147483648") >= 0 && text::CompareIntegers(value, "2147483647") <= 0;
		if (!m_bigint && !fitsCell)
			return Format("'%s' is too large for an integer", value.c_str());
		return CheckBounds(m_bounds, value);
	}

	Scalar Coerce(const std::string &value) const override
	{
		Scalar scalar{text::CanonicalInteger(value), text::ToInt64(value), {}, {}, {}};
		if (scalar.integer)
			scalar.real = static_cast<double>(*scalar.integer);
		return scalar;
	}

private:
	Bounds m_bounds;
	bool m_bigint;
};

class FloatType : public Type {
public:
	explicit FloatType(const TypeKeys &keys) : m_bounds(BoundsOf(keys)) {}

	TypeKind Kind() const override { return TypeKind::Value; }

	Problem Check(const std::string &value) const override
	{
		if (text::IsNonFiniteWord(value))
			return Format("'%s' is not a finite number", value.c_str());
		std::optional<double> number = text::ToDouble(value);
		if (!number)
			return Format("'%s' is not a number", value.c_str());
		if (!isfinite(*number))
			return Format("'%s' is not a finite number", value.c_str());
		return CheckBounds(m_bounds, value);
	}

	Scalar Coerce(const std::string &value) const override
	{
		double number = *text::ToDouble(value);
		return Scalar{text::FormatDouble(number), {}, number, {}, {}};
	}

private:
	Bounds m_bounds;
};

bool IsSchemeCharacter(char c)
{
	return isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '+' || c == '-';
}

/* scheme://rest: a scheme that starts with a letter, then at least one
 * character, without whitespace. */
bool IsUrl(const std::string &value)
{
	size_t separator = value.find("://");
	if (separator == std::string::npos || separator == 0 || separator + 3 == value.size())
		return false;
	if (!isalpha(static_cast<unsigned char>(value[0])))
		return false;
	for (size_t i = 1; i < separator; i++) {
		if (!IsSchemeCharacter(value[i]))
			return false;
	}
	for (size_t i = separator + 3; i < value.size(); i++) {
		if (isspace(static_cast<unsigned char>(value[i])))
			return false;
	}
	return true;
}

class UrlType : public Type {
public:
	TypeKind Kind() const override { return TypeKind::Value; }

	Problem Check(const std::string &value) const override
	{
		if (IsUrl(value))
			return std::nullopt;
		return Format("'%s' is not a URL", value.c_str());
	}

	std::string Label() const override { return "URL"; }
};

const char *const kDefaultTimeZone = "local";

/* Without a bound, a date or duration must fit a cell, so that GetInt can
 * return its number: a Unix timestamp from 1901-12-13T20:45:52Z to
 * 2038-01-19T03:14:07Z, or up to 2147483647 seconds. */
const int64_t kCellMin = INT32_MIN;
const int64_t kCellMax = INT32_MAX;

std::string TimeZoneNameOf(const TypeKeys &keys)
{
	return KeyOf(keys, "timezone").value_or(kDefaultTimeZone);
}

Problem CheckTimeZoneKey(const TypeKeys &keys)
{
	std::string name = TimeZoneNameOf(keys);
	if (name.empty())
		return std::string("timezone must be a non-empty string");
	if (!temporal::TimeZone::Named(name))
		return Format("timezone '%s' is not a known time zone", name.c_str());
	return std::nullopt;
}

/* The zone a temporal type calculates in. CheckTimeZoneKey has accepted
 * the name before a type is created. */
temporal::TimeZone ZoneOf(const TypeKeys &keys)
{
	return temporal::TimeZone::Named(TimeZoneNameOf(keys)).value_or(temporal::TimeZone::Utc());
}

/* A bound of a date at the moment now: a Unix timestamp, or a date as a
 * value is written ("now", "2030-01-01"). Nothing when it is neither. */
std::optional<int64_t> DateBoundAt(const std::string &bound, const temporal::TimeZone &zone, int64_t now)
{
	if (text::IsInteger(bound))
		return text::ToInt64(bound);
	return temporal::ParseDate(bound, zone, now);
}

/* A bound of a duration: a number of seconds, or a duration as a value is
 * written ("1 minute", "100 years"). Nothing when it is neither. */
std::optional<temporal::Duration> DurationBound(const std::string &bound)
{
	if (!text::IsInteger(bound))
		return temporal::ParseDuration(bound);
	std::optional<int64_t> seconds = text::ToInt64(bound);
	if (!seconds || *seconds < 0)
		return std::nullopt;
	return temporal::Duration{0, 0, *seconds};
}

/* The seconds a duration lasts from now, on the calendar of zone. */
int64_t SecondsFrom(int64_t now, const temporal::Duration &duration, const temporal::TimeZone &zone)
{
	return temporal::AddDuration(now, duration, zone) - now;
}

Problem CheckDateKeys(const TypeKeys &keys, const Clock &clock)
{
	if (Problem problem = CheckTimeZoneKey(keys))
		return problem;

	temporal::TimeZone zone = ZoneOf(keys);
	int64_t now = clock();
	Bounds bounds = BoundsOf(keys);
	std::optional<int64_t> earliest = bounds.min ? DateBoundAt(*bounds.min, zone, now) : kCellMin;
	std::optional<int64_t> latest = bounds.max ? DateBoundAt(*bounds.max, zone, now) : kCellMax;
	if (!earliest)
		return Format("min must be a date or a Unix timestamp, not '%s'", bounds.min->c_str());
	if (!latest)
		return Format("max must be a date or a Unix timestamp, not '%s'", bounds.max->c_str());
	if (*earliest <= *latest)
		return std::nullopt;
	if (bounds.min && bounds.max)
		return Format("min %s is larger than max %s", bounds.min->c_str(), bounds.max->c_str());
	if (bounds.min)
		return Format("min %s is later than the cell's maximum of %s; set max as well", bounds.min->c_str(),
		              temporal::FormatIso(kCellMax, zone).c_str());
	return Format("max %s is earlier than the cell's minimum of %s; set min as well", bounds.max->c_str(),
	              temporal::FormatIso(kCellMin, zone).c_str());
}

Problem CheckDurationKeys(const TypeKeys &keys, const Clock &clock)
{
	if (Problem problem = CheckTimeZoneKey(keys))
		return problem;

	Bounds bounds = BoundsOf(keys);
	std::optional<temporal::Duration> shortest = bounds.min ? DurationBound(*bounds.min) : temporal::Duration{};
	std::optional<temporal::Duration> longest = bounds.max ? DurationBound(*bounds.max) : temporal::Duration{0, 0, kCellMax};
	if (!shortest)
		return Format("min must be a duration or a number of seconds, not '%s'", bounds.min->c_str());
	if (!longest)
		return Format("max must be a duration or a number of seconds, not '%s'", bounds.max->c_str());

	temporal::TimeZone zone = ZoneOf(keys);
	int64_t now = clock();
	if (!bounds.min || SecondsFrom(now, *shortest, zone) <= SecondsFrom(now, *longest, zone))
		return std::nullopt;
	if (bounds.max)
		return Format("min %s is larger than max %s", bounds.min->c_str(), bounds.max->c_str());
	return Format("min %s is longer than the cell's maximum of %s; set max as well", bounds.min->c_str(),
	              temporal::FormatDuration(*longest).c_str());
}

Conversion Refused(const std::string &problem)
{
	return Conversion{problem, {}};
}

/* A point in time in natural language, counted from the moment the value
 * is converted, within the bounds of the spec or a cell's. */
class DateType : public Type {
public:
	DateType(const TypeKeys &keys, Clock clock) : m_zone(ZoneOf(keys)), m_bounds(BoundsOf(keys)), m_clock(std::move(clock)) {}

	TypeKind Kind() const override { return TypeKind::Value; }

	Problem Check(const std::string &value) const override { return Convert(value).problem; }

	Scalar Coerce(const std::string &value) const override { return Convert(value).scalar; }

	/* The value and the bounds are read at one moment. */
	Conversion Convert(const std::string &value) const override
	{
		int64_t now = m_clock();
		std::optional<int64_t> epoch = temporal::ParseDate(value, m_zone, now);
		if (!epoch)
			return Refused(Format("'%s' is not a date", value.c_str()));

		std::optional<int64_t> earliest = m_bounds.min ? DateBoundAt(*m_bounds.min, m_zone, now) : kCellMin;
		std::optional<int64_t> latest = m_bounds.max ? DateBoundAt(*m_bounds.max, m_zone, now) : kCellMax;
		if (!earliest || !latest)
			return Refused(Format("'%s' cannot be compared with a bound that is no date right now", value.c_str()));
		if (*epoch < *earliest)
			return Refused(Format("'%s' is earlier than the minimum of %s", value.c_str(), temporal::FormatIso(*earliest, m_zone).c_str()));
		if (*epoch > *latest)
			return Refused(Format("'%s' is later than the maximum of %s", value.c_str(), temporal::FormatIso(*latest, m_zone).c_str()));
		return Conversion{std::nullopt, Scalar{temporal::FormatIso(*epoch, m_zone), *epoch, static_cast<double>(*epoch), {}, {}}};
	}

	bool FollowsClock() const override { return true; }

	std::string Label() const override { return "Date"; }

private:
	temporal::TimeZone m_zone;
	Bounds m_bounds;
	Clock m_clock;
};

/* A bound CheckDurationKeys has accepted, read for a type. */
temporal::Duration AcceptedDurationBound(const std::string &bound)
{
	std::optional<temporal::Duration> duration = DurationBound(bound);
	if (!duration)
		throw std::logic_error("a duration type was created with an unchecked bound '" + bound + "'");
	return *duration;
}

/* A length of time. Its number is the seconds it lasts from the moment the
 * value is converted, on the calendar of its zone, within the bounds of the
 * spec or a cell's. */
class DurationType : public Type {
public:
	DurationType(const TypeKeys &keys, Clock clock) : m_zone(ZoneOf(keys)), m_clock(std::move(clock))
	{
		Bounds bounds = BoundsOf(keys);
		if (bounds.min)
			m_shortest = AcceptedDurationBound(*bounds.min);
		m_longest = bounds.max ? AcceptedDurationBound(*bounds.max) : temporal::Duration{0, 0, kCellMax};
	}

	TypeKind Kind() const override { return TypeKind::Value; }

	Problem Check(const std::string &value) const override { return Convert(value).problem; }

	Scalar Coerce(const std::string &value) const override { return Convert(value).scalar; }

	/* The value and the bounds are counted from one moment. */
	Conversion Convert(const std::string &value) const override
	{
		std::optional<temporal::Duration> duration = temporal::ParseDuration(value);
		if (!duration)
			return Refused(Format("'%s' is not a duration", value.c_str()));

		int64_t now = m_clock();
		int64_t seconds = SecondsFrom(now, *duration, m_zone);
		if (m_shortest && seconds < SecondsFrom(now, *m_shortest, m_zone))
			return Refused(Format("'%s' is shorter than the minimum of %s", value.c_str(), temporal::FormatDuration(*m_shortest).c_str()));
		if (seconds > SecondsFrom(now, m_longest, m_zone))
			return Refused(Format("'%s' is longer than the maximum of %s", value.c_str(), temporal::FormatDuration(m_longest).c_str()));
		return Conversion{std::nullopt, Scalar{temporal::FormatDuration(*duration), seconds, static_cast<double>(seconds), *duration, {}}};
	}

	bool FollowsClock() const override { return true; }

	std::string Label() const override { return "Duration"; }

private:
	temporal::TimeZone m_zone;
	Clock m_clock;
	std::optional<temporal::Duration> m_shortest;
	temporal::Duration m_longest;
};

/* A definition of a type that needs nothing from the spec. */
template <typename T>
TypeDefinition PlainType(std::vector<std::string> names)
{
	return TypeDefinition{std::move(names), {}, nullptr, [](const TypeKeys &) { return std::make_shared<T>(); }};
}

} // namespace

/* A 32-bit process has a 32-bit time_t, which ends in January 2038; the
 * kernel's 64-bit clock does not. */
int64_t SystemClock()
{
#if defined(__i386__) && defined(SYS_clock_gettime64)
	struct {
		int64_t seconds;
		int64_t nanoseconds;
	} now;
	if (syscall(SYS_clock_gettime64, CLOCK_REALTIME, &now) == 0)
		return now.seconds;
#endif
	return static_cast<int64_t>(time(nullptr));
}

TypeRegistry::TypeRegistry(Clock clock)
{
	Register(PlainType<FlagType>({"flag"}));
	Register(PlainType<BoolType>({"!", "bool", "boolean"}));
	Register(PlainType<CounterType>({"+", "counter", "count"}));
	Register(PlainType<StringType>({"s", "string", "str"}));
	Register(PlainType<UrlType>({"url", "uri"}));
	Register({{"i", "int", "integer"}, {"min", "max", "bigint"}, CheckIntKeys,
	          [](const TypeKeys &keys) { return std::make_shared<IntType>(keys); }});
	Register({{"f", "float", "num", "number"}, {"min", "max"}, CheckBoundKeys,
	          [](const TypeKeys &keys) { return std::make_shared<FloatType>(keys); }});
	Register({{"date"}, {"timezone", "min", "max"}, [clock](const TypeKeys &keys) { return CheckDateKeys(keys, clock); },
	          [clock](const TypeKeys &keys) { return std::make_shared<DateType>(keys, clock); }});
	Register({{"duration"}, {"timezone", "min", "max"}, [clock](const TypeKeys &keys) { return CheckDurationKeys(keys, clock); },
	          [clock](const TypeKeys &keys) { return std::make_shared<DurationType>(keys, clock); }});
}

void TypeRegistry::Register(TypeDefinition definition)
{
	if (definition.names.empty())
		throw std::invalid_argument("an option type needs at least one name");

	for (std::string &name : definition.names) {
		name = text::ToLower(name);
		if (name.empty())
			throw std::invalid_argument("an option type name must not be empty");
		if (m_byName.count(name))
			throw std::invalid_argument(Format("option type name '%s' is already registered", name.c_str()));
	}

	auto shared = std::make_shared<const TypeDefinition>(std::move(definition));
	for (const std::string &name : shared->names)
		m_byName[name] = shared;
}

void TypeRegistry::SetParent(const TypeRegistry *parent)
{
	m_parent = parent;
}

const TypeDefinition *TypeRegistry::Find(const std::string &name) const
{
	auto found = m_byName.find(text::ToLower(name));
	if (found != m_byName.end())
		return found->second.get();
	return m_parent ? m_parent->Find(name) : nullptr;
}

std::vector<std::string> TypeRegistry::KnownNames() const
{
	std::set<std::string> names;
	for (const auto &entry : m_byName)
		names.insert(entry.first);
	if (m_parent) {
		for (const std::string &name : m_parent->KnownNames())
			names.insert(name);
	}
	return std::vector<std::string>(names.begin(), names.end());
}

} // namespace smgetopt
