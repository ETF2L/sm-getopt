/**
 * Copyright 2026 ETF2L Contributors, Dave
 *
 * The tests of getopt.inc, run with smrunner (sm-runner) against the built
 * extension:
 *
 *     smrunner test test_getopt.smx --extensions <dist>/extensions \
 *         --plugin getopt-other.smx
 *
 * Every native is called as a plugin calls it: specs with every type and
 * shape, the command line syntax, the outcomes and their output, commands
 * and inherited options, the help, the value pipeline with its callbacks,
 * types of the plugin's own and shared ones, spec errors, errors of the
 * plugin, and ownership (tests/getopt-other.sp is another plugin with a
 * spec, a result and a shared type of its own).
 */

// no critic (ProhibitLongFiles)
#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <getopt>
#include <smrunner>

public Plugin myinfo = {
	name = "Getopt tests",
	author = "ETF2L Contributors, Dave",
	description = "The smrunner tests of the getopt extension",
	version = "3.2.1",
	url = "https://github.com/etf2l/sm-getopt"
};

/* ------------------------------------------------------------------ helpers */

/* Runs failing, which must stop with a native error containing text. */
void ExpectNativeError(Function failing, const char[] text) {
	Runner.ExpectError(text);
	Call_StartFunction(null, failing);
	Call_Finish();
}

/* The spec and result the failing functions below use. */
GetoptSpec g_spec;
GetoptResult g_result;

/* Parses line, which must be valid, and returns the result. */
GetoptResult Parsed(GetoptSpec spec, const char[] line) {
	GetoptResult result = spec.Parse(line);
	char error[GETOPT_LINE_LENGTH];
	result.GetError(error, sizeof(error));
	Assert.Equal(result.Outcome, Getopt_Parsed, "'%s' parses: %s", line, error);
	return result;
}

/* Parses line, which must be rejected with error. */
void Rejects(GetoptSpec spec, const char[] line, const char[] expected) {
	GetoptResult result = spec.Parse(line);
	Assert.Equal(result.Outcome, Getopt_Rejected, "'%s' is rejected", line);
	char error[GETOPT_LINE_LENGTH];
	Assert.True(result.GetError(error, sizeof(error)));
	Assert.StrEqual(error, expected, "the error of '%s'", line);
	delete result;
}

/* All output lines of a result, joined with newlines. */
void JoinOutput(GetoptResult result, char[] buffer, int maxlength) {
	buffer[0] = '\0';
	for (int i = 0; i < result.OutputLines; i++) {
		char line[GETOPT_LINE_LENGTH];
		result.GetOutputLine(i, line, sizeof(line));
		Format(buffer, maxlength, "%s%s\n", buffer, line);
	}
}

/* All help lines of a result, joined with newlines. */
void JoinHelp(GetoptResult result, char[] buffer, int maxlength) {
	buffer[0] = '\0';
	for (int i = 0; i < result.HelpLines; i++) {
		char line[GETOPT_LINE_LENGTH];
		result.GetHelpLine(i, line, sizeof(line));
		Format(buffer, maxlength, "%s%s\n", buffer, line);
	}
}

/* --------------------------------------------------------- types and values */

public void Test_EveryBuiltInTypeReachesThePlugin() {
	GetoptSpec spec = new GetoptSpec("sm_types");
	spec.Option("flag|f");
	spec.Option("color", "bool");
	spec.Option("verbose|v", "counter");
	spec.Option("name", "string");
	spec.Option("count", "int");
	spec.Option("ratio", "float");
	spec.Option("mirror", "url");
	spec.Option("when", "date").TimeZone("UTC");
	spec.Option("for", "duration");

	GetoptResult result = Parsed(spec, "-f --no-color -vvv --name \"two words\" --count 007 --ratio 1.5 "
	                             ... "--mirror \"https://example.com/x\" --when 2026-10-06T14:00:00 --for \"2 weeks\"");
	Assert.True(result.GetBool("flag"));
	Assert.Equal(result.GetInt("flag"), 1);
	Assert.True(result.IsSet("color"));
	Assert.False(result.GetBool("color"), "--no-color turned it off");
	Assert.Equal(result.GetInt("verbose"), 3);

	char text[64];
	Assert.True(result.GetString("name", text, sizeof(text)));
	Assert.StrEqual(text, "two words");
	Assert.Equal(result.GetInt("count"), 7);
	result.GetString("count", text, sizeof(text));
	Assert.StrEqual(text, "7", "a number as written without leading zeros");
	Assert.FloatEqual(result.GetFloat("ratio"), 1.5);
	result.GetString("mirror", text, sizeof(text));
	Assert.StrEqual(text, "https://example.com/x");
	result.GetString("when", text, sizeof(text));
	Assert.StrEqual(text, "2026-10-06T14:00:00");
	Assert.Equal(result.GetInt("when"), 1791295200, "a Unix timestamp");

	int months;
	int days;
	int seconds;
	Assert.True(result.GetDuration("for", months, days, seconds));
	Assert.Equal(months, 0);
	Assert.Equal(days, 14);
	Assert.Equal(seconds, 0);
	delete result;
	delete spec;
}

public void Test_TypeNamesHaveAliasesInAnyCase() {
	GetoptSpec spec = new GetoptSpec("sm_aliases");
	spec.Option("a", "BOOLEAN");
	spec.Option("b", "!");
	spec.Option("c", "+");
	spec.Option("d", "Integer");
	spec.Option("e", "num");
	spec.Option("g", "uri");
	spec.Arg("rest", "str");
	GetoptResult result = Parsed(spec, "-a --nob -cc -d 5 -e 2 -g \"x://y\" word");
	Assert.True(result.GetBool("a"));
	Assert.False(result.GetBool("b"));
	Assert.Equal(result.GetInt("c"), 2);
	Assert.Equal(result.GetInt("d"), 5);
	Assert.FloatEqual(result.GetFloat("e"), 2.0);
	delete result;
	delete spec;
}

