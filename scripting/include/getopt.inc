/**
 * Copyright 2026 ETF2L Contributors, Dave
 *
 * Getopt: declarative parsing of console command arguments.
 *
 * A plugin describes the arguments of a command once, as a spec: every
 * option and positional argument with its type, default, allowed values and
 * help text. Parsing a command line with the spec checks every value and
 * gives a result to read the values from by name. The spec also answers
 * --help and --version, and words a wrong command line as an error with the
 * help below it.
 *
 *     GetoptSpec g_Backup;
 *
 *     public void OnPluginStart()
 *     {
 *         g_Backup = new GetoptSpec("sm_backup");
 *         g_Backup.Description("Copy a map's files to the backup folder.");
 *         g_Backup.Option("keep|k", "int").Default("7").Min(1).Help("Number of backups to keep");
 *         g_Backup.Option("verbose|v", "counter").Help("Print more; repeat for even more");
 *         g_Backup.Arg("map").Required().Help("Map to back up");
 *         g_Backup.Validate();
 *         RegAdminCmd("sm_backup", Command_Backup, ADMFLAG_ROOT);
 *     }
 *
 *     Action Command_Backup(int client, int args)
 *     {
 *         GetoptResult opt = Getopt_ParseCommand(g_Backup, client);
 *         if (opt == null)
 *             return Plugin_Handled;      // the user was told why
 *
 *         char map[64];
 *         opt.GetString("map", map, sizeof(map));
 *         int keep = opt.GetInt("keep");
 *         delete opt;
 *         ...
 *     }
 *
 * The project's README and docs/ describe everything in full.
 */

#if defined _getopt_included
#endinput
#endif
#define _getopt_included

/**
 * The size that fits every line a result hands out: the extension breaks
 * longer lines of an output or a help into several.
 */
#define GETOPT_LINE_LENGTH 512

/**
 * How a parse ended.
 */
enum GetoptOutcome {
	Getopt_Parsed = 0, /**< The command line is valid; the result holds its values */
	Getopt_Rejected,   /**< A mistake on the command line; the output is the error and the help */
	Getopt_Answered    /**< --help or --version was given; the output is its answer */
};

/**
 * How an option of a type you register appears on the command line.
 */
enum GetoptKind {
	GetoptKind_Flag = 0, /**< --name, takes no value */
	GetoptKind_Bool,     /**< --name and --no-name, takes no value */
	GetoptKind_Counter,  /**< --name --name or -nn, counts how often it is given */
	GetoptKind_Value     /**< --name VALUE */
};

/**
 * The spec keys an option or arg gave its type, such as "min" for an int.
 * Lent to the callbacks of a type you register; it cannot be kept or given
 * to another plugin.
 */
methodmap GetoptKeys {
	/**
	 * The client the parse runs for, 0 for the server, in the resolve,
	 * verify and prepare callbacks. -1 in the others, which also run when a
	 * spec is checked.
	 */
	property int Caller {
		public native get();
	}

	/**
	 * Whether the option or arg set the key.
	 */
	public native bool Has(const char[] key);

	/**
	 * Reads the key's value.
	 *
	 * @return            True when the option or arg set the key.
	 */
	public native bool Get(const char[] key, char[] buffer, int maxlength);
}

/**
 * What a type's resolve callback found for a value. Lent to the callback;
 * it cannot be kept or given to another plugin.
 */
methodmap GetoptResolved {
	/**
	 * Replaces the value with what it stands for, such as a player's name
	 * with the client index.
	 */
	public native void SetValue(const char[] value);

	/**
	 * Adds a named attribute to the value, which the plugin that parsed
	 * reads with GetAttribute: for example the clients a target matched.
	 */
	public native void SetAttribute(const char[] name, const char[] value);
}

/**
 * A list a callback fills. Lent to the callback; it cannot be kept or
 * given to another plugin.
 */
methodmap GetoptList {
	public native void Add(const char[] text);
}

/**
 * One level of a parse: how the parse ended and, when the command line was
 * valid, the values of the level's options and args. A handle: delete it
 * when you are done.
 *
 * Values are read by the option's primary name (the first name of its key)
 * or the arg's name, with the methods that fit the option's shape:
 *
 *     single value              GetString, GetInt, GetFloat, GetBool
 *     multiple, multiple arg    Count, GetStringAt, GetIntAt, GetFloatAt
 *     hash                      Count, GetKey, HasKey, GetStringOf, GetIntOf, GetFloatOf
 *     objectlist                Count, FieldCount, GetField, HasField, GetStringIn, GetIntIn, GetFloatIn
 *
 * Reading a value with the methods of another shape, a name the level does
 * not have, or values of a result that was not parsed is an error.
 */
