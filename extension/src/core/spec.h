/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The description of a command line. A plugin declares it piece by piece
 * as a raw spec; Spec::Compile checks all of it and builds the spec the
 * parser and the help work with, which never changes afterwards. Nothing
 * past Compile looks at raw declarations again. */
#ifndef GETOPT_CORE_SPEC_H
#define GETOPT_CORE_SPEC_H

#include "types.h"
#include "value.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace smgetopt {

class LevelResult;
class Spec;

/* The values an option allows right now. */
using ValidSource = std::function<std::vector<std::string>()>;

/* Whether a converted value is acceptable. */
using LazyValid = std::function<bool(const std::string &value)>;

/* What a processValue callback sees besides the value: the checked values
 * of the level before any value was replaced, and the spec they belong to. */
struct ProcessContext {
	std::shared_ptr<const Spec> spec;
	std::shared_ptr<const LevelResult> values;
};

/* What a reader returns in place of value. */
using ProcessValue = std::function<std::string(const ProcessContext &context, const std::string &value)>;

/* ------------------------------------------------------------ declaration */

/* One piece of a default: a value, a key=value pair of a mapping, or a
 * field of an entry of an objectlist. */
struct RawDefault {
	enum class Kind { Value, Pair, Field };

	Kind kind;
	int entry;
	std::string key;
	std::string value;
};

/* An option or an arg as declared: its key (the option's names, or the
 * arg's name), its spec keys as text, and what text cannot hold. */
struct RawEntry {
	std::string key;
	std::map<std::string, std::string> keys;
	bool hasValidList = false;
	std::vector<std::string> validList;
	ValidSource validSource;
	LazyValid lazyValid;
	ProcessValue processValue;
	std::vector<RawDefault> defaults;
};

struct Example {
	std::string text;
	std::string args;
};

struct RawLevel {
	std::map<std::string, std::string> keys;
	std::vector<std::unique_ptr<RawEntry>> options;
	std::vector<std::unique_ptr<RawEntry>> args;
	std::vector<std::pair<std::string, std::unique_ptr<RawLevel>>> commands;
	std::vector<Example> examples;
};

struct RawSpec {
	/* The name the help shows: the console command. */
	std::string programName;

	/* What --version shows when the spec sets no version. */
	std::string fallbackVersion;

	RawLevel root;
};

/* --------------------------------------------------------------- the spec */

/* Told about a problem with a value, worded without the option's name.
 * Throws; never returns. */
using Report = std::function<void(const std::string &problem)>;

/* What the command line gave one option: how often a switch was given, the
 * words of a value-taking option, or the key=value pairs of a hash or
 * objectlist option, in the order given. */
struct Given {
	int64_t count = 0;
	std::vector<std::string> words;
	std::vector<std::pair<std::string, std::string>> pairs;
};

enum class AutoOption { None, Help, Version };

class Option {
public:
	std::string name;
	std::vector<std::string> aliases;
	std::shared_ptr<const Type> type;
	std::string typeName;
	bool required = false;
	bool hasDefault = false;
	/* The default as checked when the spec was compiled. */
	Value defaultValue;
	/* The default as the spec wrote it, for the help. */
	std::string presentedDefault;
	/* The pieces of the default, for a type that converts it anew. */
	std::vector<RawDefault> defaultPieces;
	bool hasValid = false;
	bool hasValidList = false;
	std::vector<std::string> validList;
	ValidSource validSource;
	LazyValid lazyValid;
	ProcessValue processValue;
	std::string group = "Options";
	std::string help;
	bool multiple = false;
	bool hash = false;
	bool csv = false;
	bool objectlist = false;
	bool hidden = false;
	bool inherit = false;
	std::optional<std::string> typehint;
	AutoOption automatic = AutoOption::None;

	Shape ValueShape() const;

	/* The primary name as it is typed. */
	std::string Spelling() const;

	/* The tag the help shows after the help text: the typehint, or what the
	 * type calls itself. Empty for none. */
	std::string TypeLabel() const;

	/* Every name, the primary name first. */
	std::vector<std::string> Names() const;

	/* What a reader gets when nothing set the option: nothing, or an empty
	 * list or mapping. */
	Value EmptyValue() const;

	/* The value the command line gave, in the option's shape, every single
	 * value checked by the type, the valid list and the lazyValid check, and
	 * converted. */
	Value FromCommandLine(const Given &given, const Report &report) const;

	/* The default for a parse now: the one checked at compile, or for a type
	 * that follows the clock the default checked and converted anew, so that
	 * "tomorrow" is tomorrow of the parse. */
	Value CurrentDefault(const Report &report) const;
};

class Arg {
public:
	std::string name;
	std::shared_ptr<const Type> type;
	std::string typeName;
	bool required = false;
	bool multiple = false;
	std::string help;
	std::optional<std::string> typehint;
	ProcessValue processValue;

	std::string TypeLabel() const;
	Scalar CheckedScalar(const std::string &raw, const Report &report) const;
};

/* One node of the spec: the top level, or one command. */
class Level {
public:
	/* The command words that lead here, separated by spaces; empty for the
	 * top level. */
	std::string path;
	std::vector<std::string> pathWords;

	/* The level's own options: the declared ones in the order of their
	 * keys, then the automatic ones. */
	std::vector<std::unique_ptr<Option>> options;

	/* The options the levels above pass on, outermost level first. */
	std::vector<const Option *> inherited;

	std::vector<Arg> args;
	std::map<std::string, std::unique_ptr<Level>> commands;
	bool commandRequired = true;
	std::string description;
	std::vector<Example> examples;

	bool IsRoot() const { return path.empty(); }
	bool HasCommands() const { return !commands.empty(); }
	std::vector<std::string> CommandNames() const;
	const Level *Command(const std::string &name) const;

	/* Every option the level's command line accepts: its own, then the
	 * inherited ones. */
	std::vector<const Option *> AllOptions() const;

	/* Its own options without the automatic ones. */
	std::vector<const Option *> DeclaredOptions() const;

	/* The option that answers to name on this level's command line. */
	const Option *OptionNamed(const std::string &name) const;

	/* "the top level" or "command 'PATH'". */
	std::string Label() const;

	/* How a spec error about this level starts: nothing for the top level,
	 * "command 'PATH': " for a command. */
	std::string Where() const;

private:
	friend class SpecCompiler;
	std::map<std::string, const Option *> m_byName;
	std::map<std::string, std::string> m_takenNames;
};

class Spec {
public:
	std::string programName;
	std::string version;
	int helpWidth = 100;
	std::unique_ptr<Level> root;

	/* Checks the whole declaration and builds the spec. Throws SpecError
	 * for the first mistake found. */
	static std::shared_ptr<const Spec> Compile(const RawSpec &raw, const TypeRegistry &types);
};

} // namespace smgetopt

#endif // GETOPT_CORE_SPEC_H