public void Test_OptionsWithoutAValueAreNotSet() {
	GetoptSpec spec = new GetoptSpec("sm_unset");
	spec.Option("flag");
	spec.Option("count", "int");
	spec.Option("tag", "string").Multiple();
	spec.Option("define", "string").Hash();
	GetoptResult result = Parsed(spec, "");
	Assert.False(result.IsSet("flag"));
	Assert.False(result.GetBool("flag"));
	Assert.Equal(result.GetInt("count", 42), 42, "the fallback");
	Assert.FloatEqual(result.GetFloat("count", 1.5), 1.5);
	char text[16] = "unchanged";
	Assert.False(result.GetString("count", text, sizeof(text)));
	Assert.StrEqual(text, "", "the buffer is empty then");
	Assert.Equal(result.Count("count"), 0);
	Assert.Equal(result.Count("tag"), 0, "an empty list");
	Assert.False(result.IsSet("define"), "an empty mapping");
	delete result;
	delete spec;
}

public void Test_DefaultsApplyAndAreChecked() {
	GetoptSpec spec = new GetoptSpec("sm_defaults");
	spec.Option("keep|k", "int").Default("007");
	spec.Option("color", "bool").Default("1");
	spec.Option("tag", "string").Multiple().Default("red").Default("blue");
	spec.Option("define", "string").Hash().DefaultPair("os", "linux");
	spec.Option("server", "string").ObjectList().DefaultField(0, "host", "alpha").DefaultField(1, "host", "beta");
	GetoptResult result = Parsed(spec, "");
	Assert.Equal(result.GetInt("keep"), 7, "the checked default");
	Assert.True(result.GetBool("color"));
	Assert.Equal(result.Count("tag"), 2);
	char text[32];
	result.GetStringAt("tag", 1, text, sizeof(text));
	Assert.StrEqual(text, "blue");
	result.GetStringOf("define", "os", text, sizeof(text));
	Assert.StrEqual(text, "linux");
	Assert.Equal(result.Count("server"), 2);
	result.GetStringIn("server", 1, "host", text, sizeof(text));
	Assert.StrEqual(text, "beta");
	delete result;

	result = Parsed(spec, "--tag green");
	Assert.Equal(result.Count("tag"), 1, "the command line replaces a list default whole");
	delete result;
	delete spec;
}

public void Test_ListsMappingsAndObjectListsHoldEveryValue() {
	GetoptSpec spec = new GetoptSpec("sm_shapes");
	spec.Option("tag|t", "string").Multiple().Csv();
	spec.Option("port", "int").Multiple();
	spec.Option("define|D", "string").Hash();
	spec.Option("server", "string").ObjectList();
	GetoptResult result = Parsed(spec, "-t \"red, blue,\" --tag green --port 80 --port 443 -D os=linux "
	                             ... "--define arch=x86 -D path=a=b -D os=bsd "
	                             ... "--server 1.host=beta --server 0.host=alpha --server 0.port=80");
	Assert.Equal(result.Count("tag"), 3);
	char text[32];
	result.GetStringAt("tag", 1, text, sizeof(text));
	Assert.StrEqual(text, "blue", "csv items are trimmed");
	Assert.Equal(result.GetIntAt("port", 1), 443);
	Assert.FloatEqual(result.GetFloatAt("port", 0), 80.0);

	Assert.Equal(result.Count("define"), 3);
	result.GetKey("define", 0, text, sizeof(text));
	Assert.StrEqual(text, "arch", "keys are sorted");
	Assert.True(result.HasKey("define", "path"));
	Assert.False(result.HasKey("define", "missing"));
	result.GetStringOf("define", "path", text, sizeof(text));
	Assert.StrEqual(text, "a=b", "split at the first =");
	result.GetStringOf("define", "os", text, sizeof(text));
	Assert.StrEqual(text, "bsd", "a repeated key wins");
	Assert.Equal(result.GetIntOf("define", "missing", -1), -1);

	Assert.Equal(result.Count("server"), 2);
	Assert.Equal(result.FieldCount("server", 0), 2);
	result.GetField("server", 0, 0, text, sizeof(text));
	Assert.StrEqual(text, "host");
	Assert.True(result.HasField("server", 0, "port"));
	Assert.False(result.HasField("server", 1, "port"));
	Assert.Equal(result.GetIntIn("server", 0, "port"), 80);
	Assert.Equal(result.GetIntIn("server", 1, "port", -1), -1);
	result.GetStringIn("server", 1, "host", text, sizeof(text));
	Assert.StrEqual(text, "beta");
	delete result;
	delete spec;
}

public void Test_ArgsTakeThePositionalWords() {
	GetoptSpec spec = new GetoptSpec("sm_args");
	spec.Option("verbose|v");
	spec.Arg("map").Required();
	spec.Arg("count", "int");
	spec.Arg("rest").Multiple();
	GetoptResult result = Parsed(spec, "cp_badlands -v 3 a b c");
	char text[32];
	result.GetString("map", text, sizeof(text));
	Assert.StrEqual(text, "cp_badlands");
	Assert.Equal(result.GetInt("count"), 3);
	Assert.True(result.GetBool("verbose"), "options and args mix");
	Assert.Equal(result.Count("rest"), 3);
	result.GetStringAt("rest", 2, text, sizeof(text));
	Assert.StrEqual(text, "c");
	delete result;

	result = Parsed(spec, "map 1 -- -5 --x");
	Assert.Equal(result.Count("rest"), 2, "-- ends the options");
	result.GetStringAt("rest", 0, text, sizeof(text));
	Assert.StrEqual(text, "-5");
	delete result;
	delete spec;
}

