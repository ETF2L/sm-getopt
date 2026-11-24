/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Tests of the parser: the command line syntax, the value shapes, args,
 * commands, inherited options, the checks a value passes and every user
 * error with its message. */
#include "check.h"
#include "support.h"

#include <algorithm>

using namespace smgetopt;
using namespace support;

namespace {

/* A spec with a flag -a, a counter -v/--verbose, a bool --color/-c, a
 * value option --owner/-o and the args "first" and "rest...". */
RawSpec SyntaxSpec()
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "all|a");
	AddOption(raw.root, "verbose|v", {{"type", "counter"}});
	AddOption(raw.root, "color|c", {{"type", "bool"}});
	AddOption(raw.root, "owner|o", {{"type", "string"}});
	AddArg(raw.root, "first");
	AddArg(raw.root, "rest", {{"multiple", "1"}});
	return raw;
}

const LevelResult &ValuesOf(const ParseResult &result)
{
	return *result.values;
}

} // namespace

/* ---------------------------------------------------------------- options */

TEST(basic_options)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "owner|o", {{"type", "s"}, {"required", "1"}});
	AddOption(raw.root, "work-dir", {{"type", "s"}});
	AddOption(raw.root, "repo", {{"type", "s"}});

	ParseResult result = ParseWith(raw, {"--owner", "dave", "--work-dir", "/tmp"});
	CHECK(result.outcome == Outcome::Parsed);
	CHECK(result.output.empty());
	CHECK_EQ(result.error, "");
	CHECK_EQ(TextOf(ValuesOf(result), "owner"), "dave");
	CHECK_EQ(TextOf(ValuesOf(result), "work-dir"), "/tmp");
	CHECK_EQ(TextOf(ValuesOf(result), "repo"), "(unset)");
	CHECK(ValuesOf(result).level == result.spec->root.get());
	CHECK(!ValuesOf(result).command);
	CHECK(ValuesOf(result).subcommand == nullptr);
	CHECK(!Holds(ValuesOf(result), "help"));
}

TEST(long_options)
{
	RawSpec raw = SyntaxSpec();
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--owner", "dave"})), "owner"), "dave");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--owner=dave"})), "owner"), "dave");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--owner=a=b"})), "owner"), "a=b");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--owner", ""})), "owner"), "");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--o", "dave"})), "owner"), "dave");
	CHECK_EQ(ErrorOf(raw, {"--own", "x"}), "Unknown option: own");
	CHECK_EQ(ErrorOf(raw, {"--Owner", "x"}), "Unknown option: Owner");
	CHECK_EQ(ErrorOf(raw, {"--owner"}), "Option owner requires an argument");
	CHECK_EQ(ErrorOf(raw, {"--owner="}), "Option owner requires an argument");
	CHECK_EQ(ErrorOf(raw, {"--all=1"}), "Option all does not take an argument");
	CHECK_EQ(ErrorOf(raw, {"--verbose=2"}), "Option verbose does not take an argument");
	CHECK_EQ(ErrorOf(raw, {"--frobnicate"}), "Unknown option: frobnicate");
	CHECK_EQ(ErrorOf(raw, {"--frobnicate=5"}), "Unknown option: frobnicate");
}

TEST(short_options_and_bundling)
{
	RawSpec raw = SyntaxSpec();
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-o", "dave"})), "owner"), "dave");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-odave"})), "owner"), "dave");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-o=dave"})), "owner"), "=dave");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-vvv"})), "verbose"), "3");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-v", "--verbose", "-vv"})), "verbose"), "4");

	ParseResult bundled = ParseWith(raw, {"-avodave"});
	CHECK_EQ(TextOf(ValuesOf(bundled), "all"), "1");
	CHECK_EQ(TextOf(ValuesOf(bundled), "verbose"), "1");
	CHECK_EQ(TextOf(ValuesOf(bundled), "owner"), "dave");

	ParseResult next = ParseWith(raw, {"-vo", "dave"});
	CHECK_EQ(TextOf(ValuesOf(next), "verbose"), "1");
	CHECK_EQ(TextOf(ValuesOf(next), "owner"), "dave");

	CHECK_EQ(ErrorOf(raw, {"-o"}), "Option o requires an argument");
	CHECK_EQ(ErrorOf(raw, {"-vo"}), "Option o requires an argument");
	CHECK_EQ(ErrorOf(raw, {"-x"}), "Unknown option: x");
	CHECK_EQ(ErrorOf(raw, {"-5"}), "Unknown option: 5");
	CHECK_EQ(ErrorOf(raw, {"-v=1"}), "Unknown option: =; Unknown option: 1");
	CHECK_EQ(ErrorOf(raw, {"-\xC3\xB6"}), "Unknown option: \xC3\xB6");
	CHECK_EQ(ErrorOf(raw, {"-V"}), "Unknown option: V");
	CHECK_EQ(ErrorOf(raw, {"--=foo"}), "Unknown option: =foo");
	/* What is left of a bundle is read as a word of its own with a dash. */
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-v-owner=x"})), "owner"), "x");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-v-", "-a"})), "first"), "-a");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-v-", "-a"})), "all"), "(unset)");
	/* Only a dash starts an option. */
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"+v"})), "first"), "+v");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"+v"})), "verbose"), "(unset)");
	/* A long name with one dash is read letter by letter. */
	CHECK_EQ(ErrorOf(raw, {"-verbose"}), "Unknown option: e; Unknown option: r; Unknown option: b");
	CHECK_EQ(ErrorOf(raw, {"-all"}), "Unknown option: l; Unknown option: l");
}

TEST(negation)
{
	RawSpec raw = SyntaxSpec();
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--color"})), "color"), "1");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-c"})), "color"), "1");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--no-color"})), "color"), "0");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--nocolor"})), "color"), "0");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--no-c"})), "color"), "0");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--no-color", "--color"})), "color"), "1");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--color", "--no-color"})), "color"), "0");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {})), "color"), "(unset)");
	CHECK_EQ(ErrorOf(raw, {"--no-all"}), "Unknown option: no-all");
	CHECK_EQ(ErrorOf(raw, {"--no-verbose"}), "Unknown option: no-verbose");
	CHECK_EQ(ErrorOf(raw, {"--no-owner"}), "Unknown option: no-owner");
	CHECK_EQ(ErrorOf(raw, {"--no-color=1"}), "Option no-color does not take an argument");
}

TEST(repeating_options)
{
	RawSpec raw = SyntaxSpec();
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--owner", "a", "-o", "b"})), "owner"), "b");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-a", "--all", "-a"})), "all"), "1");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {})), "all"), "(unset)");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {})), "verbose"), "(unset)");
}

TEST(values_that_start_with_a_dash)
{
	RawSpec raw = SyntaxSpec();
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--owner", "-5"})), "owner"), "-5");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--owner", "--all"})), "owner"), "--all");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--owner", "--all"})), "all"), "(unset)");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-o", "--"})), "owner"), "--");
}

