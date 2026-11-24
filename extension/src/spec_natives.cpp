/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The natives that declare a spec and parse with it. They only collect the
 * declaration; the core library checks it when it is first used. */
#include "extension.h"

#include "text.h"

#include <utility>

using smgetopt::RawDefault;
using smgetopt::RawEntry;
using smgetopt::RawLevel;
using smgetopt::text::Format;

namespace {

/* The spec a part belongs to, ready to be changed: what it compiled to is
 * dropped. Throws when the spec is closed or being checked right now. */
std::shared_ptr<SpecState> Changing(const std::shared_ptr<SpecState> &spec)
{
	if (!spec)
		throw NativeError("The spec of this part is closed");
	if (spec->compiling)
		throw NativeError("A spec cannot be changed while it is being checked");
	spec->compiled.reset();
	return spec;
}

std::shared_ptr<SpecState> Changing(const Node &node)
{
	return Changing(node.spec.lock());
}

/* Clears the compiling mark when the compiler is done, however it ends. */
class CompilingScope {
public:
	explicit CompilingScope(SpecState &spec) : m_spec(spec) { m_spec.compiling = true; }
	~CompilingScope() { m_spec.compiling = false; }

private:
	SpecState &m_spec;
};

std::shared_ptr<const smgetopt::Spec> Compiled(SpecState &spec)
{
	if (spec.compiled)
		return spec.compiled;
	if (spec.compiling)
		throw NativeError("A spec cannot be used while it is being checked");

	CompilingScope compiling(spec);
	spec.compiled = smgetopt::Spec::Compile(spec.raw, g_Getopt.TypesOf(*spec.owner));
	return spec.compiled;
}

/* The primary name of an option key, or the name of an arg. */
std::string NameOf(const RawEntry &entry)
{
	return entry.key.substr(0, entry.key.find('|'));
}

// GetoptSpec.GetoptSpec(name)
cell_t Spec_Create(IPluginContext *context, const cell_t *params)
{
	std::string name = ReadString(context, params[1]);
	if (name.empty())
		throw NativeError("A spec needs a name: the console command it describes");
	return static_cast<cell_t>(g_Getopt.CreateSpec(context, name));
}

// GetoptSpec.Root.get
cell_t Spec_Root(IPluginContext *context, const cell_t *params)
{
	std::shared_ptr<SpecState> spec = g_Getopt.ReadSpec(context, static_cast<Handle_t>(params[1]));
	if (spec->nodes.empty())
		g_Getopt.AddNode(spec, NodeKind::Level, &spec->raw.root, nullptr);
	return spec->nodes.front();
}

// GetoptSpec.Validate()
cell_t Spec_Validate(IPluginContext *context, const cell_t *params)
{
	std::shared_ptr<SpecState> spec = g_Getopt.ReadSpec(context, static_cast<Handle_t>(params[1]));
	Compiled(*spec);
	return 0;
}

// GetoptSpec.Parse(line, caller)
cell_t Spec_Parse(IPluginContext *context, const cell_t *params)
{
	std::shared_ptr<SpecState> spec = g_Getopt.ReadSpec(context, static_cast<Handle_t>(params[1]));
	std::vector<std::string> words = smgetopt::text::SplitWords(ReadString(context, params[2]));
	cell_t caller = params[0] >= 3 ? params[3] : 0;
	if (caller < 0 || caller > playerhelpers->GetMaxClients())
		throw NativeError(Format("Invalid caller %d: a client index, or 0 for the server", caller));

	smgetopt::ParseResult parsed = smgetopt::Parse(Compiled(*spec), words, smgetopt::ParseContext{caller});
	ResultState result;
	result.spec = parsed.spec;
	result.outcome = parsed.outcome;
	result.error = std::move(parsed.error);
	result.output = std::move(parsed.output);
	result.values = std::move(parsed.values);
	return static_cast<cell_t>(g_Getopt.CreateResult(context, std::move(result)));
}

// GetoptLevel.Set(key, value)
cell_t Level_Set(IPluginContext *context, const cell_t *params)
{
	Node &node = g_Getopt.FindNode(context, params[1], NodeKind::Level);
	Changing(node);
	node.level->keys[ReadString(context, params[2])] = ReadString(context, params[3]);
	return params[1];
}

cell_t AddEntry(IPluginContext *context, const cell_t *params, NodeKind kind)
{
	Node &node = g_Getopt.FindNode(context, params[1], NodeKind::Level);
	std::shared_ptr<SpecState> spec = Changing(node);

	auto entry = std::make_unique<RawEntry>();
	entry->key = ReadString(context, params[2]);
	std::string type = ReadString(context, params[3]);
	if (!type.empty())
		entry->keys["type"] = type;

	std::vector<std::unique_ptr<RawEntry>> &entries = kind == NodeKind::Option ? node.level->options : node.level->args;
	entries.push_back(std::move(entry));
	return g_Getopt.AddNode(spec, kind, nullptr, entries.back().get());
}

// GetoptLevel.Option(key, type)
cell_t Level_Option(IPluginContext *context, const cell_t *params)
{
	return AddEntry(context, params, NodeKind::Option);
}

// GetoptLevel.Arg(name, type)
cell_t Level_Arg(IPluginContext *context, const cell_t *params)
{
	return AddEntry(context, params, NodeKind::Arg);
}

// GetoptLevel.Command(name)
cell_t Level_Command(IPluginContext *context, const cell_t *params)
{
	Node &node = g_Getopt.FindNode(context, params[1], NodeKind::Level);
	std::shared_ptr<SpecState> spec = Changing(node);
	node.level->commands.emplace_back(ReadString(context, params[2]), std::make_unique<RawLevel>());
	return g_Getopt.AddNode(spec, NodeKind::Level, node.level->commands.back().second.get(), nullptr);
}

// GetoptLevel.Example(text, args)
cell_t Level_Example(IPluginContext *context, const cell_t *params)
{
	Node &node = g_Getopt.FindNode(context, params[1], NodeKind::Level);
	Changing(node);
	node.level->examples.push_back({ReadString(context, params[2]), ReadString(context, params[3])});
	return params[1];
}

/* The option or arg a native changes. */
RawEntry &ChangingEntry(IPluginContext *context, cell_t id)
{
	Node &node = g_Getopt.FindNode(context, id, NodeKind::Option, NodeKind::Arg);
	Changing(node);
	return *node.entry;
}

// GetoptOption.Set(key, value), GetoptArg.Set(key, value)
cell_t Entry_Set(IPluginContext *context, const cell_t *params)
{
	ChangingEntry(context, params[1]).keys[ReadString(context, params[2])] = ReadString(context, params[3]);
	return params[1];
}

// GetoptOption.Valid(...)
cell_t Entry_Valid(IPluginContext *context, const cell_t *params)
{
	RawEntry &entry = ChangingEntry(context, params[1]);
	entry.hasValidList = true;
	entry.validList.clear();
	entry.validSource = nullptr;
	for (cell_t i = 2; i <= params[0]; i++)
		entry.validList.push_back(ReadString(context, params[i]));
	return params[1];
}

// GetoptOption.ValidFrom(callback, data)
cell_t Entry_ValidFrom(IPluginContext *context, const cell_t *params)
{
	RawEntry &entry = ChangingEntry(context, params[1]);
	IPluginFunction *function = ReadFunction(context, params[2]);
	cell_t data = params[3];
	std::string name = NameOf(entry);

	entry.hasValidList = false;
	entry.validList.clear();
	entry.validSource = [context, function, data, name] {
		std::vector<std::string> values;
		LentList lent(values, context);
		PluginCall(function, Format("valid callback of option '%s'", name.c_str())).Cell(lent.Id()).Text(name).Cell(data).Run();
		return values;
	};
	return params[1];
}

// GetoptOption.LazyValid(callback, data)
cell_t Entry_LazyValid(IPluginContext *context, const cell_t *params)
{
	RawEntry &entry = ChangingEntry(context, params[1]);
	IPluginFunction *function = ReadFunction(context, params[2]);
	cell_t data = params[3];
	std::string name = NameOf(entry);

	entry.lazyValid = [function, data, name](const std::string &value) {
		return PluginCall(function, Format("lazyValid callback of option '%s'", name.c_str())).Text(value).Text(name).Cell(data).Run() != 0;
	};
	return params[1];
}

// GetoptOption.ProcessValue(callback, data), GetoptArg.ProcessValue(callback, data)
cell_t Entry_ProcessValue(IPluginContext *context, const cell_t *params)
{
	RawEntry &entry = ChangingEntry(context, params[1]);
	IPluginFunction *function = ReadFunction(context, params[2]);
	cell_t data = params[3];
	std::string name = NameOf(entry);

	entry.processValue = [context, function, data, name](const smgetopt::ProcessContext &process, const std::string &value) {
		/* The callback reads the level's other values through a result of
		 * its own, closed again when it returns. */
		ResultState unprocessed;
		unprocessed.spec = process.spec;
		unprocessed.values = process.values;
		Handle_t handle = g_Getopt.CreateResult(context, std::move(unprocessed));

		std::string processed = value;
		try {
			PluginCall(function, Format("processValue callback of '%s'", name.c_str()))
				.Cell(static_cast<cell_t>(handle)).Text(value).Buffer(processed).Text(name).Cell(data).Run();
		} catch (...) {
			g_Getopt.FreeResult(context, handle);
			throw;
		}
		g_Getopt.FreeResult(context, handle);
		return processed;
	};
	return params[1];
}

void AddDefault(IPluginContext *context, cell_t id, RawDefault piece)
{
	ChangingEntry(context, id).defaults.push_back(std::move(piece));
}

// GetoptOption.Default(value)
cell_t Entry_Default(IPluginContext *context, const cell_t *params)
{
	AddDefault(context, params[1], {RawDefault::Kind::Value, 0, "", ReadString(context, params[2])});
	return params[1];
}

// GetoptOption.DefaultPair(key, value)
cell_t Entry_DefaultPair(IPluginContext *context, const cell_t *params)
{
	AddDefault(context, params[1], {RawDefault::Kind::Pair, 0, ReadString(context, params[2]), ReadString(context, params[3])});
	return params[1];
}

// GetoptOption.DefaultField(entry, field, value)
cell_t Entry_DefaultField(IPluginContext *context, const cell_t *params)
{
	AddDefault(context, params[1], {RawDefault::Kind::Field, params[2], ReadString(context, params[3]), ReadString(context, params[4])});
	return params[1];
}

} // namespace

