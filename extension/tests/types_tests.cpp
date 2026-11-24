/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Tests of the built-in types and the registry. */
#include "check.h"
#include "support.h"

#include <algorithm>

#include <stdexcept>

using namespace smgetopt;
using support::Types;

namespace {

std::shared_ptr<const Type> TypeNamed(const std::string &name, const TypeKeys &keys = {})
{
	return Types().Find(name)->create(keys);
}

/* The problem a type has with value, "" when it accepts it. */
std::string ProblemOf(const Type &type, const std::string &value)
{
	return type.Check(value).value_or("");
}

std::string KeyProblemOf(const std::string &name, const TypeKeys &keys)
{
	const TypeDefinition *definition = Types().Find(name);
	return definition->checkSpecKeys ? definition->checkSpecKeys(keys).value_or("") : "";
}

} // namespace

TEST(registry_resolves_names_and_aliases)
{
	CHECK(Types().Find("s") == Types().Find("string"));
	CHECK(Types().Find("str") == Types().Find("string"));
	CHECK(Types().Find("!") == Types().Find("boolean"));
	CHECK(Types().Find("+") == Types().Find("count"));
	CHECK(Types().Find("i") == Types().Find("integer"));
	CHECK(Types().Find("f") == Types().Find("number"));
	CHECK(Types().Find("num") == Types().Find("float"));
	CHECK(Types().Find("uri") == Types().Find("url"));
	CHECK(Types().Find("INT") == Types().Find("int"));
	CHECK(Types().Find("nope") == nullptr);
	CHECK(Types().Find("") == nullptr);
	CHECK_EQ(text::Join(Types().KnownNames(), " "),
	         "! + bool boolean count counter date duration f flag float i int integer num number s str string uri url");
}

TEST(kinds)
{
	CHECK(TypeNamed("flag")->Kind() == TypeKind::Flag);
	CHECK(TypeNamed("bool")->Kind() == TypeKind::Bool);
	CHECK(TypeNamed("counter")->Kind() == TypeKind::Counter);
	CHECK(!TypeNamed("flag")->TakesValue());
	CHECK(!TypeNamed("bool")->TakesValue());
	CHECK(!TypeNamed("counter")->TakesValue());
	for (const char *name : {"string", "int", "float", "url", "date", "duration"})
		CHECK(TypeNamed(name)->TakesValue());
}

TEST(flag_bool_and_counter)
{
	auto flag = TypeNamed("flag");
	for (const char *value : {"1", "0", ""})
		CHECK_EQ(ProblemOf(*flag, value), "");
	CHECK_EQ(ProblemOf(*TypeNamed("bool"), "no"), "'no' is not a boolean (use 1 or 0)");
	CHECK_EQ(flag->Coerce("1").text, "1");
	CHECK_EQ(*flag->Coerce("1").integer, 1);
	CHECK_EQ(flag->Coerce("").text, "0");
	CHECK_EQ(*flag->Coerce("0").integer, 0);

	auto counter = TypeNamed("counter");
	CHECK_EQ(ProblemOf(*counter, "3"), "");
	CHECK_EQ(*counter->Coerce("3").integer, 3);
	CHECK_EQ(counter->Coerce("003").text, "3");
	CHECK_EQ(ProblemOf(*counter, "lots"), "'lots' is not a count");
	CHECK_EQ(ProblemOf(*counter, "-1"), "'-1' is not a count");
	CHECK_EQ(ProblemOf(*counter, "+1"), "'+1' is not a count");
	CHECK_EQ(ProblemOf(*counter, ""), "'' is not a count");
	CHECK_EQ(ProblemOf(*counter, "99999999999999999999"), "'99999999999999999999' is not a count");
}

TEST(string_takes_anything)
{
	auto string = TypeNamed("string");
	CHECK_EQ(ProblemOf(*string, ""), "");
	CHECK_EQ(string->Coerce(" a b ").text, " a b ");
	CHECK(!string->Coerce("5").integer);
	CHECK_EQ(string->Label(), "");
}