TEST(options_and_args_mix)
{
	RawSpec raw = SyntaxSpec();
	ParseResult result = ParseWith(raw, {"a.txt", "--all", "b.txt", "-v", "c.txt"});
	CHECK_EQ(TextOf(ValuesOf(result), "first"), "a.txt");
	CHECK_EQ(ListOf(ValuesOf(result), "rest"), "b.txt|c.txt");
	CHECK_EQ(TextOf(ValuesOf(result), "all"), "1");
	CHECK_EQ(TextOf(ValuesOf(result), "verbose"), "1");
}

TEST(the_end_of_options)
{
	RawSpec raw = SyntaxSpec();
	ParseResult result = ParseWith(raw, {"-a", "--", "-5", "--owner", "-v"});
	CHECK_EQ(TextOf(ValuesOf(result), "first"), "-5");
	CHECK_EQ(ListOf(ValuesOf(result), "rest"), "--owner|-v");
	CHECK_EQ(TextOf(ValuesOf(result), "owner"), "(unset)");
	CHECK_EQ(TextOf(ValuesOf(result), "verbose"), "(unset)");

	ParseResult early = ParseWith(raw, {"x", "--", "-y"});
	CHECK_EQ(TextOf(ValuesOf(early), "first"), "x");
	CHECK_EQ(ListOf(ValuesOf(early), "rest"), "-y");

	/* Only the first -- ends the options; a later one is a word. */
	CHECK_EQ(ListOf(ValuesOf(ParseWith(raw, {"--", "a", "--", "b"})), "rest"), "--|b");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"-"})), "first"), "-");
	CHECK_EQ(ErrorOf(raw, {"-5"}), "Unknown option: 5");
}

TEST(all_problems_of_a_level_are_reported_together)
{
	RawSpec raw = SyntaxSpec();
	CHECK_EQ(ErrorOf(raw, {"--bogus", "--all=1", "-x", "--owner"}),
	         "Unknown option: bogus; Option all does not take an argument; Unknown option: x; Option owner requires an argument");
	ParseResult result = ParseWith(raw, {"--bogus"});
	CHECK(result.outcome == Outcome::Rejected);
	CHECK(result.values == nullptr);
	CHECK_EQ(result.output[0], "ERROR: Unknown option: bogus");
	CHECK_EQ(result.output[1], "");
	CHECK_EQ(result.output[2], "# tool [options] [first] [rest...]");
}

/* ------------------------------------------------------------- value shapes */

TEST(defaults_and_required)
{
	RawSpec raw = NewSpec();
	AddDefault(AddOption(raw.root, "private", {{"type", "!"}}), "1");
	RawEntry &logLevel = AddOption(raw.root, "log-level", {{"type", "s"}});
	AddDefault(logLevel, "info");
	SetValid(logLevel, {"debug", "info", "warn"});
	AddDefault(AddOption(raw.root, "keep", {{"type", "i"}}), "007");
	AddDefault(AddOption(raw.root, "force", {}), "0");

	ParseResult defaults = ParseWith(raw, {});
	CHECK_EQ(TextOf(ValuesOf(defaults), "private"), "1");
	CHECK_EQ(TextOf(ValuesOf(defaults), "log-level"), "info");
	CHECK_EQ(TextOf(ValuesOf(defaults), "keep"), "7");
	CHECK_EQ(*ValuesOf(defaults).At("keep")->integer, 7);
	CHECK_EQ(TextOf(ValuesOf(defaults), "force"), "0");

	ParseResult given = ParseWith(raw, {"--no-private", "--log-level", "debug", "--keep=3", "--force"});
	CHECK_EQ(TextOf(ValuesOf(given), "private"), "0");
	CHECK_EQ(TextOf(ValuesOf(given), "log-level"), "debug");
	CHECK_EQ(TextOf(ValuesOf(given), "keep"), "3");
	CHECK_EQ(TextOf(ValuesOf(given), "force"), "1");

	RawSpec required = NewSpec();
	AddOption(required.root, "owner", {{"type", "s"}, {"required", "1"}});
	AddOption(required.root, "w", {{"type", "int"}, {"required", "1"}});
	AddOption(required.root, "force", {{"required", "1"}});
	CHECK_EQ(ErrorOf(required, {}), "missing required option '--force'");
	CHECK_EQ(ErrorOf(required, {"--force"}), "missing required option '--owner'");
	CHECK_EQ(ErrorOf(required, {"--force", "--owner", "x"}), "missing required option '-w'");
	CHECK_EQ(ErrorOf(required, {"--force", "--owner", "x", "-w", "1"}), "(parsed)");
}

TEST(multiple)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "tag|t", {{"type", "s"}, {"multiple", "1"}});
	CHECK_EQ(ListOf(ValuesOf(ParseWith(raw, {"--tag", "a", "-t", "b", "--tag=c", "-td"})), "tag"), "a|b|c|d");
	CHECK_EQ(ListOf(ValuesOf(ParseWith(raw, {"--tag", "a,b"})), "tag"), "a,b");
	ParseResult absent = ParseWith(raw, {});
	CHECK(ValuesOf(absent).ValueNamed("tag").shape == Shape::List);
	CHECK_EQ(ValuesOf(absent).Count("tag"), 0u);

	RawSpec withDefault = NewSpec();
	RawEntry &tag = AddOption(withDefault.root, "tag", {{"type", "i"}, {"multiple", "1"}});
	AddDefault(tag, "1");
	AddDefault(tag, "02");
	CHECK_EQ(ListOf(ValuesOf(ParseWith(withDefault, {})), "tag"), "1|2");
	CHECK_EQ(ListOf(ValuesOf(ParseWith(withDefault, {"--tag", "9"})), "tag"), "9");
	CHECK_EQ(ErrorOf(withDefault, {"--tag", "1", "--tag", "x"}), "option '--tag': 'x' is not an integer");
}

TEST(csv)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "tag", {{"type", "s"}, {"multiple", "1"}, {"csv", "1"}});
	CHECK_EQ(ListOf(ValuesOf(ParseWith(raw, {"--tag", "a, b", "--tag", "c,"})), "tag"), "a|b|c");
	CHECK_EQ(ListOf(ValuesOf(ParseWith(raw, {"--tag", " red , blue ,"})), "tag"), "red|blue");
	CHECK_EQ(ListOf(ValuesOf(ParseWith(raw, {"--tag", "solo"})), "tag"), "solo");
	CHECK_EQ(ErrorOf(raw, {"--tag", "a,,b"}), "option '--tag': 'a,,b' contains an empty item");
	CHECK_EQ(ErrorOf(raw, {"--tag", ","}), "option '--tag': ',' contains an empty item");
	CHECK_EQ(ErrorOf(raw, {"--tag", ",a"}), "option '--tag': ',a' contains an empty item");
	CHECK_EQ(ErrorOf(raw, {"--tag", "a,,"}), "option '--tag': 'a,,' contains an empty item");
	CHECK_EQ(ErrorOf(raw, {"--tag", ""}), "option '--tag': '' contains an empty item");
	CHECK_EQ(ErrorOf(raw, {"--tag", "  "}), "option '--tag': '  ' contains an empty item");

	RawSpec typed = NewSpec();
	AddOption(typed.root, "n", {{"type", "i"}, {"multiple", "1"}, {"csv", "1"}});
	CHECK_EQ(ListOf(ValuesOf(ParseWith(typed, {"-n", "1,02"})), "n"), "1|2");
	CHECK_EQ(ErrorOf(typed, {"-n", "1,x"}), "option '-n': 'x' is not an integer");
	/* Every word is split before any item is checked. */
	CHECK_EQ(ErrorOf(typed, {"-n", "x", "-n", "1,,2"}), "option '-n': '1,,2' contains an empty item");

	RawSpec valid = NewSpec();
	SetValid(AddOption(valid.root, "tag", {{"type", "s"}, {"multiple", "1"}, {"csv", "1"}}), {"a"});
	CHECK_EQ(ErrorOf(valid, {"--tag", "a,x"}), "option '--tag': 'x' is not one of: a");
}