/* ------------------------------------------------------ command line syntax */

public void Test_WordsQuotesAndColonsAreKept() {
	GetoptSpec spec = new GetoptSpec("sm_words");
	spec.Option("name", "string");
	spec.Arg("words").Multiple();
	GetoptResult result = Parsed(spec, "--name=\"my map\" STEAM_0:1:23 14:00 \"\" 'single' \"open quote");
	char text[64];
	result.GetString("name", text, sizeof(text));
	Assert.StrEqual(text, "my map");
	Assert.Equal(result.Count("words"), 5);
	result.GetStringAt("words", 0, text, sizeof(text));
	Assert.StrEqual(text, "STEAM_0:1:23", "a colon does not split");
	result.GetStringAt("words", 2, text, sizeof(text));
	Assert.StrEqual(text, "", "an empty word");
	result.GetStringAt("words", 3, text, sizeof(text));
	Assert.StrEqual(text, "'single'", "single quotes are ordinary");
	result.GetStringAt("words", 4, text, sizeof(text));
	Assert.StrEqual(text, "open quote", "an open quote runs to the end");
	delete result;
	delete spec;
}

public void Test_ShortOptionsBundleAndTakeValues() {
	GetoptSpec spec = new GetoptSpec("sm_short");
	spec.Option("v", "counter");
	spec.Option("x", "string");
	spec.Option("owner|o", "string");
	GetoptResult result = Parsed(spec, "-vvxfoo -odave");
	Assert.Equal(result.GetInt("v"), 2);
	char text[16];
	result.GetString("x", text, sizeof(text));
	Assert.StrEqual(text, "foo", "a letter that takes a value ends the bundle");
	result.GetString("owner", text, sizeof(text));
	Assert.StrEqual(text, "dave");
	delete result;

	result = Parsed(spec, "-o=dave --o -5");
	result.GetString("owner", text, sizeof(text));
	Assert.StrEqual(text, "-5", "the next word, also with a dash; the last wins");
	delete result;
	delete spec;
}

public void Test_CommandLineMistakesAreRejected() {
	GetoptSpec spec = new GetoptSpec("sm_mistakes");
	spec.Option("keep", "int").Min(1).Max(10);
	spec.Option("host", "string").Required();
	spec.Option("flag");
	spec.Option("ratio", "float");
	spec.Option("mirror", "url");
	spec.Option("level", "string").Valid("debug", "info");
	spec.Option("define", "string").Hash();
	spec.Option("server", "string").ObjectList();
	spec.Option("tag", "string").Multiple().Csv();
	spec.Arg("map");

	Rejects(spec, "--host h --kep 1", "Unknown option: kep");
	Rejects(spec, "--host h --keep", "Option keep requires an argument");
	Rejects(spec, "--host h --keep=", "Option keep requires an argument");
	Rejects(spec, "--host h --flag=1", "Option flag does not take an argument");
	Rejects(spec, "", "missing required option '--host'");
	Rejects(spec, "--host h a b", "unexpected extra argument 'b'");
	Rejects(spec, "--host h --keep x", "option '--keep': 'x' is not an integer");
	Rejects(spec, "--host h --keep 99999999999", "option '--keep': '99999999999' is too large for an integer");
	Rejects(spec, "--host h --keep 0", "option '--keep': 0 is smaller than the minimum of 1");
	Rejects(spec, "--host h --keep 11", "option '--keep': 11 is larger than the maximum of 10");
	Rejects(spec, "--host h --ratio 0x10", "option '--ratio': '0x10' is not a number");
	Rejects(spec, "--host h --ratio inf", "option '--ratio': 'inf' is not a finite number");
	Rejects(spec, "--host h --mirror example.com", "option '--mirror': 'example.com' is not a URL");
	Rejects(spec, "--host h --level warn", "option '--level': 'warn' is not one of: debug, info");
	Rejects(spec, "--host h --define os", "Option define, key \"os\", requires a value");
	Rejects(spec, "--host h --define =x", "option '--define': empty key");
	Rejects(spec, "--host h --server 1.host=a", "option '--server': missing index 0");
	Rejects(spec, "--host h --server x=a", "option '--server': invalid key 'x', expected INDEX.FIELD=VALUE");
	Rejects(spec, "--host h --server 00.host=a",
	        "option '--server': invalid key '00.host', the index must not have leading zeros");
	Rejects(spec, "--host h --tag a,,b", "option '--tag': 'a,,b' contains an empty item");
	Rejects(spec, "--host h --x --y", "Unknown option: x; Unknown option: y");
	delete spec;
}

public void Test_ARejectedResultHasTheErrorAndTheHelp() {
	GetoptSpec spec = new GetoptSpec("sm_backup");
	spec.Description("Copy a map's files to the backup folder.");
	spec.Arg("map").Required().Help("Map to back up");
	GetoptResult result = spec.Parse("");
	Assert.Equal(result.Outcome, Getopt_Rejected);
	char output[2048];
	JoinOutput(result, output, sizeof(output));
	Assert.StrContains(output, "ERROR: missing required argument <map>\n\n# sm_backup");
	Assert.StrContains(output, "# Copy a map's files to the backup folder.");
	Assert.StrContains(output, "Map to back up");
	delete result;
	delete spec;
}

