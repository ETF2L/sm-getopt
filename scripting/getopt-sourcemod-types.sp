/**
 * Copyright 2026 ETF2L Contributors, Dave
 *
 * The getopt-sourcemod-types plugin: option types that know the server,
 * shared with every plugin's specs. map, cvar, command, player, target,
 * key, plugin and config look their values up in what the server has at
 * the moment of the parse, so they work through the resolve step of the
 * getopt extension, with the client the command runs for. Only SourceMod's
 * own natives are used, none of a game.
 */
#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <getopt>

#define GETOPT_SOURCEMOD_TYPES_VERSION "1.0.0"

public Plugin myinfo = {
	name = "Getopt SourceMod types",
	author = "ETF2L Contributors, Dave",
	description = "Option types for maps, cvars, commands, players, targets, keys, plugins and configs",
	version = GETOPT_SOURCEMOD_TYPES_VERSION,
	url = "https://github.com/etf2l/sm-getopt"
};

/* Long enough for every map, cvar, command, plugin and config name. */
#define NAME_LENGTH PLATFORM_MAX_PATH

/* The words of the filter key of player and target options, and the
 * target filter flags they stand for. */
static const char g_FilterWords[][] = {"alive", "dead", "connected", "nobots", "noimmunity"};
static const int g_FilterFlags[] = {
	COMMAND_FILTER_ALIVE, COMMAND_FILTER_DEAD, COMMAND_FILTER_CONNECTED, COMMAND_FILTER_NO_BOTS,
	COMMAND_FILTER_NO_IMMUNITY
};

/* The names the engine's bind command takes for keys and buttons, in the
 * engine's spelling, from the button code table of its input system. */