methodmap GetoptResult < Handle {
	/**
	 * How the parse ended. Only a Getopt_Parsed result has values.
	 */
	property GetoptOutcome Outcome {
		public native get();
	}

	/**
	 * The lines to show the user when the parse did not end as
	 * Getopt_Parsed: the error, an empty line and the help of the level the
	 * error belongs to, or the answer of --help or --version.
	 */
	property int OutputLines {
		public native get();
	}

	public native void GetOutputLine(int line, char[] buffer, int maxlength);

	/**
	 * The error of a rejected command line, without the help.
	 *
	 * @return            True when the command line was rejected.
	 */
	public native bool GetError(char[] buffer, int maxlength);

	/**
	 * The name of the command selected on this level.
	 *
	 * @return            False when the level has no commands or none was given.
	 */
	public native bool GetCommand(char[] buffer, int maxlength);

	/**
	 * The result of the selected command's level, as a new handle to delete,
	 * or null when the level has no commands or none was given.
	 */
	property GetoptResult Subcommand {
		public native get();
	}

	/**
	 * Whether the option or arg has a value: from the command line or its
	 * default. For multiple, hash and objectlist: whether it has any entry.
	 */
	public native bool IsSet(const char[] name);

	/**
	 * The number of values of a multiple option or arg, of keys of a hash
	 * option, of entries of an objectlist option. For a single value 1 or 0.
	 */
	public native int Count(const char[] name);

	public native int KeyCount(const char[] name, int entry);
	public native void ReadKey(const char[] name, int entry, int index, char[] buffer, int maxlength);
	public native bool ReadString(const char[] name, int entry, const char[] key, char[] buffer, int maxlength);
	public native bool ReadInt(const char[] name, int entry, const char[] key, int &value);
	public native bool ReadFloat(const char[] name, int entry, const char[] key, float &value);
	public native bool ReadDuration(const char[] name, int entry, const char[] key, int &months, int &days,
	                                int &seconds);
	public native bool ReadAttribute(const char[] name, int entry, const char[] key, const char[] attribute,
	                                 char[] buffer, int maxlength);

	/* ------------------------------------------------------ single values */

	/**
	 * The value as text. Every value has one: a number as written without
	 * leading zeros, a date as 2026-10-06T14:00:00, a flag as 1.
	 *
	 * @return            False when the option or arg has no value; the
	 *                    buffer is empty then.
	 */
	public bool GetString(const char[] name, char[] buffer, int maxlength) {
		return this.ReadString(name, -1, "", buffer, maxlength);
	}

	/**
	 * The value as a whole number: an int, a counter's count, 1 or 0 of a
	 * flag or bool, a date as a Unix timestamp, a duration in seconds, or a
	 * text that is a whole number. Dates and durations fit a cell unless
	 * the option's min or max allows more.
	 *
	 * @param fallback    Returned when the option or arg has no value.
	 * @error             A value that is no whole number or does not fit a cell.
	 */
	public int GetInt(const char[] name, int fallback = 0) {
		int value;
		return this.ReadInt(name, -1, "", value) ? value : fallback;
	}

	/**
	 * The value as a number.
	 *
	 * @param fallback    Returned when the option or arg has no value.
	 * @error             A value that is no number.
	 */
	public float GetFloat(const char[] name, float fallback = 0.0) {
		float value;
		return this.ReadFloat(name, -1, "", value) ? value : fallback;
	}

	/**
	 * Whether a flag or bool is on. False when it has no value.
	 */
	public bool GetBool(const char[] name) {
		int value;
		return this.ReadInt(name, -1, "", value) && value != 0;
	}

	/**
	 * The parts of a duration: months and days as the calendar counts them,
	 * and seconds. "2 weeks" is 14 days, "1 year" 12 months, "90 minutes"
	 * 5400 seconds.
	 *
	 * @return            False when the option or arg has no value.
	 * @error             A value that is no duration.
	 */
	public bool GetDuration(const char[] name, int &months, int &days, int &seconds) {
		return this.ReadDuration(name, -1, "", months, days, seconds);
	}

	/**
	 * An attribute a type gave the value, such as "clients" of a target.
	 *
	 * @return            False when the option or arg has no value, or the
	 *                    value no such attribute; the buffer is empty then.
	 */
	public bool GetAttribute(const char[] name, const char[] attribute, char[] buffer, int maxlength) {
		return this.ReadAttribute(name, -1, "", attribute, buffer, maxlength);
	}

	/* ------------------------------------------- multiple options and args */

	public void GetStringAt(const char[] name, int index, char[] buffer, int maxlength) {
		this.ReadString(name, index, "", buffer, maxlength);
	}

	public int GetIntAt(const char[] name, int index) {
		int value;
		this.ReadInt(name, index, "", value);
		return value;
	}

	public float GetFloatAt(const char[] name, int index) {
		float value;
		this.ReadFloat(name, index, "", value);
		return value;
	}

	public bool GetAttributeAt(const char[] name, int index, const char[] attribute, char[] buffer, int maxlength) {
		return this.ReadAttribute(name, index, "", attribute, buffer, maxlength);
	}

	/* -------------------------------------------------------- hash options */

	/**
	 * A key of a hash option. The keys are sorted by byte value; index runs
	 * from 0 to Count(name) - 1.
	 */
	public void GetKey(const char[] name, int index, char[] buffer, int maxlength) {
		this.ReadKey(name, -1, index, buffer, maxlength);
	}

	public bool HasKey(const char[] name, const char[] key) {
		char unused[2];
		return this.ReadString(name, -1, key, unused, sizeof(unused));
	}

	/**
	 * @return            False when the hash has no such key.
	 */
	public bool GetStringOf(const char[] name, const char[] key, char[] buffer, int maxlength) {
		return this.ReadString(name, -1, key, buffer, maxlength);
	}

	public int GetIntOf(const char[] name, const char[] key, int fallback = 0) {
		int value;
		return this.ReadInt(name, -1, key, value) ? value : fallback;
	}

	public float GetFloatOf(const char[] name, const char[] key, float fallback = 0.0) {
		float value;
		return this.ReadFloat(name, -1, key, value) ? value : fallback;
	}

	/* -------------------------------------------------- objectlist options */

	/**
	 * The number of fields of one entry of an objectlist option.
	 */
	public int FieldCount(const char[] name, int entry) {
		return this.KeyCount(name, entry);
	}

	/**
	 * A field name of one entry. The fields are sorted by byte value.
	 */
	public void GetField(const char[] name, int entry, int index, char[] buffer, int maxlength) {
		this.ReadKey(name, entry, index, buffer, maxlength);
	}

	public bool HasField(const char[] name, int entry, const char[] field) {
		char unused[2];
		return this.ReadString(name, entry, field, unused, sizeof(unused));
	}

	/**
	 * @return            False when the entry has no such field.
	 */
	public bool GetStringIn(const char[] name, int entry, const char[] field, char[] buffer, int maxlength) {
		return this.ReadString(name, entry, field, buffer, maxlength);
	}

	public int GetIntIn(const char[] name, int entry, const char[] field, int fallback = 0) {
		int value;
		return this.ReadInt(name, entry, field, value) ? value : fallback;
	}

	public float GetFloatIn(const char[] name, int entry, const char[] field, float fallback = 0.0) {
		float value;
		return this.ReadFloat(name, entry, field, value) ? value : fallback;
	}

	/* ------------------------------------------------------ help and version */

	/**
	 * The help of this level, line by line. See Getopt_PrintHelp.
	 */
	property int HelpLines {
		public native get();
	}

	public native void GetHelpLine(int line, char[] buffer, int maxlength);

	/**
	 * The line --version prints: the spec's name and version.
	 */
	public native void GetVersion(char[] buffer, int maxlength);
}

