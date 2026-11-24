/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The natives that read a result: how the parse ended, what to show the
 * user, and the values of one level. The core's result has the rules of
 * reading values; the natives read the parameters, write the plugin's
 * buffers and keep integers within a cell. */
#include "extension.h"

#include "help.h"
#include "text.h"

#include <limits.h>

using smgetopt::Outcome;
using smgetopt::Scalar;
using smgetopt::text::Format;

namespace {

ResultState &ResultOf(IPluginContext *context, const cell_t *params)
{
	return g_Getopt.ReadResult(context, static_cast<Handle_t>(params[1]));
}

/* The values of a result. Throws when the parse gave none. */
const smgetopt::LevelResult &ValuesOf(const ResultState &result)
{
	if (!result.values)
		throw NativeError("This result holds no values: the command line was rejected or answered");
	return *result.values;
}

/* The single value at the address a native's parameters give, from its
 * second on: the name, the entry and the key. */
const Scalar *ScalarAt(IPluginContext *context, const cell_t *params, std::string &name)
{
	name = ReadString(context, params[2]);
	std::string key = ReadString(context, params[4]);
	return ValuesOf(ResultOf(context, params)).At(name, params[3], key);
}

/* The help of the result's level, rendered when first asked for. The
 * notes callbacks of a plugin's types run while it is rendered and may do
 * anything to the result, so it is looked up again afterwards. */
const std::vector<std::string> &HelpOf(IPluginContext *context, const cell_t *params)
{
	ResultState &result = ResultOf(context, params);
	if (result.help)
		return *result.help;

	std::shared_ptr<const smgetopt::Spec> spec = result.spec;
	std::shared_ptr<const smgetopt::LevelResult> values = result.values;
	if (!values)
		throw NativeError("This result holds no values: the command line was rejected or answered");
	std::vector<std::string> help = smgetopt::text::FitLines(smgetopt::RenderHelp(*spec, *values->level), kMaxLineBytes);

	ResultState &current = ResultOf(context, params);
	current.help = std::move(help);
	return *current.help;
}

cell_t WriteLine(IPluginContext *context, const cell_t *params, const std::vector<std::string> &lines)
{
	if (params[2] < 0 || static_cast<size_t>(params[2]) >= lines.size())
		throw NativeError(Format("There is no line %d (there are %zu)", params[2], lines.size()));
	WriteString(context, params[3], params[4], lines[params[2]]);
	return 0;
}

// GetoptResult.Outcome.get
cell_t Result_Outcome(IPluginContext *context, const cell_t *params)
{
	return static_cast<cell_t>(ResultOf(context, params).outcome);
}

// GetoptResult.OutputLines.get
cell_t Result_OutputLines(IPluginContext *context, const cell_t *params)
{
	return static_cast<cell_t>(ResultOf(context, params).output.size());
}

// GetoptResult.GetOutputLine(line, buffer, maxlength)
cell_t Result_GetOutputLine(IPluginContext *context, const cell_t *params)
{
	return WriteLine(context, params, ResultOf(context, params).output);
}

// GetoptResult.GetError(buffer, maxlength)
cell_t Result_GetError(IPluginContext *context, const cell_t *params)
{
	ResultState &result = ResultOf(context, params);
	WriteString(context, params[2], params[3], result.error);
	return result.outcome == Outcome::Rejected ? 1 : 0;
}

// GetoptResult.GetCommand(buffer, maxlength)
cell_t Result_GetCommand(IPluginContext *context, const cell_t *params)
{
	const smgetopt::LevelResult &values = ValuesOf(ResultOf(context, params));
	WriteString(context, params[2], params[3], values.command.value_or(""));
	return values.command ? 1 : 0;
}

// GetoptResult.Subcommand.get
cell_t Result_Subcommand(IPluginContext *context, const cell_t *params)
{
	ResultState &result = ResultOf(context, params);
	const smgetopt::LevelResult &values = ValuesOf(result);
	if (!values.subcommand)
		return BAD_HANDLE;

	ResultState sub;
	sub.spec = result.spec;
	sub.values = values.subcommand;
	return static_cast<cell_t>(g_Getopt.CreateResult(context, std::move(sub)));
}

// GetoptResult.IsSet(name)
cell_t Result_IsSet(IPluginContext *context, const cell_t *params)
{
	std::string name = ReadString(context, params[2]);
	return ValuesOf(ResultOf(context, params)).IsSet(name) ? 1 : 0;
}

// GetoptResult.Count(name)
cell_t Result_Count(IPluginContext *context, const cell_t *params)
{
	std::string name = ReadString(context, params[2]);
	return static_cast<cell_t>(ValuesOf(ResultOf(context, params)).Count(name));
}

// GetoptResult.KeyCount(name, entry)
cell_t Result_KeyCount(IPluginContext *context, const cell_t *params)
{
	std::string name = ReadString(context, params[2]);
	return static_cast<cell_t>(ValuesOf(ResultOf(context, params)).MappingAt(name, params[3]).size());
}

// GetoptResult.ReadKey(name, entry, index, buffer, maxlength)
cell_t Result_ReadKey(IPluginContext *context, const cell_t *params)
{
	std::string name = ReadString(context, params[2]);
	WriteString(context, params[5], params[6], ValuesOf(ResultOf(context, params)).KeyAt(name, params[3], params[4]));
	return 0;
}

// GetoptResult.ReadString(name, entry, key, buffer, maxlength)
cell_t Result_ReadString(IPluginContext *context, const cell_t *params)
{
	std::string name;
	const Scalar *scalar = ScalarAt(context, params, name);
	WriteString(context, params[5], params[6], scalar ? scalar->text : "");
	return scalar ? 1 : 0;
}

/* An integer a plugin can calculate with: it must fit a cell, the only
 * integer SourcePawn has. */
cell_t IntegerOf(const Scalar &scalar, const std::string &name)
{
	std::optional<int64_t> integer = smgetopt::WholeNumberOf(scalar, name);
	if (!integer || *integer < INT_MIN || *integer > INT_MAX)
		throw NativeError(Format("The value '%s' of '%s' does not fit a cell; read it as a string", scalar.text.c_str(), name.c_str()));
	return static_cast<cell_t>(*integer);
}

// GetoptResult.ReadAttribute(name, entry, key, attribute, buffer, maxlength)
cell_t Result_ReadAttribute(IPluginContext *context, const cell_t *params)
{
	std::string name;
	const Scalar *scalar = ScalarAt(context, params, name);
	auto found = scalar ? scalar->attributes.find(ReadString(context, params[5])) : std::map<std::string, std::string>::const_iterator();
	bool has = scalar && found != scalar->attributes.end();
	WriteString(context, params[6], params[7], has ? found->second : "");
	return has ? 1 : 0;
}

// GetoptResult.ReadInt(name, entry, key, &value)
cell_t Result_ReadInt(IPluginContext *context, const cell_t *params)
{
	std::string name;
	const Scalar *scalar = ScalarAt(context, params, name);
	if (!scalar)
		return 0;

	ReadReference(context, params[5]) = IntegerOf(*scalar, name);
	return 1;
}

// GetoptResult.ReadFloat(name, entry, key, &value)
cell_t Result_ReadFloat(IPluginContext *context, const cell_t *params)
{
	std::string name;
	const Scalar *scalar = ScalarAt(context, params, name);
	if (!scalar)
		return 0;

	ReadReference(context, params[5]) = sp_ftoc(smgetopt::FloatOf(*scalar, name));
	return 1;
}

cell_t DurationPart(int64_t part, const Scalar &scalar, const std::string &name)
{
	if (part < INT_MIN || part > INT_MAX)
		throw NativeError(Format("The duration '%s' of '%s' does not fit cells", scalar.text.c_str(), name.c_str()));
	return static_cast<cell_t>(part);
}

// GetoptResult.ReadDuration(name, entry, key, &months, &days, &seconds)
cell_t Result_ReadDuration(IPluginContext *context, const cell_t *params)
{
	std::string name;
	const Scalar *scalar = ScalarAt(context, params, name);
	if (!scalar)
		return 0;
	const smgetopt::temporal::Duration &duration = smgetopt::DurationOf(*scalar, name);
	const int64_t parts[] = {duration.months, duration.days, duration.seconds};
	for (int i = 0; i < 3; i++)
		ReadReference(context, params[5 + i]) = DurationPart(parts[i], *scalar, name);
	return 1;
}

// GetoptResult.HelpLines.get
cell_t Result_HelpLines(IPluginContext *context, const cell_t *params)
{
	return static_cast<cell_t>(HelpOf(context, params).size());
}

// GetoptResult.GetHelpLine(line, buffer, maxlength)
cell_t Result_GetHelpLine(IPluginContext *context, const cell_t *params)
{
	return WriteLine(context, params, HelpOf(context, params));
}

// GetoptResult.GetVersion(buffer, maxlength)
cell_t Result_GetVersion(IPluginContext *context, const cell_t *params)
{
	WriteString(context, params[2], params[3], smgetopt::RenderVersion(*ResultOf(context, params).spec));
	return 0;
}

} // namespace

