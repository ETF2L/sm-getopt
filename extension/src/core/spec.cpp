/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The value pipeline of options and args, and the compiler that turns a
 * raw declaration into a spec. */
#include "spec.h"

#include "errors.h"
#include "text.h"

#include <ctype.h>

#include <algorithm>
#include <stdexcept>

namespace smgetopt {

namespace {

using text::Format;

const char *const kDefaultGroup = "Options";
const char *const kDefaultOptionType = "flag";
const char *const kDefaultArgType = "string";

/* Hands problem to report, which throws. */
[[noreturn]] void Fail(const Report &report, const std::string &problem)
{
	report(problem);
	throw std::logic_error("a value problem was not reported: " + problem);
}

/* A report that says first where the problem sits, e.g. "key 'os': ...". */
Report Under(const Report &report, const std::string &where)
{
	return [report, where](const std::string &problem) { report(where + ": " + problem); };
}

/* The items of one word of a csv option. Items are trimmed; one trailing
 * comma is tolerated, an empty item is not. */
std::vector<std::string> CsvItems(const std::string &word, const Report &report)
{
	std::string list = text::Trim(word);
	if (!list.empty() && list.back() == ',')
		list.pop_back();

	std::vector<std::string> items;
	size_t start = 0;
	while (true) {
		size_t comma = list.find(',', start);
		items.push_back(text::Trim(list.substr(start, comma == std::string::npos ? comma : comma - start)));
		if (comma == std::string::npos)
			break;
		start = comma + 1;
	}

	if (std::find(items.begin(), items.end(), "") != items.end())
		Fail(report, Format("'%s' contains an empty item", word.c_str()));
	return items;
}

/* The items a multiple option received: its words, each split at commas
 * for a csv option. Every word is split before any item is checked. */
std::vector<std::string> ListItems(const std::vector<std::string> &words, bool csv, const Report &report)
{
	if (!csv)
		return words;

	std::vector<std::string> items;
	for (const std::string &word : words) {
		std::vector<std::string> wordItems = CsvItems(word, report);
		items.insert(items.end(), wordItems.begin(), wordItems.end());
	}
	return items;
}

/* A value converted by its type, or the type's problem with it. */
Scalar Converted(const Type &type, const std::string &raw, const Report &report)
{
	Conversion conversion = type.Convert(raw);
	if (conversion.problem)
		Fail(report, *conversion.problem);
	return std::move(conversion.scalar);
}

/* The value pipeline of one option for the values of one run, from the
 * command line or the default: every single value is checked by the type,
 * the valid list and the lazyValid check, and converted. A valid list from
 * a callback is asked for once per run, when the first value needs it. */
class OptionPipeline {
public:
	explicit OptionPipeline(const Option &option) : m_option(option) {}

	Scalar Checked(const std::string &raw, const Report &report)
	{
		Scalar scalar = Converted(*m_option.type, raw, report);
		if (m_option.hasValid) {
			const std::vector<std::string> &allowed = Allowed();
			if (std::find(allowed.begin(), allowed.end(), scalar.text) == allowed.end())
				Fail(report, Format("'%s' is not one of: %s", scalar.text.c_str(), text::Join(allowed, ", ").c_str()));
		}
		if (m_option.lazyValid && !m_option.lazyValid(scalar.text))
			Fail(report, Format("'%s' is not a valid value", scalar.text.c_str()));
		return scalar;
	}

private:
	const std::vector<std::string> &Allowed()
	{
		if (!m_allowed)
			m_allowed = m_option.hasValidList ? m_option.validList : m_option.validSource();
		return *m_allowed;
	}