/**
 * Gives the values an option allows right now, by adding them to values.
 * Called once in each parse whose command line gives the option, when its
 * first value is checked, and when the default is checked: with the spec,
 * and for a date or duration option in every parse that uses it.
 *
 * @param values     The list to fill.
 * @param option     The option's primary name.
 * @param data       The data passed to ValidFrom.
 */
typedef GetoptValidSource = function void (GetoptList values, const char[] option, any data);

/**
 * Tells whether a value is acceptable. Called after the type's conversion
 * and after the valid list.
 *
 * @param value      The converted value as text.
 * @param option     The option's primary name.
 * @param data       The data passed to LazyValid.
 * @return           True to accept the value.
 */
typedef GetoptLazyValid = function bool (const char[] value, const char[] option, any data);

/**
 * Converts a value into what the result returns for it. Called once all
 * options and args of the level passed their checks, once per value.
 *
 * @param values     The level's values before any was processed. It is
 *                   closed when the callback returns.
 * @param value      The checked value as text.
 * @param processed  What the result is to return; it starts as the value.
 * @param maxlength  The size of processed.
 * @param name       The option's primary name or the arg's name.
 * @param data       The data passed to ProcessValue.
 */
typedef GetoptProcessValue = function void (GetoptResult values, const char[] value, char[] processed, int maxlength,
                                            const char[] name, any data);

