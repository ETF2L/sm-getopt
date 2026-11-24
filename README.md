# sm-getopt

The `getopt` SourceMod extension parses the arguments of console commands for plugins.

A plugin describes a command's arguments once, as a **spec**: each option and arg (positional value) with its type,
default, allowed values and help text. The extension then:

- parses the command line and checks every value;
- hands the plugin a result to read the values from by name;
- answers `--help` and `--version` by itself;
- tells the user what is wrong with a bad command line, followed by the help.

It is a port of the Perl module [Getopt::Pad](https://metacpan.org/pod/Getopt::Pad): the same command line syntax,
types, help layout and messages, with the [differences](docs/reference.md#differences-from-getoptpad) a game server
needs. Config files, shell completion and path types are left out: on a game server a command's settings live in cvars
and SourceMod's config files, the console has no completion a server could feed, and a path from a player's command
line is nothing a plugin should check.

## Example

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

	// ... do the work ...
	return Plugin_Handled;
}
```

`sm_backup -k 3 -vv --no-compress cp_badlands` gives keep 3, verbosity 2, compress off, map `cp_badlands`.

`sm_backup --help` prints:

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

`sm_backup --keep 0 cp_badlands` prints this error, then the help:

```
ERROR: option '--keep': 0 is smaller than the minimum of 1
```

The same works from the server console, over RCON, from a player's console and from chat (`!backup -k 3 cp_badlands`).

## Features

| Feature | Details |
|---|---|
| Types | `flag`, `bool` (`--x`, `--no-x`), `counter` (`-vvv`), `string`, `int`, `float`, `url`, `date`, `duration`. [Types](docs/reference.md#types) |
| SourceMod types | `map`, `cvar`, `command`, `player`, `target`, `key`, `plugin`, `config`, from the `getopt-sourcemod-types` plugin. [SourceMod types](docs/sourcemod-types.md) |
| Natural dates | `tomorrow 3pm`, `3 days ago`, `2 weeks`, with time zones. [Dates](docs/dates.md) |
| Bounds | `min` and `max` for numbers, dates and durations. [Min, Max](docs/reference.md#min-max) |
| Allowed values | Fixed lists, lists asked for at parse time, checks of your own. [Valid](docs/reference.md#valid) |
| Shapes | Repeatable options (`multiple`, `csv`), `key=value` mappings (`hash`), lists of records (`objectlist`). [Multiple](docs/reference.md#multiple) |
| Args | Typed, required or optional, the last one may take all remaining words. [Args](docs/reference.md#args) |
| Commands | Nested to any depth, each with its own options, args and help. Options can be inherited by every level below. [Commands](docs/reference.md#commands) |
| Help | Generated: groups, required marks, allowed values, defaults, examples, wrapped. [Help output](docs/reference.md#help-output) |
| Errors | A bad command line gets the problem and the help of its level. A bad spec is a plugin error naming the option. [Messages](docs/messages.md) |
| Your own types | Registered with callbacks, for your plugin or shared with all. [Types of your own](docs/reference.md#types-of-your-own) |

## Documentation

| File | What is in it |
|---|---|
| [docs/examples.md](docs/examples.md) | Complete plugins, one feature each, with their real output. Start here. |
| [docs/reference.md](docs/reference.md) | Every method, type, rule of the command line syntax and step of the checks. |
| [docs/messages.md](docs/messages.md) | Every message, with its meaning. |
| [docs/sourcemod-types.md](docs/sourcemod-types.md) | The SourceMod types of `getopt-sourcemod-types`. |
| [docs/dates.md](docs/dates.md) | What `date` and `duration` understand, and time zones. |
| `scripting/include/getopt.inc` | The API, with comments. |
| `scripting/include/getopt_sourcemod.inc` | The helpers of the SourceMod types. |

## Installing

Unpack the release archive `sm-getopt-v<version>-linux.tar.gz` into the game folder (for Team Fortress 2 `tf/`). It
holds, below `addons/sourcemod/`:

| File | What |
|---|---|
| `extensions/getopt.ext.so` | The extension, 32-bit. |
| `extensions/x64/getopt.ext.so` | The extension, 64-bit. |
| `plugins/getopt-sourcemod-types.smx` | The SourceMod types. Needed only by plugins that use them. |
| `scripting/` | The types plugin's source, `getopt.inc` and `getopt_sourcemod.inc`. |
| `licenses/getopt/` | sm-getopt's license. |

- Needs SourceMod 1.12.
- A plugin that includes `<getopt>` loads the extension by itself, and fails to load without it.
- The extension has no config, no cvars and no commands.
- Named time zones (`Europe/Berlin`) need the system's time zone files (`tzdata`, `/usr/share/zoneinfo`). `UTC` works
  without them.

## Building

`.github/workflows/ci.yml` builds the extension for x86 and x86_64 on an Ubuntu 22.04 runner and is the reference for
a local build: AMBuild against a pinned SourceMod commit (`SOURCEMOD_COMMIT`), with clang:

```
python extension/configure.py --enable-optimize --targets=x86,x86_64 --sm-path=<sourcemod>
ambuild
```

It runs the unit tests of both architectures, checks that the binaries export only `GetSMExtAPI` and need at most
glibc 2.35, compiles `getopt-sourcemod-types.sp` with the spcomp of a pinned SourceMod build (`SOURCEMOD_DROP`) and
packs `sm-getopt-<label>-linux.tar.gz`, laid out like a game folder with the files above. For a pushed `v*` tag it
publishes the archive as a GitHub release of `etf2l/sm-getopt`, after checking the tag against `SMEXT_CONF_VERSION`
in `extension/src/smsdk_config.h`. A separate job checks `scripting/` with pawncritic.

The unit tests also build without AMBuild, for quick turns:

```
cd extension
g++ -std=c++17 -Wall -Werror -Isrc/core -Itests src/core/*.cpp tests/*.cpp -o /tmp/getopt-tests && /tmp/getopt-tests
```

## Layout

| Path | What |
|---|---|
| `extension/src/core/` | The library that does the work, without SourceMod: `text` (words, names, numbers as text), `types` (built-in types, registry), `temporal` (time zones, dates, durations), `spec` (declaration, checks, value pipeline), `parser`, `result` (reading values), `help`, `ownership` (each plugin's types, shared types, which specs check themselves again). |
| `extension/src/` | The SourceMod layer: handles, plugins as owners, callbacks (`extension`), the natives of specs, types and results. |
| `extension/tests/` | Unit tests of the core library, one program per architecture. |
| `scripting/include/getopt.inc` | The API. |
| `scripting/getopt-sourcemod-types.sp`, `scripting/include/getopt_sourcemod.inc` | The SourceMod types. |
| `docs/` | The documentation. |
| `LICENSE.txt`, `licenses/` | The license and the GPL texts. |

## AI disclosure

AI helped build this project. If that bothers you, no hard feelings - this project is just not for you.

## License

sm-getopt is licensed under the GNU General Public License version 3, with the same exceptions SourceMod grants; see
[LICENSE.txt](LICENSE.txt). The release installs it into `addons/sourcemod/licenses/getopt/`.