static const char g_KeyNames[][] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
	"n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
	"KP_INS", "KP_END", "KP_DOWNARROW", "KP_PGDN", "KP_LEFTARROW", "KP_5", "KP_RIGHTARROW", "KP_HOME",
	"KP_UPARROW", "KP_PGUP", "KP_SLASH", "KP_MULTIPLY", "KP_MINUS", "KP_PLUS", "KP_ENTER", "KP_DEL",
	"[", "]", "SEMICOLON", "'", "`", ",", ".", "/", "\\", "-", "=",
	"ENTER", "SPACE", "BACKSPACE", "TAB", "CAPSLOCK", "NUMLOCK", "ESCAPE", "SCROLLLOCK", "INS", "DEL",
	"HOME", "END", "PGUP", "PGDN", "PAUSE", "SHIFT", "RSHIFT", "ALT", "RALT", "CTRL", "RCTRL", "LWIN",
	"RWIN", "APP", "UPARROW", "LEFTARROW", "DOWNARROW", "RIGHTARROW",
	"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
	"CAPSLOCKTOGGLE", "NUMLOCKTOGGLE", "SCROLLLOCKTOGGLE",
	"MOUSE1", "MOUSE2", "MOUSE3", "MOUSE4", "MOUSE5", "MWHEELUP", "MWHEELDOWN",
	"JOY1", "JOY2", "JOY3", "JOY4", "JOY5", "JOY6", "JOY7", "JOY8", "JOY9", "JOY10", "JOY11", "JOY12",
	"JOY13", "JOY14", "JOY15", "JOY16", "JOY17", "JOY18", "JOY19", "JOY20", "JOY21", "JOY22", "JOY23",
	"JOY24", "JOY25", "JOY26", "JOY27", "JOY28", "JOY29", "JOY30", "JOY31", "JOY32",
	"POV_UP", "POV_RIGHT", "POV_DOWN", "POV_LEFT",
	"X AXIS POS", "X AXIS NEG", "Y AXIS POS", "Y AXIS NEG", "Z AXIS POS", "Z AXIS NEG",
	"R AXIS POS", "R AXIS NEG", "U AXIS POS", "U AXIS NEG", "V AXIS POS", "V AXIS NEG",
	"FALCON_1", "FALCON_2", "FALCON_3", "FALCON_4", "FALCON2_1", "FALCON2_2", "FALCON2_3", "FALCON2_4",
	"SC_A", "SC_B", "SC_X", "SC_Y", "SC_DPAD_UP", "SC_DPAD_RIGHT", "SC_DPAD_DOWN", "SC_DPAD_LEFT",
	"SC_LEFT_BUMPER", "SC_RIGHT_BUMPER", "SC_LEFT_TRIGGER", "SC_RIGHT_TRIGGER", "SC_LEFT_GRIP",
	"SC_RIGHT_GRIP", "SC_LEFT_PAD_TOUCH", "SC_RIGHT_PAD_TOUCH", "SC_LEFT_PAD_CLICK", "SC_RIGHT_PAD_CLICK",
	"SC_LPAD_UP", "SC_LPAD_RIGHT", "SC_LPAD_DOWN", "SC_LPAD_LEFT", "SC_RPAD_UP", "SC_RPAD_RIGHT",
	"SC_RPAD_DOWN", "SC_RPAD_LEFT", "SC_SELECT", "SC_START", "SC_STEAM",
	"SC_LPAD_AXIS_RIGHT", "SC_LPAD_AXIS_LEFT", "SC_LPAD_AXIS_DOWN", "SC_LPAD_AXIS_UP",
	"SC_AXIS_L_TRIGGER", "SC_AXIS_R_TRIGGER", "SC_RPAD_AXIS_RIGHT", "SC_RPAD_AXIS_LEFT",
	"SC_RPAD_AXIS_DOWN", "SC_RPAD_AXIS_UP", "SC_GYRO_AXIS_PITCH_POSITIVE", "SC_GYRO_AXIS_PITCH_NEGATIVE",
	"SC_GYRO_AXIS_ROLL_POSITIVE", "SC_GYRO_AXIS_ROLL_NEGATIVE", "SC_GYRO_AXIS_YAW_POSITIVE",
	"SC_GYRO_AXIS_YAW_NEGATIVE", "SC_F1", "SC_F2", "SC_F3", "SC_F4", "SC_F5", "SC_F6", "SC_F7", "SC_F8",
	"SC_F9", "SC_F10", "SC_F11", "SC_F12",
	"A_BUTTON", "B_BUTTON", "X_BUTTON", "Y_BUTTON", "L_SHOULDER", "R_SHOULDER", "BACK", "START", "STICK1",
	"STICK2", "UP", "RIGHT", "DOWN", "LEFT", "S1_RIGHT", "S1_LEFT", "S1_DOWN", "S1_UP", "L_TRIGGER",
	"R_TRIGGER", "S2_RIGHT", "S2_LEFT", "S2_DOWN", "S2_UP"
};

public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int err_max) {
	RegPluginLibrary("getopt-sourcemod-types");
	return APLRes_Success;
}

public void OnPluginStart() {
	Getopt_RegisterType("map", GetoptKind_Value, "exact", true)
		.SetLabel("Map").OnCheck(Map_Check).OnResolve(Map_Resolve);
	Getopt_RegisterType("cvar convar", GetoptKind_Value, "", true)
		.SetLabel("Cvar").OnCheck(Name_Check).OnResolve(Cvar_Resolve);
	Getopt_RegisterType("command", GetoptKind_Value, "", true)
		.SetLabel("Command").OnCheck(Name_Check).OnResolve(Command_Resolve);
	Getopt_RegisterType("player", GetoptKind_Value, "filter", true)
		.SetLabel("Player").OnCheck(Name_Check).OnCheckKeys(Filter_CheckKeys).OnResolve(Player_Resolve);
	Getopt_RegisterType("target", GetoptKind_Value, "filter", true)
		.SetLabel("Target").OnCheck(Name_Check).OnCheckKeys(Filter_CheckKeys).OnResolve(Target_Resolve);
	Getopt_RegisterType("key", GetoptKind_Value, "", true)
		.SetLabel("Key").OnCheck(Key_Check).OnCoerce(Key_Coerce);
	Getopt_RegisterType("plugin", GetoptKind_Value, "", true)
		.SetLabel("Plugin").OnCheck(Name_Check).OnResolve(Plugin_Resolve);
	Getopt_RegisterType("config", GetoptKind_Value, "", true)
		.SetLabel("Config").OnCheck(Config_Check).OnResolve(Config_Resolve);
}