public void Test_HelpAndVersionAreAnswered() {
	GetoptSpec spec = new GetoptSpec("sm_backup");
	spec.Arg("map").Required();
	GetoptResult result = spec.Parse("--help");
	Assert.Equal(result.Outcome, Getopt_Answered, "without the required map");
	char output[2048];
	JoinOutput(result, output, sizeof(output));
	Assert.StrContains(output, "# sm_backup map");
	char error[16] = "unchanged";
	Assert.False(result.GetError(error, sizeof(error)), "no error when answered");
	delete result;

	result = spec.Parse("--version");
	Assert.Equal(result.Outcome, Getopt_Answered);
	char line[GETOPT_LINE_LENGTH];
	result.GetOutputLine(0, line, sizeof(line));
	Assert.StrEqual(line, "sm_backup 3.2.1", "the version of the plugin's myinfo");
	delete result;

	spec.Version("1.2.0");
	result = spec.Parse("--version");
	result.GetVersion(line, sizeof(line));
	Assert.StrEqual(line, "sm_backup 1.2.0");
	delete result;

	Rejects(spec, "-h", "Unknown option: h");
	delete spec;
}

public void Test_TheHelpListsGroupsDefaultsAndExamples() {
	GetoptSpec spec = new GetoptSpec("sm_mirror");
	spec.Description("Copy demos to a web server.");
	spec.Arg("demo").Multiple().Help("Demos to copy; all without");
	spec.Option("limit", "int").Default("10").Group("Selection").Help("At most this many demos");
	spec.Option("host", "string").Required().Group("Target").TypeHint("Hostname").Help("Where to copy to");
	spec.Option("secret").Hidden();
	spec.Option("level", "string").Valid("debug", "info");
	spec.Option("color", "bool");
	spec.Example("Copy two demos", "--host demos.example.com a.dem b.dem");
	GetoptResult result = Parsed(spec, "--host h");
	char help[4096];
	JoinHelp(result, help, sizeof(help));
	Assert.StrContains(help, "# sm_mirror [options] [demo...]\n# Copy demos to a web server.");
	Assert.StrContains(help, "## Arguments\n");
	Assert.StrContains(help, "## Selection\n");
	Assert.StrContains(help, "Default = 10");
	Assert.StrContains(help, "## Target\n");
	Assert.StrContains(help, "[REQ] Where to copy to [Hostname]");
	Assert.StrContains(help, "--[no-]color");
	Assert.StrContains(help, "debug, info");
	Assert.StrContains(help, "# Examples:\n## Copy two demos\n##   sm_mirror --host demos.example.com a.dem b.dem");
	Assert.False(StrContains(help, "secret") != -1, "a hidden option is not listed");
	Assert.False(StrContains(help, "--help") != -1, "the automatic options are not listed");
	delete result;
	delete spec;
}

/* ----------------------------------------------------------------- commands */

GetoptSpec NewToolSpec() {
	GetoptSpec spec = new GetoptSpec("sm_user");
	spec.Option("verbose|v").Inherit();
	spec.Option("dry");
	GetoptLevel add = spec.Command("add").Description("Add a user");
	add.Option("role", "string").Valid("admin", "player").Default("player");
	add.Arg("name").Required();
	GetoptLevel remove = spec.Command("remove");
	remove.Arg("name").Required();
	return spec;
}

public void Test_CommandsHaveLevelsOfTheirOwn() {
	GetoptSpec spec = NewToolSpec();
	GetoptResult result = Parsed(spec, "--dry add --role admin -v alice");
	char text[32];
	Assert.True(result.GetCommand(text, sizeof(text)));
	Assert.StrEqual(text, "add");
	Assert.True(result.GetBool("dry"));

	GetoptResult add = result.Subcommand;
	Assert.True(add != null);
	add.GetString("name", text, sizeof(text));
	Assert.StrEqual(text, "alice");
	add.GetString("role", text, sizeof(text));
	Assert.StrEqual(text, "admin");
	Assert.True(add.Subcommand == null, "a level without commands");
	delete add;
	Assert.True(result.GetBool("verbose"), "an inherited option given below belongs to its level");
	delete result;

	Rejects(spec, "", "missing command, expected one of: add, remove");
	Rejects(spec, "delete x", "unknown command 'delete', expected one of: add, remove");
	Rejects(spec, "add --dry alice", "Unknown option: dry");
	delete spec;
}

public void Test_ACommandCanBeOptional() {
	GetoptSpec spec = NewToolSpec();
	spec.CommandRequired(false);
	GetoptResult result = Parsed(spec, "--dry");
	char text[16];
	Assert.False(result.GetCommand(text, sizeof(text)));
	Assert.True(result.Subcommand == null);
	delete result;
	delete spec;
}

public void Test_HelpOfACommandIsItsLevels() {
	GetoptSpec spec = NewToolSpec();
	GetoptResult result = spec.Parse("add --help");
	Assert.Equal(result.Outcome, Getopt_Answered);
	char output[2048];
	JoinOutput(result, output, sizeof(output));
	Assert.StrContains(output, "# sm_user add");
	Assert.StrContains(output, "--role");
	delete result;

	result = spec.Parse("add");
	JoinOutput(result, output, sizeof(output));
	Assert.StrContains(output, "ERROR: missing required argument <name>\n\n# sm_user add", "the help of the level");
	delete result;
	delete spec;
}

/* ------------------------------------------------------- the value pipeline */

int g_validCalls;

void Levels_Valid(GetoptList values, const char[] option, any data) {
	g_validCalls++;
	Assert.StrEqual(option, "level");
	Assert.Equal(data, 7);
	values.Add("low");
	values.Add("high");
}

bool Prefix_IsValid(const char[] value, const char[] option, any data) {
	return strlen(value) <= 3;
}