	const Option &m_option;
	std::optional<std::vector<std::string>> m_allowed;
};

Mapping CheckedMapping(OptionPipeline &pipeline, const std::map<std::string, std::string> &pairs, const Report &report)
{
	Mapping checked;
	for (const auto &pair : pairs) {
		if (pair.first.empty())
			Fail(report, "empty key");
		checked[pair.first] = pipeline.Checked(pair.second, Under(report, Format("key '%s'", pair.first.c_str())));
	}
	return checked;
}

std::vector<Mapping> CheckedObjects(OptionPipeline &pipeline, const std::vector<std::map<std::string, std::string>> &objects,
                                    const Report &report)
{
	std::vector<Mapping> checked;
	for (size_t i = 0; i < objects.size(); i++)
		checked.push_back(CheckedMapping(pipeline, objects[i], Under(report, Format("entry %zu", i))));
	return checked;
}

/* Later pairs replace earlier ones with the same key. */
std::map<std::string, std::string> Merged(const std::vector<std::pair<std::string, std::string>> &pairs)
{
	std::map<std::string, std::string> merged;
	for (const auto &pair : pairs)
		merged[pair.first] = pair.second;
	return merged;
}

/* Orders the indices of an objectlist by their number, whatever its size. */
struct IndexLess {
	bool operator()(const std::string &left, const std::string &right) const
	{
		return text::CompareIntegers(left, right) < 0;
	}
};

bool IsFieldName(const std::string &field)
{
	if (field.empty())
		return false;
	for (char c : field) {
		if (!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
			return false;
	}
	return true;
}

/* The objects an objectlist option collects from its INDEX.FIELD=VALUE
 * words. The indices must be 0 to n-1, each written one way only: a
 * leading zero would make 0 and 00 two keys of one object. */
std::vector<std::map<std::string, std::string>> ObjectsFromPairs(const std::map<std::string, std::string> &pairs,
                                                                  const Report &report)
{
	std::map<std::string, std::map<std::string, std::string>, IndexLess> objectAt;
	for (const auto &pair : pairs) {
		const std::string &key = pair.first;
		size_t dot = key.find('.');
		bool hasIndex = dot != std::string::npos && dot > 0 && key.find_first_not_of("0123456789") == dot;
		if (hasIndex && dot > 1 && key[0] == '0')
			Fail(report, Format("invalid key '%s', the index must not have leading zeros", key.c_str()));
		if (!hasIndex || !IsFieldName(key.substr(dot + 1)))
			Fail(report, Format("invalid key '%s', expected INDEX.FIELD=VALUE", key.c_str()));
		objectAt[key.substr(0, dot)][key.substr(dot + 1)] = pair.second;
	}

	std::vector<std::map<std::string, std::string>> objects;
	for (const auto &entry : objectAt) {
		if (entry.first != std::to_string(objects.size()))
			Fail(report, Format("missing index %zu", objects.size()));
		objects.push_back(entry.second);
	}
	return objects;
}

/* The default of an option in its shape, checked like a value from the
 * command line, and as the spec wrote it for the help. */
struct CheckedDefault {
	Value value;
	std::string presented;
};

bool AllOfKind(const std::vector<RawDefault> &pieces, RawDefault::Kind kind)
{
	return std::all_of(pieces.begin(), pieces.end(), [kind](const RawDefault &piece) { return piece.kind == kind; });
}

std::string PresentedPairs(const std::map<std::string, std::string> &pairs, const std::string &prefix)
{
	std::vector<std::string> shown;
	for (const auto &pair : pairs)
		shown.push_back(prefix + pair.first + "=" + pair.second);
	return text::Join(shown, ", ");
}

CheckedDefault CheckedSingleDefault(OptionPipeline &pipeline, const std::vector<RawDefault> &pieces, const Report &report)
{
	if (pieces.size() != 1 || pieces[0].kind != RawDefault::Kind::Value)
		Fail(report, "expected a single value, not a list or mapping");

	CheckedDefault checked;
	checked.value.single = pipeline.Checked(pieces[0].value, report);
	checked.presented = pieces[0].value;
	return checked;
}

CheckedDefault CheckedListDefault(OptionPipeline &pipeline, const std::vector<RawDefault> &pieces, const Report &report)
{
	if (!AllOfKind(pieces, RawDefault::Kind::Value))
		Fail(report, "expected a list of values");

	CheckedDefault checked;
	std::vector<std::string> shown;
	for (const RawDefault &piece : pieces) {
		checked.value.list.push_back(pipeline.Checked(piece.value, report));
		shown.push_back(piece.value);
	}
	checked.presented = text::Join(shown, ", ");
	return checked;
}

CheckedDefault CheckedMappingDefault(OptionPipeline &pipeline, const std::vector<RawDefault> &pieces, const Report &report)
{
	if (!AllOfKind(pieces, RawDefault::Kind::Pair))
		Fail(report, "expected a mapping of keys to values");

	std::map<std::string, std::string> pairs;
	for (const RawDefault &piece : pieces)
		pairs[piece.key] = piece.value;

	CheckedDefault checked;
	checked.value.mapping = CheckedMapping(pipeline, pairs, report);
	checked.presented = PresentedPairs(pairs, "");
	return checked;
}

CheckedDefault CheckedObjectsDefault(OptionPipeline &pipeline, const std::vector<RawDefault> &pieces, const Report &report)
{
	if (!AllOfKind(pieces, RawDefault::Kind::Field))
		Fail(report, "expected a list of mappings");

	std::map<int, std::map<std::string, std::string>> objectAt;
	for (const RawDefault &piece : pieces)
		objectAt[piece.entry][piece.key] = piece.value;

	std::vector<std::map<std::string, std::string>> objects;
	std::vector<std::string> shown;
	for (const auto &entry : objectAt) {
		if (entry.first != static_cast<int>(objects.size()))
			Fail(report, Format("missing index %zu", objects.size()));
		shown.push_back(PresentedPairs(entry.second, std::to_string(entry.first) + "."));
		objects.push_back(entry.second);
	}

	CheckedDefault checked;
	checked.value.objects = CheckedObjects(pipeline, objects, report);
	checked.presented = text::Join(shown, ", ");
	return checked;
}

CheckedDefault CheckedDefaultOf(const Option &option, const std::vector<RawDefault> &pieces, const Report &report)
{
	OptionPipeline pipeline(option);
	CheckedDefault checked;
	switch (option.ValueShape()) {
	case Shape::Single:
		checked = CheckedSingleDefault(pipeline, pieces, report);
		break;
	case Shape::List:
		checked = CheckedListDefault(pipeline, pieces, report);
		break;
	case Shape::Mapping:
		checked = CheckedMappingDefault(pipeline, pieces, report);
		break;
	case Shape::Objects:
		checked = CheckedObjectsDefault(pipeline, pieces, report);
		break;
	}
	checked.value.shape = option.ValueShape();
	return checked;
}

/* Takes key out of keys; nothing when it is not there. */
std::optional<std::string> TakeKey(std::map<std::string, std::string> &keys, const std::string &key)
{
	auto found = keys.find(key);
	if (found == keys.end())
		return std::nullopt;
	std::string value = std::move(found->second);
	keys.erase(found);
	return value;
}

bool TakeSwitch(std::map<std::string, std::string> &keys, const std::string &key)
{
	return text::IsTrue(TakeKey(keys, key).value_or(""));
}

std::vector<std::string> SplitNames(const std::string &key)
{
	std::vector<std::string> names;
	size_t start = 0;
	while (true) {
		size_t pipe = key.find('|', start);
		names.push_back(key.substr(start, pipe == std::string::npos ? pipe : pipe - start));
		if (pipe == std::string::npos)
			return names;
		start = pipe + 1;
	}
}

} // namespace

/* ------------------------------------------------------------------ Option */

Shape Option::ValueShape() const
{
	if (multiple)
		return Shape::List;
	if (hash)
		return Shape::Mapping;
	if (objectlist)
		return Shape::Objects;
	return Shape::Single;
}

std::string Option::Spelling() const
{
	return text::OptionSpelling(name);
}

std::string Option::TypeLabel() const
{
	return typehint ? *typehint : type->Label();
}

std::vector<std::string> Option::Names() const
{
	std::vector<std::string> names{name};
	names.insert(names.end(), aliases.begin(), aliases.end());
	return names;
}

Value Option::EmptyValue() const
{
	Value value;
	value.shape = ValueShape();
	return value;
}

Value Option::CurrentDefault(const Report &report) const
{
	if (!type->FollowsClock())
		return defaultValue;
	return CheckedDefaultOf(*this, defaultPieces, report).value;
}

Value Option::FromCommandLine(const Given &given, const Report &report) const
{
	OptionPipeline pipeline(*this);
	Value value = EmptyValue();
	if (!type->TakesValue()) {
		value.single = pipeline.Checked(std::to_string(given.count), report);
		return value;
	}

	switch (value.shape) {
	case Shape::Single:
		value.single = pipeline.Checked(given.words.back(), report);
		break;
	case Shape::List:
		for (const std::string &item : ListItems(given.words, csv, report))
			value.list.push_back(pipeline.Checked(item, report));
		break;
	case Shape::Mapping:
		value.mapping = CheckedMapping(pipeline, Merged(given.pairs), report);
		break;
	case Shape::Objects:
		value.objects = CheckedObjects(pipeline, ObjectsFromPairs(Merged(given.pairs), report), report);
		break;
	}
	return value;
}

/* --------------------------------------------------------------------- Arg */

std::string Arg::TypeLabel() const
{
	return typehint ? *typehint : type->Label();
}

Scalar Arg::CheckedScalar(const std::string &raw, const Report &report) const
{
	return Converted(*type, raw, report);
}

/* ------------------------------------------------------------------- Level */

std::vector<std::string> Level::CommandNames() const
{
	std::vector<std::string> names;
	for (const auto &command : commands)
		names.push_back(command.first);
	return names;
}

const Level *Level::Command(const std::string &name) const
{
	auto found = commands.find(name);
	return found == commands.end() ? nullptr : found->second.get();
}

std::vector<const Option *> Level::AllOptions() const
{
	std::vector<const Option *> all;
	for (const auto &option : options)
		all.push_back(option.get());
	all.insert(all.end(), inherited.begin(), inherited.end());
	return all;
}

std::vector<const Option *> Level::DeclaredOptions() const
{
	std::vector<const Option *> declared;
	for (const auto &option : options) {
		if (option->automatic == AutoOption::None)
			declared.push_back(option.get());
	}
	return declared;
}

const Option *Level::OptionNamed(const std::string &name) const
{
	auto found = m_byName.find(name);
	return found == m_byName.end() ? nullptr : found->second;
}

std::string Level::Label() const
{
	return IsRoot() ? "the top level" : Format("command '%s'", path.c_str());
}

std::string Level::Where() const
{
	return IsRoot() ? "" : Label() + ": ";
}

/* ---------------------------------------------------------------- compiler */

class SpecCompiler {
public:
	explicit SpecCompiler(const TypeRegistry &types) : m_types(types) {}