/* ------------------------------------------------------------------ shared */

/* Every type that looks a value up needs some text to look up. */
public bool Name_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	if (value[0] != '\0') {
		return true;
	}
	strcopy(problem, maxlength, "an empty value names nothing");
	return false;
}

/* The longest filter key, and the most words it may have: Filter_CheckKeys
 * refuses longer ones, so everything else reads it whole. */
#define FILTER_LENGTH 256
#define FILTER_WORDS 16

/* The words of a key, separated by spaces or commas, are lower case. */
bool KeyHasWord(GetoptKeys keys, const char[] key, const char[] word) {
	char value[FILTER_LENGTH];
	if (!keys.Get(key, value, sizeof(value))) {
		return false;
	}
	ReplaceString(value, sizeof(value), ",", " ");
	char words[FILTER_WORDS][32];
	int count = ExplodeString(value, " ", words, sizeof(words), sizeof(words[]));
	for (int i = 0; i < count; i++) {
		if (StrEqual(words[i], word)) {
			return true;
		}
	}
	return false;
}

bool KeyIsTrue(GetoptKeys keys, const char[] key) {
	char value[16];
	return keys.Get(key, value, sizeof(value)) && value[0] != '\0' && !StrEqual(value, "0");
}

/* --------------------------------------------------------------------- map */

/* What a map's name can be made of, workshop maps' paths included; the
 * engine finds nothing else, and complains in the console about it. */
public bool Map_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	if (!Name_Check(keys, value, problem, maxlength)) {
		return false;
	}
	for (int i = 0; value[i] != '\0'; i++) {
		if (!IsCharAlpha(value[i]) && !IsCharNumeric(value[i]) && FindCharInString("_-./", value[i]) == -1) {
			Format(problem, maxlength, "'%s' is no map name", value);
			return false;
		}
	}
	return true;
}

/* A map the server can change to, as FindMap and SourceMod's sm_map find
 * it: a partial name finds the map it is the start of, unless the option
 * is exact; a workshop map is named by its full name. */
public bool Map_Resolve(GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem, int maxlength) {
	char found[NAME_LENGTH];
	FindMapResult result = FindMap(value, found, sizeof(found));
	if (result == FindMap_NotFound) {
		Format(problem, maxlength, "'%s' is no map on this server", value);
		return false;
	}
	if (result == FindMap_FuzzyMatch && KeyIsTrue(keys, "exact")) {
		Format(problem, maxlength, "'%s' is no map on this server (did you mean '%s'?)", value, found);
		return false;
	}

	resolved.SetValue(found);
	char display[NAME_LENGTH];
	if (GetMapDisplayName(found, display, sizeof(display))) {
		resolved.SetAttribute("display", display);
	}
	return true;
}

/* -------------------------------------------------------- cvar and command */

/* The name of the console variable or command called value, in its own
 * spelling, which may differ in case. */
bool FindConsoleName(const char[] value, bool wantCommand, char[] name, int maxlength, bool &otherKind) {
	otherKind = false;
	char candidate[NAME_LENGTH];
	bool isCommand;
	Handle iterator = FindFirstConCommand(candidate, sizeof(candidate), isCommand);
	if (iterator == null) {
		return false;
	}

	bool found;
	do {
		if (!StrEqual(candidate, value, false)) {
			continue;
		}
		if (isCommand != wantCommand) {
			otherKind = true;
			continue;
		}
		strcopy(name, maxlength, candidate);
		found = true;
		break;
	} while (FindNextConCommand(iterator, candidate, sizeof(candidate), isCommand));
	delete iterator;
	return found;
}

public bool Cvar_Resolve(GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem, int maxlength) {
	char name[NAME_LENGTH];
	bool isCommand;
	if (FindConsoleName(value, false, name, sizeof(name), isCommand)) {
		resolved.SetValue(name);
		return true;
	}
	if (isCommand) {
		Format(problem, maxlength, "'%s' is a command, not a console variable", value);
	} else {
		Format(problem, maxlength, "'%s' is no console variable", value);
	}
	return false;
}