const sp_nativeinfo_t g_SpecNatives[] = {
	{"GetoptSpec.GetoptSpec",     Guarded<Spec_Create>},
	{"GetoptSpec.Root.get",       Guarded<Spec_Root>},
	{"GetoptSpec.Validate",       Guarded<Spec_Validate>},
	{"GetoptSpec.Parse",          Guarded<Spec_Parse>},
	{"GetoptLevel.Set",           Guarded<Level_Set>},
	{"GetoptLevel.Option",        Guarded<Level_Option>},
	{"GetoptLevel.Arg",           Guarded<Level_Arg>},
	{"GetoptLevel.Command",       Guarded<Level_Command>},
	{"GetoptLevel.Example",       Guarded<Level_Example>},
	{"GetoptOption.Set",          Guarded<Entry_Set>},
	{"GetoptOption.Valid",        Guarded<Entry_Valid>},
	{"GetoptOption.ValidFrom",    Guarded<Entry_ValidFrom>},
	{"GetoptOption.LazyValid",    Guarded<Entry_LazyValid>},
	{"GetoptOption.ProcessValue", Guarded<Entry_ProcessValue>},
	{"GetoptOption.Default",      Guarded<Entry_Default>},
	{"GetoptOption.DefaultPair",  Guarded<Entry_DefaultPair>},
	{"GetoptOption.DefaultField", Guarded<Entry_DefaultField>},
	{"GetoptArg.Set",             Guarded<Entry_Set>},
	{"GetoptArg.ProcessValue",    Guarded<Entry_ProcessValue>},
	{nullptr,                     nullptr},
};