/**
 * An option of a spec. Every method returns the option, so calls can be
 * chained:
 *
 *     spec.Option("keep|k", "int").Default("7").Min(1).Help("Backups to keep");
 */
methodmap GetoptOption {
	/**
	 * Sets a spec key by name. The methods below are shorter ways to write
	 * the same; Set is needed for the keys of types you register.
	 */
	public native GetoptOption Set(const char[] key, const char[] value);

	/**
	 * The values the option accepts, a fixed list of strings:
	 * Valid("debug", "info", "warn"). The help lists them.
	 */
	public native GetoptOption Valid(any ...);

	/**
	 * The values the option accepts, asked for when its value is checked
	 * (see GetoptValidSource). Valid and ValidFrom replace each other.
	 */
	public native GetoptOption ValidFrom(GetoptValidSource callback, any data = 0);

	/**
	 * A check of your own for what a list cannot say.
	 */
	public native GetoptOption LazyValid(GetoptLazyValid callback, any data = 0);

	/**
	 * Converts each value into what the result returns for it.
	 */
	public native GetoptOption ProcessValue(GetoptProcessValue callback, any data = 0);

	/**
	 * The value when the command line does not set the option. Call it once
	 * per value for a multiple option.
	 */
	public native GetoptOption Default(const char[] value);

	/**
	 * One key=value pair of the default of a hash option.
	 */
	public native GetoptOption DefaultPair(const char[] key, const char[] value);

	/**
	 * One field of an entry of the default of an objectlist option. Entries
	 * are numbered from 0.
	 */
	public native GetoptOption DefaultField(int entry, const char[] field, const char[] value);

	/** The option must be given. Not together with a default. */
	public GetoptOption Required(bool required = true) {
		return this.Set("required", required ? "1" : "0");
	}

	/** The text next to the option in the help. */
	public GetoptOption Help(const char[] text) {
		return this.Set("help", text);
	}

	/** The heading the option is listed under in the help; "Options" without. */
	public GetoptOption Group(const char[] name) {
		return this.Set("group", name);
	}

	/** The option works, but the help does not list it. */
	public GetoptOption Hidden(bool hidden = true) {
		return this.Set("hidden", hidden ? "1" : "0");
	}

	/** The option may be given more than once; its values are a list. */
	public GetoptOption Multiple(bool multiple = true) {
		return this.Set("multiple", multiple ? "1" : "0");
	}

	/** Every value of a multiple option is split at commas. */
	public GetoptOption Csv(bool csv = true) {
		return this.Set("csv", csv ? "1" : "0");
	}

	/** The option takes KEY=VALUE words; its value is a mapping. */
	public GetoptOption Hash(bool hash = true) {
		return this.Set("hash", hash ? "1" : "0");
	}

	/** The option takes INDEX.FIELD=VALUE words; its value is a list of mappings. */
	public GetoptOption ObjectList(bool objectlist = true) {
		return this.Set("objectlist", objectlist ? "1" : "0");
	}

	/** The option is accepted on every command level below this one too. */
	public GetoptOption Inherit(bool inherit = true) {
		return this.Set("inherit", inherit ? "1" : "0");
	}

	/** Replaces the type's label at the end of the help text. */
	public GetoptOption TypeHint(const char[] label) {
		return this.Set("typehint", label);
	}

	/**
	 * The smallest accepted value of an int option; of a date option as a
	 * Unix timestamp, of a duration option in seconds. A date or duration
	 * bound can also be set as text with Set("min", "now").
	 */
	public GetoptOption Min(int min) {
		char value[16];
		IntToString(min, value, sizeof(value));
		return this.Set("min", value);
	}

	/**
	 * The largest accepted value of an int option; of a date option as a
	 * Unix timestamp, of a duration option in seconds. A date or duration
	 * that does not fit a cell needs a max set as text, such as
	 * Set("max", "9999-12-31") or Set("max", "10000 years"), and is read with
	 * GetString then.
	 */
	public GetoptOption Max(int max) {
		char value[16];
		IntToString(max, value, sizeof(value));
		return this.Set("max", value);
	}

	/** The smallest accepted value of a float option. */
	public GetoptOption MinFloat(float min) {
		char value[32];
		FloatToString(min, value, sizeof(value));
		return this.Set("min", value);
	}

	/** The largest accepted value of a float option. */
	public GetoptOption MaxFloat(float max) {
		char value[32];
		FloatToString(max, value, sizeof(value));
		return this.Set("max", value);
	}

	/**
	 * An int option accepts integers of any size, exact when read as a
	 * string. Its bounds are set with Set("min", "...") and Set("max", "...").
	 */
	public GetoptOption BigInt(bool bigint = true) {
		return this.Set("bigint", bigint ? "1" : "0");
	}

	/** The time zone of a date or duration option; "local" without. */
	public GetoptOption TimeZone(const char[] name) {
		return this.Set("timezone", name);
	}
}