public bool Command_Resolve(GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem,
                            int maxlength) {
	char name[NAME_LENGTH];
	bool isCvar;
	if (FindConsoleName(value, true, name, sizeof(name), isCvar)) {
		resolved.SetValue(name);
		return true;
	}
	if (isCvar) {
		Format(problem, maxlength, "'%s' is a console variable, not a command", value);
	} else {
		Format(problem, maxlength, "'%s' is no console command", value);
	}
	return false;
}

/* ------------------------------------------------------ player and target */

public bool Filter_CheckKeys(GetoptKeys keys, char[] problem, int maxlength) {
	char value[FILTER_LENGTH];
	if (!keys.Get("filter", value, sizeof(value))) {
		return true;
	}

	/* A filter that does not fit would be cut without a word. */
	ReplaceString(value, sizeof(value), ",", " ");
	char words[FILTER_WORDS][32];
	int count = ExplodeString(value, " ", words, sizeof(words), sizeof(words[]), true);
	if (strlen(value) >= sizeof(value) - 1 || count >= sizeof(words)) {
		strcopy(problem, maxlength, "filter is too long: it takes each of its five words once at most");
		return false;
	}
	for (int i = 0; i < count; i++) {
		if (words[i][0] != '\0' && FilterFlagOf(words[i]) == 0) {
			Format(problem, maxlength, "filter '%s' is none of: alive, dead, connected, nobots, noimmunity", words[i]);
			return false;
		}
	}
	if (KeyHasWord(keys, "filter", "alive") && KeyHasWord(keys, "filter", "dead")) {
		strcopy(problem, maxlength, "filter cannot be both alive and dead");
		return false;
	}
	return true;
}

int FilterFlagOf(const char[] word) {
	for (int i = 0; i < sizeof(g_FilterWords); i++) {
		if (StrEqual(word, g_FilterWords[i])) {
			return g_FilterFlags[i];
		}
	}
	return 0;
}

int FilterFlags(GetoptKeys keys) {
	int flags;
	for (int i = 0; i < sizeof(g_FilterWords); i++) {
		if (KeyHasWord(keys, "filter", g_FilterWords[i])) {
			flags |= g_FilterFlags[i];
		}
	}
	return flags;
}

/* What ProcessTargetString's reason for finding nobody means. */
void TargetProblem(int reason, const char[] value, char[] problem, int maxlength) {
	switch (reason) {
		case COMMAND_TARGET_NOT_ALIVE: {
			Format(problem, maxlength, "'%s' is not alive", value);
		}
		case COMMAND_TARGET_NOT_DEAD: {
			Format(problem, maxlength, "'%s' is not dead", value);
		}
		case COMMAND_TARGET_NOT_IN_GAME: {
			Format(problem, maxlength, "'%s' is not in the game yet", value);
		}
		case COMMAND_TARGET_IMMUNE: {
			Format(problem, maxlength, "'%s' cannot be targeted by you", value);
		}
		case COMMAND_TARGET_EMPTY_FILTER: {
			Format(problem, maxlength, "'%s' matches no player right now", value);
		}
		case COMMAND_TARGET_NOT_HUMAN: {
			Format(problem, maxlength, "'%s' is a bot", value);
		}
		case COMMAND_TARGET_AMBIGUOUS: {
			Format(problem, maxlength, "'%s' matches more than one player", value);
		}
		default: {
			Format(problem, maxlength, "'%s' matches no player", value);
		}
	}
}

/* One player, from the view of the client the command runs for: its
 * immunity counts and @me is that client. The value becomes the player's
 * client index. */
public bool Player_Resolve(GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem,
                           int maxlength) {
	int targets[1];
	char name[MAX_TARGET_LENGTH];
	bool isPhrase;
	int count = ProcessTargetString(value, keys.Caller, targets, sizeof(targets),
	                                FilterFlags(keys) | COMMAND_FILTER_NO_MULTI,
	                                name, sizeof(name), isPhrase);
	if (count <= 0) {
		TargetProblem(count, value, problem, maxlength);
		return false;
	}

	char text[16];
	IntToString(targets[0], text, sizeof(text));
	resolved.SetValue(text);
	IntToString(GetClientUserId(targets[0]), text, sizeof(text));
	resolved.SetAttribute("userid", text);
	GetClientName(targets[0], name, sizeof(name));
	resolved.SetAttribute("name", name);
	return true;
}

