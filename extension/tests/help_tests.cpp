/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Tests of the help text and the version line. */
#include "check.h"
#include "support.h"

using namespace smgetopt;
using namespace support;

namespace {

std::vector<std::string> HelpOf(const RawSpec &raw)
{
	auto spec = Compile(raw);
	return RenderHelp(*spec, *spec->root);
}

/* The spec of a program that migrates a repository, with every kind of
 * entry the help knows. */
RawSpec MigrateSpec()
{
	RawSpec raw = NewSpec();
	raw.programName = "migrate";
	raw.root.keys["description"] = "Migrate one Git repository.";
	raw.root.examples.push_back({"Basic run", "--owner dave x.git"});

	AddOption(raw.root, "owner|o", {{"type", "s"}, {"required", "1"}, {"help", "Target owner"}, {"group", "Target"}});
	AddOption(raw.root, "dry-run", {{"help", "Change nothing"}, {"group", "General"}});
	RawEntry &logLevel = AddOption(raw.root, "log-level", {{"type", "s"}, {"help", "Logging level to use"}, {"group", "General"}});
	AddDefault(logLevel, "info");
	SetValid(logLevel, {"debug", "info", "warn"});
	AddDefault(AddOption(raw.root, "private", {{"type", "!"}, {"help", "Create as private"}, {"group", "Target"}}), "1");
	RawEntry &define = AddOption(raw.root, "define|D", {{"type", "s"}, {"hash", "1"}, {"help", "Build variables"}, {"group", "General"}});
	AddDefaultPair(define, "os", "linux");
	AddDefaultPair(define, "arch", "x86");
	AddOption(raw.root, "tag", {{"type", "s"}, {"multiple", "1"}, {"csv", "1"}, {"help", "Labels to attach"}, {"group", "General"}});
	AddOption(raw.root, "host", {{"type", "s"}, {"typehint", "Hostname"}, {"help", "Where to connect"}, {"group", "General"}});
	RawEntry &server = AddOption(raw.root, "server", {{"type", "s"}, {"objectlist", "1"}, {"help", "Upstreams"}, {"group", "General"}});
	AddDefaultField(server, 0, "host", "a");
	AddDefaultField(server, 0, "port", "80");
	AddDefaultField(server, 1, "host", "b");
	AddOption(raw.root, "since", {{"type", "date"}, {"timezone", "UTC"}, {"help", "Start"}, {"group", "General"}});
	AddOption(raw.root, "wait", {{"type", "duration"}, {"group", "General"}});

	AddArg(raw.root, "source-url", {{"type", "url"}, {"required", "1"}, {"help", "The source address"}});
	AddArg(raw.root, "mirror", {{"type", "url"}, {"help", "A second address"}, {"typehint", "Mirror URL"}});
	return raw;
}

} // namespace

TEST(help_layout)
{
	std::vector<std::string> lines = HelpOf(MigrateSpec());
	CHECK_EQ(Joined(lines),
	         "# migrate [options] source-url [mirror]\n"
	         "# Migrate one Git repository.\n"
	         "\n"
	         "## Arguments\n"
	         "   <source-url>                 [REQ] The source address [URL]\n"
	         "   <mirror>                     A second address [Mirror URL]\n"
	         "\n"
	         "## General\n"
	         "   --define, -D <key=value>     Build variables\n"
	         "                                    Default = arch=x86, os=linux\n"
	         "   --dry-run                    Change nothing\n"
	         "   --host <>                    Where to connect [Hostname]\n"
	         "   --log-level <>               Logging level to use\n"
	         "                                    Valid   = [ debug, info, warn ]\n"
	         "                                    Default = info\n"
	         "   --server <N.key=value>       Upstreams\n"
	         "                                    Default = 0.host=a, 0.port=80, 1.host=b\n"
	         "   --since <>                   Start [Date]\n"
	         "   --tag <a,b,...>              Labels to attach\n"
	         "   --wait <>                    [Duration]\n"
	         "\n"
	         "## Target\n"
	         "   --owner, -o <>               [REQ] Target owner\n"
	         "   --[no-]private               Create as private\n"
	         "                                    Default = 1\n"
	         "\n"
	         "# Examples:\n"
	         "## Basic run\n"
	         "##   migrate --owner dave x.git");
	CHECK(!Contains(Joined(lines), "--help"));
	CHECK(!Contains(Joined(lines), "--version"));
}

TEST(help_of_an_empty_spec)
{
	CHECK_EQ(Joined(HelpOf(NewSpec())), "# tool");
}