void Minutes_ToSeconds(GetoptResult values, const char[] value, char[] processed, int maxlength, const char[] name,
                       any data) {
	Assert.Equal(values.GetInt("minutes"), StringToInt(value), "values before any was processed");
	Format(processed, maxlength, "%d", StringToInt(value) * data);
}

public void Test_ValidFromLazyValidAndProcessValueTakePart() {
	GetoptSpec spec = new GetoptSpec("sm_pipeline");
	spec.Option("level", "string").ValidFrom(Levels_Valid, 7);
	spec.Option("prefix", "string").LazyValid(Prefix_IsValid);
	spec.Option("minutes", "int").ProcessValue(Minutes_ToSeconds, 60);
	spec.Arg("rest", "int").ProcessValue(Minutes_ToSeconds, 2);
	spec.Validate();
	g_validCalls = 0;

	GetoptResult result = Parsed(spec, "--level high --prefix ab --minutes 3");
	Assert.Equal(g_validCalls, 1, "asked once in the parse");
	Assert.Equal(result.GetInt("minutes"), 180, "the processed value");
	delete result;

	Rejects(spec, "--level mid", "option '--level': 'mid' is not one of: low, high");
	Rejects(spec, "--prefix abcd", "option '--prefix': 'abcd' is not a valid value");
	delete spec;
}

/* ------------------------------------------------------------ dates and durations */

public void Test_DatesAndDurationsKeepToTheirBounds() {
	GetoptSpec spec = new GetoptSpec("sm_when");
	spec.Option("when", "date").TimeZone("UTC");
	spec.Option("for", "duration");
	spec.Option("far", "date").TimeZone("UTC").Set("max", "9999-12-31");

	GetoptResult result = Parsed(spec, "--for \"90 minutes\" --far 2040-01-01");
	int months;
	int days;
	int seconds;
	result.GetDuration("for", months, days, seconds);
	Assert.Equal(seconds, 5400);
	Assert.Equal(result.GetInt("for"), 5400);
	char text[32];
	result.GetString("for", text, sizeof(text));
	Assert.StrEqual(text, "90 minutes");
	result.GetString("far", text, sizeof(text));
	Assert.StrEqual(text, "2040-01-01T00:00:00", "a date beyond a cell, read as text");
	g_result = result;
	ExpectNativeError(GetIntOfFar, "does not fit a cell; read it as a string");
	delete result;

	Rejects(spec, "--when 2040-01-01",
	        "option '--when': '2040-01-01' is later than the maximum of 2038-01-19T03:14:07");
	Rejects(spec, "--when someday", "option '--when': 'someday' is not a date");
	Rejects(spec, "--for 1h", "option '--for': '1h' is not a duration");
	Rejects(spec, "--for \"100 years\"",
	        "option '--for': '100 years' is longer than the maximum of 2147483647 seconds");
	delete spec;
}

void GetIntOfFar() {
	g_result.GetInt("far");
}

public void Test_BigIntTakesAnySize() {
	GetoptSpec spec = new GetoptSpec("sm_big");
	spec.Option("id", "int").BigInt();
	GetoptResult result = Parsed(spec, "--id 76561197960287930");
	char text[32];
	result.GetString("id", text, sizeof(text));
	Assert.StrEqual(text, "76561197960287930");
	g_result = result;
	ExpectNativeError(GetIntOfId, "The value '76561197960287930' of 'id' does not fit a cell; read it as a string");
	delete result;
	delete spec;
}

void GetIntOfId() {
	g_result.GetInt("id");
}

/* ------------------------------------------------------- types of your own */

bool Even_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	Assert.Equal(keys.Caller, -1, "check runs without a caller");
	int number;
	if (value[0] == '\0' || StringToIntEx(value, number) != strlen(value)) {
		Format(problem, maxlength, "'%s' is not a whole number", value);
		return false;
	}
	if (number % 2 != 0) {
		Format(problem, maxlength, "%d is not an even number", number);
		return false;
	}
	return true;
}

void Even_Coerce(GetoptKeys keys, const char[] value, char[] coerced, int maxlength) {
	char factor[16] = "1";
	keys.Get("factor", factor, sizeof(factor));
	Format(coerced, maxlength, "%d", StringToInt(value) * StringToInt(factor));
}

bool Even_CheckKeys(GetoptKeys keys, char[] problem, int maxlength) {
	char factor[16];
	if (keys.Get("factor", factor, sizeof(factor)) && StringToInt(factor) < 1) {
		strcopy(problem, maxlength, "factor must be positive");
		return false;
	}
	return true;
}

void Even_Notes(GetoptKeys keys, GetoptList notes) {
	if (keys.Has("factor")) {
		notes.Add("scaled");
	}
}

int g_steps[8];
int g_stepCount;
int g_caller;

bool Even_Resolve(GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem, int maxlength) {
	g_steps[g_stepCount++] = 1;
	g_caller = keys.Caller;
	char half[16];
	IntToString(StringToInt(value) / 2, half, sizeof(half));
	resolved.SetAttribute("half", half);
	return true;
}

bool Even_Verify(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	g_steps[g_stepCount++] = 2;
	if (StrEqual(value, "666")) {
		strcopy(problem, maxlength, "is not allowed here");
		return false;
	}
	return true;
}

bool Even_Prepare(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	g_steps[g_stepCount++] = 3;
	return true;
}

void RegisterEven() {
	Getopt_RegisterType("even pair", GetoptKind_Value, "factor")
		.SetLabel("Even")
		.OnCheck(Even_Check)
		.OnCoerce(Even_Coerce)
		.OnCheckKeys(Even_CheckKeys)
		.OnNotes(Even_Notes)
		.OnResolve(Even_Resolve)
		.OnVerify(Even_Verify)
		.OnPrepare(Even_Prepare);
}

