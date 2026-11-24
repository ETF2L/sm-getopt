/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Tests of the spec compiler: what a valid declaration becomes, and every
 * mistake in a declaration with its message. */
#include "check.h"
#include "support.h"

using namespace smgetopt;
using namespace support;

namespace {

/* The spec error of a spec with one option. */
std::string OptionError(const std::string &key, Keys keys, const std::function<void(RawEntry &)> &more = nullptr)
{
	RawSpec raw = NewSpec();
	RawEntry &entry = AddOption(raw.root, key, std::move(keys));
	if (more)
		more(entry);
	return SpecErrorOf(raw);
}

std::string ArgError(const std::string &name, Keys keys, const std::function<void(RawEntry &)> &more = nullptr)
{
	RawSpec raw = NewSpec();
	RawEntry &entry = AddArg(raw.root, name, std::move(keys));
	if (more)
		more(entry);
	return SpecErrorOf(raw);
}

} // namespace

TEST(a_valid_spec_builds)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "owner|o", {{"type", "s"}, {"required", "1"}, {"group", "Target"}, {"help", "Target owner"}});
	AddDefault(AddOption(raw.root, "private", {{"type", "!"}}), "1");
	AddOption(raw.root, "dry-run");
	RawEntry &logLevel = AddOption(raw.root, "log-level", {{"type", "string"}});
	AddDefault(logLevel, "info");
	SetValid(logLevel, {"debug", "info", "warn"});
	AddArg(raw.root, "source-url", {{"type", "url"}, {"required", "1"}});
	raw.root.keys["description"] = "Test tool";
	raw.root.examples.push_back({"Example", "--owner dave"});

	auto spec = Compile(raw);
	const Level &root = *spec->root;
	const Option *owner = root.OptionNamed("owner");
	CHECK(owner != nullptr);
	CHECK(root.OptionNamed("o") == owner);
	CHECK_EQ(owner->name, "owner");
	CHECK_EQ(text::Join(owner->aliases, ","), "o");
	CHECK(owner->required);
	CHECK_EQ(owner->group, "Target");
	CHECK_EQ(owner->Spelling(), "--owner");
	CHECK_EQ(root.OptionNamed("dry-run")->typeName, "flag");
	CHECK_EQ(root.OptionNamed("dry-run")->group, "Options");
	CHECK(root.OptionNamed("private")->type->Kind() == TypeKind::Bool);
	CHECK(root.OptionNamed("private")->hasDefault);
	CHECK_EQ(root.OptionNamed("log-level")->defaultValue.single->text, "info");
	CHECK_EQ(root.args.size(), 1u);
	CHECK_EQ(root.args[0].name, "source-url");
	CHECK_EQ(root.args[0].typeName, "url");
	CHECK_EQ(root.description, "Test tool");
	CHECK_EQ(root.examples.size(), 1u);
	CHECK_EQ(root.examples[0].args, "--owner dave");
	CHECK_EQ(spec->programName, "tool");
	CHECK_EQ(spec->helpWidth, 100);
}

TEST(declared_options_are_ordered_by_key_and_followed_by_the_automatic_ones)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "zeta");
	AddOption(raw.root, "alpha|a");
	AddOption(raw.root, "Beta");
	auto spec = Compile(raw);

	std::vector<std::string> names;
	for (const auto &option : spec->root->options)
		names.push_back(option->name);
	CHECK_EQ(text::Join(names, " "), "Beta alpha zeta help version");
	CHECK_EQ(spec->root->DeclaredOptions().size(), 3u);
	CHECK(spec->root->OptionNamed("help")->automatic == AutoOption::Help);
	CHECK(spec->root->OptionNamed("version")->automatic == AutoOption::Version);
	CHECK(spec->root->OptionNamed("help")->hidden);
}

TEST(the_spec_needs_a_name)
{
	RawSpec raw;
	CHECK_EQ(SpecErrorOf(raw), "the spec has no name");
}