/**
 * A positional argument of a spec. Every method returns the arg.
 */
methodmap GetoptArg {
	public native GetoptArg Set(const char[] key, const char[] value);

	/**
	 * Converts each value into what the result returns for it.
	 */
	public native GetoptArg ProcessValue(GetoptProcessValue callback, any data = 0);

	/** The arg must be given. Required args come before optional ones. */
	public GetoptArg Required(bool required = true) {
		return this.Set("required", required ? "1" : "0");
	}

	/** The last arg takes all remaining words; its values are a list. */
	public GetoptArg Multiple(bool multiple = true) {
		return this.Set("multiple", multiple ? "1" : "0");
	}

	public GetoptArg Help(const char[] text) {
		return this.Set("help", text);
	}

	public GetoptArg TypeHint(const char[] label) {
		return this.Set("typehint", label);
	}

	public GetoptArg Min(int min) {
		char value[16];
		IntToString(min, value, sizeof(value));
		return this.Set("min", value);
	}

	public GetoptArg Max(int max) {
		char value[16];
		IntToString(max, value, sizeof(value));
		return this.Set("max", value);
	}

	public GetoptArg MinFloat(float min) {
		char value[32];
		FloatToString(min, value, sizeof(value));
		return this.Set("min", value);
	}

	public GetoptArg MaxFloat(float max) {
		char value[32];
		FloatToString(max, value, sizeof(value));
		return this.Set("max", value);
	}

	public GetoptArg BigInt(bool bigint = true) {
		return this.Set("bigint", bigint ? "1" : "0");
	}

	public GetoptArg TimeZone(const char[] name) {
		return this.Set("timezone", name);
	}
}

/**
 * One level of a spec: the top level, or a command. A level has options and
 * either args or commands.
 */
methodmap GetoptLevel {
	public native GetoptLevel Set(const char[] key, const char[] value);

	/**
	 * Declares an option.
	 *
	 * @param key         The option's name, followed by its aliases separated
	 *                    by "|": "owner|o" is --owner with the alias -o.
	 * @param type        The type's name; "flag" without.
	 */
	public native GetoptOption Option(const char[] key, const char[] type = "");

	/**
	 * Declares the next positional argument.
	 *
	 * @param name        The name the value is read by.
	 * @param type        The type's name; "string" without.
	 */
	public native GetoptArg Arg(const char[] name, const char[] type = "");

	/**
	 * Declares a command and returns its level.
	 */
	public native GetoptLevel Command(const char[] name);

	/**
	 * Adds a usage example to the end of the level's help.
	 *
	 * @param text        A short explanation.
	 * @param args        The command line after the command's name.
	 */
	public native GetoptLevel Example(const char[] text, const char[] args);

	/** The line below the usage line of the level's help. */
	public GetoptLevel Description(const char[] text) {
		return this.Set("description", text);
	}

	/** Whether the command line must name one of the level's commands. */
	public GetoptLevel CommandRequired(bool required) {
		return this.Set("commandRequired", required ? "1" : "0");
	}
}

/**
 * The description of a command's arguments. A handle: it lives as long as
 * the plugin unless it is deleted.
 *
 * Declaring only collects; the spec is checked as a whole when it is first
 * used, by Validate or Parse. A mistake in it is an error of that call that
 * starts with "Getopt spec:".
 */