public void Test_ATypeOfYourOwnRunsEveryCallback() {
	RegisterEven();
	GetoptSpec spec = new GetoptSpec("sm_workers");
	spec.Option("workers", "even").Set("factor", "3").Help("Number of workers");
	spec.Option("pairs", "PAIR");
	spec.Validate();

	g_stepCount = 0;
	int client = Runner.ConnectClient("caller");
	GetoptResult result = spec.Parse("--workers 4 --pairs 2", client);
	Assert.Equal(result.Outcome, Getopt_Parsed);
	Assert.Equal(result.GetInt("workers"), 12, "coerced with the key");
	char half[16];
	Assert.True(result.GetAttribute("workers", "half", half, sizeof(half)));
	Assert.StrEqual(half, "6");
	Assert.False(result.GetAttribute("workers", "none", half, sizeof(half)));
	Assert.Equal(g_caller, client, "resolve gets the caller");
	Assert.Equal(g_stepCount, 6);
	Assert.Equal(g_steps[0], 1, "resolve, verify and prepare each run on all values in turn");
	Assert.Equal(g_steps[1], 1);
	Assert.Equal(g_steps[2], 2);
	Assert.Equal(g_steps[4], 3);

	char help[2048];
	JoinHelp(result, help, sizeof(help));
	Assert.StrContains(help, "[scaled] Number of workers [Even]");
	delete result;

	Rejects(spec, "--workers 3", "option '--workers': 3 is not an even number");
	Rejects(spec, "--workers 222", "option '--workers': is not allowed here");
	delete spec;

	spec = new GetoptSpec("sm_bad");
	spec.Option("workers", "even").Set("factor", "0");
	g_spec = spec;
	ExpectNativeError(ValidateTheSpec, "Getopt spec: option 'workers': factor must be positive");
	delete spec;
}

void ValidateTheSpec() {
	g_spec.Validate();
}

public void Test_RegisteringATakenNameIsAnError() {
	RegisterEven();
	ExpectNativeError(RegisterString, "Getopt: option type name 'string' is already registered");
	ExpectNativeError(RegisterEvenAgain, "Getopt: option type name 'even' is already registered");
	ExpectNativeError(RegisterNoName, "Getopt: an option type needs at least one name");
	ExpectNativeError(RegisterBadKind, "Invalid GetoptKind 9");
}

void RegisterString() {
	Getopt_RegisterType("string");
}

void RegisterEvenAgain() {
	Getopt_RegisterType("EVEN");
}

void RegisterNoName() {
	Getopt_RegisterType("");
}

void RegisterBadKind() {
	Getopt_RegisterType("odd", view_as<GetoptKind>(9));
}

public void Test_FlagKindsOfYourOwnAreSwitches() {
	Getopt_RegisterType("toggle", GetoptKind_Bool);
	GetoptSpec spec = new GetoptSpec("sm_toggle");
	spec.Option("light", "toggle");
	GetoptResult result = Parsed(spec, "--no-light");
	Assert.True(result.IsSet("light"));
	Assert.False(result.GetBool("light"));
	delete result;

	spec.Arg("value", "toggle");
	g_spec = spec;
	ExpectNativeError(ValidateTheSpec, "Getopt spec: arg 'value': type 'toggle' cannot be used for a positional arg");
	delete spec;
}

/* ----------------------------------------------------------- shared types */

public void Test_SharedTypesOfAnotherPluginCanBeNamed() {
	GetoptSpec spec = new GetoptSpec("sm_shared");
	spec.Option("color", "hexcolor");
	GetoptResult result = Parsed(spec, "--color ff8800");
	char text[16];
	result.GetString("color", text, sizeof(text));
	Assert.StrEqual(text, "#FF8800", "converted by the other plugin's callback");
	delete result;
	Rejects(spec, "--color nope", "option '--color': 'nope' is not a hex color");
	delete spec;
}

bool MyHex_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	return true;
}

public void Test_APluginsOwnTypeComesBeforeASharedOne() {
	Getopt_RegisterType("hexcolor").OnCheck(MyHex_Check);
	GetoptSpec spec = new GetoptSpec("sm_own");
	spec.Option("color", "hexcolor");
	GetoptResult result = Parsed(spec, "--color nope");
	char text[16];
	result.GetString("color", text, sizeof(text));
	Assert.StrEqual(text, "nope");
	delete result;
	delete spec;

	ExpectNativeError(RegisterSharedHexcolor, "Getopt: option type name 'hexcolor' is already registered");
}

void RegisterSharedHexcolor() {
	Getopt_RegisterType("hexcolor", GetoptKind_Value, "", true);
}

public void Test_SharedTypesLeaveWithTheirPlugin() {
	GetoptSpec spec = new GetoptSpec("sm_leaving");
	spec.Option("color", "hexcolor");
	spec.Validate();
	Runner.Command("sm plugins unload getopt-other");
	Runner.Frames(2);
	g_spec = spec;
	ExpectNativeError(ValidateTheSpec, "Getopt spec: option 'color': unknown option type 'hexcolor'");
	delete spec;
}

/* ------------------------------------------------------------- spec errors */