TEST(int_checks_and_converts)
{
	auto bounded = TypeNamed("int", {{"min", "1"}, {"max", "10"}});
	CHECK_EQ(ProblemOf(*bounded, "5"), "");
	CHECK_EQ(ProblemOf(*bounded, "1"), "");
	CHECK_EQ(ProblemOf(*bounded, "10"), "");
	CHECK_EQ(ProblemOf(*bounded, "abc"), "'abc' is not an integer");
	CHECK_EQ(ProblemOf(*bounded, "0"), "0 is smaller than the minimum of 1");
	CHECK_EQ(ProblemOf(*bounded, "11"), "11 is larger than the maximum of 10");
	CHECK_EQ(ProblemOf(*bounded, "5\n"), "'5\n' is not an integer");
	CHECK_EQ(ProblemOf(*bounded, "1.0"), "'1.0' is not an integer");
	CHECK_EQ(ProblemOf(*bounded, ""), "'' is not an integer");
	CHECK_EQ(ProblemOf(*bounded, "\xD9\xA3"), "'\xD9\xA3' is not an integer");

	Scalar five = bounded->Coerce("05");
	CHECK_EQ(five.text, "5");
	CHECK_EQ(*five.integer, 5);
	CHECK_EQ(*five.real, 5.0);
	CHECK_EQ(bounded->Coerce("+007").text, "7");

	auto fraction = TypeNamed("int", {{"min", "1.5"}});
	CHECK_EQ(ProblemOf(*fraction, "1"), "1 is smaller than the minimum of 1.5");
	CHECK_EQ(ProblemOf(*fraction, "2"), "");
}

TEST(int_fits_a_cell)
{
	auto wide = TypeNamed("int");
	for (const char *value : {"-0", "+007", "-007", "2147483647", "-2147483648", "0002147483647"})
		CHECK_EQ(ProblemOf(*wide, value), "");
	CHECK_EQ(ProblemOf(*wide, "2147483648"), "'2147483648' is too large for an integer");
	CHECK_EQ(ProblemOf(*wide, "-2147483649"), "'-2147483649' is too large for an integer");
	CHECK(support::Contains(ProblemOf(*wide, std::string(400, '9')), "is too large for an integer"));
	CHECK_EQ(wide->Coerce("-0").text, "0");
	CHECK_EQ(*wide->Coerce("-2147483648").integer, -2147483648LL);
}

TEST(bigint_keeps_any_size_exactly)
{
	const std::string huge = "123456789012345678901234567890";
	auto big = TypeNamed("int", {{"bigint", "1"}, {"min", "100000000000000000001"}});
	CHECK_EQ(ProblemOf(*big, huge), "");
	CHECK_EQ(big->Coerce(huge).text, huge);
	CHECK(!big->Coerce(huge).integer);
	CHECK(!big->Coerce(huge).real);
	CHECK_EQ(big->Coerce("+00123456789012345678901").text, "123456789012345678901");
	CHECK_EQ(*TypeNamed("int", {{"bigint", "1"}})->Coerce("76561198000000000").integer, 76561198000000000LL);
	CHECK_EQ(ProblemOf(*big, "100000000000000000000"),
	         "100000000000000000000 is smaller than the minimum of 100000000000000000001");
	CHECK_EQ(ProblemOf(*big, "100000000000000000001"), "");
	CHECK_EQ(ProblemOf(*big, "1.5"), "'1.5' is not an integer");
	CHECK_EQ(ProblemOf(*TypeNamed("int", {{"bigint", "0"}}), "2147483648"), "'2147483648' is too large for an integer");
}