	std::shared_ptr<const Spec> Compile(const RawSpec &raw)
	{
		if (raw.programName.empty())
			throw SpecError("the spec has no name");

		auto spec = std::make_shared<Spec>();
		spec->programName = raw.programName;

		std::map<std::string, std::string> keys = raw.root.keys;
		spec->version = TakeKey(keys, "version").value_or(raw.fallbackVersion.empty() ? "unknown" : raw.fallbackVersion);
		if (std::optional<std::string> width = TakeKey(keys, "helpWidth"))
			spec->helpWidth = HelpWidth(*width);

		spec->root = CompileLevel(raw.root, std::move(keys), "");
		AttachAutoOptions(*spec->root);
		PassInheritedOptions(*spec->root, {});
		CheckNegations(*spec->root);
		return spec;
	}

private:
	/* An option an outer level passes on, with the level that declares it. */
	struct Passed {
		const Option *option;
		const Level *from;
	};

	static int HelpWidth(const std::string &width)
	{
		std::optional<int64_t> columns = text::IsInteger(width) ? text::ToInt64(width) : std::nullopt;
		if (!columns || *columns < 1 || *columns > 10000)
			throw SpecError(Format("helpWidth must be a whole number from 1 to 10000, not '%s'", width.c_str()));
		return static_cast<int>(*columns);
	}