TEST(hash)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "define|D", {{"type", "s"}, {"hash", "1"}});
	CHECK_EQ(MappingOf(ValuesOf(ParseWith(raw, {"--define", "os=linux", "--define", "os=bsd", "-D", "flags=a=b"})), "define"),
	         "flags=a=b,os=bsd");
	CHECK_EQ(MappingOf(ValuesOf(ParseWith(raw, {"--define=os=linux", "-Darch=x86", "--define", "empty="})), "define"),
	         "arch=x86,empty=,os=linux");
	CHECK_EQ(ValuesOf(ParseWith(raw, {})).Count("define"), 0u);
	CHECK(ValuesOf(ParseWith(raw, {})).ValueNamed("define").shape == Shape::Mapping);
	CHECK_EQ(ErrorOf(raw, {"--define", "bare"}), "Option define, key \"bare\", requires a value");
	CHECK_EQ(ErrorOf(raw, {"-D", "bare"}), "Option D, key \"bare\", requires a value");
	CHECK_EQ(ErrorOf(raw, {"--define", "=x"}), "option '--define': empty key");
	CHECK_EQ(ErrorOf(raw, {"--define"}), "Option define requires an argument");
	/* In a bundle, a word that is no pair is read on as options. */
	CHECK_EQ(ErrorOf(raw, {"-Dab"}), "Option D, key \"ab\", requires a value; Unknown option: a; Unknown option: b");

	RawSpec typed = NewSpec();
	AddDefaultPair(AddOption(typed.root, "limit", {{"type", "i"}, {"hash", "1"}}), "cpu", "1");
	CHECK_EQ(MappingOf(ValuesOf(ParseWith(typed, {})), "limit"), "cpu=1");
	CHECK_EQ(MappingOf(ValuesOf(ParseWith(typed, {"--limit", "cpu=02", "--limit", "mem=512"})), "limit"), "cpu=2,mem=512");
	CHECK_EQ(MappingOf(ValuesOf(ParseWith(typed, {"--limit", "mem=512"})), "limit"), "mem=512");
	CHECK_EQ(ErrorOf(typed, {"--limit", "cpu=lots"}), "option '--limit': key 'cpu': 'lots' is not an integer");

	RawSpec valid = NewSpec();
	SetValid(AddOption(valid.root, "env", {{"type", "s"}, {"hash", "1"}}), {"on", "off"});
	CHECK_EQ(MappingOf(ValuesOf(ParseWith(valid, {"--env", "anything=on"})), "env"), "anything=on");
	CHECK_EQ(ErrorOf(valid, {"--env", "x=maybe"}), "option '--env': key 'x': 'maybe' is not one of: on, off");
}

TEST(objectlist)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "server", {{"type", "s"}, {"objectlist", "1"}});
	CHECK_EQ(ObjectsOf(ValuesOf(ParseWith(raw, {"--server", "1.host=b", "--server", "0.host=a", "--server", "0.port=80",
	                                             "--server", "0.host=c"})), "server"),
	         "host=c,port=80|host=b");
	CHECK_EQ(ObjectsOf(ValuesOf(ParseWith(raw, {"--server", "0.a_b-c=x=y"})), "server"), "a_b-c=x=y");
	CHECK_EQ(ValuesOf(ParseWith(raw, {})).Count("server"), 0u);
	CHECK(ValuesOf(ParseWith(raw, {})).ValueNamed("server").shape == Shape::Objects);

	std::vector<std::string> many;
	for (int i = 11; i >= 0; i--) {
		many.push_back("--server");
		many.push_back(std::to_string(i) + ".n=" + std::to_string(i));
	}
	CHECK_EQ(ObjectsOf(ValuesOf(ParseWith(raw, many)), "server"), "n=0|n=1|n=2|n=3|n=4|n=5|n=6|n=7|n=8|n=9|n=10|n=11");

	CHECK_EQ(ErrorOf(raw, {"--server", "0.host=a", "--server", "2.host=b"}), "option '--server': missing index 1");
	CHECK_EQ(ErrorOf(raw, {"--server", "host=a"}), "option '--server': invalid key 'host', expected INDEX.FIELD=VALUE");
	CHECK_EQ(ErrorOf(raw, {"--server", "0.a.b=x"}), "option '--server': invalid key '0.a.b', expected INDEX.FIELD=VALUE");
	CHECK_EQ(ErrorOf(raw, {"--server", "0.=x"}), "option '--server': invalid key '0.', expected INDEX.FIELD=VALUE");
	CHECK_EQ(ErrorOf(raw, {"--server", ".host=x"}), "option '--server': invalid key '.host', expected INDEX.FIELD=VALUE");
	CHECK_EQ(ErrorOf(raw, {"--server", "-1.host=x"}), "option '--server': invalid key '-1.host', expected INDEX.FIELD=VALUE");
	CHECK_EQ(ErrorOf(raw, {"--server", "=x"}), "option '--server': invalid key '', expected INDEX.FIELD=VALUE");
	CHECK_EQ(ErrorOf(raw, {"--server", "0.ho st=x"}), "option '--server': invalid key '0.ho st', expected INDEX.FIELD=VALUE");
	CHECK_EQ(ErrorOf(raw, {"--server", "00.host=b", "--server", "0.host=a"}),
	         "option '--server': invalid key '00.host', the index must not have leading zeros");
	CHECK_EQ(ErrorOf(raw, {"--server", "01.host=b"}), "option '--server': invalid key '01.host', the index must not have leading zeros");
	CHECK_EQ(ErrorOf(raw, {"--server", "4000000000.host=a"}), "option '--server': missing index 0");
	CHECK_EQ(ErrorOf(raw, {"--server", "99999999999999999999999.host=a"}), "option '--server': missing index 0");
	CHECK_EQ(ErrorOf(raw, {"--server", "bare"}), "Option server, key \"bare\", requires a value");

	RawSpec typed = NewSpec();
	AddDefaultField(AddOption(typed.root, "limit", {{"type", "i"}, {"objectlist", "1"}}), 0, "cpu", "1");
	CHECK_EQ(ObjectsOf(ValuesOf(ParseWith(typed, {})), "limit"), "cpu=1");
	CHECK_EQ(ObjectsOf(ValuesOf(ParseWith(typed, {"--limit", "0.cpu=02"})), "limit"), "cpu=2");
	CHECK_EQ(ErrorOf(typed, {"--limit", "0.cpu=lots"}), "option '--limit': entry 0: key 'cpu': 'lots' is not an integer");
	CHECK_EQ(ErrorOf(typed, {"--limit", "0.cpu=1", "--limit", "1.cpu=x"}), "option '--limit': entry 1: key 'cpu': 'x' is not an integer");
}