TEST(number_keys_are_checked)
{
	CHECK_EQ(KeyProblemOf("int", {{"min", "1"}, {"max", "10"}}), "");
	CHECK_EQ(KeyProblemOf("int", {{"min", "5"}, {"max", "5"}}), "");
	CHECK_EQ(KeyProblemOf("int", {{"min", "abc"}}), "min must be a number, not 'abc'");
	CHECK_EQ(KeyProblemOf("int", {{"max", ""}}), "max must be a number, not ''");
	CHECK_EQ(KeyProblemOf("int", {{"min", "10"}, {"max", "5"}}), "min 10 is larger than max 5");
	CHECK_EQ(KeyProblemOf("float", {{"min", "1.5"}, {"max", "1.25"}}), "min 1.5 is larger than max 1.25");
	CHECK_EQ(KeyProblemOf("float", {{"min", "x"}}), "min must be a number, not 'x'");
	CHECK_EQ(KeyProblemOf("int", {{"bigint", "1"}, {"max", "1e3"}}), "max must be an integer with bigint, not '1e3'");
	CHECK_EQ(KeyProblemOf("int", {{"bigint", "1"}, {"min", "1.5"}}), "min must be an integer with bigint, not '1.5'");
	CHECK_EQ(KeyProblemOf("int", {{"bigint", "1"}, {"min", "abc"}}), "min must be a number, not 'abc'");
	CHECK_EQ(KeyProblemOf("int", {{"bigint", "1"}, {"min", "100000000000000000001"}, {"max", "100000000000000000000"}}),
	         "min 100000000000000000001 is larger than max 100000000000000000000");
	CHECK_EQ(KeyProblemOf("int", {{"bigint", "0"}, {"max", "1e3"}}), "");
	/* The checks every number type has come first. */
	CHECK_EQ(KeyProblemOf("int", {{"bigint", "1"}, {"min", "5.5"}, {"max", "1"}}), "min 5.5 is larger than max 1");
}

TEST(float_checks_and_converts)
{
	auto number = TypeNamed("float");
	for (const char *value : {"3.14", "-2", ".5", "1e3", "5."})
		CHECK_EQ(ProblemOf(*number, value), "");
	CHECK_EQ(ProblemOf(*number, "x"), "'x' is not a number");
	CHECK_EQ(ProblemOf(*number, ""), "'' is not a number");
	CHECK_EQ(ProblemOf(*number, "0x10"), "'0x10' is not a number");
	CHECK_EQ(ProblemOf(*number, "1,5"), "'1,5' is not a number");
	CHECK_EQ(ProblemOf(*number, "NaN"), "'NaN' is not a finite number");
	CHECK_EQ(ProblemOf(*number, "-Inf"), "'-Inf' is not a finite number");
	CHECK_EQ(ProblemOf(*number, "infinity"), "'infinity' is not a finite number");
	CHECK_EQ(ProblemOf(*number, "1e999"), "'1e999' is not a finite number");

	Scalar thousand = number->Coerce("1e3");
	CHECK_EQ(thousand.text, "1000");
	CHECK_EQ(*thousand.real, 1000.0);
	CHECK(!thousand.integer);
	CHECK_EQ(number->Coerce(".5").text, "0.5");

	auto bounded = TypeNamed("float", {{"min", "0.5"}, {"max", "2"}});
	CHECK_EQ(ProblemOf(*bounded, "0.5"), "");
	CHECK_EQ(ProblemOf(*bounded, "2.0"), "");
	CHECK_EQ(ProblemOf(*bounded, "0.25"), "0.25 is smaller than the minimum of 0.5");
	CHECK_EQ(ProblemOf(*bounded, "2.5"), "2.5 is larger than the maximum of 2");
}

TEST(url)
{
	auto url = TypeNamed("url");
	CHECK_EQ(ProblemOf(*url, "https://example.com/x"), "");
	CHECK_EQ(ProblemOf(*url, "ssh://git@host/project.git"), "");
	CHECK_EQ(ProblemOf(*url, "file:///tmp/x"), "");
	CHECK_EQ(ProblemOf(*url, "steam+x.y-z://1"), "");
	CHECK_EQ(ProblemOf(*url, "dave@oldserver.example.com:/srv/git/project.git"),
	         "'dave@oldserver.example.com:/srv/git/project.git' is not a URL");
	CHECK_EQ(ProblemOf(*url, "not a url"), "'not a url' is not a URL");
	CHECK_EQ(ProblemOf(*url, "example.com"), "'example.com' is not a URL");
	CHECK_EQ(ProblemOf(*url, "mailto:me@example.com"), "'mailto:me@example.com' is not a URL");
	CHECK_EQ(ProblemOf(*url, "https://"), "'https://' is not a URL");
	CHECK_EQ(ProblemOf(*url, "://x"), "'://x' is not a URL");
	CHECK_EQ(ProblemOf(*url, "1http://x"), "'1http://x' is not a URL");
	CHECK_EQ(ProblemOf(*url, "ht tp://x"), "'ht tp://x' is not a URL");
	CHECK_EQ(ProblemOf(*url, "https://example.com/x\n"), "'https://example.com/x\n' is not a URL");
	CHECK_EQ(ProblemOf(*url, "https://exa mple.com"), "'https://exa mple.com' is not a URL");
	CHECK_EQ(url->Label(), "URL");
	CHECK_EQ(url->Coerce("https://example.com/x").text, "https://example.com/x");
}