	std::unique_ptr<Level> CompileLevel(const RawLevel &raw, std::map<std::string, std::string> keys, const std::string &path)
	{
		auto level = std::make_unique<Level>();
		level->path = path;
		level->pathWords = text::SplitAtWhitespace(path);
		std::string where = level->Where();

		for (const RawEntry *rawOption : SortedByKey(raw.options))
			AddOption(*level, CompileOption(*rawOption, where));

		for (const auto &rawArg : raw.args)
			level->args.push_back(CompileArg(*rawArg, where));
		CheckArgOrder(*level);

		for (const auto &command : SortedByName(raw.commands)) {
			const std::string &name = command->first;
			if (!text::IsValidName(name))
				throw SpecError(Format("%sinvalid command name '%s'", where.c_str(), name.c_str()));
			if (level->commands.count(name))
				throw SpecError(Format("%scommand '%s' is declared twice", where.c_str(), name.c_str()));
			level->commands[name] = CompileLevel(*command->second, command->second->keys, path.empty() ? name : path + " " + name);
		}

		if (!level->args.empty() && level->HasCommands())
			throw SpecError(where + "args and commands are mutually exclusive on one level");
		for (const Option *option : level->DeclaredOptions()) {
			if (option->inherit && !level->HasCommands())
				throw SpecError(Format("%soption '%s': inherit requires commands on the same level", where.c_str(), option->name.c_str()));
		}

		if (std::optional<std::string> commandRequired = TakeKey(keys, "commandRequired")) {
			if (!level->HasCommands())
				throw SpecError(where + "commandRequired without commands");
			level->commandRequired = text::IsTrue(*commandRequired);
		}
		level->description = TakeKey(keys, "description").value_or("");
		level->examples = raw.examples;

		if (!keys.empty())
			throw SpecError(Format("%sunknown key(s): %s", where.c_str(), KeyNames(keys).c_str()));
		CheckNamesAreUnique(*level);
		return level;
	}

