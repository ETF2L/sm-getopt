/**
 * Copyright 2026 ETF2L Contributors, Dave
 *
 * The tests of the getopt-sourcemod-types plugin (scripting/
 * getopt-sourcemod-types.sp) and getopt_sourcemod.inc, run with smrunner
 * (sm-runner) against the built extension:
 *
 *     smrunner test test_sourcemod_types.smx --extensions <dist>/extensions \
 *         --plugin getopt-sourcemod-types.smx --maps cp_process_final,koth_viaduct
 *
 * Each type looks its value up on the simulated server: maps, cvars,
 * commands, players and targets from the caller's view, keys, plugins and
 * configs.
 */
#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <getopt>
#include <getopt_sourcemod>
#include <smrunner>

/* ------------------------------------------------------------------ helpers */

/* Runs failing, which must stop with a native error containing text. */
void ExpectNativeError(Function failing, const char[] text) {
	Runner.ExpectError(text);
	Call_StartFunction(null, failing);
	Call_Finish();
}

GetoptSpec g_spec;

void ValidateTheSpec() {
	g_spec.Validate();
}

/* A spec with one option of type and the given filter, if any. */
GetoptSpec SpecOf(const char[] type, const char[] filter = "") {
	GetoptSpec spec = new GetoptSpec("sm_types");
	GetoptOption option = spec.Option("value", type);
	if (filter[0] != '\0') {
		option.Set("filter", filter);
	}
	return spec;
}

/* Parses "--value VALUE" for caller with an option of type; it must parse,
 * and the value as text is written into buffer. Returns the result. */
GetoptResult Finds(const char[] type, const char[] value, char[] buffer, int maxlength, int caller = 0,
                   const char[] filter = "") {
	GetoptSpec spec = SpecOf(type, filter);
	char line[256];
	Format(line, sizeof(line), "--value \"%s\"", value);
	GetoptResult result = spec.Parse(line, caller);
	delete spec;
	char error[GETOPT_LINE_LENGTH];
	result.GetError(error, sizeof(error));
	Assert.Equal(result.Outcome, Getopt_Parsed, "%s '%s' is found: %s", type, value, error);
	result.GetString("value", buffer, maxlength);
	return result;
}

/* Parses "--value VALUE" with an option of type; it must be rejected with
 * the problem. */
void Refuses(const char[] type, const char[] value, const char[] problem, int caller = 0, const char[] filter = "") {
	GetoptSpec spec = SpecOf(type, filter);
	char line[256];
	Format(line, sizeof(line), "--value \"%s\"", value);
	GetoptResult result = spec.Parse(line, caller);
	delete spec;
	Assert.Equal(result.Outcome, Getopt_Rejected, "%s '%s' is refused", type, value);
	char error[GETOPT_LINE_LENGTH];
	result.GetError(error, sizeof(error));
	char expected[GETOPT_LINE_LENGTH];
	Format(expected, sizeof(expected), "option '--value': %s", problem);
	Assert.StrEqual(error, expected);
	delete result;
}

/* -------------------------------------------------------------------- map */

/* smrunner's FindMap knows exact map names only, so finding a map by the
 * start of its name (and the "did you mean" of exact) is not tested here. */
public void Test_MapFindsAMapOfTheServer() {
	char text[64];
	delete Finds("map", "cp_badlands", text, sizeof(text));
	Assert.StrEqual(text, "cp_badlands");
	GetoptResult result = Finds("map", "koth_viaduct", text, sizeof(text));
	Assert.StrEqual(text, "koth_viaduct");
	char display[64];
	Assert.True(result.GetAttribute("value", "display", display, sizeof(display)));
	Assert.StrEqual(display, "koth_viaduct");
	delete result;
}

public void Test_MapRefusesWhatIsNoMap() {
	Refuses("map", "no_such_map", "'no_such_map' is no map on this server");
	Refuses("map", "bad name!", "'bad name!' is no map name");
	Refuses("map", "", "an empty value names nothing");
}

/* ---------------------------------------------------------- cvar and command */

public void Test_CvarAndCommandTakeTheServersSpelling() {
	char text[64];
	delete Finds("cvar", "SM_SHOW_ACTIVITY", text, sizeof(text));
	Assert.StrEqual(text, "sm_show_activity");
	delete Finds("convar", "sm_show_activity", text, sizeof(text));
	delete Finds("command", "SM_TYPES_PROBE", text, sizeof(text));
	Assert.StrEqual(text, "sm_types_probe");
}

public void Test_CvarAndCommandTellOneFromTheOther() {
	Refuses("cvar", "sm_types_probe", "'sm_types_probe' is a command, not a console variable");
	Refuses("command", "sm_show_activity", "'sm_show_activity' is a console variable, not a command");
	Refuses("cvar", "no_such_cvar", "'no_such_cvar' is no console variable");
	Refuses("command", "no_such_command", "'no_such_command' is no console command");
}

Action Command_Probe(int args) {
	return Plugin_Handled;
}

public void OnPluginStart() {
	RegServerCmd("sm_types_probe", Command_Probe);
}

/* ---------------------------------------------------------- player and target */

public void Test_PlayerBecomesTheClientIndex() {
	int alice = Runner.ConnectClient("Alice");
	Runner.ConnectClient("Bob");
	char text[64];
	GetoptResult result = Finds("player", "ali", text, sizeof(text));
	Assert.Equal(result.GetInt("value"), alice);
	char name[64];
	result.GetAttribute("value", "name", name, sizeof(name));
	Assert.StrEqual(name, "Alice");
	char userid[16];
	result.GetAttribute("value", "userid", userid, sizeof(userid));
	Assert.Equal(StringToInt(userid), GetClientUserId(alice));
	delete result;

	Format(text, sizeof(text), "#%d", GetClientUserId(alice));
	result = Finds("player", text, text, sizeof(text));
	Assert.Equal(result.GetInt("value"), alice, "by #userid");
	delete result;
}