methodmap GetoptSpec < Handle {
	/**
	 * @param name        The console command the spec describes, as the help
	 *                    is to show it.
	 */
	public native GetoptSpec(const char[] name);

	/**
	 * The top level.
	 */
	property GetoptLevel Root {
		public native get();
	}

	/**
	 * Checks the whole spec now. Call it at the end of OnPluginStart, so a
	 * mistake in the spec shows when the plugin loads rather than when the
	 * command is first used.
	 *
	 * @error             A mistake in the spec.
	 */
	public native void Validate();

	/**
	 * Parses a command's argument string, as GetCmdArgString gives it.
	 * Whitespace separates words and double quotes keep it in a word.
	 *
	 * @param line        The argument string.
	 * @param caller      The client the command runs for, 0 for the server.
	 *                    Types that look players up target from its view:
	 *                    @me, immunity.
	 * @return            A result to delete, whatever its outcome.
	 * @error             A mistake in the spec, a callback that failed, or
	 *                    a caller that is no client index.
	 */
	public native GetoptResult Parse(const char[] line, int caller = 0);

	public GetoptOption Option(const char[] key, const char[] type = "") {
		return this.Root.Option(key, type);
	}

	public GetoptArg Arg(const char[] name, const char[] type = "") {
		return this.Root.Arg(name, type);
	}

	public GetoptLevel Command(const char[] name) {
		return this.Root.Command(name);
	}

	public void Example(const char[] text, const char[] args) {
		this.Root.Example(text, args);
	}

	public void Description(const char[] text) {
		this.Root.Set("description", text);
	}

	public void CommandRequired(bool required) {
		this.Root.Set("commandRequired", required ? "1" : "0");
	}

	/**
	 * What --version prints after the name. Without it, the version of the
	 * plugin's myinfo, or "unknown".
	 */
	public void Version(const char[] version) {
		this.Root.Set("version", version);
	}

	/**
	 * The width the help is wrapped to, 100 columns without.
	 */
	public void HelpWidth(int columns) {
		char value[16];
		IntToString(columns, value, sizeof(value));
		this.Root.Set("helpWidth", value);
	}
}

/**
 * Tells whether a value is acceptable to a type.
 *
 * @param keys       The spec keys the option or arg gave the type.
 * @param value      The value.
 * @param problem    What is wrong with the value, without the option's name:
 *                   "'x' is not a hexadecimal number".
 * @param maxlength  The size of problem.
 * @return           True to accept the value.
 */
typedef GetoptTypeCheck = function bool (GetoptKeys keys, const char[] value, char[] problem, int maxlength);

/**
 * Converts a value that passed the check into what the result returns.
 *
 * @param coerced    The converted value; it starts as the value.
 */
typedef GetoptTypeCoerce = function void (GetoptKeys keys, const char[] value, char[] coerced, int maxlength);

/**
 * Checks the spec keys one option or arg gave the type.
 *
 * @return           True when they are acceptable.
 */
typedef GetoptTypeCheckKeys = function bool (GetoptKeys keys, char[] problem, int maxlength);

/**
 * Gives the notes the help shows in brackets before the help text.
 */
typedef GetoptTypeNotes = function void (GetoptKeys keys, GetoptList notes);

/**
 * Looks a value up in what the parse runs in, such as the players on the
 * server, and may replace it with what it found.
 *
 * @param keys       The spec keys; keys.Caller is the client the parse runs for.
 * @param value      The value, converted.
 * @param resolved   What to replace the value with, and attributes to add.
 * @param problem    What is wrong, when the value finds nothing.
 * @param maxlength  The size of problem.
 * @return           True when the value is good.
 */
typedef GetoptTypeResolve = function bool (GetoptKeys keys, const char[] value, GetoptResolved resolved, char[] problem,
                                           int maxlength);

enum GetoptTypeCallback {
	GetoptTypeCallback_Check = 0,
	GetoptTypeCallback_Coerce,
	GetoptTypeCallback_Verify,
	GetoptTypeCallback_Prepare,
	GetoptTypeCallback_CheckKeys,
	GetoptTypeCallback_Notes,
	GetoptTypeCallback_Resolve
};

/**
 * An option type a plugin registered. Every method returns the type.
 */