	static std::vector<const RawEntry *> SortedByKey(const std::vector<std::unique_ptr<RawEntry>> &entries)
	{
		std::vector<const RawEntry *> sorted;
		for (const auto &entry : entries)
			sorted.push_back(entry.get());
		std::stable_sort(sorted.begin(), sorted.end(), [](const RawEntry *a, const RawEntry *b) { return a->key < b->key; });
		return sorted;
	}

	static std::vector<const std::pair<std::string, std::unique_ptr<RawLevel>> *> SortedByName(
		const std::vector<std::pair<std::string, std::unique_ptr<RawLevel>>> &commands)
	{
		std::vector<const std::pair<std::string, std::unique_ptr<RawLevel>> *> sorted;
		for (const auto &command : commands)
			sorted.push_back(&command);
		std::stable_sort(sorted.begin(), sorted.end(), [](const auto *a, const auto *b) { return a->first < b->first; });
		return sorted;
	}

	static std::string KeyNames(const std::map<std::string, std::string> &keys)
	{
		std::vector<std::string> names;
		for (const auto &key : keys)
			names.push_back(key.first);
		return text::Join(names, ", ");
	}

	static void CheckArgOrder(const Level &level)
	{
		bool sawOptional = false;
		for (size_t i = 0; i < level.args.size(); i++) {
			const Arg &arg = level.args[i];
			if (arg.multiple && i + 1 != level.args.size())
				throw SpecError(Format("%sarg '%s': multiple is only allowed on the last arg", level.Where().c_str(), arg.name.c_str()));
			if (arg.required && sawOptional)
				throw SpecError(Format("%sarg '%s': a required arg cannot follow an optional one", level.Where().c_str(), arg.name.c_str()));
			sawOptional = sawOptional || !arg.required;
		}
	}

	/* A result is read by name, so no two options or args of one level may
	 * share one. Aliases are no names of values. */
	static void CheckNamesAreUnique(const Level &level)
	{
		std::map<std::string, std::string> user;
		for (const Option *option : level.DeclaredOptions())
			user[option->name] = Format("option '%s'", option->name.c_str());
		for (const Arg &arg : level.args) {
			std::string label = Format("arg '%s'", arg.name.c_str());
			auto seen = user.find(arg.name);
			if (seen != user.end())
				throw SpecError(Format("%s%s and %s share one name", level.Where().c_str(), label.c_str(), seen->second.c_str()));
			user[arg.name] = label;
		}
	}

	/* The type an option or arg names, built from the spec keys the type
	 * declares; those and the type name are taken out of keys. */
	std::shared_ptr<const Type> TakeType(std::map<std::string, std::string> &keys, const char *defaultName,
	                                     const std::string &owner, std::string &typeName) const
	{
		typeName = TakeKey(keys, "type").value_or(defaultName);
		const TypeDefinition *definition = m_types.Find(typeName);
		if (!definition) {
			throw SpecError(Format("%s: unknown option type '%s' (known: %s)", owner.c_str(), typeName.c_str(),
			                       text::Join(m_types.KnownNames(), ", ").c_str()));
		}

		TypeKeys typeKeys;
		for (const std::string &key : definition->specKeys) {
			if (std::optional<std::string> value = TakeKey(keys, key))
				typeKeys[key] = *value;
		}
		if (definition->checkSpecKeys) {
			if (Problem problem = definition->checkSpecKeys(typeKeys))
				throw SpecError(owner + ": " + *problem);
		}
		return definition->create(typeKeys);
	}