/* ------------------------------------------------------------------- checks */

TEST(validation_failures)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "width", {{"type", "i"}, {"min", "1"}, {"max", "100"}});
	SetValid(AddOption(raw.root, "log-level", {{"type", "s"}}), {"info", "warn"});
	AddOption(raw.root, "user-id", {{"type", "i"}}).validSource = [] { return std::vector<std::string>{"1", "2"}; };
	AddOption(raw.root, "even", {{"type", "i"}}).lazyValid = [](const std::string &value) { return (value.back() - '0') % 2 == 0; };
	SetValid(AddOption(raw.root, "port", {{"type", "i"}}), {"80", "443"});
	SetValid(AddOption(raw.root, "none", {{"type", "s"}}), {});
	AddOption(raw.root, "ratio", {{"type", "float"}});
	AddOption(raw.root, "url", {{"type", "url"}});

	CHECK_EQ(ErrorOf(raw, {"--width", "abc"}), "option '--width': 'abc' is not an integer");
	CHECK_EQ(ErrorOf(raw, {"--width", "0"}), "option '--width': 0 is smaller than the minimum of 1");
	CHECK_EQ(ErrorOf(raw, {"--width", "101"}), "option '--width': 101 is larger than the maximum of 100");
	CHECK_EQ(ErrorOf(raw, {"--log-level", "nope"}), "option '--log-level': 'nope' is not one of: info, warn");
	CHECK_EQ(ErrorOf(raw, {"--log-level", "INFO"}), "option '--log-level': 'INFO' is not one of: info, warn");
	CHECK_EQ(ErrorOf(raw, {"--user-id", "3"}), "option '--user-id': '3' is not one of: 1, 2");
	CHECK_EQ(ErrorOf(raw, {"--user-id", "2"}), "(parsed)");
	CHECK_EQ(ErrorOf(raw, {"--even", "3"}), "option '--even': '3' is not a valid value");
	CHECK_EQ(ErrorOf(raw, {"--even", "4"}), "(parsed)");
	CHECK_EQ(ErrorOf(raw, {"--none", "x"}), "option '--none': 'x' is not one of: ");
	CHECK_EQ(ErrorOf(raw, {"--ratio", "x"}), "option '--ratio': 'x' is not a number");
	CHECK_EQ(ErrorOf(raw, {"--url", "x"}), "option '--url': 'x' is not a URL");

	/* A valid list is compared with the converted value. */
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"--port", "080"})), "port"), "80");
	CHECK_EQ(ErrorOf(raw, {"--port", "81"}), "option '--port': '81' is not one of: 80, 443");
}

TEST(a_valid_source_is_asked_once_per_parse)
{
	int calls = 0;
	RawSpec raw = NewSpec();
	AddOption(raw.root, "tag", {{"type", "s"}, {"multiple", "1"}}).validSource = [&calls] {
		calls++;
		return std::vector<std::string>{"a", "b"};
	};
	std::shared_ptr<const Spec> spec = Compile(raw);
	CHECK_EQ(Parse(spec, {"--tag", "a", "--tag", "b", "--tag", "c"}).error, "option '--tag': 'c' is not one of: a, b");
	CHECK_EQ(calls, 1);
	CHECK(Parse(spec, {"--tag", "a", "--tag", "b"}).outcome == Outcome::Parsed);
	CHECK_EQ(calls, 2);
}

TEST(a_default_that_follows_the_clock_is_converted_for_every_parse)
{
	int64_t now = kNow;
	TypeRegistry types([&now] { return now; });
	RawSpec raw = NewSpec();
	AddDefault(AddOption(raw.root, "since", {{"type", "date"}, {"timezone", "UTC"}}), "today");
	AddDefault(AddOption(raw.root, "until", {{"type", "date"}, {"timezone", "UTC"}, {"max", "2026-10-07"}}), "tomorrow");
	std::shared_ptr<const Spec> spec = Spec::Compile(raw, types);
	CHECK_EQ(TextOf(*Parse(spec, {}).values, "since"), "2026-10-06T00:00:00");

	now += 86400;
	ParseResult nextDay = Parse(spec, {"--until", "2026-10-07"});
	CHECK_EQ(TextOf(*nextDay.values, "since"), "2026-10-07T00:00:00");
	CHECK_EQ(Parse(spec, {}).error, "option '--until': default value: 'tomorrow' is later than the maximum of 2026-10-07T00:00:00");
}

TEST(the_valid_list_comes_before_lazy_valid)
{
	int calls = 0;
	RawSpec raw = NewSpec();
	RawEntry &option = AddOption(raw.root, "n", {{"type", "i"}});
	SetValid(option, {"1", "2"});
	option.lazyValid = [&calls](const std::string &value) {
		calls++;
		return value == "2";
	};
	CHECK_EQ(ErrorOf(raw, {"-n", "3"}), "option '-n': '3' is not one of: 1, 2");
	CHECK_EQ(calls, 0);
	CHECK_EQ(ErrorOf(raw, {"-n", "1"}), "option '-n': '1' is not a valid value");
	CHECK_EQ(ErrorOf(raw, {"-n", "02"}), "(parsed)");
	CHECK_EQ(calls, 2);
}