TEST(help_names_and_placeholders)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "v", {{"type", "+"}});
	AddOption(raw.root, "color|c", {{"type", "!"}});
	AddOption(raw.root, "x|y", {{"type", "!"}});
	AddOption(raw.root, "q|quiet|silent", {{"type", "!"}});
	AddDefault(AddOption(raw.root, "keep", {{"type", "int"}}), "007");
	AddOption(raw.root, "n|num|N", {{"type", "int"}});
	std::vector<std::string> lines = HelpOf(raw);
	CHECK(HasLine(lines, "   -v"));
	CHECK(HasLine(lines, "   --[no-]color, -c"));
	CHECK(HasLine(lines, "   -x, -y, --no-x"));
	CHECK(HasLine(lines, "   -q, --[no-]quiet, --silent"));
	CHECK(HasLine(lines, "   --keep <>"));
	CHECK(HasLine(lines, "   -n, --num, -N <>"));
	CHECK(Contains(Joined(lines), "Default = 007"));
}

TEST(help_header)
{
	RawSpec args = NewSpec();
	AddArg(args.root, "source", {{"required", "1"}});
	AddArg(args.root, "target");
	AddArg(args.root, "more", {{"multiple", "1"}});
	CHECK_EQ(HelpOf(args).at(0), "# tool source [target] [more...]");

	RawSpec required = NewSpec();
	AddArg(required.root, "files", {{"required", "1"}, {"multiple", "1"}});
	CHECK_EQ(HelpOf(required).at(0), "# tool files...");

	/* Without visible options there is no [options]. */
	RawSpec hidden = NewSpec();
	AddOption(hidden.root, "secret", {{"type", "s"}, {"hidden", "1"}});
	CHECK_EQ(Joined(HelpOf(hidden)), "# tool");
}

TEST(help_hides_hidden_options_and_empty_defaults)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "owner", {{"type", "s"}, {"help", "Target owner"}});
	AddOption(raw.root, "secret", {{"type", "s"}, {"hidden", "1"}, {"help", "Not for the help text"}});
	SetValid(AddOption(raw.root, "who", {{"type", "s"}}), {});
	AddOption(raw.root, "live", {{"type", "s"}}).validSource = [] { return std::vector<std::string>{"x"}; };
	std::string text = Joined(HelpOf(raw));
	CHECK(Contains(text, "--owner"));
	CHECK(!Contains(text, "secret"));
	CHECK(!Contains(text, "Default"));
	AddDefault(AddOption(raw.root, "empty", {{"type", "s"}}), "");
	CHECK(HasLine(HelpOf(raw), "                      Default = "));
	CHECK(Contains(text, "Valid   = [  ]"));
	/* A valid list that is asked for at run time is not listed. */
	CHECK(!Contains(text, "[ x ]"));

	ParseResult result = ParseWith(raw, {"--secret", "s"});
	CHECK_EQ(TextOf(*result.values, "secret"), "s");
}

TEST(help_of_commands_and_levels)
{
	RawSpec raw = NewSpec();
	AddOption(raw.root, "verbose|v", {{"type", "counter"}, {"inherit", "1"}, {"help", "More output"}});
	AddOption(raw.root, "alpha", {{"help", "Root only"}});
	RawLevel &document = AddCommand(raw.root, "document", {{"description", "Work on documents"}});
	AddOption(document, "zeta", {{"type", "s"}, {"inherit", "1"}});
	RawLevel &create = AddCommand(document, "create", {{"description", "Create a new document"}});
	AddOption(create, "beta");
	AddOption(create, "omega");
	AddArg(create, "title", {{"required", "1"}});
	create.examples.push_back({"A report", "--beta Report"});
	create.examples.push_back({"Another", "Other"});
	AddCommand(raw.root, "image", {{"description", "Work on images"}});
	AddCommand(raw.root, "x");

	auto spec = Compile(raw);
	CHECK_EQ(Joined(RenderHelp(*spec, *spec->root)),
	         "# tool [options] <command>\n"
	         "\n"
	         "## Options\n"
	         "   --alpha           Root only\n"
	         "   --verbose, -v     More output\n"
	         "\n"
	         "## Commands\n"
	         "   document          Work on documents\n"
	         "   image             Work on images\n"
	         "   x");

	/* A level's own options come first, then the inherited ones, the outer
	 * level's first. */
	const Level &createLevel = *spec->root->Command("document")->Command("create");
	CHECK_EQ(Joined(RenderHelp(*spec, createLevel)),
	         "# tool document create [options] title\n"
	         "# Create a new document\n"
	         "\n"
	         "## Arguments\n"
	         "   <title>           [REQ]\n"
	         "\n"
	         "## Options\n"
	         "   --beta\n"
	         "   --omega\n"
	         "   --verbose, -v     More output\n"
	         "   --zeta <>\n"
	         "\n"
	         "# Examples:\n"
	         "## A report\n"
	         "##   tool document create --beta Report\n"
	         "\n"
	         "## Another\n"
	         "##   tool document create Other");

	const Level &imageLevel = *spec->root->Command("image");
	CHECK_EQ(RenderHelp(*spec, imageLevel).at(0), "# tool image [options]");
}