/* Any number of players. The value stays as typed; the players are its
 * attributes, with the name ShowActivity and ReplyToCommand take. */
public bool Target_Resolve(GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem,
                           int maxlength) {
	int targets[MAXPLAYERS];
	char name[MAX_TARGET_LENGTH];
	bool isPhrase;
	int count = ProcessTargetString(value, keys.Caller, targets, sizeof(targets), FilterFlags(keys), name, sizeof(name),
	                                isPhrase);
	if (count <= 0) {
		TargetProblem(count, value, problem, maxlength);
		return false;
	}

	char clients[MAXPLAYERS * 4], number[8];
	for (int i = 0; i < count; i++) {
		IntToString(targets[i], number, sizeof(number));
		if (i > 0) {
			StrCat(clients, sizeof(clients), " ");
		}
		StrCat(clients, sizeof(clients), number);
	}
	resolved.SetAttribute("clients", clients);
	IntToString(count, number, sizeof(number));
	resolved.SetAttribute("count", number);
	resolved.SetAttribute("name", name);
	resolved.SetAttribute("ml", isPhrase ? "1" : "0");
	return true;
}

/* --------------------------------------------------------------------- key */

int KeyIndexOf(const char[] value) {
	for (int i = 0; i < sizeof(g_KeyNames); i++) {
		if (StrEqual(g_KeyNames[i], value, false)) {
			return i;
		}
	}
	return -1;
}

public bool Key_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	if (KeyIndexOf(value) != -1) {
		return true;
	}
	Format(problem, maxlength, "'%s' is no key name", value);
	return false;
}

/* bind takes key names in any case; the value is the engine's spelling. */
public void Key_Coerce(GetoptKeys keys, const char[] value, char[] coerced, int maxlength) {
	strcopy(coerced, maxlength, g_KeyNames[KeyIndexOf(value)]);
}

/* ------------------------------------------------------------------ plugin */

/* Whether a plugin's file is value: the path below plugins/, or its last
 * part, with or without .smx. */
bool FileMatches(const char[] file, const char[] value, bool lastPartOnly) {
	int start = 0;
	if (lastPartOnly) {
		start = FindCharInString(file, '/', true) + 1;
	}

	if (StrEqual(file[start], value, false)) {
		return true;
	}
	char withExtension[NAME_LENGTH];
	Format(withExtension, sizeof(withExtension), "%s.smx", value);
	return StrEqual(file[start], withExtension, false);
}

/* How a value can name a plugin, from the most exact to the least. */
enum PluginMatch {
	PluginMatch_Path = 0,
	PluginMatch_File,
	PluginMatch_Name,
	PluginMatch_Count
};

bool PluginMatches(Handle plugin, const char[] value, PluginMatch how) {
	/* SourceMod also lists plugins that failed to load. */
	PluginStatus status = GetPluginStatus(plugin);
	if (status != Plugin_Running && status != Plugin_Paused) {
		return false;
	}

	char text[NAME_LENGTH];
	if (how == PluginMatch_Name) {
		return GetPluginInfo(plugin, PlInfo_Name, text, sizeof(text)) && StrEqual(text, value, false);
	}

	GetPluginFilename(plugin, text, sizeof(text));
	return FileMatches(text, value, how == PluginMatch_File);
}

/* A loaded plugin, by its file below plugins/ (with or without .smx), by
 * the last part of that path, or by the name in its myinfo: the first of
 * these that finds exactly one plugin. The value becomes the file. */