public void Test_SpecMistakesAreErrorsOfValidate() {
	char failing[][] = {
		"Spec_BadDefault", "Spec_RequiredAndDefault", "Spec_SameName", "Spec_HelpOption", "Spec_ArgsAndCommands",
		"Spec_CsvWithoutMultiple", "Spec_MultipleFlag", "Spec_UnknownKey", "Spec_UnknownType", "Spec_MinAboveMax",
		"Spec_RequiredAfterOptional", "Spec_BadTimeZone", "Spec_InheritWithoutCommands", "Spec_NegationTaken",
		"Spec_ArgAndOptionShareAName", "Spec_BadHelpWidth",
	};
	char texts[][] = {
		"Getopt spec: option 'keep': default value: 'seven' is not an integer",
		"Getopt spec: option 'host': required and default are mutually exclusive",
		"Getopt spec: option 'other': name 'keep' is already used by option 'keep'",
		"Getopt spec: option 'help' collides with the automatic --help option",
		"Getopt spec: args and commands are mutually exclusive on one level",
		"Getopt spec: option 'tag': csv requires multiple",
		"Getopt spec: option 'flag': multiple requires a value-taking type, not 'flag'",
		"Getopt spec: option 'name': unknown key(s): min",
		"Getopt spec: option 'x': unknown option type 'nosuch'",
		"Getopt spec: option 'n': min 5 is larger than max 1",
		"Getopt spec: arg 'b': a required arg cannot follow an optional one",
		"Getopt spec: option 'when': timezone 'Mars/Olympus' is not a known time zone",
		"Getopt spec: option 'v': inherit requires commands on the same level",
		"Getopt spec: option 'color': its negation '--no-color' is already used by option 'no-color'",
		"Getopt spec: arg 'name' and option 'name' share one name",
		"Getopt spec: helpWidth must be a whole number from 1 to 10000, not '0'",
	};
	for (int i = 0; i < sizeof(failing); i++) {
		g_spec = new GetoptSpec("sm_spec");
		Call_StartFunction(null, GetFunctionByName(null, failing[i]));
		Call_Finish();
		ExpectNativeError(ValidateTheSpec, texts[i]);
		delete g_spec;
	}
}

public void Spec_BadDefault() {
	g_spec.Option("keep", "int").Default("seven");
}

public void Spec_RequiredAndDefault() {
	g_spec.Option("host", "string").Required().Default("x");
}

public void Spec_SameName() {
	g_spec.Option("keep");
	g_spec.Option("other|keep");
}

public void Spec_HelpOption() {
	g_spec.Option("help");
}

public void Spec_ArgsAndCommands() {
	g_spec.Arg("a");
	g_spec.Command("c");
}

public void Spec_CsvWithoutMultiple() {
	g_spec.Option("tag", "string").Csv();
}

public void Spec_MultipleFlag() {
	g_spec.Option("flag").Multiple();
}

public void Spec_UnknownKey() {
	g_spec.Option("name", "string").Set("min", "1");
}

public void Spec_UnknownType() {
	g_spec.Option("x", "nosuch");
}

public void Spec_MinAboveMax() {
	g_spec.Option("n", "int").Min(5).Max(1);
}

public void Spec_RequiredAfterOptional() {
	g_spec.Arg("a");
	g_spec.Arg("b").Required();
}

public void Spec_BadTimeZone() {
	g_spec.Option("when", "date").TimeZone("Mars/Olympus");
}

public void Spec_InheritWithoutCommands() {
	g_spec.Option("v").Inherit();
}

public void Spec_NegationTaken() {
	g_spec.Option("color", "bool");
	g_spec.Option("no-color");
}

public void Spec_ArgAndOptionShareAName() {
	g_spec.Option("name", "string");
	g_spec.Arg("name");
}

public void Spec_BadHelpWidth() {
	g_spec.HelpWidth(0);
}

public void Test_ASpecIsCheckedAgainAfterAChange() {
	GetoptSpec spec = new GetoptSpec("sm_change");
	spec.Option("keep", "int");
	spec.Validate();
	spec.Option("keep2", "int").Default("x");
	g_spec = spec;
	ExpectNativeError(ParseTheSpec, "Getopt spec: option 'keep2': default value: 'x' is not an integer");
	delete spec;
}

void ParseTheSpec() {
	delete g_spec.Parse("");
}

/* ------------------------------------------------------ errors of the plugin */

public void Test_ResultsAreReadWithTheMethodsOfTheirShape() {
	GetoptSpec spec = new GetoptSpec("sm_read");
	spec.Option("name", "string").Default("x");
	spec.Option("tag", "string").Multiple().Default("a");
	spec.Option("define", "string").Hash().DefaultPair("k", "v");
	g_result = Parsed(spec, "");
	ExpectNativeError(ReadMissingName, "This level has no option or arg 'nosuch'");
	ExpectNativeError(ReadSingleAsList, "'name' is a single value, read with GetString, GetInt, GetFloat or GetBool");
	ExpectNativeError(ReadListAsSingle, "'tag' is a list, read with the ...At methods");
	ExpectNativeError(ReadHashAsSingle, "'define' is a mapping, read with the ...Of methods");
	ExpectNativeError(ReadListBeyondTheEnd, "'tag' has no entry 5 (it has 1)");
	ExpectNativeError(ReadTextAsInt, "The value 'x' of 'name' is no whole number");
	ExpectNativeError(ReadTextAsFloat, "The value 'x' of 'name' is no number");
	ExpectNativeError(ReadTextAsDuration, "The value 'x' of 'name' is no duration");
	ExpectNativeError(ReadHelpBeyondTheEnd, "There is no line 9999 (there are");
	delete g_result;

	g_result = spec.Parse("--help");
	ExpectNativeError(ReadMissingName, "This result holds no values: the command line was rejected or answered");
	delete g_result;
	delete spec;
}

void ReadMissingName() {
	g_result.IsSet("nosuch");
}

void ReadSingleAsList() {
	g_result.GetIntAt("name", 0);
}