	std::unique_ptr<Option> CompileOption(const RawEntry &raw, const std::string &where) const
	{
		if (raw.key.empty())
			throw SpecError(where + "option key must be a non-empty string");

		std::vector<std::string> names = SplitNames(raw.key);
		for (const std::string &name : names) {
			if (!text::IsValidName(name))
				throw SpecError(Format("%soption '%s': invalid name '%s'", where.c_str(), raw.key.c_str(), name.c_str()));
		}

		auto option = std::make_unique<Option>();
		option->name = names[0];
		option->aliases.assign(names.begin() + 1, names.end());
		std::string owner = Format("%soption '%s'", where.c_str(), option->name.c_str());

		std::map<std::string, std::string> keys = raw.keys;
		option->type = TakeType(keys, kDefaultOptionType, owner, option->typeName);
		option->required = TakeSwitch(keys, "required");
		option->group = TakeKey(keys, "group").value_or(kDefaultGroup);
		option->help = TakeKey(keys, "help").value_or("");
		option->multiple = TakeSwitch(keys, "multiple");
		option->hash = TakeSwitch(keys, "hash");
		option->csv = TakeSwitch(keys, "csv");
		option->objectlist = TakeSwitch(keys, "objectlist");
		option->hidden = TakeSwitch(keys, "hidden");
		option->inherit = TakeSwitch(keys, "inherit");
		option->typehint = TakeKey(keys, "typehint");
		option->hasValidList = raw.hasValidList;
		option->validList = raw.validList;
		option->validSource = raw.validSource;
		option->hasValid = raw.hasValidList || raw.validSource;
		option->lazyValid = raw.lazyValid;
		option->processValue = raw.processValue;
		option->hasDefault = !raw.defaults.empty();

		if (!keys.empty())
			throw SpecError(Format("%s: unknown key(s): %s", owner.c_str(), KeyNames(keys).c_str()));
		if (option->required && option->hasDefault)
			throw SpecError(owner + ": required and default are mutually exclusive");
		CheckShape(*option, owner);
		if (option->typehint && option->typehint->empty())
			throw SpecError(owner + ": typehint must be a non-empty string");

		if (option->hasDefault) {
			CheckedDefault checked = CheckedDefaultOf(*option, raw.defaults, [&owner](const std::string &problem) {
				throw SpecError(owner + ": default value: " + problem);
			});
			option->defaultValue = std::move(checked.value);
			option->presentedDefault = std::move(checked.presented);
			option->defaultPieces = raw.defaults;
		}
		return option;
	}

	static void CheckShape(const Option &option, const std::string &owner)
	{
		std::vector<std::string> shapes;
		if (option.multiple)
			shapes.push_back("multiple");
		if (option.hash)
			shapes.push_back("hash");
		if (option.objectlist)
			shapes.push_back("objectlist");

		if (!shapes.empty() && !option.type->TakesValue()) {
			throw SpecError(Format("%s: %s requires a value-taking type, not '%s'", owner.c_str(), shapes[0].c_str(),
			                       option.typeName.c_str()));
		}
		if (shapes.size() > 1)
			throw SpecError(Format("%s: %s are mutually exclusive", owner.c_str(), text::Join(shapes, " and ").c_str()));
		if (option.csv && !option.multiple)
			throw SpecError(owner + ": csv requires multiple");
	}

	Arg CompileArg(const RawEntry &raw, const std::string &where) const
	{
		if (!text::IsValidName(raw.key))
			throw SpecError(Format("%sarg: invalid name '%s'", where.c_str(), raw.key.c_str()));

		Arg arg;
		arg.name = raw.key;
		std::string owner = Format("%sarg '%s'", where.c_str(), arg.name.c_str());

		std::map<std::string, std::string> keys = raw.keys;
		arg.type = TakeType(keys, kDefaultArgType, owner, arg.typeName);
		if (!arg.type->TakesValue())
			throw SpecError(Format("%s: type '%s' cannot be used for a positional arg", owner.c_str(), arg.typeName.c_str()));

		arg.required = TakeSwitch(keys, "required");
		arg.multiple = TakeSwitch(keys, "multiple");
		arg.help = TakeKey(keys, "help").value_or("");
		arg.typehint = TakeKey(keys, "typehint");
		arg.processValue = raw.processValue;

		/* What an option may carry besides its keys is as unknown to an arg
		 * as a key it does not have. */
		if (raw.hasValidList || raw.validSource)
			keys["valid"];
		if (raw.lazyValid)
			keys["lazyValid"];
		if (!raw.defaults.empty())
			keys["default"];
		if (!keys.empty())
			throw SpecError(Format("%s: unknown key(s): %s", owner.c_str(), KeyNames(keys).c_str()));
		if (arg.typehint && arg.typehint->empty())
			throw SpecError(owner + ": typehint must be a non-empty string");
		return arg;
	}