const sp_nativeinfo_t g_ResultNatives[] = {
	{"GetoptResult.Outcome.get",     Guarded<Result_Outcome>},
	{"GetoptResult.OutputLines.get", Guarded<Result_OutputLines>},
	{"GetoptResult.GetOutputLine",   Guarded<Result_GetOutputLine>},
	{"GetoptResult.GetError",        Guarded<Result_GetError>},
	{"GetoptResult.GetCommand",      Guarded<Result_GetCommand>},
	{"GetoptResult.Subcommand.get",  Guarded<Result_Subcommand>},
	{"GetoptResult.IsSet",           Guarded<Result_IsSet>},
	{"GetoptResult.Count",           Guarded<Result_Count>},
	{"GetoptResult.KeyCount",        Guarded<Result_KeyCount>},
	{"GetoptResult.ReadKey",         Guarded<Result_ReadKey>},
	{"GetoptResult.ReadString",      Guarded<Result_ReadString>},
	{"GetoptResult.ReadInt",         Guarded<Result_ReadInt>},
	{"GetoptResult.ReadFloat",       Guarded<Result_ReadFloat>},
	{"GetoptResult.ReadDuration",    Guarded<Result_ReadDuration>},
	{"GetoptResult.ReadAttribute",   Guarded<Result_ReadAttribute>},
	{"GetoptResult.HelpLines.get",   Guarded<Result_HelpLines>},
	{"GetoptResult.GetHelpLine",     Guarded<Result_GetHelpLine>},
	{"GetoptResult.GetVersion",      Guarded<Result_GetVersion>},
	{nullptr,                        nullptr},
};