TEST(date)
{
	auto date = TypeNamed("date", {{"timezone", "UTC"}});
	CHECK_EQ(ProblemOf(*date, "tomorrow 3pm"), "");
	CHECK_EQ(ProblemOf(*date, "blurb"), "'blurb' is not a date");
	CHECK_EQ(ProblemOf(*date, ""), "'' is not a date");

	Scalar given = date->Coerce("2026-10-06 14:00");
	CHECK_EQ(given.text, "2026-10-06T14:00:00");
	CHECK_EQ(*given.integer, support::kNow + 2 * 3600);
	CHECK_EQ(*given.real, static_cast<double>(support::kNow + 2 * 3600));

	Scalar relative = date->Coerce("tomorrow 3pm");
	CHECK_EQ(relative.text, "2026-10-07T15:00:00");
	CHECK_EQ(date->Label(), "Date");

	CHECK_EQ(TypeNamed("date", {{"timezone", "floating"}})->Coerce("2026-10-06 14:00").text, "2026-10-06T14:00:00");
	CHECK_EQ(ProblemOf(*TypeNamed("date"), "now"), "");
}

TEST(duration)
{
	auto duration = TypeNamed("duration", {{"timezone", "UTC"}});
	Scalar minutes = duration->Coerce("90 minutes");
	CHECK_EQ(minutes.text, "90 minutes");
	CHECK_EQ(*minutes.integer, 5400);
	CHECK_EQ(minutes.duration->seconds, 5400);
	CHECK_EQ(minutes.duration->days, 0);
	CHECK_EQ(minutes.duration->months, 0);

	Scalar days = duration->Coerce("for 2 days");
	CHECK_EQ(days.duration->days, 2);
	CHECK_EQ(*days.integer, 2 * 86400);

	/* A month counts from now: October has 31 days. */
	Scalar month = duration->Coerce("1 month");
	CHECK_EQ(month.duration->months, 1);
	CHECK_EQ(*month.integer, 31 * 86400);

	CHECK_EQ(ProblemOf(*duration, "1 hour 30 minutes"), "'1 hour 30 minutes' is not a duration");
	CHECK_EQ(ProblemOf(*duration, "soon"), "'soon' is not a duration");
	CHECK_EQ(ProblemOf(*duration, "1h"), "'1h' is not a duration");
	CHECK_EQ(duration->Label(), "Duration");
}