TEST(verify_and_prepare_run_on_the_values_a_parse_settles_on)
{
	/* A type that records what it is asked, and fails for "bad" in verify
	 * and "stuck" in prepare. */
	struct Recording : Type {
		explicit Recording(std::vector<std::string> &log) : log(log) {}
		TypeKind Kind() const override { return TypeKind::Value; }
		Problem Verify(const Scalar &value, const ParseContext &) const override
		{
			log.push_back("verify " + value.text);
			return value.text == "bad" ? Problem("'bad' is not there") : Problem();
		}
		Problem Prepare(const Scalar &value, const ParseContext &) const override
		{
			log.push_back("prepare " + value.text);
			return value.text == "stuck" ? Problem("cannot make 'stuck'") : Problem();
		}
		std::vector<std::string> &log;
	};

	std::vector<std::string> log;
	TypeRegistry types;
	types.Register({{"recording"}, {}, nullptr, [&log](const TypeKeys &) { return std::make_shared<Recording>(log); }});

	RawSpec raw = NewSpec();
	AddDefault(AddOption(raw.root, "a", {{"type", "recording"}}), "default-a");
	AddOption(raw.root, "b", {{"type", "recording"}, {"multiple", "1"}});
	AddOption(raw.root, "c", {{"type", "recording"}});
	AddOption(raw.root, "must", {{"type", "s"}});
	AddArg(raw.root, "word", {{"type", "recording"}});
	auto spec = Spec::Compile(raw, types);
	CHECK(log.empty());

	ParseResult result = Parse(spec, {"-b", "x", "-b", "y", "w"});
	CHECK(result.outcome == Outcome::Parsed);
	CHECK_EQ(text::Join(log, ", "),
	         "verify default-a, verify x, verify y, verify w, prepare default-a, prepare x, prepare y, prepare w");

	log.clear();
	CHECK_EQ(Parse(spec, {"-a", "given"}).error, "");
	CHECK_EQ(text::Join(log, ", "), "verify given, prepare given");

	log.clear();
	CHECK_EQ(Parse(spec, {"-b", "x", "-b", "bad", "w"}).error, "option '-b': 'bad' is not there");
	CHECK_EQ(text::Join(log, ", "), "verify default-a, verify x, verify bad");

	log.clear();
	CHECK_EQ(Parse(spec, {"-a", "bad"}).error, "option '-a': 'bad' is not there");
	CHECK_EQ(Parse(spec, {"bad"}).error, "argument <word>: 'bad' is not there");
	CHECK_EQ(Parse(spec, {"-c", "stuck"}).error, "option '-c': cannot make 'stuck'");

	/* Nothing is verified or prepared when the parse fails before. */
	log.clear();
	CHECK_EQ(Parse(spec, {"-a", "x", "--bogus"}).error, "Unknown option: bogus");
	CHECK_EQ(Parse(spec, {"-a", "x", "a", "b"}).error, "unexpected extra argument 'b'");
	CHECK(Parse(spec, {"-a", "x", "--help"}).outcome == Outcome::Answered);
	CHECK(log.empty());

	RawSpec defaults = NewSpec();
	AddDefault(AddOption(defaults.root, "a", {{"type", "recording"}}), "bad");
	AddDefault(AddOption(defaults.root, "z", {{"type", "recording"}}), "stuck");
	auto defaultSpec = Spec::Compile(defaults, types);
	CHECK_EQ(Parse(defaultSpec, {}).error, "option '-a': default value: 'bad' is not there");
	CHECK_EQ(Parse(defaultSpec, {"-a", "ok"}).error, "option '-z': default value: cannot make 'stuck'");
	CHECK_EQ(Parse(defaultSpec, {"-a", "ok", "-z", "ok"}).error, "");
}

namespace {

/* A type that looks its values up in a directory of players by name: it
 * replaces a name with the player's number and tells their team as an
 * attribute. "me" is whoever runs the parse. It records what it is asked. */
struct PlayerType : Type {
	explicit PlayerType(std::vector<std::string> &log) : log(log) {}
	TypeKind Kind() const override { return TypeKind::Value; }

	Problem Resolve(Scalar &value, const ParseContext &context) const override
	{
		log.push_back("resolve " + value.text + " for " + std::to_string(context.caller));
		std::string name = value.text == "me" ? std::to_string(context.caller) : value.text;
		const std::map<std::string, std::pair<int, std::string>> players{{"dave", {3, "red"}}, {"anna", {7, "blu"}},
		                                                                   {"5", {5, "red"}}};
		auto found = players.find(name);
		if (found == players.end())
			return "no player is called '" + value.text + "'";
		value = Scalar{std::to_string(found->second.first), found->second.first, {}, {}, {{"team", found->second.second}}};
		return std::nullopt;
	}

	Problem Verify(const Scalar &value, const ParseContext &) const override
	{
		log.push_back("verify " + value.text);
		return std::nullopt;
	}

	std::vector<std::string> &log;
};

} // namespace

TEST(resolve_replaces_values_with_what_the_parse_runs_in)
{
	std::vector<std::string> log;
	TypeRegistry types;
	types.Register({{"player"}, {}, nullptr, [&log](const TypeKeys &) { return std::make_shared<PlayerType>(log); }});

	RawSpec raw = NewSpec();
	AddDefault(AddOption(raw.root, "target", {{"type", "player"}}), "me");
	AddOption(raw.root, "also", {{"type", "player"}, {"multiple", "1"}});
	AddOption(raw.root, "seen", {{"type", "s"}}).processValue = [](const ProcessContext &context, const std::string &value) {
		return value + ":" + TextOf(*context.values, "target");
	};
	AddArg(raw.root, "who", {{"type", "player"}}).processValue = [](const ProcessContext &, const std::string &value) {
		return "#" + value;
	};
	auto spec = Spec::Compile(raw, types);
	CHECK(log.empty());

	ParseContext context{5};
	ParseResult result = Parse(spec, {"--also", "dave", "--also", "anna", "--seen", "x", "dave"}, context);
	CHECK(result.outcome == Outcome::Parsed);
	const LevelResult &values = *result.values;
	/* The default is resolved for the caller of the parse. */
	CHECK_EQ(TextOf(values, "target"), "5");
	CHECK_EQ(*values.At("target")->integer, 5);
	CHECK_EQ(values.At("target")->attributes.at("team"), "red");
	CHECK_EQ(ListOf(values, "also"), "3|7");
	CHECK_EQ(values.At("also", 1)->attributes.at("team"), "blu");
	/* processValue sees resolved values, and attributes stay through it. */
	CHECK_EQ(TextOf(values, "seen"), "x:5");
	CHECK_EQ(TextOf(values, "who"), "#3");
	CHECK(!values.At("who")->integer);
	CHECK_EQ(values.At("who")->attributes.at("team"), "red");
	/* Every value is resolved before any is verified. */
	CHECK_EQ(text::Join(log, ", "),
	         "resolve dave for 5, resolve anna for 5, resolve me for 5, resolve dave for 5, verify 3, verify 7, verify 5, verify 3");

	log.clear();
	CHECK_EQ(Parse(spec, {"--also", "nobody"}, context).error, "option '--also': no player is called 'nobody'");
	CHECK_EQ(Parse(spec, {"nobody"}, context).error, "argument <who>: no player is called 'nobody'");
	/* The default caller is 0, and a default that does not resolve is told
	 * as one. */
	CHECK_EQ(Parse(spec, {}).error, "option '--target': default value: no player is called 'me'");
	CHECK(log.end() == std::find(log.begin(), log.end(), "verify 3"));
}