TEST(top_level_keys)
{
	RawSpec raw = NewSpec();
	raw.root.keys["typo"] = "1";
	raw.root.keys["other"] = "1";
	CHECK_EQ(SpecErrorOf(raw), "unknown key(s): other, typo");

	RawSpec versioned = NewSpec();
	CHECK_EQ(Compile(versioned)->version, "unknown");
	versioned.fallbackVersion = "0.9";
	CHECK_EQ(Compile(versioned)->version, "0.9");
	versioned.root.keys["version"] = "2.0";
	CHECK_EQ(Compile(versioned)->version, "2.0");

	RawSpec wide = NewSpec();
	wide.root.keys["helpWidth"] = "60";
	CHECK_EQ(Compile(wide)->helpWidth, 60);
	for (const char *width : {"0", "-5", "wide", "", "10001", "99999999999999999999"}) {
		wide.root.keys["helpWidth"] = width;
		CHECK_EQ(SpecErrorOf(wide), std::string("helpWidth must be a whole number from 1 to 10000, not '") + width + "'");
	}
}

TEST(option_keys_and_names)
{
	CHECK_EQ(OptionError("owner", {{"type", "s"}, {"frobnicate", "1"}}), "option 'owner': unknown key(s): frobnicate");
	CHECK_EQ(OptionError("owner", {{"zz", "1"}, {"aa", "1"}}), "option 'owner': unknown key(s): aa, zz");
	CHECK_EQ(OptionError("owner", {{"type", "s"}, {"min", "1"}}), "option 'owner': unknown key(s): min");
	CHECK_EQ(OptionError("", {}), "option key must be a non-empty string");
	CHECK_EQ(OptionError("owner|", {{"type", "s"}}), "option 'owner|': invalid name ''");
	CHECK_EQ(OptionError("|o", {}), "option '|o': invalid name ''");
	CHECK_EQ(OptionError("2fast", {}), "option '2fast': invalid name '2fast'");
	CHECK_EQ(OptionError("owner|o x", {}), "option 'owner|o x': invalid name 'o x'");
	CHECK_EQ(OptionError("owner", {{"type", "nope"}}),
	         "option 'owner': unknown option type 'nope' (known: " + text::Join(Types().KnownNames(), ", ") + ")");
	CHECK_EQ(OptionError("owner", {{"type", "STRING"}}), "");
}

TEST(names_are_taken_once_per_level)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "verbose|v");
	AddOption(raw.root, "version-check|v");
	CHECK_EQ(SpecErrorOf(raw), "option 'version-check': name 'v' is already used by an alias of option 'verbose'");

	RawSpec twice = NewSpec();
	AddOption(twice.root, "owner");
	AddOption(twice.root, "owner");
	CHECK_EQ(SpecErrorOf(twice), "option 'owner': name 'owner' is already used by option 'owner'");

	RawSpec alias = NewSpec();
	AddOption(alias.root, "a|x");
	AddOption(alias.root, "x");
	CHECK_EQ(SpecErrorOf(alias), "option 'x': name 'x' is already used by an alias of option 'a'");

	RawSpec shared = NewSpec();
	AddOption(shared.root, "source", {{"type", "s"}});
	AddArg(shared.root, "source");
	CHECK_EQ(SpecErrorOf(shared), "arg 'source' and option 'source' share one name");

	RawSpec args = NewSpec();
	AddArg(args.root, "file");
	AddArg(args.root, "file");
	CHECK_EQ(SpecErrorOf(args), "arg 'file' and arg 'file' share one name");

	/* An alias names no value, so an arg may use it. */
	RawSpec free = NewSpec();
	AddOption(free.root, "source|s", {{"type", "s"}});
	AddArg(free.root, "s");
	CHECK_EQ(SpecErrorOf(free), "");
}

TEST(automatic_options_keep_their_names)
{
	CHECK_EQ(OptionError("help", {{"type", "s"}}), "option 'help' collides with the automatic --help option");
	CHECK_EQ(OptionError("usage|help", {}), "option 'help' collides with the automatic --help option");
	CHECK_EQ(OptionError("version", {}), "option 'version' collides with the automatic --version option");
	CHECK_EQ(OptionError("v|version", {}), "option 'version' collides with the automatic --version option");

	RawSpec nested = NewSpec();
	AddOption(AddCommand(nested.root, "doc"), "help");
	CHECK_EQ(SpecErrorOf(nested), "command 'doc': option 'help' collides with the automatic --help option");

	/* --version exists on the top level only. */
	RawSpec free = NewSpec();
	AddOption(AddCommand(free.root, "doc"), "version", {{"type", "s"}});
	CHECK_EQ(SpecErrorOf(free), "");
}