TEST(dates_and_durations_fit_a_cell_unless_widened)
{
	auto date = TypeNamed("date", {{"timezone", "UTC"}});
	CHECK_EQ(ProblemOf(*date, "2038-01-19 03:14:07"), "");
	CHECK_EQ(ProblemOf(*date, "2038-01-19 03:14:08"), "'2038-01-19 03:14:08' is later than the maximum of 2038-01-19T03:14:07");
	CHECK_EQ(ProblemOf(*date, "1901-12-13 20:45:52"), "");
	CHECK_EQ(ProblemOf(*date, "1901-12-13 20:45:51"), "'1901-12-13 20:45:51' is earlier than the minimum of 1901-12-13T20:45:52");

	auto wide = TypeNamed("date", {{"timezone", "UTC"}, {"min", "0001-01-01"}, {"max", "253402300799"}});
	CHECK_EQ(*wide->Coerce("2040-01-01").integer, 2208988800LL);
	CHECK_EQ(ProblemOf(*wide, "1066-10-14"), "");

	auto future = TypeNamed("date", {{"timezone", "UTC"}, {"min", "now"}, {"max", "in 1 week"}});
	CHECK_EQ(ProblemOf(*future, "tomorrow"), "");
	CHECK_EQ(ProblemOf(*future, "yesterday"), "'yesterday' is earlier than the minimum of 2026-10-06T12:00:00");
	CHECK_EQ(ProblemOf(*future, "in 2 weeks"), "'in 2 weeks' is later than the maximum of 2026-10-13T12:00:00");

	auto duration = TypeNamed("duration", {{"timezone", "UTC"}});
	CHECK_EQ(ProblemOf(*duration, "2147483647 seconds"), "");
	CHECK_EQ(ProblemOf(*duration, "2147483648 seconds"), "'2147483648 seconds' is longer than the maximum of 2147483647 seconds");
	CHECK_EQ(ProblemOf(*duration, "100 years"), "'100 years' is longer than the maximum of 2147483647 seconds");

	auto long_ = TypeNamed("duration", {{"timezone", "UTC"}, {"max", "10000 years"}});
	CHECK_EQ(long_->Coerce("100 years").text, "100 years");
	CHECK_EQ(*long_->Coerce("100 years").integer, 3155673600LL);

	auto bounded = TypeNamed("duration", {{"timezone", "UTC"}, {"min", "60"}, {"max", "1 day"}});
	CHECK_EQ(ProblemOf(*bounded, "30 seconds"), "'30 seconds' is shorter than the minimum of 1 minute");
	CHECK_EQ(ProblemOf(*bounded, "25 hours"), "'25 hours' is longer than the maximum of 1 day");
	CHECK_EQ(ProblemOf(*bounded, "24 hours"), "");
}

TEST(a_date_reads_the_clock_once_per_value)
{
	/* A clock that moves on with every reading. */
	int64_t readings = 0;
	TypeRegistry types([&readings] { return support::kNow + readings++; });
	auto date = types.Find("date")->create({{"timezone", "UTC"}, {"min", "now"}});
	Conversion now = date->Convert("now");
	CHECK(!now.problem);
	CHECK_EQ(*now.scalar.integer, support::kNow);
	CHECK_EQ(now.scalar.text, "2026-10-06T12:00:00");
	CHECK_EQ(readings, 1);

	auto duration = types.Find("duration")->create({{"timezone", "UTC"}, {"max", "1 month"}});
	CHECK(!duration->Convert("1 month").problem);
	CHECK_EQ(readings, 2);
}

TEST(temporal_bounds_are_checked)
{
	CHECK_EQ(KeyProblemOf("date", {{"min", "now"}, {"max", "2030-01-01"}}), "");
	CHECK_EQ(KeyProblemOf("date", {{"max", "4102444800"}}), "");
	CHECK_EQ(KeyProblemOf("date", {{"min", "soon"}}), "min must be a date or a Unix timestamp, not 'soon'");
	CHECK_EQ(KeyProblemOf("date", {{"max", "1 day"}}), "max must be a date or a Unix timestamp, not '1 day'");
	CHECK_EQ(KeyProblemOf("date", {{"min", "tomorrow"}, {"max", "today"}}), "min tomorrow is larger than max today");
	CHECK_EQ(KeyProblemOf("date", {{"timezone", "UTC"}, {"min", "2040-01-01"}}),
	         "min 2040-01-01 is later than the cell's maximum of 2038-01-19T03:14:07; set max as well");
	CHECK_EQ(KeyProblemOf("date", {{"timezone", "UTC"}, {"max", "1800-01-01"}}),
	         "max 1800-01-01 is earlier than the cell's minimum of 1901-12-13T20:45:52; set min as well");
	CHECK_EQ(KeyProblemOf("duration", {{"min", "1 minute"}, {"max", "86400"}}), "");
	CHECK_EQ(KeyProblemOf("duration", {{"min", "-1"}}), "min must be a duration or a number of seconds, not '-1'");
	CHECK_EQ(KeyProblemOf("duration", {{"max", "tomorrow"}}), "max must be a duration or a number of seconds, not 'tomorrow'");
	CHECK_EQ(KeyProblemOf("duration", {{"min", "1 month"}, {"max", "30 days"}}), "min 1 month is larger than max 30 days");
	CHECK_EQ(KeyProblemOf("duration", {{"min", "100 years"}}),
	         "min 100 years is longer than the cell's maximum of 2147483647 seconds; set max as well");
}