TEST(resolve_reaches_every_shape_and_every_level)
{
	std::vector<std::string> log;
	TypeRegistry types;
	types.Register({{"player"}, {}, nullptr, [&log](const TypeKeys &) { return std::make_shared<PlayerType>(log); }});

	RawSpec raw = NewSpec();
	AddOption(raw.root, "outer", {{"type", "player"}, {"inherit", "1"}});
	RawLevel &run = AddCommand(raw.root, "run");
	AddOption(run, "pair", {{"type", "player"}, {"hash", "1"}});
	AddOption(run, "entry", {{"type", "player"}, {"objectlist", "1"}});
	AddArg(run, "who", {{"type", "player"}});
	auto spec = Spec::Compile(raw, types);

	ParseResult result = Parse(spec, {"--outer", "anna", "run", "--pair", "x=dave", "--entry", "0.a=anna", "--entry", "1.a=me", "5"},
	                           ParseContext{5});
	CHECK(result.outcome == Outcome::Parsed);
	const LevelResult &root = *result.values;
	const LevelResult &inner = *root.subcommand;
	CHECK_EQ(TextOf(root, "outer"), "7");
	CHECK_EQ(MappingOf(inner, "pair"), "x=3");
	CHECK_EQ(inner.At("pair", kNoEntry, "x")->attributes.at("team"), "red");
	CHECK_EQ(ObjectsOf(inner, "entry"), "a=7|a=5");
	CHECK_EQ(inner.At("entry", 1, "a")->attributes.at("team"), "red");
	CHECK_EQ(TextOf(inner, "who"), "5");
	/* The innermost level first, its options in the order of their keys,
	 * and every level resolved before any value is verified. */
	CHECK_EQ(text::Join(log, ", "), "resolve anna for 5, resolve me for 5, resolve dave for 5, resolve 5 for 5, resolve anna for 5, "
	                                "verify 7, verify 5, verify 3, verify 5, verify 7");

	CHECK_EQ(Parse(spec, {"run", "--pair", "x=nobody"}, ParseContext{5}).error,
	         "option '--pair': no player is called 'nobody'");
	CHECK_EQ(Parse(spec, {"run", "--entry", "0.a=nobody"}, ParseContext{5}).error,
	         "option '--entry': no player is called 'nobody'");
	/* The problem of an inherited option belongs to its level. */
	ParseResult outer = Parse(spec, {"run", "--outer", "nobody"}, ParseContext{5});
	CHECK_EQ(outer.error, "option '--outer': no player is called 'nobody'");
	CHECK_EQ(outer.output.at(2), "# tool [options] <command>");
}

TEST(process_value)
{
	std::vector<std::string> seen;
	ProcessValue wrap = [&seen](const ProcessContext &context, const std::string &value) {
		seen.push_back(TextOf(*context.values, "prefix"));
		return "[" + value + "]";
	};

	RawSpec raw = NewSpec();
	RawEntry &prefix = AddOption(raw.root, "prefix", {{"type", "s"}});
	AddDefault(prefix, "p");
	prefix.processValue = [](const ProcessContext &, const std::string &value) { return value + value; };
	AddOption(raw.root, "name", {{"type", "s"}}).processValue = wrap;
	AddOption(raw.root, "tag", {{"type", "s"}, {"multiple", "1"}}).processValue = wrap;
	AddOption(raw.root, "env", {{"type", "s"}, {"hash", "1"}}).processValue = wrap;
	AddOption(raw.root, "server", {{"type", "s"}, {"objectlist", "1"}}).processValue = wrap;
	RawEntry &level = AddOption(raw.root, "level", {{"type", "i"}});
	AddDefault(level, "07");
	level.processValue = wrap;
	AddOption(raw.root, "unset", {{"type", "s"}}).processValue = wrap;
	AddOption(raw.root, "nothing", {{"type", "s"}, {"multiple", "1"}}).processValue = wrap;
	AddOption(raw.root, "quiet", {}).processValue = wrap;
	AddArg(raw.root, "word").processValue = wrap;
	AddArg(raw.root, "more", {{"multiple", "1"}}).processValue = wrap;

	ParseResult result = ParseWith(raw, {"--name", "a", "--tag", "x", "--tag", "y", "--env", "os=linux", "--server", "0.host=h",
	                                     "--quiet", "rest", "m1", "m2"});
	const LevelResult &values = ValuesOf(result);
	CHECK_EQ(TextOf(values, "prefix"), "pp");
	CHECK_EQ(TextOf(values, "name"), "[a]");
	CHECK_EQ(ListOf(values, "tag"), "[x]|[y]");
	CHECK_EQ(MappingOf(values, "env"), "os=[linux]");
	CHECK_EQ(ObjectsOf(values, "server"), "host=[h]");
	CHECK_EQ(TextOf(values, "level"), "[7]");
	CHECK(!values.At("level")->integer);
	CHECK_EQ(TextOf(values, "unset"), "(unset)");
	CHECK_EQ(values.Count("nothing"), 0u);
	CHECK_EQ(TextOf(values, "quiet"), "[1]");
	CHECK_EQ(TextOf(values, "word"), "[rest]");
	CHECK_EQ(ListOf(values, "more"), "[m1]|[m2]");
	/* Every callback saw the prefix before it was processed. */
	CHECK_EQ(seen.size(), 10u);
	for (const std::string &prefixSeen : seen)
		CHECK_EQ(prefixSeen, "p");

	seen.clear();
	CHECK_EQ(ErrorOf(raw, {"--name", "a", "--bogus"}), "Unknown option: bogus");
	CHECK(seen.empty());
}

TEST(process_value_sees_the_processed_subcommand)
{
	RawSpec raw = NewSpec();
	std::string seen;
	std::shared_ptr<const Spec> specSeen;
	AddOption(raw.root, "outer", {{"type", "s"}}).processValue = [&seen, &specSeen](const ProcessContext &context, const std::string &value) {
		seen = TextOf(*context.values->subcommand, "inner");
		specSeen = context.spec;
		return value;
	};
	AddOption(AddCommand(raw.root, "run"), "inner", {{"type", "s"}}).processValue = [](const ProcessContext &, const std::string &value) {
		return value + "!";
	};
	ParseResult result = ParseWith(raw, {"--outer", "o", "run", "--inner", "i"});
	CHECK(result.outcome == Outcome::Parsed);
	CHECK_EQ(seen, "i!");
	CHECK(specSeen == result.spec);
}

/* --------------------------------------------------------------------- args */

TEST(fixed_args)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "owner", {{"type", "s"}});
	AddArg(raw.root, "source-url", {{"type", "url"}, {"required", "1"}});
	ParseResult result = ParseWith(raw, {"--owner", "dave", "https://example.com/x.git"});
	CHECK_EQ(TextOf(ValuesOf(result), "source-url"), "https://example.com/x.git");
	ParseResult mixed = ParseWith(raw, {"https://example.com/x.git", "--owner", "dave"});
	CHECK_EQ(TextOf(ValuesOf(mixed), "owner"), "dave");
	CHECK_EQ(TextOf(ValuesOf(mixed), "source-url"), "https://example.com/x.git");
}