TEST(a_negation_is_no_name_of_another_option)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "color", {{"type", "bool"}});
	AddOption(raw.root, "no-color", {{"type", "s"}});
	CHECK_EQ(SpecErrorOf(raw), "option 'color': its negation '--no-color' is already used by option 'no-color'");

	RawSpec alias = NewSpec();
	AddOption(alias.root, "x|quiet", {{"type", "bool"}});
	AddOption(alias.root, "silent|noquiet");
	CHECK_EQ(SpecErrorOf(alias), "option 'x': its negation '--noquiet' is already used by an alias of option 'silent'");

	/* An inherited bool keeps its negations on the levels below too. */
	RawSpec inherited = NewSpec();
	AddOption(inherited.root, "color", {{"type", "bool"}, {"inherit", "1"}});
	AddOption(AddCommand(inherited.root, "run"), "nocolor");
	CHECK_EQ(SpecErrorOf(inherited), "command 'run': option 'color': its negation '--nocolor' is already used by option 'nocolor'");

	/* Only a bool has negations. */
	RawSpec flag = NewSpec();
	AddOption(flag.root, "color");
	AddOption(flag.root, "no-color");
	CHECK_EQ(SpecErrorOf(flag), "");
}

TEST(option_rules)
{
	auto withDefault = [](RawEntry &entry) { AddDefault(entry, "x"); };
	CHECK_EQ(OptionError("owner", {{"type", "s"}, {"required", "1"}}, withDefault),
	         "option 'owner': required and default are mutually exclusive");
	CHECK_EQ(OptionError("verbose", {{"type", "!"}, {"multiple", "1"}}),
	         "option 'verbose': multiple requires a value-taking type, not '!'");
	CHECK_EQ(OptionError("verbose", {{"type", "!"}, {"hash", "1"}}), "option 'verbose': hash requires a value-taking type, not '!'");
	CHECK_EQ(OptionError("verbose", {{"type", "+"}, {"objectlist", "1"}}),
	         "option 'verbose': objectlist requires a value-taking type, not '+'");
	CHECK_EQ(OptionError("verbose", {{"multiple", "1"}}), "option 'verbose': multiple requires a value-taking type, not 'flag'");
	CHECK_EQ(OptionError("define", {{"type", "s"}, {"multiple", "1"}, {"hash", "1"}}),
	         "option 'define': multiple and hash are mutually exclusive");
	CHECK_EQ(OptionError("server", {{"type", "s"}, {"hash", "1"}, {"objectlist", "1"}}),
	         "option 'server': hash and objectlist are mutually exclusive");
	CHECK_EQ(OptionError("server", {{"type", "s"}, {"multiple", "1"}, {"objectlist", "1"}}),
	         "option 'server': multiple and objectlist are mutually exclusive");
	CHECK_EQ(OptionError("server", {{"type", "s"}, {"multiple", "1"}, {"hash", "1"}, {"objectlist", "1"}}),
	         "option 'server': multiple and hash and objectlist are mutually exclusive");
	CHECK_EQ(OptionError("tag", {{"type", "s"}, {"csv", "1"}}), "option 'tag': csv requires multiple");
	CHECK_EQ(OptionError("tag", {{"type", "s"}, {"csv", "1"}, {"hash", "1"}}), "option 'tag': csv requires multiple");
	CHECK_EQ(OptionError("owner", {{"type", "s"}, {"typehint", ""}}), "option 'owner': typehint must be a non-empty string");
	CHECK_EQ(OptionError("owner", {{"type", "s"}, {"required", "0"}, {"multiple", ""}, {"hidden", "0"}}), "");
}

