# SourceMod types

The `getopt-sourcemod-types` plugin shares eight option types that know the server. Every plugin's specs can name
them. They use SourceMod's natives only, so they work in every game SourceMod runs.

| Type | Takes | The value becomes | Attributes | Label |
|---|---|---|---|---|
| [`map`](#map) | a map's name, or the start of one | the map's full name | `display` | `[Map]` |
| [`cvar`, `convar`](#cvar-and-command) | a console variable's name, any case | its name as the server spells it | | `[Cvar]` |
| [`command`](#cvar-and-command) | a console command's name, any case | its name as the server spells it | | `[Command]` |
| [`player`](#player-and-target) | one player, as SourceMod targets | the client index | `userid`, `name` | `[Player]` |
| [`target`](#player-and-target) | any number of players, as SourceMod targets | unchanged | `clients`, `count`, `name`, `ml` | `[Target]` |
| [`key`](#key) | a key name, any case | the engine's spelling | | `[Key]` |
| [`plugin`](#plugin) | a loaded plugin's file or name | its file below `plugins/` | `name`, `version` | `[Plugin]` |
| [`config`](#config) | a config file below `cfg/` | its path below `cfg/`, with `.cfg` | | `[Config]` |

## Using them

```sourcepawn
#include <sourcemod>
#include <sdktools>
#include <getopt>
#include <getopt_sourcemod>

GetoptSpec g_Slay;

public void OnPluginStart()
{
	LoadTranslations("common.phrases");          // target names such as "all players" are phrases
	g_Slay = new GetoptSpec("sm_myslay");
	g_Slay.Arg("target", "target").Required().Help("Who to slay");
	g_Slay.Option("then", "map").Help("Change to this map afterwards");
	RegAdminCmd("sm_myslay", Command_Slay, ADMFLAG_SLAY);
}

// The types come from another plugin: check the spec once every plugin runs.
public void OnAllPluginsLoaded()
{
	g_Slay.Validate();
}

Action Command_Slay(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Slay, client);
	if (opt == null)
		return Plugin_Handled;

	int targets[MAXPLAYERS];
	char targetName[MAX_TARGET_LENGTH];
	bool isPhrase;
	int count = Getopt_GetTargets(opt, "target", targets, sizeof(targets), targetName, sizeof(targetName), isPhrase);
	for (int i = 0; i < count; i++)
		ForcePlayerSuicide(targets[i]);

	if (isPhrase)
		ShowActivity2(client, "[SM] ", "Slayed %t.", targetName);
	else
		ShowActivity2(client, "[SM] ", "Slayed %s.", targetName);

	delete opt;
	return Plugin_Handled;
}
```

- Include `getopt_sourcemod.inc`. It makes your plugin require `getopt-sourcemod-types`: SourceMod does not start
  your plugin without it.
- To make it optional, define `GETOPT_SOURCEMOD_OPTIONAL` before the include. A spec that names these types is then a
  spec error while the types plugin is not loaded.
- Validate in `OnAllPluginsLoaded`, not `OnPluginStart`. SourceMod starts plugins in load order, so the types may not
  be registered yet in `OnPluginStart`.

How the types work:

- `map`, `cvar`, `command`, `player`, `target`, `plugin` and `config` look the value up when the parse runs (the
  resolve step) and replace it with what was found. `player` and `target` look from the view of the
  [caller](reference.md#the-caller).
- Their check (an empty value, map characters, the config path) also runs on the default when the spec is checked.
- `key` has no lookup, so its default is checked fully with the spec.
- A value that finds nothing is a user error that says why.
- An empty value is `an empty value names nothing` for `map`, `cvar`, `command`, `player`, `target` and `plugin`.
  `key` and `config` refuse it with their own messages below.

## Helpers

`getopt_sourcemod.inc` has two stocks.

### `Getopt_GetTargets`

```sourcepawn
int Getopt_GetTargets(GetoptResult result, const char[] name, int[] clients, int maxClients,
                      char[] targetName, int maxlength, bool &isPhrase)
```

Reads the attributes of a `target` value.

- Fills `clients` with the client indexes. Returns their number, or 0 when the option has no value.
- `targetName` and `isPhrase` are what `ShowActivity2` and `ReplyToCommand` take: a phrase for `%t` when `isPhrase`
  is true, else a player's name for `%s`.
- It reads a single value. On a `multiple` target it is a plugin error (`'NAME' is a list, ...`). For each item of a
  multiple target, use `GetAttributeAt(name, index, "clients", ...)` and the like.
- `%t` needs `LoadTranslations("common.phrases")`.

### `Getopt_GetPlugin`

```sourcepawn
Handle Getopt_GetPlugin(GetoptResult result, const char[] name)
```

The handle of the plugin a `plugin` value names. Null when the option has no value, or the plugin unloaded since the
parse.

## map

A map the server can change to, found like `sm_map` finds it, with SourceMod's `FindMap`.

- An exact name, or the start of one: `cp_badl` finds `cp_badlands`.
- A workshop map by its full name: `workshop/12345`. One the server has not downloaded yet is accepted, as `sm_map`
  accepts it.
- The value becomes the map's full name.
- Attribute `display`: the name `GetMapDisplayName` gives.

Key `exact` (`Set("exact", "1")`): the start of a name is refused. A name `FindMap` reports as non-canonical (another
spelling of an existing map) is still accepted.

| Message | Meaning |
|---|---|
| `'VALUE' is no map name` | Characters other than letters, digits, `_`, `-`, `.` and `/`. |
| `'VALUE' is no map on this server` | No map has that name, or starts with it. |
| `'VALUE' is no map on this server (did you mean 'NAME'?)` | With `exact`: only the start of a name. |

## cvar and command

A console variable (`cvar`, also `convar`) or console command (`command`) that exists on the server: the engine's, the
game's, SourceMod's and every plugin's.

- Matched in any case. The value becomes the server's spelling: `SV_CHEATS` becomes `sv_cheats`.

| Message | Meaning |
|---|---|
| `'VALUE' is a command, not a console variable` | A `cvar` option got a command. |
| `'VALUE' is a console variable, not a command` | A `command` option got a cvar. |
| `'VALUE' is no console variable` | No such cvar. |
| `'VALUE' is no console command` | No such command. |

## player and target

One player (`player`) or any number of players (`target`), found by SourceMod's `ProcessTargetString` from the view of
the caller.

- Accepted: part of a name, `#` and a user id, and the target groups: `@all`, `@red`, `@me`, `@bots`, `@alive`, `@aim`,
  ...
- `@me` is the caller. SourceMod's immunity rules apply from the caller's view.

What the value becomes:

| Type | Value | Attributes |
|---|---|---|
| `player` | the client index; `GetInt` reads it | `userid`, `name` |
| `target` | unchanged, as typed | `clients` (client indexes separated by spaces), `count`, `name`, `ml` (`1` when `name` is a phrase) |

Read a `target` with [`Getopt_GetTargets`](#getopt_gettargets). A player can leave right after the command. Keep the
`userid`, not the client index, for later.

### Key `filter`

Limits who can be targeted: `Set("filter", "alive, nobots")`. Words separated by spaces or commas, in lower case:

| Word | Effect |
|---|---|
| `alive` | Only living players. |
| `dead` | Only dead players. |
| `connected` | Players still connecting count too. |
| `nobots` | No bots. |
| `noimmunity` | Immunity is not checked. |

- `alive` and `dead` cannot be combined.
- Without `noimmunity`, SourceMod's immunity rules apply. A client without admin rights can then target nobody, not
  even itself. Commands for players without admin rights need `noimmunity`.

Spec errors of the key:

| Message | Meaning |
|---|---|
| `filter 'WORD' is none of: alive, dead, connected, nobots, noimmunity` | An unknown word. |
| `filter cannot be both alive and dead` | Both words. |
| `filter is too long: it takes each of its five words once at most` | 255 characters or more, or 16 parts or more (a comma and a space count as two separators). |

### Messages

| Message | Meaning |
|---|---|
| `'VALUE' matches no player` | No player has that name. |
| `'VALUE' matches more than one player` | A `player` option matched several. |
| `'VALUE' matches no player right now` | A group such as `@blue` is empty, or every match was filtered out. |
| `'VALUE' is not alive` | The filter `alive` left the match out. |
| `'VALUE' is not dead` | The filter `dead` left the match out. |
| `'VALUE' is not in the game yet` | The match is still connecting. |
| `'VALUE' cannot be targeted by you` | Immunity. |
| `'VALUE' is a bot` | The filter `nobots` left the match out. |

## key

A key name of the engine's `bind` command.

- Letters and digits, `SPACE`, `MOUSE1` to `MOUSE5`, `MWHEELUP`, `KP_ENTER`, `F1` to `F12`, `JOY1` to `JOY32`, the
  controller buttons and the engine's other button names.
- Matched in any case. The value becomes the engine's spelling: letters lower case, the rest upper case. `mouse1`
  becomes `MOUSE1`, `A` becomes `a`.
- Names with spaces, such as `X AXIS POS`, need quotes.

| Message | Meaning |
|---|---|
| `'VALUE' is no key name` | Not a key name. |

## plugin

A loaded plugin: running or paused. Plugins that failed to load do not count.

The steps run in order, matching in any case. The first step that finds any plugin decides: one plugin is the value,
several are an error. The steps:

1. Its file below `plugins/`, with or without `.smx`: `basecommands`, `disabled/x.smx`.
2. The last part of that path: `x` for `disabled/x.smx`.
3. The name in its `myinfo`: `"Basic Commands"`.

- The value becomes the file below `plugins/`.
- Attributes: `name` and `version` from its `myinfo`.
- [`Getopt_GetPlugin`](#getopt_getplugin) gives its handle.

| Message | Meaning |
|---|---|
| `'VALUE' matches several plugins: FILES` | A step found several plugins. |
| `'VALUE' is no loaded plugin` | No step found one. |

## config

A config file that `exec` can run.

- A path below `cfg/`, ending in `.cfg` or in nothing, which means `.cfg`: `server`, `sourcemod/sourcemod.cfg`.
- It must exist in the game's file system, which includes the `custom/` folders and the game's VPK files.
- The value becomes the path below `cfg/`, with `.cfg`.

| Message | Meaning |
|---|---|
| `'VALUE' is no config name: a path below cfg/ without '..'` | Empty, or a `..` part, a leading `/`, a `\` or a `:`. |
| `'VALUE' is no config: configs end in .cfg` | Another file ending. |
| `'VALUE' is no config on this server` | No such file. |