TEST(optional_and_multiple_args)
{
	RawSpec raw = NewSpec();
	AddArg(raw.root, "input", {{"required", "1"}});
	AddArg(raw.root, "output");
	ParseResult result = ParseWith(raw, {"in.txt"});
	CHECK_EQ(TextOf(ValuesOf(result), "input"), "in.txt");
	CHECK_EQ(TextOf(ValuesOf(result), "output"), "(unset)");
	CHECK_EQ(TextOf(ValuesOf(ParseWith(raw, {"in.txt", "out.txt"})), "output"), "out.txt");

	RawSpec slurpy = NewSpec();
	AddArg(slurpy.root, "inputs", {{"required", "1"}, {"multiple", "1"}, {"type", "int"}});
	CHECK_EQ(ListOf(ValuesOf(ParseWith(slurpy, {"1", "02", "3"})), "inputs"), "1|2|3");
	CHECK_EQ(ErrorOf(slurpy, {}), "missing required argument <inputs>");
	CHECK_EQ(ErrorOf(slurpy, {"1", "x"}), "argument <inputs>: 'x' is not an integer");

	RawSpec empty = NewSpec();
	AddArg(empty.root, "inputs", {{"multiple", "1"}});
	ParseResult none = ParseWith(empty, {});
	CHECK(none.outcome == Outcome::Parsed);
	CHECK(ValuesOf(none).ValueNamed("inputs").shape == Shape::List);
	CHECK_EQ(ValuesOf(none).Count("inputs"), 0u);

	RawSpec both = NewSpec();
	AddArg(both.root, "first", {{"required", "1"}});
	AddArg(both.root, "rest", {{"multiple", "1"}});
	CHECK_EQ(ListOf(ValuesOf(ParseWith(both, {"a"})), "rest"), "");
	CHECK_EQ(ListOf(ValuesOf(ParseWith(both, {"a", "b", "c"})), "rest"), "b|c");
}

TEST(arg_failures)
{
	RawSpec required = NewSpec();
	AddArg(required.root, "source", {{"required", "1"}});
	CHECK_EQ(ErrorOf(required, {}), "missing required argument <source>");

	RawSpec one = NewSpec();
	AddArg(one.root, "only");
	CHECK_EQ(ErrorOf(one, {"x", "y"}), "unexpected extra argument 'y'");

	RawSpec none = NewSpec();
	CHECK_EQ(ErrorOf(none, {"stray"}), "unexpected extra argument 'stray'");
	CHECK_EQ(ErrorOf(none, {}), "(parsed)");

	RawSpec typed = NewSpec();
	AddArg(typed.root, "source", {{"type", "url"}});
	AddArg(typed.root, "count", {{"type", "int"}, {"min", "1"}});
	CHECK_EQ(ErrorOf(typed, {"not a url"}), "argument <source>: 'not a url' is not a URL");
	CHECK_EQ(ErrorOf(typed, {"x://y", "0"}), "argument <count>: 0 is smaller than the minimum of 1");
}

/* ----------------------------------------------------------------- commands */

namespace {

RawSpec CommandSpec()
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "verbose", {{"type", "!"}, {"help", "Print more information"}});
	RawLevel &document = AddCommand(raw.root, "document", {{"description", "Work on documents"}});
	AddOption(document, "path", {{"type", "s"}});
	RawLevel &create = AddCommand(document, "create", {{"description", "Create a new document"}});
	SetValid(AddOption(create, "format", {{"type", "s"}}), {"pdf", "docx"});
	AddArg(create, "title", {{"required", "1"}});
	RawLevel &image = AddCommand(raw.root, "image", {{"description", "Work on images"}});
	AddOption(image, "width", {{"type", "i"}, {"min", "1"}});
	AddOption(image, "height", {{"type", "i"}, {"min", "1"}});
	return raw;
}

} // namespace

TEST(nested_commands)
{
	ParseResult result = ParseWith(CommandSpec(), {"--verbose", "document", "create", "--format", "pdf", "My Doc"});
	CHECK(result.outcome == Outcome::Parsed);
	const LevelResult &root = ValuesOf(result);
	CHECK_EQ(TextOf(root, "verbose"), "1");
	CHECK_EQ(*root.command, "document");
	const LevelResult &document = *root.subcommand;
	CHECK_EQ(*document.command, "create");
	CHECK_EQ(document.level->path, "document");
	CHECK_EQ(TextOf(document, "path"), "(unset)");
	const LevelResult &create = *document.subcommand;
	CHECK_EQ(TextOf(create, "format"), "pdf");
	CHECK_EQ(TextOf(create, "title"), "My Doc");
	CHECK(!create.command);
	CHECK(create.subcommand == nullptr);
	CHECK_EQ(create.level->path, "document create");
	CHECK(!Holds(root, "format"));
}

TEST(options_bind_to_their_own_level)
{
	RawSpec raw = CommandSpec();
	ParseResult result = ParseWith(raw, {"image", "--width", "640"});
	CHECK_EQ(*ValuesOf(result).command, "image");
	CHECK_EQ(TextOf(*ValuesOf(result).subcommand, "width"), "640");
	CHECK_EQ(ErrorOf(raw, {"--width", "640", "image"}), "Unknown option: width");
	CHECK_EQ(ErrorOf(raw, {"image", "--verbose"}), "Unknown option: verbose");
	/* A level with commands stops at the command word: what follows is the
	 * command's. */
	CHECK_EQ(ErrorOf(raw, {"image", "--width", "640", "stray"}), "unexpected extra argument 'stray'");
}

TEST(missing_and_unknown_commands)
{
	RawSpec raw = CommandSpec();
	CHECK_EQ(ErrorOf(raw, {}), "missing command, expected one of: document, image");
	CHECK_EQ(ErrorOf(raw, {"--verbose"}), "missing command, expected one of: document, image");
	CHECK_EQ(ErrorOf(raw, {"frobnicate"}), "unknown command 'frobnicate', expected one of: document, image");
	CHECK_EQ(ErrorOf(raw, {"Image"}), "unknown command 'Image', expected one of: document, image");
	CHECK_EQ(ErrorOf(raw, {"document"}), "missing command, expected one of: create");

	RawSpec optional = NewSpec();
	AddOption(optional.root, "verbose", {{"type", "!"}});
	AddCommand(optional.root, "doc");
	optional.root.keys["commandRequired"] = "0";
	ParseResult result = ParseWith(optional, {"--verbose"});
	CHECK(result.outcome == Outcome::Parsed);
	CHECK(!ValuesOf(result).command);
	CHECK(ValuesOf(result).subcommand == nullptr);
	CHECK_EQ(*ValuesOf(ParseWith(optional, {"doc"})).command, "doc");
	CHECK_EQ(ErrorOf(optional, {"nope"}), "unknown command 'nope', expected one of: doc");
}

TEST(the_end_of_options_before_a_command)
{
	RawSpec raw = CommandSpec();
	ParseResult result = ParseWith(raw, {"--", "image", "--width", "5"});
	CHECK_EQ(*ValuesOf(result).command, "image");
	CHECK_EQ(TextOf(*ValuesOf(result).subcommand, "width"), "5");
	CHECK_EQ(ErrorOf(raw, {"--", "--verbose"}), "unknown command '--verbose', expected one of: document, image");

	ParseResult title = ParseWith(raw, {"document", "create", "--", "-5"});
	CHECK_EQ(TextOf(*ValuesOf(title).subcommand->subcommand, "title"), "-5");
}