TEST(type_keys_are_checked_for_their_option)
{
	CHECK_EQ(OptionError("workers", {{"type", "i"}, {"min", "abc"}}), "option 'workers': min must be a number, not 'abc'");
	CHECK_EQ(OptionError("workers", {{"type", "i"}, {"min", "10"}, {"max", "5"}}), "option 'workers': min 10 is larger than max 5");
	CHECK_EQ(OptionError("id", {{"type", "i"}, {"bigint", "1"}, {"max", "1.5"}}),
	         "option 'id': max must be an integer with bigint, not '1.5'");
	CHECK_EQ(OptionError("since", {{"type", "date"}, {"timezone", "Nowhere/Bogus"}}),
	         "option 'since': timezone 'Nowhere/Bogus' is not a known time zone");
	CHECK_EQ(OptionError("since", {{"type", "date"}, {"timezone", ""}}), "option 'since': timezone must be a non-empty string");
	CHECK_EQ(OptionError("ratio", {{"type", "float"}, {"bigint", "1"}}), "option 'ratio': unknown key(s): bigint");
	CHECK_EQ(ArgError("count", {{"type", "i"}, {"max", "x"}}), "arg 'count': max must be a number, not 'x'");
}

TEST(defaults_are_checked_when_the_spec_is_built)
{
	auto one = [](const char *value) { return [value](RawEntry &entry) { AddDefault(entry, value); }; };
	CHECK_EQ(OptionError("retries", {{"type", "i"}}, one("nope")), "option 'retries': default value: 'nope' is not an integer");
	CHECK_EQ(OptionError("retries", {{"type", "i"}, {"min", "1"}}, one("0")),
	         "option 'retries': default value: 0 is smaller than the minimum of 1");
	CHECK_EQ(OptionError("level", {{"type", "s"}}, [](RawEntry &entry) {
		AddDefault(entry, "bad");
		SetValid(entry, {"a", "b"});
	}), "option 'level': default value: 'bad' is not one of: a, b");
	CHECK_EQ(OptionError("even", {{"type", "i"}}, [](RawEntry &entry) {
		AddDefault(entry, "3");
		entry.lazyValid = [](const std::string &value) { return (value.back() - '0') % 2 == 0; };
	}), "option 'even': default value: '3' is not a valid value");
	CHECK_EQ(OptionError("dry-run", {}, one("yes")), "option 'dry-run': default value: 'yes' is not a boolean (use 1 or 0)");
	CHECK_EQ(OptionError("verbose", {{"type", "counter"}}, one("many")), "option 'verbose': default value: 'many' is not a count");
	CHECK_EQ(OptionError("since", {{"type", "date"}, {"timezone", "UTC"}}, one("blurb")),
	         "option 'since': default value: 'blurb' is not a date");

	RawSpec nested = NewSpec();
	AddDefault(AddOption(AddCommand(nested.root, "doc"), "retries", {{"type", "i"}}), "three");
	CHECK_EQ(SpecErrorOf(nested), "command 'doc': option 'retries': default value: 'three' is not an integer");
}