public bool Plugin_Resolve(GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem,
                           int maxlength) {
	for (PluginMatch how = PluginMatch_Path; how < PluginMatch_Count; how++) {
		ArrayList files = new ArrayList(ByteCountToCells(NAME_LENGTH));

		Handle iterator = GetPluginIterator();
		Handle found = null;
		char file[NAME_LENGTH];
		while (MorePlugins(iterator)) {
			Handle plugin = ReadPlugin(iterator);
			if (!PluginMatches(plugin, value, how)) {
				continue;
			}
			found = plugin;
			GetPluginFilename(plugin, file, sizeof(file));
			files.PushString(file);
		}
		delete iterator;

		int count = files.Length;
		if (count > 1) {
			SeveralPluginsProblem(files, value, problem, maxlength);
		}
		delete files;
		if (count > 1) {
			return false;
		}
		if (count == 1) {
			SetPluginValue(found, file, resolved);
			return true;
		}
	}

	Format(problem, maxlength, "'%s' is no loaded plugin", value);
	return false;
}

/* The files in alphabetical order, so the message does not depend on the
 * order the plugins were loaded in. */
void SeveralPluginsProblem(ArrayList files, const char[] value, char[] problem, int maxlength) {
	files.Sort(Sort_Ascending, Sort_String);
	Format(problem, maxlength, "'%s' matches several plugins: ", value);
	for (int i = 0; i < files.Length; i++) {
		char file[NAME_LENGTH];
		files.GetString(i, file, sizeof(file));
		Format(problem, maxlength, "%s%s%s", problem, i > 0 ? ", " : "", file);
	}
}

void SetPluginValue(Handle plugin, const char[] file, GetoptResolved resolved) {
	resolved.SetValue(file);
	char text[NAME_LENGTH];
	if (GetPluginInfo(plugin, PlInfo_Name, text, sizeof(text))) {
		resolved.SetAttribute("name", text);
	}
	if (GetPluginInfo(plugin, PlInfo_Version, text, sizeof(text))) {
		resolved.SetAttribute("version", text);
	}
}

/* ------------------------------------------------------------------ config */

/* What exec takes: a path below cfg/, without .. and without a drive or a
 * leading slash, ending in .cfg or in nothing, which exec reads as .cfg. */
public bool Config_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	bool isPath = value[0] != '\0' && value[0] != '/' && FindCharInString(value, '\\') == -1
		&& FindCharInString(value, ':') == -1 && !IsDotDotIn(value);
	if (!isPath) {
		Format(problem, maxlength, "'%s' is no config name: a path below cfg/ without '..'", value);
		return false;
	}

	char extension[NAME_LENGTH];
	if (ExtensionOf(value, extension, sizeof(extension)) && !StrEqual(extension, "cfg", false)) {
		Format(problem, maxlength, "'%s' is no config: configs end in .cfg", value);
		return false;
	}
	return true;
}

/* Whether a part of the path is "..", however many parts it has. */
bool IsDotDotIn(const char[] path) {
	int length = strlen(path);
	for (int start = 0; start <= length;) {
		int end = FindCharInString(path[start], '/');
		int partLength = end == -1 ? length - start : end;
		if (partLength == 2 && path[start] == '.' && path[start + 1] == '.') {
			return true;
		}
		if (end == -1) {
			return false;
		}
		start += end + 1;
	}
	return false;
}

/* The extension of the last part of a path, without the dot. */
bool ExtensionOf(const char[] path, char[] extension, int maxlength) {
	int dot = FindCharInString(path, '.', true);
	int slash = FindCharInString(path, '/', true);
	if (dot == -1 || dot < slash) {
		return false;
	}
	strcopy(extension, maxlength, path[dot + 1]);
	return true;
}

/* A config that exists in the game's file system, custom folders and VPKs
 * included. The value becomes the name with .cfg. */
public bool Config_Resolve(GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem,
                           int maxlength) {
	char name[NAME_LENGTH];
	strcopy(name, sizeof(name), value);
	char extension[8];
	if (!ExtensionOf(name, extension, sizeof(extension))) {
		StrCat(name, sizeof(name), ".cfg");
	}

	char path[NAME_LENGTH];
	Format(path, sizeof(path), "cfg/%s", name);
	if (!FileExists(path, true)) {
		Format(problem, maxlength, "'%s' is no config on this server", value);
		return false;
	}
	resolved.SetValue(name);
	return true;
}