void ReadListAsSingle() {
	g_result.GetInt("tag");
}

void ReadHashAsSingle() {
	char text[8];
	g_result.GetString("define", text, sizeof(text));
}

void ReadListBeyondTheEnd() {
	char text[8];
	g_result.GetStringAt("tag", 5, text, sizeof(text));
}

void ReadTextAsInt() {
	g_result.GetInt("name");
}

void ReadTextAsFloat() {
	g_result.GetFloat("name");
}

void ReadTextAsDuration() {
	int months;
	int days;
	int seconds;
	g_result.GetDuration("name", months, days, seconds);
}

void ReadHelpBeyondTheEnd() {
	char line[16];
	g_result.GetHelpLine(9999, line, sizeof(line));
}

public void Test_WrongUseOfTheApiIsAnError() {
	ExpectNativeError(SpecWithoutName, "A spec needs a name: the console command it describes");
	g_spec = new GetoptSpec("sm_misuse");
	ExpectNativeError(ParseForABadCaller, "Invalid caller 99: a client index, or 0 for the server");
	ExpectNativeError(ValidFromNoFunction, "Invalid callback function");
	ExpectNativeError(BadTypeCallback, "Invalid type callback 9");
	ExpectNativeError(OptionOfADeletedSpec, "is no level of an open spec");
}

void SpecWithoutName() {
	delete new GetoptSpec("");
}

void ParseForABadCaller() {
	delete g_spec.Parse("", 99);
}

void ValidFromNoFunction() {
	g_spec.Option("x", "string").ValidFrom(view_as<GetoptValidSource>(INVALID_FUNCTION));
}

void BadTypeCallback() {
	GetoptType type = Getopt_RegisterType("misuse");
	Getopt_SetTypeCallback(type, view_as<GetoptTypeCallback>(9), MyHex_Check);
}

void OptionOfADeletedSpec() {
	GetoptLevel root = g_spec.Root;
	delete g_spec;
	root.Option("x");
}

GetoptList g_keptList;

void KeepTheList(GetoptList values, const char[] option, any data) {
	g_keptList = values;
	values.Add("a");
}

public void Test_LentObjectsCannotBeKept() {
	GetoptSpec spec = new GetoptSpec("sm_lent");
	spec.Option("x", "string").ValidFrom(KeepTheList);
	spec.Validate();
	ExpectNativeError(AddToTheKeptList, "GetoptList can only be used inside the callback it was passed to");
	delete spec;
}

void AddToTheKeptList() {
	g_keptList.Add("b");
}

/* -------------------------------------------------------------- ownership */

GetoptType g_type;

public void Test_SpecsResultsAndTypesBelongToTheirPlugin() {
	Handle other = FindPluginByFile("getopt-other.smx");
	Assert.True(other != null, "getopt-other.smx runs");
	g_spec = view_as<GetoptSpec>(Runner.GetPluginValue(other, "g_Spec"));
	g_result = view_as<GetoptResult>(Runner.GetPluginValue(other, "g_Result"));
	g_type = view_as<GetoptType>(Runner.GetPluginValue(other, "g_Type"));
	ExpectNativeError(ParseTheSpec, "This spec belongs to another plugin");
	ExpectNativeError(OptionOfTheSpec, "This spec belongs to another plugin");
	ExpectNativeError(ReadTheResult, "This result belongs to another plugin");
	ExpectNativeError(LabelTheType, "This option type belongs to another plugin");
}

void OptionOfTheSpec() {
	g_spec.Option("mine");
}

void ReadTheResult() {
	g_result.IsSet("x");
}

void LabelTheType() {
	g_type.SetLabel("Mine");
}

/* ------------------------------------------------------------- the stocks */

GetoptSpec g_command;

public void OnTestBoot(const char[] test) {
	g_command = new GetoptSpec("sm_greet");
	g_command.Option("loud");
	g_command.Arg("name").Required();
	RegConsoleCmd("sm_greet", Command_Greet);
}

Action Command_Greet(int client, int args) {
	GetoptResult opt = Getopt_ParseCommand(g_command, client);
	if (opt == null) {
		return Plugin_Handled;
	}
	char name[64];
	opt.GetString("name", name, sizeof(name));
	ReplyToCommand(client, "Hello %s%s", name, opt.GetBool("loud") ? "!" : ".");
	delete opt;
	return Plugin_Handled;
}

public void Test_ParseCommandReadsTheWholeArgumentString() {
	char output[1024];
	Runner.CommandOutput(output, sizeof(output), "sm_greet --loud \"STEAM_0:1:23\"");
	Assert.StrContains(output, "Hello STEAM_0:1:23!");
}

public void Test_ParseCommandTellsTheServerConsoleWhy() {
	char output[2048];
	Runner.CommandOutput(output, sizeof(output), "sm_greet");
	Assert.StrContains(output, "ERROR: missing required argument <name>");
	Assert.StrContains(output, "# sm_greet [options] name");
	Assert.False(StrContains(output, "Hello") != -1, "the command did not run");
}

/* smrunner simulates no chat triggers, so the chat replies of Getopt_Reply
 * are not tested here. */
public void Test_ParseCommandTellsAPlayerInTheirConsole() {
	int client = Runner.ConnectClient("alice");
	Runner.ClientCommand(client, "sm_greet");
	Assert.ClientReceived(client, RunnerMessage_Console, "ERROR: missing required argument <name>");
	Assert.ClientReceived(client, RunnerMessage_Console, "# sm_greet [options] name");
	Assert.False(Runner.ClientReceived(client, RunnerMessage_Chat, "ERROR"), "nothing in chat from the console");
}