methodmap GetoptType {
	/**
	 * The tag the help shows in brackets after the help text.
	 */
	public native GetoptType SetLabel(const char[] label);

	/** Checks a value. Without it every value is accepted. */
	public GetoptType OnCheck(GetoptTypeCheck callback) {
		Getopt_SetTypeCallback(this, GetoptTypeCallback_Check, callback);
		return this;
	}

	/** Converts a checked value. Without it the value stays as given. */
	public GetoptType OnCoerce(GetoptTypeCoerce callback) {
		Getopt_SetTypeCallback(this, GetoptTypeCallback_Coerce, callback);
		return this;
	}

	/**
	 * Checks what depends on the server rather than on the value, such as
	 * whether a map exists. Runs only on the values a parse ends up with,
	 * never on a default while the spec is checked.
	 */
	public GetoptType OnVerify(GetoptTypeCheck callback) {
		Getopt_SetTypeCallback(this, GetoptTypeCallback_Verify, callback);
		return this;
	}

	/**
	 * Arranges what a value needs. Runs only when every value of the parse
	 * passed all checks.
	 */
	public GetoptType OnPrepare(GetoptTypeCheck callback) {
		Getopt_SetTypeCallback(this, GetoptTypeCallback_Prepare, callback);
		return this;
	}

	/** Checks the type's spec keys of one option or arg. */
	public GetoptType OnCheckKeys(GetoptTypeCheckKeys callback) {
		Getopt_SetTypeCallback(this, GetoptTypeCallback_CheckKeys, callback);
		return this;
	}

	/** Gives the notes the help shows before the help text. */
	public GetoptType OnNotes(GetoptTypeNotes callback) {
		Getopt_SetTypeCallback(this, GetoptTypeCallback_Notes, callback);
		return this;
	}

	/**
	 * Looks a value up in what the parse runs in and may replace it. Runs on
	 * the values a parse ends up with, defaults included, before any is
	 * verified.
	 */
	public GetoptType OnResolve(GetoptTypeResolve callback) {
		Getopt_SetTypeCallback(this, GetoptTypeCallback_Resolve, callback);
		return this;
	}
}

/**
 * Registers an option type for the specs of this plugin, or of every
 * plugin.
 *
 * @param names      The names the type answers to, separated by spaces.
 * @param kind       Whether options of the type take a value.
 * @param keys       The extra spec keys options and args of the type may
 *                   set, separated by spaces.
 * @param shared     True to let every plugin's specs name the type. A
 *                   plugin's own type of the same name comes first in its
 *                   specs. The type is gone when this plugin unloads.
 * @error            A name that is taken: by a built-in type, by one of
 *                   this plugin's own types, or, for a shared type, by
 *                   another shared type.
 */
native GetoptType Getopt_RegisterType(const char[] names, GetoptKind kind = GetoptKind_Value, const char[] keys = "",
                                      bool shared = false);

native void Getopt_SetTypeCallback(GetoptType type, GetoptTypeCallback which, Function callback);

/**
 * Prints one line to a client's console, or to the server console for
 * client 0.
 */
stock void Getopt_PrintLine(int client, const char[] line) {
	if (client == 0) {
		PrintToServer("%s", line);
	} else {
		PrintToConsole(client, "%s", line);
	}
}

/**
 * Shows the user what a parse that did not end as Getopt_Parsed has to say:
 * the error with the help, or the answer of --help or --version. It goes to
 * the console; a command typed in chat also gets the error and a pointer to
 * the console as chat replies.
 */
stock void Getopt_Reply(GetoptResult result, int client) {
	bool inChat = client != 0 && GetCmdReplySource() == SM_REPLY_TO_CHAT;
	char line[GETOPT_LINE_LENGTH];

	if (inChat && result.GetError(line, sizeof(line))) {
		ReplyToCommand(client, "ERROR: %s", line);
	}

	int lines = result.OutputLines;
	for (int i = 0; i < lines; i++) {
		result.GetOutputLine(i, line, sizeof(line));
		Getopt_PrintLine(client, line);
	}

	if (inChat) {
		ReplyToCommand(client, "See console for output.");
	}
}

/**
 * Parses the arguments of the console command that is being run.
 *
 * @param spec       The command's spec.
 * @param client     The client that runs the command, 0 for the server.
 * @return           The values, a handle to delete; or null when the command
 *                   line was wrong or asked for --help or --version, in which
 *                   case the client has been told and the command is done.
 * @error            A mistake in the spec, or a callback that failed.
 */
