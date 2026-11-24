# Getopt examples

Complete plugins, one feature each. Every output on this page is what the plugin really prints. The
[reference](reference.md) explains each method in full.

- [A first command](#a-first-command): options, an arg, `--help`, errors
- [Lists](#lists): `multiple`, `csv`, an arg that takes many words
- [Mappings](#mappings): `hash`, `objectlist`
- [Commands](#commands): modes such as `add` and `remove`, inherited options
- [Allowed values and conversion](#allowed-values-and-conversion): `Valid`, `ValidFrom`, `LazyValid`, `ProcessValue`
- [Dates and durations](#dates-and-durations): `tomorrow 8pm`, `90 minutes`, bounds, time zones
- [A tidy help](#a-tidy-help): groups, hidden options, type hints, examples, the version
- [Parse a line yourself](#parse-a-line-yourself): `Parse`, `GetError`, your own `-h`
- [A type of your own](#a-type-of-your-own): check, convert, keys, notes
- [Players, maps and configs](#players-maps-and-configs): the SourceMod types

## A first command

```sourcepawn
#include <sourcemod>
#include <getopt>

GetoptSpec g_Backup;

public void OnPluginStart()
{
	g_Backup = new GetoptSpec("sm_backup");
	g_Backup.Description("Copy a map's files to the backup folder.");
	g_Backup.Option("keep|k", "int").Default("7").Min(1).Help("Number of backups to keep");
	g_Backup.Option("compress", "bool").Default("1").Help("Compress the backup");
	g_Backup.Option("verbose|v", "counter").Help("Print more; repeat for more (-vv)");
	g_Backup.Option("dry-run|n").Help("Only show what would be copied");
	g_Backup.Arg("map").Required().Help("Map to back up");
	g_Backup.Validate();

	RegAdminCmd("sm_backup", Command_Backup, ADMFLAG_ROOT, "Back up a map's files");
}

Action Command_Backup(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Backup, client);
	if (opt == null)
		return Plugin_Handled;      // bad command line, --help or --version: already answered

	char map[PLATFORM_MAX_PATH];
	opt.GetString("map", map, sizeof(map));
	int keep = opt.GetInt("keep");              // 7 unless given
	bool compress = opt.GetBool("compress");    // true unless --no-compress
	int verbosity = opt.GetInt("verbose");      // 0 unless given
	bool dryRun = opt.GetBool("dry-run");       // false unless given
	delete opt;

	ReplyToCommand(client, "map=%s keep=%d compress=%d verbosity=%d dry-run=%d",
	               map, keep, compress, verbosity, dryRun);
	return Plugin_Handled;
}
```

| Command | Prints |
|---|---|
| `sm_backup cp_badlands` | `map=cp_badlands keep=7 compress=1 verbosity=0 dry-run=0` |
| `sm_backup -k 3 -vv --no-compress cp_badlands` | `map=cp_badlands keep=3 compress=0 verbosity=2 dry-run=0` |
| `sm_backup -nvk3 cp_badlands` | `map=cp_badlands keep=3 compress=1 verbosity=1 dry-run=1` |
| `sm_backup --version` | `sm_backup unknown` (the plugin has no `myinfo` version) |

`-nvk3` bundles three short options: `-n`, `-v`, and `-k` with the value `3`.

`sm_backup --help`:

```
# sm_backup [options] map
# Copy a map's files to the backup folder.

## Arguments
   <map>               [REQ] Map to back up

## Options
   --[no-]compress     Compress the backup
                           Default = 1
   --dry-run, -n       Only show what would be copied
   --keep, -k <>       Number of backups to keep
                           Default = 7
   --verbose, -v       Print more; repeat for more (-vv)
```

Wrong command lines. Each error is followed by an empty line and the help above:

| Command | Error |
|---|---|
| `sm_backup --keep 0 cp_badlands` | `ERROR: option '--keep': 0 is smaller than the minimum of 1` |
| `sm_backup` | `ERROR: missing required argument <map>` |
| `sm_backup --kep 3 cp_badlands` | `ERROR: Unknown option: kep` |
| `sm_backup -k three cp_badlands` | `ERROR: option '--keep': 'three' is not an integer` |
| `sm_backup cp_badlands cp_process` | `ERROR: unexpected extra argument 'cp_process'` |

The command works the same from the server console, over RCON, from a player's console and from chat
(`!backup -k 3 cp_badlands`). In chat, an error line also goes to chat.

## Lists

```sourcepawn
#include <sourcemod>
#include <getopt>

GetoptSpec g_Kick;

public void OnPluginStart()
{
	g_Kick = new GetoptSpec("sm_kickmany");
	g_Kick.Option("reason|r", "string").Multiple().Help("A reason; repeat for more");
	g_Kick.Option("tag|t", "string").Multiple().Csv().Default("cheating").Help("Tags for the log");
	g_Kick.Arg("userid", "int").Required().Multiple().Help("User ids to kick");
	g_Kick.Validate();

	RegAdminCmd("sm_kickmany", Command_Kick, ADMFLAG_KICK);
}

Action Command_Kick(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Kick, client);
	if (opt == null)
		return Plugin_Handled;

	char text[64];
	for (int i = 0; i < opt.Count("reason"); i++)
	{
		opt.GetStringAt("reason", i, text, sizeof(text));
		ReplyToCommand(client, "reason %d: %s", i, text);
	}
	for (int i = 0; i < opt.Count("tag"); i++)
	{
		opt.GetStringAt("tag", i, text, sizeof(text));
		ReplyToCommand(client, "tag %d: %s", i, text);
	}
	for (int i = 0; i < opt.Count("userid"); i++)
		ReplyToCommand(client, "userid %d: %d", i, opt.GetIntAt("userid", i));

	delete opt;
	return Plugin_Handled;
}
```

`sm_kickmany 12 15`:

```
tag 0: cheating
userid 0: 12
userid 1: 15
```

`sm_kickmany -r "mic spam" -r afk -t spam,afk -t "toxic, " 12 15 17`:

```
reason 0: mic spam
reason 1: afk
tag 0: spam
tag 1: afk
tag 2: toxic
userid 0: 12
userid 1: 15
userid 2: 17
```

- The command line replaced the default `cheating`. Lists are never merged with their default. A list default is one
  `Default` call per value.
- `csv` split `spam,afk` into two values. It dropped the space and the trailing comma of `"toxic, "`.

`sm_kickmany -t spam,,afk 12`: `ERROR: option '--tag': 'spam,,afk' contains an empty item`.

The help shows the multiple arg with `...` and the csv option with `<a,b,...>`:

```
# sm_kickmany [options] userid...

## Arguments
   <userid>                [REQ] User ids to kick

## Options
   --reason, -r <>         A reason; repeat for more
   --tag, -t <a,b,...>     Tags for the log
                               Default = cheating
```

## Mappings

```sourcepawn
#include <sourcemod>
#include <getopt>

GetoptSpec g_Match;

public void OnPluginStart()
{
	g_Match = new GetoptSpec("sm_match");
	g_Match.Option("cvar|c", "string").Hash().DefaultPair("mp_timelimit", "30").Help("Cvars to set, NAME=VALUE");
	g_Match.Option("team", "string").ObjectList().Help("Teams, N.FIELD=VALUE");
	g_Match.Validate();

	RegAdminCmd("sm_match", Command_Match, ADMFLAG_CONFIG);
}

Action Command_Match(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Match, client);
	if (opt == null)
		return Plugin_Handled;

	char key[64], value[64];
	for (int i = 0; i < opt.Count("cvar"); i++)
	{
		opt.GetKey("cvar", i, key, sizeof(key));           // keys come sorted
		opt.GetStringOf("cvar", key, value, sizeof(value));
		ReplyToCommand(client, "cvar %s = %s", key, value);
	}

	for (int team = 0; team < opt.Count("team"); team++)
	{
		char name[64] = "(no name)";
		opt.GetStringIn("team", team, "name", name, sizeof(name));
		int seed = opt.GetIntIn("team", team, "seed", -1);  // -1 when not given
		ReplyToCommand(client, "team %d: name=%s seed=%d fields=%d", team, name, seed, opt.FieldCount("team", team));
	}

	delete opt;
	return Plugin_Handled;
}
```

| Command | Prints |
|---|---|
| `sm_match` | `cvar mp_timelimit = 30` |
| `sm_match -c mp_winlimit=5` | `cvar mp_winlimit = 5` (the default mapping is replaced as a whole) |
| `sm_match -c mp_winlimit=5 -c mp_timelimit=0` | `cvar mp_timelimit = 0`, `cvar mp_winlimit = 5` |

`sm_match --team 0.name=Red --team 0.seed=1 --team 1.name=Blu`:

```
cvar mp_timelimit = 30
team 0: name=Red seed=1 fields=2
team 1: name=Blu seed=-1 fields=1
```

| Command | Error |
|---|---|
| `sm_match --team 1.name=Blu` | `ERROR: option '--team': missing index 0` |
| `sm_match -c mp_winlimit` | `ERROR: Option c, key "mp_winlimit", requires a value` |
| `sm_match --team 00.name=x` | `ERROR: option '--team': invalid key '00.name', the index must not have leading zeros` |

The help:

```
# sm_match [options]

## Options
   --cvar, -c <key=value>     Cvars to set, NAME=VALUE
                                  Default = mp_timelimit=30
   --team <N.key=value>       Teams, N.FIELD=VALUE
```

## Commands

```sourcepawn
#include <sourcemod>
#include <getopt>

GetoptSpec g_Whitelist;

public void OnPluginStart()
{
	g_Whitelist = new GetoptSpec("sm_whitelist");
	g_Whitelist.Description("Manage the server's whitelist.");
	g_Whitelist.Option("verbose|v", "counter").Inherit().Help("Print more");

	GetoptLevel add = g_Whitelist.Command("add").Description("Add a player");
	add.Option("note", "string").Help("Why they are on the list");
	add.Arg("steamid").Required().Help("SteamID of the player");

	g_Whitelist.Command("remove").Description("Remove a player")
		.Arg("steamid").Required().Help("SteamID of the player");

	g_Whitelist.Command("list").Description("List all players");

	g_Whitelist.Validate();
	RegAdminCmd("sm_whitelist", Command_Whitelist, ADMFLAG_BAN);
}

Action Command_Whitelist(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Whitelist, client);
	if (opt == null)
		return Plugin_Handled;

	char command[16];
	opt.GetCommand(command, sizeof(command));
	int verbosity = opt.GetInt("verbose");    // inherited: read on the top level

	GetoptResult sub = opt.Subcommand;        // the values of "add", "remove" or "list"
	char steamId[32], note[64];
	if (StrEqual(command, "add"))
	{
		sub.GetString("steamid", steamId, sizeof(steamId));
		sub.GetString("note", note, sizeof(note));
		ReplyToCommand(client, "add %s (note '%s', verbosity %d)", steamId, note, verbosity);
	}
	else if (StrEqual(command, "remove"))
	{
		sub.GetString("steamid", steamId, sizeof(steamId));
		ReplyToCommand(client, "remove %s (verbosity %d)", steamId, verbosity);
	}
	else
	{
		ReplyToCommand(client, "list (verbosity %d)", verbosity);
	}

	delete sub;
	delete opt;
	return Plugin_Handled;
}
```

| Command | Prints |
|---|---|
| `sm_whitelist add STEAM_0:1:23 --note "clan member"` | `add STEAM_0:1:23 (note 'clan member', verbosity 0)` |
| `sm_whitelist -v list -v` | `list (verbosity 2)` |

- `STEAM_0:1:23` stays one word. The extension does not split at colons.
- `--verbose` is inherited, so it works before and after the command word. Its value belongs to the top level.

| Command | Error |
|---|---|
| `sm_whitelist` | `ERROR: missing command, expected one of: add, list, remove` |
| `sm_whitelist ban x` | `ERROR: unknown command 'ban', expected one of: add, list, remove` |
| `sm_whitelist list --note x` | `ERROR: Unknown option: note` (`--note` belongs to `add`) |

`sm_whitelist --help` shows the top level:

```
# sm_whitelist [options] <command>
# Manage the server's whitelist.

## Options
   --verbose, -v     Print more

## Commands
   add               Add a player
   list              List all players
   remove            Remove a player
```

`sm_whitelist add --help` shows the `add` level:

```
# sm_whitelist add [options] steamid
# Add a player

## Arguments
   <steamid>         [REQ] SteamID of the player

## Options
   --note <>         Why they are on the list
   --verbose, -v     Print more
```

## Allowed values and conversion

```sourcepawn
#include <sourcemod>
#include <getopt>

GetoptSpec g_Log;

public void OnPluginStart()
{
	g_Log = new GetoptSpec("sm_logto");
	g_Log.Option("level|l", "string").Default("info").Valid("debug", "info", "warn", "error")
		.Help("How much to log");
	g_Log.Option("file", "string").ValidFrom(Files_Valid).Help("A log file in logs/");
	g_Log.Option("prefix", "string").LazyValid(Prefix_IsValid).Help("Text before every line, up to 8 characters");
	g_Log.Option("minutes", "int").Min(1).ProcessValue(Minutes_ToSeconds).Help("For how long");
	g_Log.Validate();

	RegAdminCmd("sm_logto", Command_Log, ADMFLAG_ROOT);
}

// Asked once per parse that gives --file: the values allowed right now.
void Files_Valid(GetoptList values, const char[] option, any data)
{
	values.Add("errors.log");
	values.Add("chat.log");
}

// A yes/no check for what a list cannot say.
bool Prefix_IsValid(const char[] value, const char[] option, any data)
{
	return strlen(value) <= 8;
}

// Turns the checked value into what the result returns.
void Minutes_ToSeconds(GetoptResult values, const char[] value, char[] processed, int maxlength,
                       const char[] name, any data)
{
	Format(processed, maxlength, "%d", StringToInt(value) * 60);
}

Action Command_Log(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Log, client);
	if (opt == null)
		return Plugin_Handled;

	char level[16], file[64], prefix[16];
	opt.GetString("level", level, sizeof(level));
	opt.GetString("file", file, sizeof(file));
	opt.GetString("prefix", prefix, sizeof(prefix));
	ReplyToCommand(client, "level=%s file=%s prefix=%s seconds=%d", level, file, prefix, opt.GetInt("minutes"));
	delete opt;
	return Plugin_Handled;
}
```

`sm_logto --level warn --file chat.log --prefix "[x]" --minutes 5`:

```
level=warn file=chat.log prefix=[x] seconds=300
```

| Command | Error |
|---|---|
| `sm_logto -l verbose` | `ERROR: option '--level': 'verbose' is not one of: debug, info, warn, error` |
| `sm_logto --file other.log` | `ERROR: option '--file': 'other.log' is not one of: errors.log, chat.log` |
| `sm_logto --prefix "much too long"` | `ERROR: option '--prefix': 'much too long' is not a valid value` |

Only the fixed list shows in the help:

```
# sm_logto [options]

## Options
   --file <>          A log file in logs/
   --level, -l <>     How much to log
                          Valid   = [ debug, info, warn, error ]
                          Default = info
   --minutes <>       For how long
   --prefix <>        Text before every line, up to 8 characters
```

`LazyValid` cannot say why it refuses. For your own message, [register a type](#a-type-of-your-own).

## Dates and durations

```sourcepawn
#include <sourcemod>
#include <getopt>

GetoptSpec g_Event;

public void OnPluginStart()
{
	g_Event = new GetoptSpec("sm_event");
	g_Event.Option("at", "date").Required().TimeZone("Europe/Berlin").Set("min", "now")
		.Help("When the event starts");
	g_Event.Option("for", "duration").Default("2 hours").Set("max", "1 day").Help("How long it lasts");
	g_Event.Validate();

	RegAdminCmd("sm_event", Command_Event, ADMFLAG_ROOT);
}

Action Command_Event(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Event, client);
	if (opt == null)
		return Plugin_Handled;

	char when[32], length[32];
	opt.GetString("at", when, sizeof(when));        // "2026-11-25T20:00:00", in Europe/Berlin
	int start = opt.GetInt("at");                   // Unix timestamp
	opt.GetString("for", length, sizeof(length));   // "2 hours"
	int seconds = opt.GetInt("for");               // 7200

	int months, days, rest;
	opt.GetDuration("for", months, days, rest);    // 0, 0, 7200

	ReplyToCommand(client, "at %s (%d), for %s (%d s; %d months %d days %d s)",
	               when, start, length, seconds, months, days, rest);
	delete opt;
	return Plugin_Handled;
}
```

Run on 24 November 2026:

| Command | Prints |
|---|---|
| `sm_event --at "tomorrow 8pm"` | `at 2026-11-25T20:00:00 (1795633200), for 2 hours (7200 s; 0 months 0 days 7200 s)` |
| `sm_event --at "2026-12-24 18:00" --for "90 minutes"` | `at 2026-12-24T18:00:00 (1798131600), for 90 minutes (5400 s; 0 months 0 days 5400 s)` |
| `sm_event --at "next friday 8pm" --for "1 day"` | `at 2026-12-04T20:00:00 (1796410800), for 1 day (86400 s; 0 months 1 days 0 s)` |

- Quote dates and durations with spaces. Unquoted, `tomorrow` and `8pm` are two words.
- `1 day` is kept as 1 day, not 86400 seconds: across a daylight saving change, a day is not 86400 seconds.
- `Set("min", "now")` is read again at every parse.

| Command | Error |
|---|---|
| `sm_event --at yesterday` | `ERROR: option '--at': 'yesterday' is earlier than the minimum of 2026-11-24T01:44:22` |
| `sm_event --at "tomorrow 8pm" --for "2 days"` | `ERROR: option '--for': '2 days' is longer than the maximum of 1 day` |
| `sm_event --at "8pm tmrw"` | `ERROR: option '--at': '8pm tmrw' is not a date` |
| `sm_event --at tomorrow --for 2h` | `ERROR: option '--for': '2h' is not a duration` |

The minimum in the first error is the moment of the parse, in the option's time zone. [dates.md](dates.md) lists
everything the two types understand.

## A tidy help

```sourcepawn
#include <sourcemod>
#include <getopt>

public Plugin myinfo = { name = "Mirror", version = "1.4.0" };

GetoptSpec g_Mirror;

public void OnPluginStart()
{
	g_Mirror = new GetoptSpec("sm_mirror");
	g_Mirror.Description("Copy demos to a web server.");
	g_Mirror.Option("host", "string").Required().Group("Target").TypeHint("Hostname").Help("Where to copy to");
	g_Mirror.Option("port", "int").Default("443").Min(1).Max(65535).Group("Target").Help("Its port");
	g_Mirror.Option("limit", "int").Default("10").Group("Selection").Help("At most this many demos");
	g_Mirror.Option("debug").Hidden().Help("Not listed in the help");
	g_Mirror.Arg("demo").Multiple().Help("Demos to copy; all without");
	g_Mirror.Example("Copy two demos", "--host demos.example.com a.dem b.dem");
	g_Mirror.Example("Copy the newest five", "--host demos.example.com --limit 5");
	g_Mirror.Validate();

	RegAdminCmd("sm_mirror", Command_Mirror, ADMFLAG_ROOT);
}

Action Command_Mirror(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Mirror, client);
	if (opt == null)
		return Plugin_Handled;

	ReplyToCommand(client, "ok, %d demos", opt.Count("demo"));
	delete opt;
	return Plugin_Handled;
}
```

`sm_mirror --help`:

```
# sm_mirror [options] [demo...]
# Copy demos to a web server.

## Arguments
   <demo>         Demos to copy; all without

## Selection
   --limit <>     At most this many demos
                      Default = 10

## Target
   --host <>      [REQ] Where to copy to [Hostname]
   --port <>      Its port
                      Default = 443

# Examples:
## Copy two demos
##   sm_mirror --host demos.example.com a.dem b.dem

## Copy the newest five
##   sm_mirror --host demos.example.com --limit 5
```

- Groups are sorted by byte value. Options without a group go under `Options`.
- `--debug` works, but is not listed.
- `--help` answers although the required `--host` is missing.

`sm_mirror --version` prints `sm_mirror 1.4.0`, the version of `myinfo`. `Version("...")` on the spec overrides it.

## Parse a line yourself

`spec.Parse` takes any argument string and never prints. Use it for lines that do not come from a command, or to
handle the output yourself. This plugin logs errors instead of showing them, and adds `-h` as a short form of
`--help`.

```sourcepawn
#include <sourcemod>
#include <getopt>

GetoptSpec g_Spawn;

public void OnPluginStart()
{
	g_Spawn = new GetoptSpec("spawn");
	g_Spawn.Option("h").Hidden();                   // -h as a short form of --help
	g_Spawn.Option("team", "string").Valid("red", "blu").Default("red");
	g_Spawn.Option("health", "int").Min(1).Default("150");
	g_Spawn.Validate();

	RegAdminCmd("sm_spawnline", Command_SpawnLine, ADMFLAG_ROOT);
}

Action Command_SpawnLine(int client, int args)
{
	char line[GETOPT_LINE_LENGTH];
	GetCmdArgString(line, sizeof(line));

	GetoptResult opt = g_Spawn.Parse(line, client); // the caller: @me and immunity are from its view
	switch (opt.Outcome)
	{
		case Getopt_Rejected:
		{
			char error[GETOPT_LINE_LENGTH];
			opt.GetError(error, sizeof(error));     // the error alone, without the help
			LogError("Bad spawn line '%s': %s", line, error);
		}
		case Getopt_Answered:
		{
			PrintToServer("(--help or --version: %d lines)", opt.OutputLines);
		}
		case Getopt_Parsed:
		{
			if (opt.GetBool("h"))
				Getopt_PrintHelp(opt, client);      // our -h
			else
			{
				char team[8];
				opt.GetString("team", team, sizeof(team));
				PrintToServer("team=%s health=%d", team, opt.GetInt("health"));
			}
		}
	}
	delete opt;
	return Plugin_Handled;
}
```

| Command | Result |
|---|---|
| `sm_spawnline --team blu` | `team=blu health=150` |
| `sm_spawnline --team green` | Error log: `Bad spawn line '--team green': option '--team': 'green' is not one of: red, blu` |
| `sm_spawnline --help` | `(--help or --version: 8 lines)` |
| `sm_spawnline -h` | The help, printed by `Getopt_PrintHelp` |

A declared `-h` is an ordinary flag. Unlike `--help`, it does not skip the checks: with a required option missing,
`-h` is a user error.

## A type of your own

A `color` type: six hex digits, or eight when the option sets the key `alpha`. It converts every color to one form and
adds a note to the help.

```sourcepawn
#include <sourcemod>
#include <getopt>

GetoptSpec g_Paint;

public void OnPluginStart()
{
	// A type of this plugin: a hex color such as ff8000 or #FF8000.
	Getopt_RegisterType("color colour", GetoptKind_Value, "alpha")
		.SetLabel("Color")
		.OnCheck(Color_Check)
		.OnCoerce(Color_Coerce)
		.OnCheckKeys(Color_CheckKeys)
		.OnNotes(Color_Notes);

	g_Paint = new GetoptSpec("sm_paint");
	g_Paint.Option("fill", "color").Default("#ffffff").Help("Fill color");
	g_Paint.Option("glow", "color").Set("alpha", "1").Help("Glow color with alpha");
	g_Paint.Validate();

	RegAdminCmd("sm_paint", Command_Paint, ADMFLAG_ROOT);
}

// Accepts 6 hex digits, or 8 when the option sets "alpha".
bool Color_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength)
{
	int start = value[0] == '#' ? 1 : 0;
	int digits = strlen(value) - start;
	int wanted = keys.Has("alpha") ? 8 : 6;
	for (int i = start; value[i] != '\0'; i++)
	{
		if (!IsCharNumeric(value[i]) && FindCharInString("abcdefABCDEF", value[i]) == -1)
			digits = -1;
	}
	if (digits != wanted)
	{
		Format(problem, maxlength, "'%s' is not a color of %d hex digits", value, wanted);
		return false;
	}
	return true;
}

// Stores every color the same way: lower case, without '#'.
void Color_Coerce(GetoptKeys keys, const char[] value, char[] coerced, int maxlength)
{
	strcopy(coerced, maxlength, value[value[0] == '#' ? 1 : 0]);
	for (int i = 0; coerced[i] != '\0'; i++)
		coerced[i] = CharToLower(coerced[i]);
}

// Checks the type's own spec key when the spec is checked.
bool Color_CheckKeys(GetoptKeys keys, char[] problem, int maxlength)
{
	char alpha[4];
	if (!keys.Get("alpha", alpha, sizeof(alpha)) || StrEqual(alpha, "1"))
		return true;
	strcopy(problem, maxlength, "alpha must be 1 when set");
	return false;
}

// Shown in brackets before the help text.
void Color_Notes(GetoptKeys keys, GetoptList notes)
{
	notes.Add(keys.Has("alpha") ? "RRGGBBAA" : "RRGGBB");
}

Action Command_Paint(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Paint, client);
	if (opt == null)
		return Plugin_Handled;

	char fill[16], glow[16];
	opt.GetString("fill", fill, sizeof(fill));
	opt.GetString("glow", glow, sizeof(glow));
	ReplyToCommand(client, "fill=%s glow=%s", fill, glow);
	delete opt;
	return Plugin_Handled;
}
```

| Command | Prints |
|---|---|
| `sm_paint --fill "#FF8000" --glow 00ff0080` | `fill=ff8000 glow=00ff0080` |
| `sm_paint --fill orange` | `ERROR: option '--fill': 'orange' is not a color of 6 hex digits` |
| `sm_paint --glow ff8000` | `ERROR: option '--glow': 'ff8000' is not a color of 8 hex digits` |

The help shows the notes before the help text and the label after it:

```
# sm_paint [options]

## Options
   --fill <>     [RRGGBB] Fill color [Color]
                     Default = #ffffff
   --glow <>     [RRGGBBAA] Glow color with alpha [Color]
```

- The type is this plugin's own. To offer it to every plugin, register it with `shared` true. See
  [Shared types](reference.md#shared-types).
- A misspelled key value, `Set("alpha", "yes")`, is a spec error: `Getopt spec: option 'glow': alpha must be 1 when
  set`.

## Players, maps and configs

The `getopt-sourcemod-types` plugin adds types that look values up on the server: `target`, `player`, `map`, `cvar`,
`command`, `key`, `plugin` and `config`.

```sourcepawn
g_Slay.Arg("target", "target").Required().Help("Who to slay");   // @all, @red, a name, #userid
g_Slay.Option("then", "map").Help("Change to this map afterwards"); // cp_badl finds cp_badlands
g_Slay.Option("exec", "config").Help("A config to run first");      // server finds server.cfg
```

[sourcemod-types.md](sourcemod-types.md) has a complete plugin and every type's rules and messages.