TEST(temporal_keys_are_checked)
{
	CHECK_EQ(KeyProblemOf("date", {}), "");
	CHECK_EQ(KeyProblemOf("date", {{"timezone", "UTC"}}), "");
	CHECK_EQ(KeyProblemOf("duration", {{"timezone", "floating"}}), "");
	CHECK_EQ(KeyProblemOf("date", {{"timezone", ""}}), "timezone must be a non-empty string");
	CHECK_EQ(KeyProblemOf("date", {{"timezone", "Nowhere/Bogus"}}), "timezone 'Nowhere/Bogus' is not a known time zone");
	CHECK_EQ(KeyProblemOf("duration", {{"timezone", "../etc/passwd"}}), "timezone '../etc/passwd' is not a known time zone");
}

namespace {

struct StringType : Type {
	TypeKind Kind() const override { return TypeKind::Value; }
};

} // namespace

TEST(a_registry_falls_back_to_its_parent)
{
	auto plain = [](const TypeKeys &) { return std::make_shared<StringType>(); };
	TypeRegistry shared;
	shared.Register({{"map"}, {}, nullptr, plain});
	shared.Register({{"cvar"}, {}, nullptr, plain});

	TypeRegistry own;
	CHECK(own.Find("map") == nullptr);
	own.SetParent(&shared);
	CHECK(own.Find("MAP") == shared.Find("map"));
	CHECK(own.Find("string") == own.Find("s"));

	/* A program's own type comes before the parent's. */
	own.Register({{"cvar"}, {}, nullptr, plain});
	CHECK(own.Find("cvar") != nullptr);
	CHECK(own.Find("cvar") != shared.Find("cvar"));
	std::vector<std::string> names = own.KnownNames();
	CHECK_EQ(std::count(names.begin(), names.end(), "cvar"), 1);
	CHECK_EQ(std::count(names.begin(), names.end(), "map"), 1);
	CHECK_EQ(names.size(), shared.KnownNames().size());

	own.SetParent(nullptr);
	CHECK(own.Find("map") == nullptr);
	CHECK(own.Find("cvar") != nullptr);
}

TEST(custom_type_registration)
{
	struct HexType : Type {
		TypeKind Kind() const override { return TypeKind::Value; }
		Problem Check(const std::string &value) const override
		{
			bool isHex = !value.empty() && value.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
			return isHex ? Problem() : Problem("'" + value + "' is not hex");
		}
	};
	TypeDefinition hex{{"Hex", "hexadecimal"}, {}, nullptr, [](const TypeKeys &) { return std::make_shared<HexType>(); }};

	TypeRegistry types;
	types.Register(hex);
	CHECK(types.Find("hex") != nullptr);
	CHECK(types.Find("HEXADECIMAL") == types.Find("hex"));
	CHECK(!types.Find("hex")->create({})->Check("deadbeef"));
	CHECK_EQ(*types.Find("hex")->create({})->Check("xyz"), "'xyz' is not hex");
	CHECK(Types().Find("hex") == nullptr);

	auto refused = [&types](TypeDefinition definition) {
		try {
			types.Register(std::move(definition));
		} catch (const std::invalid_argument &error) {
			return std::string(error.what());
		}
		return std::string();
	};
	CHECK_EQ(refused(hex), "option type name 'hex' is already registered");
	CHECK_EQ(refused({{"mine", "STRING"}, {}, nullptr, hex.create}), "option type name 'string' is already registered");
	CHECK(types.Find("mine") == nullptr);
	CHECK_EQ(refused({{}, {}, nullptr, hex.create}), "an option type needs at least one name");
	CHECK_EQ(refused({{"ok", ""}, {}, nullptr, hex.create}), "an option type name must not be empty");
}