TEST(defaults_need_the_shape_of_their_option)
{
	auto value = [](RawEntry &entry) { AddDefault(entry, "a"); };
	auto pair = [](RawEntry &entry) { AddDefaultPair(entry, "os", "linux"); };
	auto field = [](RawEntry &entry) { AddDefaultField(entry, 0, "host", "a"); };

	CHECK_EQ(OptionError("owner", {{"type", "s"}}, pair), "option 'owner': default value: expected a single value, not a list or mapping");
	CHECK_EQ(OptionError("owner", {{"type", "s"}}, field), "option 'owner': default value: expected a single value, not a list or mapping");
	CHECK_EQ(OptionError("owner", {{"type", "s"}}, [](RawEntry &entry) {
		AddDefault(entry, "a");
		AddDefault(entry, "b");
	}), "option 'owner': default value: expected a single value, not a list or mapping");
	CHECK_EQ(OptionError("tag", {{"type", "s"}, {"multiple", "1"}}, pair), "option 'tag': default value: expected a list of values");
	CHECK_EQ(OptionError("define", {{"type", "s"}, {"hash", "1"}}, value),
	         "option 'define': default value: expected a mapping of keys to values");
	CHECK_EQ(OptionError("server", {{"type", "s"}, {"objectlist", "1"}}, pair),
	         "option 'server': default value: expected a list of mappings");
	CHECK_EQ(OptionError("server", {{"type", "s"}, {"objectlist", "1"}}, value),
	         "option 'server': default value: expected a list of mappings");
	CHECK_EQ(OptionError("define", {{"type", "i"}, {"hash", "1"}}, pair),
	         "option 'define': default value: key 'os': 'linux' is not an integer");
	CHECK_EQ(OptionError("define", {{"type", "s"}, {"hash", "1"}}, [](RawEntry &entry) { AddDefaultPair(entry, "", "x"); }),
	         "option 'define': default value: empty key");
	CHECK_EQ(OptionError("server", {{"type", "i"}, {"objectlist", "1"}}, field),
	         "option 'server': default value: entry 0: key 'host': 'a' is not an integer");
	CHECK_EQ(OptionError("server", {{"type", "s"}, {"objectlist", "1"}}, [](RawEntry &entry) {
		AddDefaultField(entry, 0, "host", "a");
		AddDefaultField(entry, 2, "host", "c");
	}), "option 'server': default value: missing index 1");
	CHECK_EQ(OptionError("server", {{"type", "s"}, {"objectlist", "1"}}, [](RawEntry &entry) { AddDefaultField(entry, 1, "host", "a"); }),
	         "option 'server': default value: missing index 0");
	CHECK_EQ(OptionError("server", {{"type", "s"}, {"objectlist", "1"}}, [](RawEntry &entry) { AddDefaultField(entry, -1, "host", "a"); }),
	         "option 'server': default value: missing index 0");
}

TEST(defaults_are_kept_checked_and_as_written)
{
	RawSpec raw = NewSpec();
	AddDefault(AddOption(raw.root, "keep", {{"type", "int"}}), "007");
	RawEntry &tag = AddOption(raw.root, "tag", {{"type", "s"}, {"multiple", "1"}});
	AddDefault(tag, "a");
	AddDefault(tag, "b");
	RawEntry &define = AddOption(raw.root, "define", {{"type", "i"}, {"hash", "1"}});
	AddDefaultPair(define, "os", "1");
	AddDefaultPair(define, "arch", "02");
	AddDefaultPair(define, "os", "3");
	RawEntry &server = AddOption(raw.root, "server", {{"type", "s"}, {"objectlist", "1"}});
	AddDefaultField(server, 1, "host", "b");
	AddDefaultField(server, 0, "port", "80");
	AddDefaultField(server, 0, "host", "a");
	AddDefault(AddOption(raw.root, "since", {{"type", "date"}, {"timezone", "UTC"}}), "yesterday");

	auto spec = Compile(raw);
	const Level &root = *spec->root;
	CHECK_EQ(root.OptionNamed("keep")->defaultValue.single->text, "7");
	CHECK_EQ(root.OptionNamed("keep")->presentedDefault, "007");
	CHECK_EQ(root.OptionNamed("tag")->defaultValue.list.size(), 2u);
	CHECK_EQ(root.OptionNamed("tag")->presentedDefault, "a, b");
	CHECK_EQ(MappingText(root.OptionNamed("define")->defaultValue.mapping), "arch=2,os=3");
	CHECK_EQ(root.OptionNamed("define")->presentedDefault, "arch=02, os=3");
	CHECK_EQ(root.OptionNamed("server")->defaultValue.objects.size(), 2u);
	CHECK_EQ(MappingText(root.OptionNamed("server")->defaultValue.objects[0]), "host=a,port=80");
	CHECK_EQ(root.OptionNamed("server")->presentedDefault, "0.host=a, 0.port=80, 1.host=b");
	CHECK_EQ(root.OptionNamed("since")->defaultValue.single->text, "2026-10-05T00:00:00");
	CHECK_EQ(root.OptionNamed("since")->presentedDefault, "yesterday");
}