TEST(help_wraps_long_text)
{
	std::string words;
	for (int i = 0; i < 40; i++)
		words += "word ";

	RawSpec raw = NewSpec();
	raw.root.keys["helpWidth"] = "60";
	AddOption(raw.root, "opt", {{"type", "s"}, {"help", words}});
	std::vector<std::string> lines = HelpOf(raw);

	std::vector<std::string> entry;
	for (const std::string &line : lines) {
		if (Contains(line, "word"))
			entry.push_back(line);
	}
	CHECK(entry.size() > 1);
	for (const std::string &line : entry)
		CHECK(line.size() <= 59);
	CHECK_EQ(entry[0], "   --opt <>     word word word word word word word word");
	CHECK_EQ(entry[1], "                word word word word word word word word");
	CHECK_EQ(entry.back().find_first_not_of(' '), 16u);
}

TEST(help_wrapping_edges)
{
	/* A width narrower than the label column still leaves room for a word,
	 * and a word longer than a line stays whole. */
	RawSpec narrow = NewSpec();
	narrow.root.keys["helpWidth"] = "12";
	AddOption(narrow.root, "work-dir", {{"type", "s"}, {"help", "Where the clone is placed"}});
	CHECK_EQ(Joined(HelpOf(narrow)),
	         "# tool [options]\n"
	         "\n"
	         "## Options\n"
	         "   --work-dir <>     Where\n"
	         "                     the\n"
	         "                     clone\n"
	         "                     is\n"
	         "                     placed");

	/* The words of the type's parts wrap like the help text. */
	RawSpec parts = NewSpec();
	parts.root.keys["helpWidth"] = "30";
	AddOption(parts.root, "u", {{"type", "url"}, {"required", "1"}, {"typehint", "Web address"}, {"help", "Where to go"}});
	CHECK_EQ(Joined(HelpOf(parts)),
	         "# tool [options]\n"
	         "\n"
	         "## Options\n"
	         "   -u <>     [REQ] Where to\n"
	         "             go [Web address]");

	/* Width counts characters, not bytes. */
	RawSpec utf8 = NewSpec();
	utf8.root.keys["helpWidth"] = "24";
	AddOption(utf8.root, "o", {{"help", "\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4 \xC3\xB6\xC3\xB6\xC3\xB6\xC3\xB6 \xC3\xBC\xC3\xBC\xC3\xBC\xC3\xBC"}});
	CHECK(HasLine(HelpOf(utf8), "   -o     \xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4 \xC3\xB6\xC3\xB6\xC3\xB6\xC3\xB6"));
	CHECK(HasLine(HelpOf(utf8), "          \xC3\xBC\xC3\xBC\xC3\xBC\xC3\xBC"));

	/* Whitespace in a help text only separates words. */
	RawSpec spaced = NewSpec();
	AddOption(spaced.root, "o", {{"help", "  a \"quoted\"\n  text  "}});
	CHECK(HasLine(HelpOf(spaced), "   -o     a \"quoted\" text"));
}

TEST(constraint_notes_show_before_the_help_text)
{
	struct Noted : Type {
		TypeKind Kind() const override { return TypeKind::Value; }
		std::string Label() const override { return "Seconds"; }
		std::vector<std::string> ConstraintNotes() const override { return {"at most 1 hour", "whole seconds"}; }
	};
	TypeRegistry types;
	types.Register({{"seconds"}, {}, nullptr, [](const TypeKeys &) { return std::make_shared<Noted>(); }});

	RawSpec raw = NewSpec();
	AddOption(raw.root, "timeout", {{"type", "seconds"}, {"required", "1"}, {"help", "How long to wait"}});
	AddArg(raw.root, "first", {{"type", "seconds"}});
	auto spec = Spec::Compile(raw, types);
	std::vector<std::string> lines = RenderHelp(*spec, *spec->root);
	CHECK(HasLine(lines, "   --timeout <>     [REQ] [at most 1 hour] [whole seconds] How long to wait [Seconds]"));
	CHECK(HasLine(lines, "   <first>          [at most 1 hour] [whole seconds] [Seconds]"));
}

TEST(version_line)
{
	RawSpec raw = NewSpec();
	raw.programName = "sm_backup";
	CHECK_EQ(RenderVersion(*Compile(raw)), "sm_backup unknown");
	raw.fallbackVersion = "1.0.0";
	CHECK_EQ(RenderVersion(*Compile(raw)), "sm_backup 1.0.0");
	raw.root.keys["version"] = "2.0";
	CHECK_EQ(RenderVersion(*Compile(raw)), "sm_backup 2.0");
}
