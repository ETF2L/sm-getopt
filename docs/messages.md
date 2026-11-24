# Getopt messages

Every message the `getopt` extension produces, with its meaning. Search this file for the fixed part of a message.

Upper case words stand for actual values: NAME, VALUE, MIN, ...

Contents:

- [User errors](#user-errors): mistakes on the command line, told to the user.
- [Spec errors](#spec-errors): mistakes in a spec, in SourceMod's error log.
- [Errors of the plugin](#errors-of-the-plugin): wrong use of the API, in SourceMod's error log.
- The messages of the SourceMod types are in [sourcemod-types.md](sourcemod-types.md).

## User errors

A user error ends the parse as `Getopt_Rejected`. The user sees `ERROR: MESSAGE`, an empty line and the help of the
level the error belongs to:

```
ERROR: missing required argument <map>

# sm_backup [options] map
# Copy a map's files to the backup folder.
...
```

- Only the first error is reported.
- Exception: all problems of one level's command line, such as several unknown options, are reported together,
  separated by `; `.
- `result.GetError` gives the message without `ERROR: ` and without the help.

How options are named:

- In messages that start with `Option` or `Unknown option`, NAME is the option as the user typed it, without dashes:
  `Unknown option: kep`.
- All other messages use the primary name with its dashes: `option '--keep': ...`, `option '-k': ...` for a
  one-letter primary name.

### Command line errors

| Message | Meaning |
|---|---|
| `Unknown option: NAME` | No such option on this level. Names cannot be abbreviated. Maybe it belongs to another level, or must come before the command word. |
| `Option NAME requires an argument` | The option takes a value, but the line ends after it, or it was given as `--name=`. |
| `Option NAME does not take an argument` | `--name=value` on a flag, bool or counter. |
| `Option NAME, key "KEY", requires a value` | A `hash` or `objectlist` option got a word without `=`. |
| `missing required option '--NAME'` | A required option was not given. |
| `missing required argument <NAME>` | A required arg is missing. |
| `unexpected extra argument 'WORD'` | More words than the level has args. |
| `missing command, expected one of: NAMES` | The level has commands, and none was named. |
| `unknown command 'WORD', expected one of: NAMES` | The word after a level's options is none of its commands. |
| `option '--NAME': PROBLEM` | An option's value failed a check. PROBLEM is one of the [value problems](#value-problems). |
| `option '--NAME': default value: PROBLEM` | The default failed in this parse: the resolve, verify or prepare callback of its type, or the check of a `date` or `duration` default. |
| `argument <NAME>: PROBLEM` | An arg's value failed a check. |

### Value problems

The PROBLEM part of the messages above.

| Problem | Meaning |
|---|---|
| `'VALUE' is not an integer` | Not an `int`. |
| `'VALUE' is too large for an integer` | An `int` beyond a cell. The option needs `BigInt` to take it. |
| `'VALUE' is not a number` | Not a `float`. |
| `'VALUE' is not a finite number` | A `float` spelled `inf`, `infinity` or `nan`, or too large (`1e999`). |
| `VALUE is smaller than the minimum of MIN` | Below the `min` of an `int` or `float`. |
| `VALUE is larger than the maximum of MAX` | Above the `max` of an `int` or `float`. |
| `'VALUE' is earlier than the minimum of MIN` | A `date` before its `min`, or before 1901-12-13T20:45:52Z without one. MIN is shown in the option's time zone. |
| `'VALUE' is later than the maximum of MAX` | A `date` after its `max`, or after 2038-01-19T03:14:07Z without one. MAX is shown in the option's time zone. |
| `'VALUE' is shorter than the minimum of MIN` | A `duration` shorter than its `min`. |
| `'VALUE' is longer than the maximum of MAX` | A `duration` longer than its `max`, or than 2147483647 seconds without one. |
| `'VALUE' cannot be compared with a bound that is no date right now` | A relative `min` or `max` of a `date` lies outside the years 1 to 9999 at this moment. |
| `'VALUE' is not a URL` | Not of the form `scheme://...`. |
| `'VALUE' is not a date` | See [dates.md](dates.md). |
| `'VALUE' is not a duration` | See [dates.md](dates.md#durations). |
| `'VALUE' is not one of: VALUES` | Not in the valid list (`Valid` or `ValidFrom`). |
| `'VALUE' is not a valid value` | `LazyValid` said no, or a type's callback refused without a text. |
| `'VALUE' contains an empty item` | A `csv` value with an empty item: `a,,b`. |
| `empty key` | A `hash` option got `=VALUE`. |
| `key 'KEY': PROBLEM` | The value of KEY of a `hash` option failed a check. |
| `invalid key 'KEY', expected INDEX.FIELD=VALUE` | An `objectlist` word of another form. |
| `invalid key 'KEY', the index must not have leading zeros` | An `objectlist` index such as `00`. |
| `missing index N` | The indices of an `objectlist` option have a gap. |
| `entry N: PROBLEM` | An `objectlist` entry failed a check. |

Types you register write their own problems. See [Type callbacks](reference.md#type-callbacks).

## Spec errors

A spec error is a bug in the plugin. `Validate` and `Parse` fail with an error that starts with `Getopt spec:`.
SourceMod logs it with the plugin and the line of the call:

```
[SM] Exception reported: Getopt spec: option 'keep': default value: 'seven' is not an integer
[SM] Blaming: backup.smx
[SM] Call stack trace:
[SM]   [0] GetoptSpec.Validate
[SM]   [1] Line 41, backup.sp::OnPluginStart
```

- The whole spec is checked, all commands included, before any command line is read.
- Messages about a command's level start with its path: `Getopt spec: command 'user add': option 'role': ...`.
- Messages about an arg start with `arg 'NAME':` where those about an option start with `option 'NAME':`.
- Full form: `Getopt spec: [command 'PATH': ][option 'NAME': | arg 'NAME': ]MESSAGE`.
- The tables include `option 'NAME':` only for messages that always carry it. The other prefixes may come before any
  message.

### Names and structure

| Message | Meaning |
|---|---|
| `unknown key(s): KEYS` | A key the level, option, arg or type does not know, such as `min` on a `string`. |
| `option key must be a non-empty string` | `Option("")`. |
| `option 'KEY': invalid name 'NAME'` | A name that does not start with a letter, has characters other than letters, digits, `_` and `-`, or is empty (`owner\|`). |
| `arg: invalid name 'NAME'` | The same for an arg. |
| `invalid command name 'NAME'` | The same for a command. |
| `command 'NAME' is declared twice` | Two commands of one level share a name. |
| `option 'NAME': name 'N' is already used by option 'OTHER'` | Two options of one level share a name. |
| `option 'NAME': name 'N' is already used by an alias of option 'OTHER'` | A name is another option's alias. |
| `option 'NAME' inherited from LEVEL: name 'N' is already used by option 'OTHER'` | An option reuses a name of an inherited option. LEVEL is `the top level` or `command 'PATH'`. |
| `option 'NAME' collides with the automatic --NAME option` | An option named `help`, or `version` on the top level. |
| `option 'NAME': its negation '--no-N' is already used by option 'OTHER'` | `--no-N` and `--noN` turn the bool `N` off, so no other option may have those names. |
| `arg 'NAME' and option 'NAME' share one name` | Values are read by name, so names must differ. |
| `args and commands are mutually exclusive on one level` | A level with both. |
| `commandRequired without commands` | `CommandRequired` on a level without commands. |
| `helpWidth must be a whole number from 1 to 10000, not 'VALUE'` | A bad `HelpWidth`. |

### Types and shapes

| Message | Meaning |
|---|---|
| `option 'NAME': unknown option type 'TYPE' (known: NAMES)` | No such type. Maybe the plugin sharing it is not loaded yet: validate in `OnAllPluginsLoaded`. |
| `option 'NAME': required and default are mutually exclusive` | Both `Required` and a default. |
| `option 'NAME': multiple requires a value-taking type, not 'TYPE'` | `Multiple` on a flag, bool or counter. |
| `option 'NAME': hash requires a value-taking type, not 'TYPE'` | `Hash` on a flag, bool or counter. |
| `option 'NAME': objectlist requires a value-taking type, not 'TYPE'` | `ObjectList` on a flag, bool or counter. |
| `option 'NAME': multiple and hash are mutually exclusive` | Both shapes. |
| `option 'NAME': multiple and objectlist are mutually exclusive` | Both shapes. |
| `option 'NAME': hash and objectlist are mutually exclusive` | Both shapes. |
| `option 'NAME': multiple and hash and objectlist are mutually exclusive` | All three shapes. |
| `option 'NAME': csv requires multiple` | `Csv` without `Multiple`. |
| `option 'NAME': inherit requires commands on the same level` | `Inherit` on a level without commands. |
| `option 'NAME': typehint must be a non-empty string` | `TypeHint("")`. |
| `arg 'NAME': type 'TYPE' cannot be used for a positional arg` | An arg of a type that takes no value. |
| `arg 'NAME': multiple is only allowed on the last arg` | `Multiple` on an arg that is not the last. |
| `arg 'NAME': a required arg cannot follow an optional one` | Required args come first. |
| `option 'NAME': invalid spec keys` | A registered type's `OnCheckKeys` refused without a text. With a text, that text is the message. |

### Defaults

| Message | Meaning |
|---|---|
| `option 'NAME': default value: PROBLEM` | The default failed a check. PROBLEM is one of the [value problems](#value-problems). |
| `option 'NAME': default value: expected a single value, not a list or mapping` | Two defaults, or a pair or field, for a single-value option. |
| `option 'NAME': default value: expected a list of values` | A pair or field for a `multiple` option. |
| `option 'NAME': default value: expected a mapping of keys to values` | Something other than `DefaultPair` for a `hash` option. |
| `option 'NAME': default value: expected a list of mappings` | Something other than `DefaultField` for an `objectlist` option. |
| `option 'NAME': default value: 'VALUE' is not a boolean (use 1 or 0)` | The default of a flag or bool. |
| `option 'NAME': default value: 'VALUE' is not a count` | The default of a counter. |

### Bounds and time zones

| Message | Meaning |
|---|---|
| `option 'NAME': min must be a number, not 'VALUE'` | A bad `min` of an `int` or `float`. |
| `option 'NAME': max must be a number, not 'VALUE'` | A bad `max` of an `int` or `float`. |
| `option 'NAME': min MIN is larger than max MAX` | The bounds leave no value. |
| `option 'NAME': min must be an integer with bigint, not 'VALUE'` | A bad `min` of a `bigint` option. |
| `option 'NAME': max must be an integer with bigint, not 'VALUE'` | A bad `max` of a `bigint` option. |
| `option 'NAME': min must be a date or a Unix timestamp, not 'VALUE'` | A bad `min` of a `date` option. |
| `option 'NAME': max must be a date or a Unix timestamp, not 'VALUE'` | A bad `max` of a `date` option. |
| `option 'NAME': min MIN is later than the cell's maximum of MAX; set max as well` | A `date` `min` after 2038-01-19T03:14:07Z without a `max`. |
| `option 'NAME': max MAX is earlier than the cell's minimum of MIN; set min as well` | A `date` `max` before 1901-12-13T20:45:52Z without a `min`. |
| `option 'NAME': min must be a duration or a number of seconds, not 'VALUE'` | A bad `min` of a `duration` option. |
| `option 'NAME': max must be a duration or a number of seconds, not 'VALUE'` | A bad `max` of a `duration` option. |
| `option 'NAME': min MIN is longer than the cell's maximum of 2147483647 seconds; set max as well` | A `duration` `min` beyond a cell without a `max`. |
| `option 'NAME': timezone 'ZONE' is not a known time zone` | No such zone, or the server has no time zone files. |
| `option 'NAME': timezone must be a non-empty string` | `TimeZone("")`. |

## Errors of the plugin

Wrong use of the API. Each stops the plugin's callback and goes to SourceMod's error log.

### Registering types

| Message | Meaning |
|---|---|
| `Getopt: option type name 'NAME' is already registered` | `Getopt_RegisterType` with a taken name: a built-in type, one of the plugin's own types, or, for a shared type, another shared type. |
| `Getopt: an option type needs at least one name` | `Getopt_RegisterType("")`. |
| `Invalid GetoptKind N` | A `kind` outside the enum. |
| `Invalid type callback N` | A `which` of `Getopt_SetTypeCallback` outside the enum. |
| `N is no registered option type` | A `GetoptType` that is not one, or whose plugin unloaded. |

### Reading results

| Message | Meaning |
|---|---|
| `This result holds no values: the command line was rejected or answered` | Anything but `Outcome`, `OutputLines`, `GetOutputLine`, `GetError` and `GetVersion` read from a result that is not `Getopt_Parsed`. |
| `This level has no option or arg 'NAME'` | A wrong name, or a name of another level. |
| `'NAME' is a single value, read with GetString, GetInt, GetFloat or GetBool` | A single value read with the methods of another shape. |
| `'NAME' is a list, read with the ...At methods` | A list read with the methods of another shape. |
| `'NAME' is a mapping, read with the ...Of methods` | A `hash` read with the methods of another shape. |
| `'NAME' is a list of mappings, read with the ...In methods` | An `objectlist` read with the methods of another shape. |
| `'NAME' has no entry N (it has COUNT)` | An index outside the list. |
| `The value 'VALUE' of 'NAME' is no whole number` | `GetInt` on such a value. |
| `The value 'VALUE' of 'NAME' does not fit a cell; read it as a string` | `GetInt` on a whole number beyond a cell: a large `bigint`, or a `date` or `duration` whose bounds allow values beyond a cell. |
| `The value 'VALUE' of 'NAME' is no duration` | `GetDuration` on a value that is no duration, also a processed one. |
| `The duration 'VALUE' of 'NAME' does not fit cells` | `GetDuration` on a duration with a part beyond a cell, such as `5259492000 minutes`. |
| `'NAME' has no key N (it has COUNT)` | `GetKey` or `GetField` with an index outside the keys. |
| `There is no line N (there are COUNT)` | `GetOutputLine` or `GetHelpLine` with a line outside the output. |
| `The value 'VALUE' of 'NAME' is no number` | `GetFloat` on such a value. |

### Handles and callbacks

| Message | Meaning |
|---|---|
| `This spec belongs to another plugin` | Only the plugin that made a spec can use it, its options, args and levels. |
| `This result belongs to another plugin` | Only the plugin that parsed can use the result. |
| `This option type belongs to another plugin` | Only the plugin that registered a type can set its callbacks and label. |
| `N is no option or arg of an open spec` | An option or arg used after its spec was deleted, or a number that is none. |
| `N is no level of an open spec` | A level used after its spec was deleted. |
| `A spec cannot be changed while it is being checked` | A callback changed the spec whose check called it. |
| `A spec cannot be used while it is being checked` | A callback parsed with the spec whose check called it. During a parse both are fine. |
| `GetoptList can only be used inside the callback it was passed to` | A lent list kept after its callback. |
| `GetoptKeys can only be used inside the callback it was passed to` | Lent keys kept after their callback. |
| `GetoptResolved can only be used inside the callback it was passed to` | A lent resolve object kept after its callback. |
| `GetoptList was passed to another plugin's callback` | Another plugin used a lent list while the callback ran. |
| `GetoptKeys was passed to another plugin's callback` | Another plugin used lent keys while the callback ran. |
| `GetoptResolved was passed to another plugin's callback` | Another plugin used a lent resolve object while the callback ran. |
| `The NAME callback of ... could not be run` | A callback of yours failed. Its own error is logged before this one. |
| `The NAME callback of type 'TYPE' cannot run: its plugin has unloaded` | A shared type's plugin unloaded. A result kept from before then asked for its help, which runs the type's notes callback. |
| `Invalid caller N: a client index, or 0 for the server` | `Parse` with a caller that is no client index. |
| `A spec needs a name: the console command it describes` | `new GetoptSpec("")`. |
| `Invalid callback function N` | A callback that is no function of the plugin. |
| `Invalid GetoptSpec handle ...`, `Invalid GetoptResult handle ...` | A handle that is closed or of another kind. |
| `An attribute needs a name` | `GetoptResolved.SetAttribute` with an empty name. |
| `Invalid buffer size N` | A negative `maxlength`. |
| `Invalid string buffer` | A buffer outside the plugin's memory. |
| `Invalid string argument`, `Invalid by-reference argument` | An argument outside the plugin's memory. |
| `Could not create the spec handle (error N)`, `Could not create the result handle (error N)` | SourceMod refused a handle, such as when the plugin has too many. |