public void Test_PlayerRefusesNoneOrSeveral() {
	Runner.ConnectClient("Alice");
	Runner.ConnectClient("Alina");
	Refuses("player", "zed", "'zed' matches no player");
	Refuses("player", "ali", "'ali' matches more than one player");
	Refuses("player", "", "an empty value names nothing");
}

public void Test_TargetFindsGroupsFromTheCallersView() {
	LoadTranslations("common.phrases");
	int alice = Runner.ConnectClient("Alice");
	int bob = Runner.ConnectClient("Bob");
	char text[64];
	GetoptResult result = Finds("target", "@all", text, sizeof(text));
	Assert.StrEqual(text, "@all", "unchanged, as typed");
	int clients[MAXPLAYERS];
	char targetName[MAX_TARGET_LENGTH];
	bool isPhrase;
	int count = Getopt_GetTargets(result, "value", clients, sizeof(clients), targetName, sizeof(targetName), isPhrase);
	Assert.Equal(count, 2);
	Assert.True(isPhrase, "a group's name is a phrase");
	Assert.True((clients[0] == alice && clients[1] == bob) || (clients[0] == bob && clients[1] == alice));
	delete result;

	SetUserFlagBits(bob, ADMFLAG_GENERIC);
	result = Finds("target", "@me", text, sizeof(text), bob);
	count = Getopt_GetTargets(result, "value", clients, sizeof(clients), targetName, sizeof(targetName), isPhrase);
	Assert.Equal(count, 1);
	Assert.Equal(clients[0], bob, "@me is the caller");
	Assert.False(isPhrase);
	Assert.StrEqual(targetName, "Bob");
	delete result;
}

public void Test_TargetFiltersAndImmunityApply() {
	LoadTranslations("common.phrases");
	int alice = Runner.ConnectClient("Alice");
	Runner.SetClientAlive(alice, false);
	int player = Runner.ConnectClient("Player");
	Refuses("target", "Alice", "'Alice' is not alive", 0, "alive");
	Refuses("target", "Alice", "'Alice' cannot be targeted by you", player);
	char text[32];
	delete Finds("target", "Alice", text, sizeof(text), player, "noimmunity");
}

public void Test_FilterKeysAreChecked() {
	g_spec = SpecOf("target", "alive, dead");
	ExpectNativeError(ValidateTheSpec, "filter cannot be both alive and dead");
	delete g_spec;
	g_spec = SpecOf("player", "sleepy");
	ExpectNativeError(ValidateTheSpec, "filter 'sleepy' is none of: alive, dead, connected, nobots, noimmunity");
	delete g_spec;
}

/* --------------------------------------------------------------------- key */

public void Test_KeyTakesTheEnginesSpelling() {
	char text[32];
	delete Finds("key", "mouse1", text, sizeof(text));
	Assert.StrEqual(text, "MOUSE1");
	delete Finds("key", "A", text, sizeof(text));
	Assert.StrEqual(text, "a");
	delete Finds("key", "kp_enter", text, sizeof(text));
	Assert.StrEqual(text, "KP_ENTER");
	Refuses("key", "MOUSE9", "'MOUSE9' is no key name");
}

/* ------------------------------------------------------------------ plugin */

public void Test_PluginFindsALoadedPlugin() {
	char text[64];
	GetoptResult result = Finds("plugin", "getopt-sourcemod-types", text, sizeof(text));
	Assert.StrEqual(text, "getopt-sourcemod-types.smx", "its file below plugins/");
	char version[16];
	result.GetAttribute("value", "version", version, sizeof(version));
	Assert.StrEqual(version, "1.0.0");
	Handle plugin = Getopt_GetPlugin(result, "value");
	Assert.True(plugin == FindPluginByFile("getopt-sourcemod-types.smx"), "Getopt_GetPlugin gives its handle");
	delete result;

	delete Finds("plugin", "Getopt SourceMod types", text, sizeof(text));
	Assert.StrEqual(text, "getopt-sourcemod-types.smx", "by the name in its myinfo");
	Refuses("plugin", "no-such-plugin", "'no-such-plugin' is no loaded plugin");
}

/* ------------------------------------------------------------------ config */

public void Test_ConfigFindsAFileBelowCfg() {
	File file = OpenFile("cfg/getopt-test.cfg", "w");
	file.WriteLine("echo test");
	delete file;
	char text[64];
	delete Finds("config", "getopt-test", text, sizeof(text));
	Assert.StrEqual(text, "getopt-test.cfg", "with .cfg");
	Refuses("config", "missing", "'missing' is no config on this server");
	Refuses("config", "../x", "'../x' is no config name: a path below cfg/ without '..'");
	Refuses("config", "x.txt", "'x.txt' is no config: configs end in .cfg");
}

/* ------------------------------------------------------------------- the help */

public void Test_TheHelpShowsTheTypesLabels() {
	GetoptSpec spec = new GetoptSpec("sm_labels");
	spec.Option("map", "map").Help("A map");
	spec.Option("who", "target").Help("Someone");
	spec.Option("key", "key").Help("A key");
	GetoptResult result = spec.Parse("--help");
	char output[2048];
	for (int i = 0; i < result.OutputLines; i++) {
		char line[GETOPT_LINE_LENGTH];
		result.GetOutputLine(i, line, sizeof(line));
		StrCat(output, sizeof(output), line);
		StrCat(output, sizeof(output), "\n");
	}
	Assert.StrContains(output, "A map [Map]");
	Assert.StrContains(output, "Someone [Target]");
	Assert.StrContains(output, "A key [Key]");
	delete result;
	delete spec;
}
