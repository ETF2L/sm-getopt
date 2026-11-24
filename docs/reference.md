# Getopt reference

Everything a plugin can do with the `getopt` extension.

- New here? Start with the [README](../README.md), then [examples.md](examples.md).
- Looking up a message? See [messages.md](messages.md).
- The SourceMod types (`map`, `target`, ...) are in [sourcemod-types.md](sourcemod-types.md).
- What `date` and `duration` values understand is in [dates.md](dates.md).

Contents:

- [Terms](#terms)
- [Specs and parsing](#specs-and-parsing)
- [Levels](#levels)
- [Options](#options)
- [Args](#args)
- [Commands](#commands)
- [Types](#types)
- [Command line syntax](#command-line-syntax)
- [How a value is checked](#how-a-value-is-checked)
- [Automatic options](#automatic-options)
- [Help output](#help-output)
- [Reading results](#reading-results)
- [Types of your own](#types-of-your-own)
- [Differences from Getopt::Pad](#differences-from-getoptpad)

## Terms

| Term | Meaning |
|---|---|
| spec | The description of one console command's arguments. A `GetoptSpec` handle. |
| level | The top level of a spec, or one command. Has options, and either args or commands. |
| option | A named `--switch` of a level, possibly with a value: `--verbose`, `-v`, `--keep 3`. |
| primary name | The first name of an option's key: `keep` in `keep\|k`. Values are read by it. |
| alias | Every other name of the key: `k` in `keep\|k`. Accepted on the command line, never used to read. |
| arg | A positional value: `cp_badlands` in `sm_backup -v cp_badlands`. |
| command | A named level below another, selected by a word: `add` in `sm_whitelist add STEAM_0:1:23`. Commands can have commands. |
| command path | The command words that lead to a level: `user add`. |
| shape | What an option's value is made of: a single value, a list (`multiple`), a mapping (`hash`) or a list of mappings (`objectlist`). |
| type | Decides whether an option takes a value and how the value is checked and converted: `int`, `date`, `map`, ... |
| result | The values of one level after a parse. A `GetoptResult` handle. |
| outcome | How a parse ended: parsed, rejected or answered. |
| caller | The client a parse runs for. 0 is the server. |
| user error | A mistake on the command line. Told to the user with the help. |
| spec error | A mistake in the spec, that is, in the plugin. An error in SourceMod's error log. |

## Specs and parsing

### Declare a spec

```sourcepawn
GetoptSpec g_Spec;

public void OnPluginStart()
{
	g_Spec = new GetoptSpec("sm_backup");          // the name the help shows
	g_Spec.Option("keep|k", "int").Default("7");
	g_Spec.Arg("map").Required();
	g_Spec.Validate();                             // check it now

	RegAdminCmd("sm_backup", Command_Backup, ADMFLAG_ROOT);
}

Action Command_Backup(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Spec, client);
	if (opt == null)
		return Plugin_Handled;                     // the user was told why

	int keep = opt.GetInt("keep");
	delete opt;
	return Plugin_Handled;
}
```

- Declare a spec once, usually in `OnPluginStart`, and keep the handle.
- The spec lives until the plugin unloads, or until you `delete` it.
- Declaring only records. The whole spec is checked on first use: `Validate`, or the first parse.
- You may change a spec later (more options, another default). It is checked again on its next use.

### `GetoptSpec.Validate`

```sourcepawn
spec.Validate();
```

Checks the whole spec now, all commands included. A mistake is a spec error of this call. Call it at the end of
`OnPluginStart`, so mistakes show when the plugin loads, not when someone first runs the command.

Validate a spec that names [shared types](#shared-types) of another plugin in `OnAllPluginsLoaded` instead.

### `Getopt_ParseCommand`

```sourcepawn
GetoptResult Getopt_ParseCommand(GetoptSpec spec, int client)
```

The usual call inside a command callback.

- Reads the running command's argument string (`GetCmdArgString`) and parses it for `client`.
- Returns the result when the command line is valid. Delete it when done.
- Returns `null` when the command line was wrong or asked for `--help` or `--version`. The client has already been
  shown the error with the help, or the answer. Just return.

### `GetoptSpec.Parse`

```sourcepawn
GetoptResult Parse(const char[] line, int caller = 0)
```

Parses any argument string, for when `Getopt_ParseCommand` does not fit: a line from a file, a menu, another plugin.

- `line`: the arguments without the command name, as `GetCmdArgString` gives them.
- `caller`: the client the parse runs for, 0 for the server. See [The caller](#the-caller).
- Always returns a result, whatever the outcome. Delete it.
- Errors of the call: a spec error, a callback that failed, a caller that is no client index.

### Outcomes

`result.Outcome`, a `GetoptOutcome`, tells how the parse ended:

| Outcome | When | The result has |
|---|---|---|
| `Getopt_Parsed` | The command line is valid. | The values. |
| `Getopt_Rejected` | A user error. | `GetError`: the error. The output: the error, an empty line, the help. |
| `Getopt_Answered` | `--help` or `--version` was given. | The output: the help, or the version line. |

Only a parsed result has values. Reading a value of another result is an error of the plugin.

### Where output goes

`Getopt_ParseCommand` shows a rejected or answered result with `Getopt_Reply(result, client)`:

- Client 0: the server console (and so an RCON client).
- A player: their console.
- A player who typed the command in chat (`!backup ...`): the answer goes to their console. In chat they get the
  error line, if any, then `See console for output.` Chat cuts long lines; the console has the error whole.

`Getopt_Reply(result, client)` is a stock you can call yourself after `Parse`. `Getopt_PrintLine(client, line)`, also a
stock, prints one line to a client's console, or to the server console for client 0.

To show the output somewhere else, read it line by line:

```sourcepawn
char line[GETOPT_LINE_LENGTH];
for (int i = 0; i < result.OutputLines; i++)
{
	result.GetOutputLine(i, line, sizeof(line));
	LogMessage("%s", line);
}
```

### The caller

The caller is the client a parse runs for. Types that look players up target from its view: `@me` is the caller,
and SourceMod's immunity rules apply. `Getopt_ParseCommand` passes the client it is given. `Parse` defaults to 0.

Inside the resolve, verify and prepare callbacks of a type, `keys.Caller` is the caller.

### Ownership

Only the plugin that made a spec, a result or a type can use it. This covers a spec's options, args and levels, and
cloned handles. Callbacks always run in the plugin that set them. Another plugin gets one of these errors:

- `This spec belongs to another plugin`
- `This result belongs to another plugin`
- `This option type belongs to another plugin`

## Levels

The spec is the top level. `Command(name)` adds a command and returns its level, a `GetoptLevel`. Both have these
methods:

| Method | Does |
|---|---|
| `Option(key, type = "flag")` | Declares an option. See [Options](#options). |
| `Arg(name, type = "string")` | Declares the next arg. See [Args](#args). |
| `Command(name)` | Declares a command and returns its level. See [Commands](#commands). |
| `Description(text)` | One line for the help: below the usage line, and next to the command in its parent's help. |
| `Example(text, args)` | A usage example at the end of the help. `text` explains, `args` is the command line after the command's name. |
| `CommandRequired(bool)` | Only on a level with commands: must the command line name one? Default: true. |

On a `GetoptLevel`, `Description`, `Example` and `CommandRequired` return the level, so calls chain. On a
`GetoptSpec` they return nothing.

On a `GetoptLevel` only:

| Method | Does |
|---|---|
| `Set(key, value)` | Sets a key by name: `description`, `commandRequired`. On the top level (`spec.Root`) also `version` and `helpWidth`. |

On the spec only:

| Method | Does |
|---|---|
| `Version(text)` | What `--version` prints after the name. Default: the `version` of the plugin's `myinfo`, else `unknown`. |
| `HelpWidth(columns)` | Wrap width of option, arg and command entries, 1 to 10000. Default: 100. See [Width](#width). |
| `Validate()` | Checks the spec now. |
| `Parse(line, caller = 0)` | Parses an argument string. |
| `Root` | The top level as a `GetoptLevel`, for code that treats all levels alike. |

Rules:

- A level has args or commands, never both.
- A command name starts with a letter, then letters, digits, `_` or `-`.

## Options

```sourcepawn
spec.Option("log-level|l", "string")       // key: primary name and aliases; type
	.Default("info")
	.Valid("debug", "info", "warn", "error")
	.Help("How much to log");
```

- Every option method returns the option, so calls chain.
- The switch methods (`Required`, `Hidden`, `Multiple`, `Csv`, `Hash`, `ObjectList`, `Inherit`, `BigInt`) take an
  optional bool, true by default: `Required(false)` turns it off again.
- Without a type, an option is a `flag`.

### Option names

The key is the primary name, then aliases, separated by `|`: `"keep|k"`, `"log-level|l|L"`.

- A name starts with a letter, then letters, digits, `_` or `-`.
- One letter is a short option: `-k`. Longer is a long option: `--keep`.
- Names are case sensitive: `-v` and `-V` are different options.
- A name is used by one option per level.
- `help` is taken on every level, `version` on the top level.
- A `bool` option also takes the names of its negation. Next to the bool `color`, no option can be named `no-color`
  or `nocolor`.

### Option methods

| Method | Key | Does |
|---|---|---|
| [`Required()`](#required) | `required` | The option must be given. |
| [`Default(value)`](#default) | | The value when the command line does not set the option. |
| [`DefaultPair(key, value)`](#default) | | One pair of a `hash` default. |
| [`DefaultField(entry, field, value)`](#default) | | One field of an `objectlist` default. |
| [`Help(text)`](#help-group-hidden) | `help` | The text next to the option in the help. |
| [`Group(name)`](#help-group-hidden) | `group` | The help section the option is listed in. Default: `Options`. |
| [`Hidden()`](#help-group-hidden) | `hidden` | The option works, but the help does not list it. |
| [`TypeHint(label)`](#typehint) | `typehint` | Replaces the type's label in the help. |
| [`Valid(...)`](#valid) | | A fixed list of allowed values. |
| [`ValidFrom(callback, data)`](#validfrom) | | Allowed values, asked for at parse time. |
| [`LazyValid(callback, data)`](#lazyvalid) | | A yes/no check of your own. |
| [`ProcessValue(callback, data)`](#processvalue) | | Converts each value before the result holds it. |
| [`Multiple()`](#multiple) | `multiple` | May be given more than once. The value is a list. |
| [`Csv()`](#csv) | `csv` | Splits each value of a multiple option at commas. |
| [`Hash()`](#hash) | `hash` | Takes `KEY=VALUE`. The value is a mapping. |
| [`ObjectList()`](#objectlist) | `objectlist` | Takes `INDEX.FIELD=VALUE`. The value is a list of mappings. |
| [`Inherit()`](#inherit) | `inherit` | Also accepted on every level below. |
| [`Min(n)`, `Max(n)`](#min-max) | `min`, `max` | Inclusive bounds of `int`, `date` and `duration`. |
| [`MinFloat(x)`, `MaxFloat(x)`](#minfloat-maxfloat) | `min`, `max` | Inclusive bounds of `float`. |
| [`BigInt()`](#bigint) | `bigint` | An `int` of any size. |
| [`TimeZone(name)`](#timezone) | `timezone` | The time zone of `date` and `duration`. |
| [`Set(key, value)`](#set) | any | Sets any key by name, as text. |

### `Required`

```sourcepawn
spec.Option("host", "string").Required();
```

- The command line must set the option. Missing: `missing required option '--host'`.
- Not together with a default.
- The help marks it `[REQ]`.

### `Default`

```sourcepawn
spec.Option("level", "string").Default("info");                                    // a single value
spec.Option("tag", "string").Multiple().Default("red").Default("blue");            // a list: one call per value
spec.Option("define", "string").Hash().DefaultPair("os", "linux");                 // a mapping
spec.Option("server", "string").ObjectList().DefaultField(0, "host", "alpha")      // a list of mappings
                                           .DefaultField(0, "port", "80")
                                           .DefaultField(1, "host", "beta");
```

- The value the option has when the command line does not set it.
- The command line replaces the default as a whole. Lists and mappings are never merged with their default.
- Written as text, checked like a command line value (type, bounds, valid list, lazyValid) when the spec is checked.
  An invalid default is a spec error, even if the option is never used.
- The result holds the checked value: the `int` default `"007"` reads as 7.
- The help shows the default as written.
- A `flag` or `bool` takes `"1"` or `"0"`. A `counter` takes a count.
- The default must fit the shape: `Default` for single values and lists, `DefaultPair` for `hash`, `DefaultField` for
  `objectlist`.

Defaults of `date` and `duration` are checked again in every parse that uses them. `Default("today")` is the day of
the parse. A default that fails then is the user error `option '--NAME': default value: PROBLEM`.

Defaults also pass the resolve, verify and prepare steps of a registered type in every parse. A default such as `@me`
is resolved for each caller.

### `Help`, `Group`, `Hidden`

```sourcepawn
spec.Option("port", "int").Group("Connection").Help("The server's port");
spec.Option("debug").Hidden();
```

- `Help(text)`: the text next to the option in the help. Line breaks and runs of spaces in it only separate words.
- `Group(name)`: the help section the option is listed in. Options without a group are listed under `Options`.
- `Hidden()`: the option works, but the help does not list it.

### `TypeHint`

```sourcepawn
spec.Option("host", "string").TypeHint("Hostname");      // help: ... [Hostname]
```

Replaces the type's label in brackets at the end of the help text. Must not be empty.

### `Valid`

```sourcepawn
spec.Option("level", "string").Valid("debug", "info", "warn", "error");
```

- Only these values are accepted. Any other value: `'VALUE' is not one of: debug, info, warn, error`.
- The comparison is exact and case sensitive.
- It compares the value after the type's conversion. For an `int` option, `--port 080` is compared as `80`.
- The help lists the values in a `Valid` line.

### `ValidFrom`

The callback is a `GetoptValidSource`:

```sourcepawn
spec.Option("file", "string").ValidFrom(Files_Valid);

void Files_Valid(GetoptList values, const char[] option, any data)
{
	values.Add("errors.log");
	values.Add("chat.log");
}
```

- For allowed values only known at run time.
- Called once in each parse whose command line gives the option, when its first value is checked.
- Also called when the default is checked: once with the spec, and for `date` and `duration` options in every parse
  that uses the default.
- `Valid` and `ValidFrom` replace each other. The last call wins.
- Rejects like `Valid`. The help does not list these values.

### `LazyValid`

The callback is a `GetoptLazyValid`:

```sourcepawn
spec.Option("prefix", "string").LazyValid(Prefix_IsValid);

bool Prefix_IsValid(const char[] value, const char[] option, any data)
{
	return strlen(value) <= 8;
}
```

- A yes/no check for what a list cannot say.
- Gets the converted value, after the valid list. Returns true to accept.
- False is the user error `'VALUE' is not a valid value`.
- For a message of your own, [register a type](#types-of-your-own) instead.

`Valid`, `ValidFrom` and `LazyValid` apply to every single value of a `multiple`, `hash` or `objectlist` option. For
`hash` and `objectlist` they check the values, not the keys.

### `ProcessValue`

The callback is a `GetoptProcessValue`:

```sourcepawn
spec.Option("minutes", "int").ProcessValue(Minutes_ToSeconds);

void Minutes_ToSeconds(GetoptResult values, const char[] value, char[] processed, int maxlength,
                       const char[] name, any data)
{
	Format(processed, maxlength, "%d", StringToInt(value) * 60);
}
```

Converts a checked value into what the result returns. Available on options and args.

- `processed` starts as the value. It holds 1024 bytes.
- Runs once per value: once per list item, mapping value or field value.
- Runs after all options and args of the level passed their checks.
- Runs only on the value the parse uses, from the command line or the default. An option without either is not
  processed.
- `values` is the level's result before any value was processed. The order the callbacks run in does not matter.
  `values` is closed when the callback returns.
- `values.Subcommand` is the command below, already processed. Levels above cannot be reached.
- The processed value is text. `GetInt` and `GetFloat` still read it when it is a number.
- A processed `date` or `duration` is plain text after it: `GetInt` reads it only when the text is a whole number, and
  `GetDuration` is an error.

### `Multiple`

```sourcepawn
spec.Option("tag", "string").Multiple();
```

- The option may be given more than once. Its value is a list in command line order.
- `--tag red --tag blue` reads as `red`, `blue`.
- Not given and without a default: an empty list.
- Needs a type that takes a value. Not together with `Hash` or `ObjectList`.
- Read with `Count`, `GetStringAt`, `GetIntAt`, `GetFloatAt`.

### `Csv`

```sourcepawn
spec.Option("tag", "string").Multiple().Csv();
```

Splits every value of a multiple option at commas. Needs `Multiple`.

| Command line | Values |
|---|---|
| `--tag red,blue --tag green` | `red`, `blue`, `green` |
| `--tag "red, blue,"` | `red`, `blue` |
| `--tag red,,blue` | Error: `'red,,blue' contains an empty item` |

- Whitespace around items is removed.
- One trailing comma is ignored.
- A comma cannot be escaped.

### `Hash`

```sourcepawn
spec.Option("define|D", "string").Hash();
```

The option takes `KEY=VALUE` words. Its value is a mapping.

| Command line | Value |
|---|---|
| `--define os=linux --define arch=x86` | `arch` -> `x86`, `os` -> `linux` |
| `--define path=a=b` | `path` -> `a=b` |
| `--define os=linux --define os=bsd` | `os` -> `bsd` |

- The word is split at the first `=`.
- A repeated key replaces the earlier value.
- A word without `=`, or an empty key, is a user error.
- Values are checked (type, valid list, lazyValid). Keys are not.
- Not given and without a default: an empty mapping.
- Read with `Count`, `GetKey`, `HasKey`, `GetStringOf`, `GetIntOf`, `GetFloatOf`. Keys are sorted by byte value:
  upper case before lower case.

### `ObjectList`

```sourcepawn
spec.Option("server", "string").ObjectList();
```

The option takes `INDEX.FIELD=VALUE` words. Its value is a list of mappings, one per index.

```
--server 0.host=alpha --server 0.port=80 --server 1.host=beta
    entry 0: host -> alpha, port -> 80
    entry 1: host -> beta
```

- INDEX counts from 0, without leading zeros.
- The indices must be exactly 0 to n-1, in any order. A gap: `missing index N`.
- FIELD has letters, digits, `_` and `-`.
- A repeated `INDEX.FIELD` replaces the earlier value.
- Values are checked (type, valid list, lazyValid).
- Read with `Count`, `FieldCount`, `GetField`, `HasField`, `GetStringIn`, `GetIntIn`, `GetFloatIn`.

### `Inherit`

```sourcepawn
spec.Option("verbose|v", "counter").Inherit();
spec.Command("scan");
spec.Command("report");
```

The option is also accepted on every level below. `sm_tool -v scan` and `sm_tool scan -v` are the same.

- Only on a level with commands.
- The value belongs to the declaring level. Read it from that level's result, here the top level's.
- Given on several levels, it counts as given on the declaring level. A single value: the last wins. `multiple`:
  all values. `hash`: all pairs. `counter`: the sum (`sm_tool -v scan -v` is 2).
- Every level below lists it in its help.
- No level below may use one of its names.

### `Min`, `Max`

```sourcepawn
spec.Option("port", "int").Min(1).Max(65535);
```

Inclusive bounds.

| Type | Bound is | Out of bounds |
|---|---|---|
| `int` | a number | `0 is smaller than the minimum of 1` |
| `date` | a Unix timestamp | `'yesterday' is earlier than the minimum of ...` |
| `duration` | seconds | `'2 days' is longer than the maximum of ...` |

Bounds a cell cannot hold, and bounds written as text, are set with [`Set`](#set). See [bigint](#bigint),
[date](#date) and [duration](#duration).

### `MinFloat`, `MaxFloat`

```sourcepawn
spec.Option("scale", "float").MinFloat(0.1).MaxFloat(10.0);
```

Inclusive bounds of a `float` option. A message shows the bound as it was set: `MaxFloat(1.0)` sets `1.000000`,
`Set("max", "1")` sets `1`.

### `BigInt`

```sourcepawn
spec.Option("steamid64", "int").BigInt().Set("min", "76561197960265728");
```

An `int` option of any size, kept exactly. Read it with `GetString`. `GetInt` is an error for a value beyond a cell.
Bounds are set as text with `Set`.

### `TimeZone`

```sourcepawn
spec.Option("at", "date").TimeZone("Europe/Berlin");
```

The time zone a `date` or `duration` option is read in: `UTC`, `floating`, `local` (the default) or an IANA name.
An unknown name is a spec error. See [dates.md](dates.md#time-zones).

### `Set`

```sourcepawn
spec.Option("glow", "color").Set("alpha", "1");        // a key of a registered type
spec.Option("id", "int").BigInt().Set("min", "1");     // a bound beyond a cell
spec.Option("at", "date").Set("min", "now");           // a bound as text
```

Sets any key by name, as text. The methods with a key in the [table](#option-methods) are short forms of it.

- A switch key is on for any value but `"0"` and `""`.
- A key the option's type does not have (`min` on a `string`) is a spec error.
- Use it for the keys of [types of your own](#types-of-your-own).

## Args

```sourcepawn
spec.Arg("source").Required().Help("Map to copy");
spec.Arg("targets").Multiple().Help("Where to copy it to");
```

Args are the words of the command line that are no options, taken in declaration order.

- The name is what the value is read by and what the help shows. It follows the rules of [option names](#option-names).
- Without a type, an arg is a `string`. `flag`, `bool` and `counter` cannot be args.
- An option and an arg of one level cannot share a name.

| Method | Key | Does |
|---|---|---|
| `Required()` | `required` | The arg must be given. Required args come before optional ones. |
| `Multiple()` | `multiple` | Last arg only: takes all remaining words. Its value is a list. With `Required`, at least one word. |
| `Help(text)` | `help` | Its text in the `Arguments` section of the help. |
| `TypeHint(label)` | `typehint` | Replaces the type's label in the help. |
| `ProcessValue(callback, data)` | | Converts each value, as for [options](#processvalue). |
| `Min`, `Max`, `MinFloat`, `MaxFloat`, `BigInt`, `TimeZone`, `Set` | | The keys of the arg's type, as for options. |

- Args have no default, valid list, lazyValid, group, `csv`, `hash` or `objectlist`.
- An optional arg that is not given has no value.
- Words beyond the last arg: `unexpected extra argument 'WORD'`. A level without args takes no words.

## Commands

Commands split a console command into modes. Each has its own options, args and help.

```sourcepawn
GetoptSpec spec = new GetoptSpec("sm_user");
spec.Option("dry-run").Help("Show what would happen");

GetoptLevel add = spec.Command("add").Description("Add a user");
add.Option("role", "string").Default("member");
add.Arg("name").Required();

spec.Command("remove").Description("Remove a user").Arg("name").Required();
```

### How a command line is read

1. The top level reads options up to the first word that is no option.
2. That word must name one of its commands. Else: `unknown command 'WORD', expected one of: NAMES`.
3. The rest of the line goes to that command's level, which reads it the same way.
4. A level without commands reads its options and args.

Rules:

- A level's options come before its command word. After it, an outer level's option is unknown, unless it is
  [inherited](#inherit).
- A line that ends before naming a command: `missing command, expected one of: NAMES`. Not with
  `CommandRequired(false)`.
- `--help` shows the help of the level it is given on: `sm_user --help` the top level, `sm_user add --help` the
  `add` level.
- An error is shown with the help of the level it belongs to. A bad value of an inherited option shows the help of the
  level that declares it.

### Reading a command's values

Each level of the command path has a result of its own. The parse returns the top level's.

```sourcepawn
// sm_user --dry-run add --role admin dave
GetoptResult opt = Getopt_ParseCommand(spec, client);
if (opt == null)
	return Plugin_Handled;

char command[16], name[32];
opt.GetCommand(command, sizeof(command));        // "add"
bool dryRun = opt.GetBool("dry-run");            // a top level option

GetoptResult sub = opt.Subcommand;               // the "add" level: a new handle
sub.GetString("name", name, sizeof(name));       // "dave"
delete sub;
delete opt;
```

- A result holds the values of its own level only.
- `GetCommand` returns false and `Subcommand` is null when the level has no commands or none was given.
- `Subcommand` returns a new handle on every call. Delete each.

## Types

The type decides whether an option takes a value, how the value is checked and converted, and what the result
returns. Type names are case insensitive. Names in one row are the same type.

| Type names | Command line | The result holds |
|---|---|---|
| `flag` (default for options) | `--name` | 1 |
| `bool`, `boolean`, `!` | `--name`, `--no-name` | 1 or 0 |
| `counter`, `count`, `+` | `--name --name`, `-nn` | how often it was given |
| `string`, `str`, `s` (default for args) | `--name VALUE` | the text |
| `int`, `integer`, `i` | `--name 42` | a whole number |
| `float`, `num`, `number`, `f` | `--name 1.5` | a number |
| `url`, `uri` | `--name "https://..."` | the URL |
| `date` | `--name "tomorrow 3pm"` | a point in time |
| `duration` | `--name "2 days"` | a length of time |

More types:

- The [SourceMod types](sourcemod-types.md): `map`, `cvar`, `command`, `player`, `target`, `key`, `plugin`,
  `config`.
- [Types of your own](#types-of-your-own).

### flag

A switch without a value.

- Given: 1. Not given: no value (unless it has a default).
- Cannot be turned off on the command line. Use `bool` for that.

### bool

A switch that can be negated.

- `--color`: 1. `--no-color` or `--nocolor`: 0. The last one given wins.
- Not given and without a default: no value. Usually declared with `Default("1")` or `Default("0")`.
- The help shows it as `--[no-]color`.

### counter

Counts how often it is given.

- `-v -v -v`, `-vvv` and `--verbose -vv` are all 3.
- Not given: no value. `GetInt` returns 0 then.

### string

Any text, also empty (`--name ""`).

### int

An integer: optional `+` or `-`, then decimal digits. `42`, `-7`, `+3`, `007`.

- The result holds the number: `007` is 7.
- Not an integer: `'VALUE' is not an integer`.
- Must fit a cell, -2147483648 to 2147483647. Else: `'VALUE' is too large for an integer`.
- With [`BigInt`](#bigint): any size, read with `GetString`.

### float

A decimal number: `1.5`, `-2`, `.5`, `1e3`. No whitespace around it.

- `inf`, `infinity`, `nan` and numbers too large (`1e999`): `'VALUE' is not a finite number`.
- Anything else, hexadecimal included: `'VALUE' is not a number`.
- Bounds: [`MinFloat`, `MaxFloat`](#minfloat-maxfloat).

### url

A URL of the form `scheme://rest`.

- The scheme starts with a letter, then letters, digits, `+`, `.` or `-`.
- At least one character after `://`. No whitespace.
- `https://example.com/x` is a URL. `example.com` and `mailto:me@example.com` are not:
  `'VALUE' is not a URL`.
- The help label is `[URL]`.
- Quote URLs on the command line. The console takes `//` as the start of a comment. See
  [What the console does first](#what-the-console-does-first).

### date

A point in time in plain English: `tomorrow 3pm`, `last monday`, `3 days ago`, `2026-10-06 14:00`.
[dates.md](dates.md) lists everything it understands.

- Not a date: `'VALUE' is not a date`.
- The help label is `[Date]`.
- As text: the date in the option's time zone, `2026-10-06T14:00:00`. The valid list compares this text.
- `GetInt`: a Unix timestamp, as `GetTime` counts.
- Time zone: [`TimeZone`](#timezone). Default: `local`.
- Relative values (`tomorrow`, `in 2 hours`) count from the moment the value is checked.

Bounds:

- Without bounds, a date must fit a cell: 1901-12-13T20:45:52Z to 2038-01-19T03:14:07Z.
  Else: `'2040-01-01' is later than the maximum of 2038-01-19T03:14:07` (on a UTC server: the bound is shown in the
  option's time zone).
- `Min` and `Max` take Unix timestamps.
- `Set("min", TEXT)` and `Set("max", TEXT)` take a date as text, read in the option's time zone:
  `Set("min", "now")` refuses the past, `Set("max", "in 1 year")` refuses more than a year ahead.
- Relative bounds are read again for every value, at the moment of the value.
- A bound beyond a cell lets more through: `Set("max", "9999-12-31")`. Read such an option with `GetString`. `GetFloat`
  loses precision there: hours near the year 9999. `GetInt` is an error for a timestamp beyond a cell.
- A bound that leaves no date at all is a spec error. `Set("min", "2040-01-01")` alone needs a `max` too. Relative
  bounds are compared at the moment the spec is checked.

### duration

A length of time: one number and one unit, written out, optionally after `for`. `90 seconds`, `2 weeks`, `1 month`,
`for 3 hours`. [dates.md](dates.md#durations) has the details.

- Abbreviations (`1h`) and combinations (`1 hour 30 minutes`) are not durations: `'VALUE' is not a duration`.
- The help label is `[Duration]`.
- As text: the duration in one unit, `90 minutes`.
- `GetDuration(name, months, days, seconds)`: its parts. A month has no fixed length, so the parts stay apart.
  `2 weeks` is 14 days, `1 year` is 12 months, `90 minutes` is 5400 seconds.
- `GetInt`: the seconds it lasts from the moment of the parse, on the calendar of the option's time zone.

Bounds:

- Without a `max`, a duration may last at most 2147483647 seconds (about 68 years) from now. Else:
  `'100 years' is longer than the maximum of 2147483647 seconds`.
- `Min` and `Max` take seconds. `Set("min", "1 minute")` takes a duration as text.
- Bounds are compared by the seconds both last from now.
- A `max` beyond a cell (`Set("max", "10000 years")`) lets longer durations through. Read them with `GetString`.
  `GetInt` is an error for seconds beyond a cell, `GetDuration` for a part beyond one (`5259492000 minutes`).
- A `min` longer than the `max` is a spec error, as is one longer than 2147483647 seconds without a `max`.

## Command line syntax

The syntax of GNU programs, as Getopt::Pad and Perl's Getopt::Long (with bundling) read it.

### Words and quotes

The extension splits the argument string into words itself:

- Whitespace separates words.
- Double quotes keep whitespace inside a word and are removed. They may start anywhere in a word:
  `--name="my map"` is `--name "my map"`.
- `""` is an empty word.
- A quote left open runs to the end of the line.
- There is no escape character. Single quotes are ordinary characters.
- A colon does not split a word, unlike `GetCmdArg`: `STEAM_0:1:23` and `14:00` are one word each.

### What the console does first

The game's console changes the line before any plugin sees it. No plugin can undo this:

- `//` outside quotes starts a comment. Quote URLs: `--mirror "https://example.com/x"`.
- `;` outside quotes ends the command.
- A command line is at most about 500 characters.

### Long options

- Two dashes: `--owner`.
- The value is the next word, or follows `=`: `--owner dave` is `--owner=dave`.
- Names are never abbreviated: `--own` is an unknown option.
- Case matters.

### Short options

- One dash: `-o`.
- The value is the next word, or follows directly: `-o dave` is `-odave`.
- `-o=dave` sets the value `=dave`.
- `--o` works too.

### Bundling

- Single-letter options can share one dash: `-abc` is `-a -b -c`. `-vvv` counts 3.
- A letter that takes a value ends the bundle: `-vxfoo` is `-v -x foo`.
- A long name with one dash is read letter by letter: `-force` is `-f -o -r -c -e`. Each unknown letter is
  reported.

### Negation

A `bool` option is turned off with `--no-NAME` or `--noNAME`.

### Repeating an option

| Kind | Given twice |
|---|---|
| single value | the last value wins |
| `bool` | the last of `--x` and `--no-x` wins |
| `multiple` | all values, in order |
| `hash`, `objectlist` | all pairs; a repeated key wins |
| `flag` | stays 1 |
| `counter` | counts |

### Values that start with a dash

An option that takes a value always takes the next word: `--offset -5` and `--pattern --x` work.

### Empty values

- `--name ""` is the empty text.
- `--name=` is the user error `Option NAME requires an argument`.

### Order of options, args and commands

- On a level without commands, options and args mix freely: `sm_tool a --verbose b` is `sm_tool --verbose a b`.
- On a level with commands, the level's options come before the command word. See [Commands](#commands).

### `--`

`--` ends the options of its level.

- On a level without commands, every word after it is an arg, also one that starts with a dash: `sm_tool -- -5`.
  Without `--`, `-5` is the short option `5`, which is unknown.
- On a level with commands, `--` ends only that level's options: the next word is still the command, whose level
  reads options again. To pass args that start with a dash, put `--` after the last command word.

## How a value is checked

Every option gets its value from the command line, else from its default. Never from both. Neither, and the option
is required: a user error. Neither, and it is not required: no value, or an empty list or mapping.

A value passes these steps in order. The first that fails gives the error.

| Step | What | Options | Args |
|---|---|---|---|
| 1 | The shape, once per option: csv items, empty keys, `INDEX.FIELD` keys and the indices | yes | |
| 2 | The type's check, with its bounds, per single value | yes | yes |
| 3 | The type's conversion (`007` to 7), per single value | yes | yes |
| 4 | The valid list, on the converted value | yes | |
| 5 | lazyValid | yes | |
| 6 | The type's resolve: may replace the value (a player name with a client index) | yes | yes |
| 7 | The type's verify | yes | yes |
| 8 | The type's prepare | yes | yes |
| 9 | processValue | yes | yes |

- Steps 6 to 9 run only on the values the parse uses.
- Steps 6, 7 and 8 each run on all values of every level of the command path before the next step starts.
- Step 9 runs per level.
- Defaults pass steps 1 to 5 when the spec is checked, and steps 6 to 9 in each parse that uses them.
- `date` and `duration` defaults pass steps 1 to 5 again in each parse that uses them.
- A `hash` or `objectlist` word without `=` is a command line error, found while the line is read, before step 1.
- Command line mistakes (unknown options, a missing value) are found from the outermost level inwards. The values
  are then checked from the innermost level outwards.

## Automatic options

| Option | On | Answers with |
|---|---|---|
| `--help` | every level | the help of that level |
| `--version` | the top level | the spec's name and version: `sm_backup 1.2.0` |

- They answer as soon as their level has been read. Nothing else is checked first: `sm_backup --help` works without
  the required map.
- Only a malformed command line on the same level, or on an outer one, wins over them.
- Levels after theirs are not read.
- The help does not list them.
- There is no short form. `-h` is unknown unless you declare an option `h`, which is then an ordinary flag. See
  [examples.md](examples.md#parse-a-line-yourself).

## Help output

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
```

### Sections

In this order:

1. **Usage line**: the spec's name, the command path, `[options]` when the help lists options, `<command>` when the
   level has commands, then the args. Required args by name, optional ones in brackets, a multiple arg followed by
   `...`.
2. **Description**, when set.
3. **Arguments**: one entry per arg.
4. **Option groups**: one section per group, sorted by byte value (`Zed` before `aaa`). Inside a group: the level's
   own options sorted by key, then the inherited ones. Hidden options are left out.
5. **Commands**: each command with its description, sorted by byte value.
6. **Examples**, when set.

Groups, options and commands are sorted. Args and examples keep the order they were declared in.

### An option's entry

The names come first, primary name first, each as typed. Then a placeholder for the value:

| Entry | For |
|---|---|
| `--name` | flag, counter |
| `--[no-]name` | bool |
| `--name <>` | a value |
| `--name <a,b,...>` | csv |
| `--name <key=value>` | hash |
| `--name <N.key=value>` | objectlist |

A bool shows its negation on its first name longer than one letter (`--[no-]color, -c`). With only one-letter names,
the negation follows them (`-c, --no-c`).

The text next to the names, in this order:

1. `[REQ]` for a required option or arg.
2. The notes of a registered type, in brackets: `[RRGGBB]`.
3. The help text.
4. The type's label or the typehint: `[URL]`, `[Date]`, `[Duration]`, `[Map]`, `[Hostname]`.

Below it:

- `Valid   = [ a, b, c ]` for a fixed valid list.
- `Default = ...` with the default as written: a list as `a, b`, a mapping as `k=v, k2=v2`, an objectlist as
  `0.host=a, 0.port=80`.

### Width

- Option, arg and command entries are wrapped to the spec's [`HelpWidth`](#levels), 100 columns by default. No
  line is longer than `HelpWidth - 1` columns.
- The usage line, the description, the `Valid` and `Default` lines and the examples are not wrapped.
- A word longer than a line stays whole.
- If the labels alone are wider than `HelpWidth`, lines grow past it.

### Help from your plugin

| Call | Does |
|---|---|
| `Getopt_PrintHelp(result, client)` | Prints the help of a parsed result's level to the client's console. |
| `Getopt_PrintVersion(result, client)` | Prints the version line. |
| `result.HelpLines`, `result.GetHelpLine(line, buffer, maxlength)` | The help, line by line. |
| `result.GetVersion(buffer, maxlength)` | The version line. |

```sourcepawn
if (opt.Count("files") == 0)
	Getopt_PrintHelp(opt, client);       // nothing to do: show the help
```

## Reading results

Values are read by the option's primary name or the arg's name, with the methods of its shape:

| Shape | Methods |
|---|---|
| single value | `GetString`, `GetInt`, `GetFloat`, `GetBool`, `GetDuration`, `GetAttribute` |
| list (`multiple` option or arg) | `Count`, `GetStringAt`, `GetIntAt`, `GetFloatAt`, `GetAttributeAt` |
| mapping (`hash`) | `Count`, `GetKey`, `HasKey`, `GetStringOf`, `GetIntOf`, `GetFloatOf` |
| list of mappings (`objectlist`) | `Count`, `FieldCount`, `GetField`, `HasField`, `GetStringIn`, `GetIntIn`, `GetFloatIn` |
| any | `IsSet` |

### Single values

| Method | Returns |
|---|---|
| `GetString(name, buffer, maxlength)` | The value as text. False and an empty buffer when there is no value. |
| `GetInt(name, fallback = 0)` | The value as a whole number. `fallback` when there is no value. |
| `GetFloat(name, fallback = 0.0)` | The value as a number. `fallback` when there is no value. |
| `GetBool(name)` | Whether a flag or bool is on. False when there is no value. |
| `GetDuration(name, months, days, seconds)` | The parts of a duration. False when there is no value. |
| `IsSet(name)` | Whether there is a value, from the command line or the default. For a list or mapping: whether it has an entry. |
| `Count(name)` | For a single value: 1 when set, else 0. |

### Text and numbers

What each type gives:

| Type | `GetString` | `GetInt` | `GetFloat` |
|---|---|---|---|
| flag, bool | `1` or `0` | 1 or 0 | 1.0 or 0.0 |
| counter | the count | the count | the count |
| string, url | the text | the number, if the text is a whole number | the number, if the text is one |
| int | the number, without `+` or leading zeros (`7` for `+007`) | the number | the number |
| int with bigint | the number, exactly | the number, if it fits a cell | the number, rounded |
| float | up to 15 significant digits (`1000` for `1e3`, `0.5` for `.5`, `1e+20` for `1e20`) | the number, if it is whole | the number |
| date | `2026-10-06T14:00:00` | the Unix timestamp | the Unix timestamp, rounded (within about a minute inside a cell) |
| duration | `90 minutes` | its seconds from the moment of the parse | the same |

Errors of the plugin:

- `GetInt` or `GetFloat` on a value that is no such number.
- `GetInt` on a number beyond a cell. Dates and durations fit a cell unless their `min` or `max` lets more through.

### Lists

For `multiple` options and args. Index 0 to `Count(name) - 1`.

| Method | Returns |
|---|---|
| `Count(name)` | The number of values. |
| `GetStringAt(name, index, buffer, maxlength)` | One value as text. |
| `GetIntAt(name, index)` | One value as a whole number. |
| `GetFloatAt(name, index)` | One value as a number. |

### Mappings

For `hash` options. Keys are sorted by byte value: `B` before `a`.

| Method | Returns |
|---|---|
| `Count(name)` | The number of keys. |
| `GetKey(name, index, buffer, maxlength)` | The key at index. |
| `HasKey(name, key)` | Whether the mapping has the key. |
| `GetStringOf(name, key, buffer, maxlength)` | The key's value as text. False when there is no such key. |
| `GetIntOf(name, key, fallback = 0)` | The key's value as a whole number. `fallback` when there is no such key. |
| `GetFloatOf(name, key, fallback = 0.0)` | The key's value as a number. `fallback` when there is no such key. |

### Lists of mappings

For `objectlist` options. Fields are sorted by byte value: `B` before `a`.

| Method | Returns |
|---|---|
| `Count(name)` | The number of entries. |
| `FieldCount(name, entry)` | The number of fields of an entry. |
| `GetField(name, entry, index, buffer, maxlength)` | The field name at index. |
| `HasField(name, entry, field)` | Whether the entry has the field. |
| `GetStringIn(name, entry, field, buffer, maxlength)` | The field's value as text. False when the entry has no such field. |
| `GetIntIn(name, entry, field, fallback = 0)` | The field's value as a whole number. `fallback` when there is none. |
| `GetFloatIn(name, entry, field, fallback = 0.0)` | The field's value as a number. `fallback` when there is none. |

### Attributes

A registered type may give a value named attributes besides its text. The
[`target`](sourcemod-types.md#player-and-target) type gives the clients it matched.

| Method | Returns |
|---|---|
| `GetAttribute(name, attribute, buffer, maxlength)` | An attribute of a single value. |
| `GetAttributeAt(name, index, attribute, buffer, maxlength)` | An attribute of one list item. |

- Both return false and an empty buffer when the value has no such attribute.
- Built-in types give no attributes.
- A value that processValue replaced keeps its attributes.

### Low-level readers

The value readers above are stocks on top of seven natives. Use the natives for what no stock covers, such as the
duration of a list item:

```sourcepawn
int months, days, seconds;
opt.ReadDuration("delay", 2, "", months, days, seconds);    // item 2 of a multiple duration option
```

Every native names a value by `name`, `entry` and `key`:

| Shape | `entry` | `key` |
|---|---|---|
| single value | -1 | `""` |
| list | the index | `""` |
| mapping | -1 | the key |
| list of mappings | the index | the field |

| Native | Returns |
|---|---|
| `ReadString(name, entry, key, buffer, maxlength)` | The text. False and an empty buffer when there is no value. |
| `ReadInt(name, entry, key, &value)` | The whole number into `value`. False when there is no value. |
| `ReadFloat(name, entry, key, &value)` | The number into `value`. False when there is no value. |
| `ReadDuration(name, entry, key, &months, &days, &seconds)` | The parts of a duration. False when there is no value. |
| `ReadAttribute(name, entry, key, attribute, buffer, maxlength)` | An attribute. False when there is none. |
| `KeyCount(name, entry)` | The number of keys of a mapping (`entry` -1) or of an objectlist entry. |
| `ReadKey(name, entry, index, buffer, maxlength)` | The key or field at `index`. |

### Outcome, output and command

| Member | Returns |
|---|---|
| `Outcome` | How the parse ended. See [Outcomes](#outcomes). |
| `OutputLines`, `GetOutputLine(line, buffer, maxlength)` | What to show the user when the outcome is not `Getopt_Parsed`. |
| `GetError(buffer, maxlength)` | The error alone, without the help. True when the line was rejected. |
| `GetCommand(buffer, maxlength)` | The command selected on this level. False when there is none. |
| `Subcommand` | The result of the selected command's level: a new handle to delete, or null. |
| `HelpLines`, `GetHelpLine(line, buffer, maxlength)` | The help of this level. |
| `GetVersion(buffer, maxlength)` | The version line. |

### Line length

A buffer of `GETOPT_LINE_LENGTH` (512) holds every line of an output or a help. The extension splits a line longer
than 500 bytes into several, such as an error that lists many allowed values.

`GetError` gives the error whole, unsplit. A buffer of 512 bytes may cut a long one.

### Reading mistakes

These are errors of the plugin. They stop the callback and go to SourceMod's error log:

- reading a value with the methods of another shape;
- a name the level does not have;
- an index a list does not have;
- reading anything but `Outcome`, `OutputLines`, `GetOutputLine`, `GetError` and `GetVersion` from a result that is
  not `Getopt_Parsed`.

[messages.md](messages.md#errors-of-the-plugin) lists the messages.

## Types of your own

A plugin can register types. Example: a type that takes only even numbers.

```sourcepawn
GetoptSpec g_Workers;

public void OnPluginStart()
{
	Getopt_RegisterType("even").SetLabel("Even").OnCheck(Even_Check);   // before the spec is checked

	g_Workers = new GetoptSpec("sm_workers");
	g_Workers.Option("workers", "even").Default("2").Help("Number of workers, in pairs");
	g_Workers.Validate();
	RegAdminCmd("sm_workers", Command_Workers, ADMFLAG_ROOT);
}

Action Command_Workers(int client, int args)
{
	GetoptResult opt = Getopt_ParseCommand(g_Workers, client);
	if (opt == null)
		return Plugin_Handled;
	ReplyToCommand(client, "workers is %d", opt.GetInt("workers"));
	delete opt;
	return Plugin_Handled;
}

bool Even_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength)
{
	int number;
	if (value[0] == '\0' || StringToIntEx(value, number) != strlen(value))
	{
		Format(problem, maxlength, "'%s' is not a whole number", value);
		return false;
	}
	if (number % 2 != 0)
	{
		Format(problem, maxlength, "%d is not an even number", number);
		return false;
	}
	return true;
}
```

```
sm_workers --workers 8        workers is 8
sm_workers --workers 7        ERROR: option '--workers': 7 is not an even number
```

[examples.md](examples.md#a-type-of-your-own) has a fuller one, with a key, a conversion and notes.

### `Getopt_RegisterType`

```sourcepawn
GetoptType Getopt_RegisterType(const char[] names, GetoptKind kind = GetoptKind_Value, const char[] keys = "",
                               bool shared = false)
```

| Parameter | Meaning |
|---|---|
| `names` | The names it answers to, separated by spaces. Case insensitive. |
| `kind` | How its options appear on the command line. See below. |
| `keys` | Extra spec keys its options and args may `Set`, separated by spaces. Unknown keys with any other type. |
| `shared` | False: only this plugin's specs can name it. True: every plugin's specs can. See [Shared types](#shared-types). |

| Kind | Command line | Like |
|---|---|---|
| `GetoptKind_Value` (default) | `--name VALUE` | `string` |
| `GetoptKind_Flag` | `--name` | `flag` |
| `GetoptKind_Bool` | `--name`, `--no-name` | `bool` |
| `GetoptKind_Counter` | `--name --name` | `counter` |

- A name that is taken is an error of the call: by a built-in type, by one of the plugin's own types, or (for a
  shared type) by another shared type. A type of yours never replaces `string` or `int` by accident.
- Types of the kinds flag, bool and counter cannot be args, and cannot be `multiple`, `hash` or `objectlist`.
- The type belongs to the plugin and is gone when it unloads.
- Register a type before the first spec that uses it is checked.

### Type callbacks

Every method returns the type, so calls chain. All are optional.

| Method | Callback (typedef) | Runs |
|---|---|---|
| `OnCheck(cb)` | `GetoptTypeCheck`: `bool (keys, value, problem, maxlength)` | On each value. Returns true to accept, or writes the problem and returns false. A value that fails reaches no other callback. |
| `OnCoerce(cb)` | `GetoptTypeCoerce`: `void (keys, value, coerced, maxlength)` | After the check. Writes what the result holds into `coerced`, which starts as the value. Valid list and lazyValid see the result. |
| `OnResolve(cb)` | `GetoptTypeResolve`: `bool (keys, value, resolved, problem, maxlength)` | On the values the parse uses, defaults included, before any is verified. Gets the converted value. Returns true to accept, or writes the problem and returns false. Looks the value up, may replace it (`resolved.SetValue`) and add attributes (`resolved.SetAttribute`). |
| `OnVerify(cb)` | `GetoptTypeCheck`: `bool (keys, value, problem, maxlength)` | After every value resolved. For checks that depend on the server, such as whether a map exists. Never on a default while the spec is checked. |
| `OnPrepare(cb)` | `GetoptTypeCheck`: `bool (keys, value, problem, maxlength)` | After every value of the parse passed every check, verify included. For arranging what a value needs. A failure ends the parse; what earlier values prepared stays. |
| `OnCheckKeys(cb)` | `GetoptTypeCheckKeys`: `bool (keys, problem, maxlength)` | When the spec is checked, once per option or arg of the type. False is a spec error with the problem text, or `invalid spec keys` without one. |
| `OnNotes(cb)` | `GetoptTypeNotes`: `void (keys, notes)` | When the help is made. `notes.Add(text)` adds a note shown in brackets before the help text. |
| `SetLabel(text)` | | The label in brackets after the help text: `[Color]`. |

Resolve, verify and prepare never run when the spec is checked; check, coerce, checkKeys and notes do.

The `On...` methods are stocks around one native, `Getopt_SetTypeCallback(type, which, callback)`, with `which` one of
`GetoptTypeCallback_Check`, `GetoptTypeCallback_Coerce`, `GetoptTypeCallback_Verify`, `GetoptTypeCallback_Prepare`,
`GetoptTypeCallback_CheckKeys`, `GetoptTypeCallback_Notes` and `GetoptTypeCallback_Resolve`.

The `problem` text:

- Leave out the option's name. The extension adds `option '--NAME': `.
- Without a text, the message is `'VALUE' is not a valid value`.

### `GetoptKeys`

Every callback gets the keys of its type (the `keys` of `Getopt_RegisterType`) that the option or arg set. Other keys,
such as `help`, are not in it.

| Member | Returns |
|---|---|
| `keys.Get(key, buffer, maxlength)` | The key's value. False when the option or arg did not set it. |
| `keys.Has(key)` | Whether the option or arg set the key. |
| `keys.Caller` | In resolve, verify and prepare: the client the parse runs for, 0 for the server. In check, coerce, checkKeys and notes: -1, since they also run without a parse. |

`GetoptKeys`, `GetoptList` and `GetoptResolved` are lent to the callback. Do not keep them, and do not pass them to
another plugin.

### Shared types

A type registered with `shared` true can be named by every plugin's specs. The
[SourceMod types](sourcemod-types.md) are shared types of one plugin.

- Its callbacks run in the plugin that registered it.
- No two shared types share a name.
- A plugin's own type of a name comes first in that plugin's specs, before a shared one.
- When the sharing plugin unloads, its types are gone from every spec. Specs check themselves again on their next
  use. A spec that names the type is then a spec error, until the plugin is back.
- SourceMod starts plugins in load order, not in dependency order. Validate a spec that names another plugin's types
  in `OnAllPluginsLoaded`.

## Differences from Getopt::Pad

The extension follows Getopt::Pad 0.06. It differs where a plugin differs from a Perl program.

**Left out**, of no use on a game server:

- Config files: the `config` block, `--config`, `--create-default-config` and the file formats.
- Shell completion: `--create-completions`.
- The `file` and `dir` types, with `mustExist` and `createPathIfMissing`. Their verify and prepare steps remain for
  types you register.
- Colors and terminal width. The help width is a spec setting.

**Added**: the caller of a parse, the resolve step, attributes, shared types and the SourceMod types.

**Different**:

- A spec is built with method calls, not a data structure. It is checked on first use, not on every call.
- Values are read by name (`opt.GetInt("keep")`), not through generated methods. So there are no reserved names
  besides `help` and `version`. In exchange, an option and an arg of one level cannot share a name.
- Nothing exits. A parse returns a result with an outcome. `Getopt_ParseCommand` returns `null` where Perl ends the
  program.
- An `int` is a 32-bit cell. Larger numbers need `bigint` and are read as text.
- A `date` is a Unix timestamp or text, a `duration` its parts or seconds. Perl has DateTime objects. The date
  grammar is a documented subset of DateTime::Format::Natural ([dates.md](dates.md)). Both have `min` and `max`, and
  must fit a cell unless those allow more.
- A valid callback is asked once per option and parse, not once per value.
- `processValue` returns text, not any Perl value.
- Types are registered per plugin or shared, with callbacks instead of a class. They get a kind instead of a
  Getopt::Long suffix.
- A flag's default is `1` or `0`. The config file booleans went with the config files.
- A mapping's keys are sorted by byte value.
- The command line is one string the extension splits. See [Words and quotes](#words-and-quotes).
- Only a dash starts an option. Getopt::Long also takes `+name`.
- A bool whose negation is another option's name is a spec error. In Perl one of the two silently wins.
- A float has no whitespace around it. Perl's `0 but true` is not a number.
- A help text is wrapped as words. Its line breaks and runs of spaces are not kept.
- Lines longer than 500 bytes are handed out in pieces.