TEST(arg_rules)
{
	CHECK_EQ(ArgError("", {}), "arg: invalid name ''");
	CHECK_EQ(ArgError("1st", {}), "arg: invalid name '1st'");
	CHECK_EQ(ArgError("a", {{"type", "flag"}}), "arg 'a': type 'flag' cannot be used for a positional arg");
	CHECK_EQ(ArgError("a", {{"type", "bool"}}), "arg 'a': type 'bool' cannot be used for a positional arg");
	CHECK_EQ(ArgError("a", {{"type", "counter"}}), "arg 'a': type 'counter' cannot be used for a positional arg");
	CHECK_EQ(ArgError("a", {{"type", "nope"}}),
	         "arg 'a': unknown option type 'nope' (known: " + text::Join(Types().KnownNames(), ", ") + ")");
	CHECK_EQ(ArgError("src", {{"min", "1"}}), "arg 'src': unknown key(s): min");
	CHECK_EQ(ArgError("src", {{"group", "x"}, {"hidden", "1"}, {"csv", "1"}, {"hash", "1"}, {"inherit", "1"}, {"objectlist", "1"}}),
	         "arg 'src': unknown key(s): csv, group, hash, hidden, inherit, objectlist");
	CHECK_EQ(ArgError("src", {}, [](RawEntry &entry) { AddDefault(entry, "x"); }), "arg 'src': unknown key(s): default");
	CHECK_EQ(ArgError("src", {}, [](RawEntry &entry) { SetValid(entry, {"x"}); }), "arg 'src': unknown key(s): valid");
	CHECK_EQ(ArgError("src", {}, [](RawEntry &entry) {
		entry.validSource = [] { return std::vector<std::string>{}; };
		entry.lazyValid = [](const std::string &) { return true; };
	}), "arg 'src': unknown key(s): lazyValid, valid");
	CHECK_EQ(ArgError("src", {{"typehint", ""}}), "arg 'src': typehint must be a non-empty string");
	CHECK_EQ(ArgError("count", {{"type", "int"}, {"min", "1"}, {"max", "5"}, {"required", "1"}, {"help", "x"}, {"typehint", "N"}}), "");

	RawSpec slurpy = NewSpec();
	AddArg(slurpy.root, "a", {{"multiple", "1"}});
	AddArg(slurpy.root, "b");
	CHECK_EQ(SpecErrorOf(slurpy), "arg 'a': multiple is only allowed on the last arg");

	RawSpec order = NewSpec();
	AddArg(order.root, "a");
	AddArg(order.root, "b", {{"required", "1"}});
	CHECK_EQ(SpecErrorOf(order), "arg 'b': a required arg cannot follow an optional one");
}