TEST(errors_belong_to_their_level)
{
	RawSpec raw = CommandSpec();
	auto headerOf = [&raw](const Words &words) { return ParseWith(raw, words).output.at(2); };
	CHECK_EQ(headerOf({"--bogus"}), "# tool [options] <command>");
	CHECK_EQ(headerOf({"nope"}), "# tool [options] <command>");
	CHECK_EQ(headerOf({"image", "--bogus"}), "# tool image [options]");
	CHECK_EQ(headerOf({"image", "--width", "0"}), "# tool image [options]");
	CHECK_EQ(headerOf({"document", "create"}), "# tool document create [options] title");
	CHECK_EQ(headerOf({"document", "create", "--format", "odt", "T"}), "# tool document create [options] title");
	CHECK_EQ(headerOf({"document", "nope"}), "# tool document [options] <command>");

	/* The innermost level is settled first. */
	RawSpec both = NewSpec();
	AddOption(both.root, "outer", {{"type", "i"}});
	AddOption(AddCommand(both.root, "run"), "inner", {{"type", "i"}});
	CHECK_EQ(ErrorOf(both, {"--outer", "x", "run", "--inner", "y"}), "option '--inner': 'y' is not an integer");
	CHECK_EQ(ErrorOf(both, {"--outer", "x", "run", "--inner", "1"}), "option '--outer': 'x' is not an integer");
	/* But a level is read before the levels below it. */
	CHECK_EQ(ErrorOf(both, {"--nope", "run", "--bogus"}), "Unknown option: nope");
}

TEST(inherited_options)
{
	RawSpec raw = NewSpec();
	SetValid(AddOption(raw.root, "color", {{"type", "s"}, {"inherit", "1"}}), {"never", "always"});
	AddOption(raw.root, "verbose|v", {{"type", "+"}, {"inherit", "1"}});
	AddOption(raw.root, "tag", {{"type", "s"}, {"multiple", "1"}, {"inherit", "1"}});
	AddOption(raw.root, "define", {{"type", "s"}, {"hash", "1"}, {"inherit", "1"}});
	AddOption(raw.root, "quiet", {{"type", "bool"}, {"inherit", "1"}});
	AddOption(raw.root, "local", {{"type", "s"}});
	RawLevel &document = AddCommand(raw.root, "document");
	AddOption(document, "path", {{"type", "s"}, {"inherit", "1"}});
	AddArg(AddCommand(document, "create"), "title");

	ParseResult result = ParseWith(raw, {"document", "create", "--color", "never", "--path", "a.txt", "T"});
	CHECK(result.outcome == Outcome::Parsed);
	const LevelResult &root = ValuesOf(result);
	CHECK_EQ(TextOf(root, "color"), "never");
	CHECK_EQ(TextOf(*root.subcommand, "path"), "a.txt");
	CHECK_EQ(TextOf(*root.subcommand->subcommand, "title"), "T");
	CHECK(!Holds(*root.subcommand, "color"));
	CHECK(!Holds(*root.subcommand->subcommand, "path"));

	ParseResult spread = ParseWith(raw, {"--color", "always", "-v", "--tag", "a", "--define", "a=1", "--quiet", "document", "-v",
	                                     "--tag", "b", "--define", "b=2", "create", "--color", "never", "-v", "--tag", "c",
	                                     "--define", "a=3", "--no-quiet", "T"});
	CHECK_EQ(TextOf(ValuesOf(spread), "color"), "never");
	CHECK_EQ(TextOf(ValuesOf(spread), "verbose"), "3");
	CHECK_EQ(ListOf(ValuesOf(spread), "tag"), "a|b|c");
	CHECK_EQ(MappingOf(ValuesOf(spread), "define"), "a=3,b=2");
	CHECK_EQ(TextOf(ValuesOf(spread), "quiet"), "0");

	CHECK_EQ(ErrorOf(raw, {"document", "create", "--color", "sometimes", "T"}),
	         "option '--color': 'sometimes' is not one of: never, always");
	/* The error belongs to the level that declares the option. */
	CHECK_EQ(ParseWith(raw, {"document", "create", "--color", "sometimes", "T"}).output.at(2), "# tool [options] <command>");
	CHECK_EQ(ErrorOf(raw, {"document", "--local", "x", "create"}), "Unknown option: local");
	CHECK_EQ(ErrorOf(raw, {"--path", "x", "document", "create"}), "Unknown option: path");

	ParseResult help = ParseWith(raw, {"document", "create", "--help"});
	CHECK(help.outcome == Outcome::Answered);
	std::string text = Joined(help.output);
	CHECK(Contains(text, "\n   --color <>"));
	CHECK(Contains(text, "\n   --path <>"));
	CHECK(!Contains(text, "--local"));
}

/* -------------------------------------------------------- automatic options */

TEST(help_and_version_answer)
{
	RawSpec raw = NewSpec();
	raw.root.keys["version"] = "1.2.3";
	AddOption(raw.root, "owner", {{"type", "s"}, {"required", "1"}});

	ParseResult help = ParseWith(raw, {"--help"});
	CHECK(help.outcome == Outcome::Answered);
	CHECK(help.values == nullptr);
	CHECK_EQ(help.error, "");
	CHECK_EQ(help.output.at(0), "# tool [options]");

	ParseResult version = ParseWith(raw, {"--version"});
	CHECK(version.outcome == Outcome::Answered);
	CHECK_EQ(Joined(version.output), "tool 1.2.3");

	/* --help comes first when both are given, and a mistake on the same
	 * level wins over both. */
	CHECK_EQ(ParseWith(raw, {"--version", "--help"}).output.at(0), "# tool [options]");
	CHECK_EQ(ErrorOf(raw, {"--help", "--bogus"}), "Unknown option: bogus");
	CHECK_EQ(ErrorOf(raw, {"--help=1"}), "Option help does not take an argument");
	CHECK_EQ(ErrorOf(raw, {"-h"}), "Unknown option: h");
	/* Nothing else is checked first. */
	CHECK(ParseWith(raw, {"--help", "stray", "words"}).outcome == Outcome::Answered);
	CHECK(ParseWith(raw, {"--owner", "x", "--help"}).outcome == Outcome::Answered);
}

TEST(help_of_commands)
{
	RawSpec raw = CommandSpec();
	AddOption(raw.root, "owner", {{"type", "s"}, {"required", "1"}});

	CHECK_EQ(ParseWith(raw, {"--help"}).output.at(0), "# tool [options] <command>");
	ParseResult nested = ParseWith(raw, {"document", "create", "--help"});
	CHECK(nested.outcome == Outcome::Answered);
	CHECK_EQ(nested.output.at(0), "# tool document create [options] title");
	CHECK_EQ(nested.output.at(1), "# Create a new document");
	/* The levels after the answering one are not read at all. */
	CHECK_EQ(ParseWith(raw, {"--help", "image", "--bogus"}).output.at(0), "# tool [options] <command>");
	/* A mistake on an outer level wins. */
	CHECK_EQ(ErrorOf(raw, {"--bogus", "image", "--help"}), "Unknown option: bogus");
	CHECK_EQ(ErrorOf(raw, {"image", "--version"}), "Unknown option: version");
	CHECK(ParseWith(raw, {"image", "--help"}).outcome == Outcome::Answered);
}