stock GetoptResult Getopt_ParseCommand(GetoptSpec spec, int client) {
	/* The engine refuses a command line of 511 bytes or more, so its
	 * argument string always fits. */
	char line[GETOPT_LINE_LENGTH];
	GetCmdArgString(line, sizeof(line));

	GetoptResult result = spec.Parse(line, client);
	if (result.Outcome == Getopt_Parsed) {
		return result;
	}

	Getopt_Reply(result, client);
	delete result;
	return null;
}

/**
 * Prints the help of a parsed result's level to the client's console.
 */
stock void Getopt_PrintHelp(GetoptResult result, int client) {
	int lines = result.HelpLines;
	for (int i = 0; i < lines; i++) {
		char line[GETOPT_LINE_LENGTH];
		result.GetHelpLine(i, line, sizeof(line));
		Getopt_PrintLine(client, line);
	}
}

/**
 * Prints the spec's name and version to the client's console.
 */
stock void Getopt_PrintVersion(GetoptResult result, int client) {
	char line[GETOPT_LINE_LENGTH];
	result.GetVersion(line, sizeof(line));
	Getopt_PrintLine(client, line);
}

/**
 * Do not edit below this line!
 */
public Extension __ext_getopt = {
	name = "Getopt",
	file = "getopt.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS
public void __ext_getopt_SetNTVOptional() {
	MarkNativeAsOptional("GetoptSpec.GetoptSpec");
	MarkNativeAsOptional("GetoptSpec.Root.get");
	MarkNativeAsOptional("GetoptSpec.Validate");
	MarkNativeAsOptional("GetoptSpec.Parse");
	MarkNativeAsOptional("GetoptLevel.Set");
	MarkNativeAsOptional("GetoptLevel.Option");
	MarkNativeAsOptional("GetoptLevel.Arg");
	MarkNativeAsOptional("GetoptLevel.Command");
	MarkNativeAsOptional("GetoptLevel.Example");
	MarkNativeAsOptional("GetoptOption.Set");
	MarkNativeAsOptional("GetoptOption.Valid");
	MarkNativeAsOptional("GetoptOption.ValidFrom");
	MarkNativeAsOptional("GetoptOption.LazyValid");
	MarkNativeAsOptional("GetoptOption.ProcessValue");
	MarkNativeAsOptional("GetoptOption.Default");
	MarkNativeAsOptional("GetoptOption.DefaultPair");
	MarkNativeAsOptional("GetoptOption.DefaultField");
	MarkNativeAsOptional("GetoptArg.Set");
	MarkNativeAsOptional("GetoptArg.ProcessValue");
	MarkNativeAsOptional("Getopt_RegisterType");
	MarkNativeAsOptional("Getopt_SetTypeCallback");
	MarkNativeAsOptional("GetoptType.SetLabel");
	MarkNativeAsOptional("GetoptKeys.Has");
	MarkNativeAsOptional("GetoptKeys.Get");
	MarkNativeAsOptional("GetoptKeys.Caller.get");
	MarkNativeAsOptional("GetoptResolved.SetValue");
	MarkNativeAsOptional("GetoptResolved.SetAttribute");
	MarkNativeAsOptional("GetoptList.Add");
	MarkNativeAsOptional("GetoptResult.Outcome.get");
	MarkNativeAsOptional("GetoptResult.OutputLines.get");
	MarkNativeAsOptional("GetoptResult.GetOutputLine");
	MarkNativeAsOptional("GetoptResult.GetError");
	MarkNativeAsOptional("GetoptResult.GetCommand");
	MarkNativeAsOptional("GetoptResult.Subcommand.get");
	MarkNativeAsOptional("GetoptResult.IsSet");
	MarkNativeAsOptional("GetoptResult.Count");
	MarkNativeAsOptional("GetoptResult.KeyCount");
	MarkNativeAsOptional("GetoptResult.ReadKey");
	MarkNativeAsOptional("GetoptResult.ReadString");
	MarkNativeAsOptional("GetoptResult.ReadInt");
	MarkNativeAsOptional("GetoptResult.ReadFloat");
	MarkNativeAsOptional("GetoptResult.ReadDuration");
	MarkNativeAsOptional("GetoptResult.ReadAttribute");
	MarkNativeAsOptional("GetoptResult.HelpLines.get");
	MarkNativeAsOptional("GetoptResult.GetHelpLine");
	MarkNativeAsOptional("GetoptResult.GetVersion");
}
#endif