TEST(command_rules)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "verbose", {{"type", "!"}});
	RawLevel &document = AddCommand(raw.root, "document", {{"description", "Work on documents"}});
	AddOption(document, "path", {{"type", "s"}});
	AddArg(AddCommand(document, "create"), "title", {{"required", "1"}});

	auto spec = Compile(raw);
	CHECK_EQ(text::Join(spec->root->CommandNames(), ","), "document");
	const Level *compiled = spec->root->Command("document");
	CHECK_EQ(compiled->path, "document");
	CHECK_EQ(compiled->description, "Work on documents");
	CHECK_EQ(compiled->Command("create")->path, "document create");
	CHECK_EQ(text::Join(compiled->Command("create")->pathWords, "|"), "document|create");
	CHECK_EQ(compiled->Command("create")->Label(), "command 'document create'");
	CHECK_EQ(spec->root->Label(), "the top level");
	CHECK(spec->root->commandRequired);
	CHECK(spec->root->Command("nope") == nullptr);
	CHECK(compiled->OptionNamed("help") != nullptr);
	CHECK(compiled->OptionNamed("version") == nullptr);

	RawSpec optional = NewSpec();
	AddCommand(optional.root, "doc");
	optional.root.keys["commandRequired"] = "0";
	CHECK(!Compile(optional)->root->commandRequired);

	RawSpec clash = NewSpec();
	AddArg(clash.root, "a");
	AddCommand(clash.root, "doc");
	CHECK_EQ(SpecErrorOf(clash), "args and commands are mutually exclusive on one level");

	RawSpec required = NewSpec();
	required.root.keys["commandRequired"] = "0";
	CHECK_EQ(SpecErrorOf(required), "commandRequired without commands");

	RawSpec badName = NewSpec();
	AddCommand(badName.root, "do it");
	CHECK_EQ(SpecErrorOf(badName), "invalid command name 'do it'");

	RawSpec twice = NewSpec();
	AddCommand(twice.root, "doc");
	AddCommand(twice.root, "doc");
	CHECK_EQ(SpecErrorOf(twice), "command 'doc' is declared twice");

	RawSpec nestedType = NewSpec();
	AddOption(AddCommand(nestedType.root, "doc"), "x", {{"type", "bad"}});
	CHECK(Contains(SpecErrorOf(nestedType), "command 'doc': option 'x': unknown option type 'bad'"));

	RawSpec nestedKey = NewSpec();
	AddOption(AddCommand(AddCommand(nestedKey.root, "doc"), "create"), "width", {{"foo", "1"}});
	CHECK_EQ(SpecErrorOf(nestedKey), "command 'doc create': option 'width': unknown key(s): foo");

	RawSpec nestedArg = NewSpec();
	AddArg(AddCommand(nestedArg.root, "doc"), "src", {{"min", "1"}});
	CHECK_EQ(SpecErrorOf(nestedArg), "command 'doc': arg 'src': unknown key(s): min");

	RawSpec rootOnly = NewSpec();
	AddCommand(rootOnly.root, "doc", {{"version", "1.0"}});
	CHECK_EQ(SpecErrorOf(rootOnly), "command 'doc': unknown key(s): version");

	RawSpec nestedClash = NewSpec();
	RawLevel &doc = AddCommand(nestedClash.root, "doc");
	AddArg(doc, "a");
	AddCommand(doc, "create");
	CHECK_EQ(SpecErrorOf(nestedClash), "command 'doc': args and commands are mutually exclusive on one level");
}

TEST(inherited_option_rules)
{
	CHECK_EQ(OptionError("color", {{"type", "s"}, {"inherit", "1"}}), "option 'color': inherit requires commands on the same level");

	RawSpec nested = NewSpec();
	AddOption(AddCommand(nested.root, "doc"), "color", {{"type", "s"}, {"inherit", "1"}});
	CHECK_EQ(SpecErrorOf(nested), "command 'doc': option 'color': inherit requires commands on the same level");

	RawSpec taken = NewSpec();
	AddOption(taken.root, "color|c", {{"type", "s"}, {"inherit", "1"}});
	AddOption(AddCommand(AddCommand(taken.root, "doc"), "create"), "c");
	CHECK_EQ(SpecErrorOf(taken),
	         "command 'doc create': option 'color' inherited from the top level: name 'c' is already used by option 'c'");

	RawSpec middle = NewSpec();
	RawLevel &doc = AddCommand(middle.root, "doc");
	AddOption(doc, "path", {{"type", "s"}, {"inherit", "1"}});
	AddOption(AddCommand(doc, "create"), "p|path");
	CHECK_EQ(SpecErrorOf(middle),
	         "command 'doc create': option 'path' inherited from command 'doc': name 'path' is already used by an alias of option 'p'");

	RawSpec raw = NewSpec();
	AddOption(raw.root, "color", {{"type", "s"}, {"inherit", "1"}});
	RawLevel &outer = AddCommand(raw.root, "doc");
	AddOption(outer, "path", {{"type", "s"}, {"inherit", "1"}});
	AddCommand(outer, "create");
	auto spec = Compile(raw);
	const Level *docLevel = spec->root->Command("doc");
	const Level *createLevel = docLevel->Command("create");
	CHECK_EQ(docLevel->inherited.size(), 1u);
	CHECK(docLevel->inherited[0] == spec->root->OptionNamed("color"));
	CHECK(docLevel->OptionNamed("color") == spec->root->OptionNamed("color"));
	CHECK_EQ(docLevel->DeclaredOptions().size(), 1u);
	CHECK_EQ(createLevel->inherited.size(), 2u);
	CHECK_EQ(createLevel->inherited[0]->name, "color");
	CHECK_EQ(createLevel->inherited[1]->name, "path");
	CHECK_EQ(createLevel->DeclaredOptions().size(), 0u);
}