	static void TakeNames(Level &level, const Option &option)
	{
		level.m_takenNames[option.name] = Format("option '%s'", option.name.c_str());
		level.m_byName[option.name] = &option;
		for (const std::string &alias : option.aliases) {
			level.m_takenNames[alias] = Format("an alias of option '%s'", option.name.c_str());
			level.m_byName[alias] = &option;
		}
	}

	static void AddOption(Level &level, std::unique_ptr<Option> option)
	{
		for (const std::string &candidate : option->Names()) {
			auto owner = level.m_takenNames.find(candidate);
			if (owner == level.m_takenNames.end())
				continue;
			if (option->automatic != AutoOption::None) {
				throw SpecError(Format("%soption '%s' collides with the automatic --%s option", level.Where().c_str(),
				                       candidate.c_str(), candidate.c_str()));
			}
			throw SpecError(Format("%soption '%s': name '%s' is already used by %s", level.Where().c_str(),
			                       option->name.c_str(), candidate.c_str(), owner->second.c_str()));
		}
		TakeNames(level, *option);
		level.options.push_back(std::move(option));
	}

	/* An option an outer level passes on: accepted on this level's command
	 * line, but read from the level that declares it. */
	static void InheritOption(Level &level, const Passed &passed)
	{
		for (const std::string &candidate : passed.option->Names()) {
			auto owner = level.m_takenNames.find(candidate);
			if (owner == level.m_takenNames.end())
				continue;
			throw SpecError(Format("%soption '%s' inherited from %s: name '%s' is already used by %s", level.Where().c_str(),
			                       passed.option->name.c_str(), passed.from->Label().c_str(), candidate.c_str(),
			                       owner->second.c_str()));
		}
		TakeNames(level, *passed.option);
		level.inherited.push_back(passed.option);
	}

	std::unique_ptr<Option> AutoOptionNamed(const char *name, AutoOption kind, const char *help) const
	{
		auto option = std::make_unique<Option>();
		option->name = name;
		option->type = m_types.Find(kDefaultOptionType)->create({});
		option->typeName = kDefaultOptionType;
		option->help = help;
		option->hidden = true;
		option->automatic = kind;
		return option;
	}

	/* --help answers on every level, --version on the top level. */
	void AttachAutoOptions(Level &level) const
	{
		AddOption(level, AutoOptionNamed("help", AutoOption::Help, "Print this help text"));
		if (level.IsRoot())
			AddOption(level, AutoOptionNamed("version", AutoOption::Version, "Print the version"));
		for (auto &command : level.commands)
			AttachAutoOptions(*command.second);
	}

	/* Every level below hands on what it received plus its own inheritable
	 * options. */
	void PassInheritedOptions(Level &level, std::vector<Passed> received) const
	{
		for (const Option *option : level.DeclaredOptions()) {
			if (option->inherit)
				received.push_back({option, &level});
		}
		for (auto &command : level.commands) {
			for (const Passed &passed : received)
				InheritOption(*command.second, passed);
			PassInheritedOptions(*command.second, received);
		}
	}

	/* --no-NAME and --noNAME turn a bool off, so on a level that accepts
	 * the bool no option may answer to those names itself. */
	static void CheckNegations(const Level &level)
	{
		for (const Option *option : level.AllOptions()) {
			if (option->type->Kind() != TypeKind::Bool)
				continue;
			for (const std::string &name : option->Names()) {
				for (const std::string &negation : {"no-" + name, "no" + name}) {
					auto owner = level.m_takenNames.find(negation);
					if (owner == level.m_takenNames.end())
						continue;
					throw SpecError(Format("%soption '%s': its negation '--%s' is already used by %s", level.Where().c_str(),
					                       option->name.c_str(), negation.c_str(), owner->second.c_str()));
				}
			}
		}
		for (const auto &command : level.commands)
			CheckNegations(*command.second);
	}

	const TypeRegistry &m_types;
};

std::shared_ptr<const Spec> Spec::Compile(const RawSpec &raw, const TypeRegistry &types)
{
	return SpecCompiler(types).Compile(raw);
}

} // namespace smgetopt
