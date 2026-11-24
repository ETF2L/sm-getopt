/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Option types that plugins write: a type is registered with its names and
 * the callbacks it needs, and the core library calls them through the Type
 * it builds for every option and arg of that type. */
#include "extension.h"

#include "text.h"

#include <utility>

using smgetopt::Problem;
using smgetopt::Scalar;
using smgetopt::TypeKeys;
using smgetopt::TypeKind;
using smgetopt::text::Format;

namespace {

IPluginFunction *CallbackOf(const CustomType &type, TypeCallback which)
{
	return type.callbacks[static_cast<size_t>(which)];
}

/* A callback that judges something: true accepts, false rejects and may
 * say why in its buffer. */
Problem Judged(PluginCall &call, const std::string &fallback)
{
	std::string problem;
	if (call.Buffer(problem).Run() != 0)
		return std::nullopt;
	return problem.empty() ? fallback : problem;
}

/* The callback of a type, or null when the type has none. A type whose
 * plugin has unloaded runs nothing: a spec that still holds it is told. */
IPluginFunction *CallbackOf(const CustomType &type, TypeCallback which, const char *callbackName)
{
	IPluginFunction *function = CallbackOf(type, which);
	if (function && !type.owner->context) {
		throw smgetopt::CallbackError(Format("The %s callback of type '%s' cannot run: its plugin has unloaded", callbackName,
		                                     type.name.c_str()));
	}
	return function;
}

/* The client a parse runs for, as a type's callbacks see it: -1 for the
 * steps that do not run in a parse. */
const cell_t kNoCaller = -1;

/* The type of one option or arg of a plugin's type, with the spec keys it
 * was given. */
class PluginType : public smgetopt::Type {
public:
	PluginType(std::shared_ptr<const CustomType> type, TypeKeys keys) : m_type(std::move(type)), m_keys(std::move(keys)) {}

	TypeKind Kind() const override { return m_type->kind; }

	Problem Check(const std::string &value) const override
	{
		return JudgeValue(TypeCallback::Check, "check", value, kNoCaller);
	}

	Scalar Coerce(const std::string &value) const override
	{
		IPluginFunction *function = CallbackOf(*m_type, TypeCallback::Coerce, "coerce");
		if (!function)
			return Scalar{value, {}, {}, {}, {}};

		std::string coerced = value;
		KeysView view{m_keys, kNoCaller};
		LentKeys keys(view, m_type->owner->context);
		PluginCall(function, Format("coerce callback of type '%s'", m_type->name.c_str())).Cell(keys.Id()).Text(value).Buffer(coerced).Run();
		return Scalar{coerced, {}, {}, {}, {}};
	}

	Problem Resolve(Scalar &value, const smgetopt::ParseContext &context) const override
	{
		IPluginFunction *function = CallbackOf(*m_type, TypeCallback::Resolve, "resolve");
		if (!function)
			return std::nullopt;

		std::string given = value.text;
		KeysView view{m_keys, static_cast<cell_t>(context.caller)};
		LentKeys keys(view, m_type->owner->context);
		LentResolved resolved(value, m_type->owner->context);
		PluginCall call(function, Format("resolve callback of type '%s'", m_type->name.c_str()));
		call.Cell(keys.Id()).Text(given).Cell(resolved.Id());
		return Judged(call, Format("'%s' is not a valid value", given.c_str()));
	}

	Problem Verify(const Scalar &value, const smgetopt::ParseContext &context) const override
	{
		return JudgeValue(TypeCallback::Verify, "verify", value.text, static_cast<cell_t>(context.caller));
	}

	Problem Prepare(const Scalar &value, const smgetopt::ParseContext &context) const override
	{
		return JudgeValue(TypeCallback::Prepare, "prepare", value.text, static_cast<cell_t>(context.caller));
	}

	std::string Label() const override { return m_type->label; }

	std::vector<std::string> ConstraintNotes() const override
	{
		std::vector<std::string> notes;
		IPluginFunction *function = CallbackOf(*m_type, TypeCallback::Notes, "notes");
		if (!function)
			return notes;

		KeysView view{m_keys, kNoCaller};
		LentKeys keys(view, m_type->owner->context);
		LentList list(notes, m_type->owner->context);
		PluginCall(function, Format("notes callback of type '%s'", m_type->name.c_str())).Cell(keys.Id()).Cell(list.Id()).Run();
		return notes;
	}

private:
	Problem JudgeValue(TypeCallback which, const char *callbackName, const std::string &value, cell_t caller) const
	{
		IPluginFunction *function = CallbackOf(*m_type, which, callbackName);
		if (!function)
			return std::nullopt;

		KeysView view{m_keys, caller};
		LentKeys keys(view, m_type->owner->context);
		PluginCall call(function, Format("%s callback of type '%s'", callbackName, m_type->name.c_str()));
		call.Cell(keys.Id()).Text(value);
		return Judged(call, Format("'%s' is not a valid value", value.c_str()));
	}

	std::shared_ptr<const CustomType> m_type;
	TypeKeys m_keys;
};

Problem CheckKeys(const CustomType &type, const TypeKeys &typeKeys)
{
	IPluginFunction *function = CallbackOf(type, TypeCallback::CheckKeys, "checkKeys");
	if (!function)
		return std::nullopt;

	KeysView view{typeKeys, kNoCaller};
	LentKeys keys(view, type.owner->context);
	PluginCall call(function, Format("checkKeys callback of type '%s'", type.name.c_str()));
	call.Cell(keys.Id());
	return Judged(call, "invalid spec keys");
}

// Getopt_RegisterType(names, kind, keys, shared)
cell_t Type_Register(IPluginContext *context, const cell_t *params)
{
	std::vector<std::string> names = smgetopt::text::SplitAtWhitespace(ReadString(context, params[1]));
	if (params[2] < 0 || params[2] > static_cast<cell_t>(TypeKind::Value))
		throw NativeError(Format("Invalid GetoptKind %d", params[2]));

	auto type = std::make_shared<CustomType>();
	type->name = names.empty() ? "" : names[0];
	type->kind = static_cast<TypeKind>(params[2]);
	type->shared = params[0] >= 4 && params[4] != 0;

	smgetopt::TypeDefinition definition;
	definition.names = names;
	definition.specKeys = smgetopt::text::SplitAtWhitespace(ReadString(context, params[3]));
	definition.checkSpecKeys = [type](const TypeKeys &keys) { return CheckKeys(*type, keys); };
	definition.create = [type](const TypeKeys &keys) { return std::make_shared<PluginType>(type, keys); };

	return g_Getopt.RegisterType(context, std::move(type), std::move(definition));
}

// GetoptType.SetLabel(label)
cell_t Type_SetLabel(IPluginContext *context, const cell_t *params)
{
	g_Getopt.FindCustomType(context, params[1]).label = ReadString(context, params[2]);
	return params[1];
}

// Getopt_SetTypeCallback(type, which, callback)
cell_t Type_SetCallback(IPluginContext *context, const cell_t *params)
{
	CustomType &type = g_Getopt.FindCustomType(context, params[1]);
	if (params[2] < 0 || params[2] >= static_cast<cell_t>(TypeCallback::Count))
		throw NativeError(Format("Invalid type callback %d", params[2]));
	type.callbacks[params[2]] = ReadFunction(context, params[3]);
	return 0;
}

// GetoptKeys.Has(key)
cell_t Keys_Has(IPluginContext *context, const cell_t *params)
{
	const TypeKeys &keys = LentKeys::Find(context, params[1], "GetoptKeys").keys;
	return keys.count(ReadString(context, params[2])) ? 1 : 0;
}

// GetoptKeys.Get(key, buffer, maxlength)
cell_t Keys_Get(IPluginContext *context, const cell_t *params)
{
	const TypeKeys &keys = LentKeys::Find(context, params[1], "GetoptKeys").keys;
	auto found = keys.find(ReadString(context, params[2]));
	WriteString(context, params[3], params[4], found == keys.end() ? "" : found->second);
	return found == keys.end() ? 0 : 1;
}

// GetoptKeys.Caller.get
cell_t Keys_Caller(IPluginContext *context, const cell_t *params)
{
	return LentKeys::Find(context, params[1], "GetoptKeys").caller;
}

// GetoptResolved.SetValue(text)
cell_t Resolved_SetValue(IPluginContext *context, const cell_t *params)
{
	Scalar &value = LentResolved::Find(context, params[1], "GetoptResolved");
	/* The numbers the type gave the old text do not hold for the new one. */
	value = Scalar{ReadString(context, params[2]), {}, {}, {}, std::move(value.attributes)};
	return 0;
}

// GetoptResolved.SetAttribute(name, value)
cell_t Resolved_SetAttribute(IPluginContext *context, const cell_t *params)
{
	std::string name = ReadString(context, params[2]);
	if (name.empty())
		throw NativeError("An attribute needs a name");
	LentResolved::Find(context, params[1], "GetoptResolved").attributes[name] = ReadString(context, params[3]);
	return 0;
}

// GetoptList.Add(text)
cell_t List_Add(IPluginContext *context, const cell_t *params)
{
	LentList::Find(context, params[1], "GetoptList").push_back(ReadString(context, params[2]));
	return 0;
}

} // namespace

const sp_nativeinfo_t g_TypeNatives[] = {
	{"Getopt_RegisterType",    Guarded<Type_Register>},
	{"GetoptType.SetLabel",    Guarded<Type_SetLabel>},
	{"Getopt_SetTypeCallback", Guarded<Type_SetCallback>},
	{"GetoptKeys.Has",         Guarded<Keys_Has>},
	{"GetoptKeys.Get",         Guarded<Keys_Get>},
	{"GetoptKeys.Caller.get",  Guarded<Keys_Caller>},
	{"GetoptResolved.SetValue",     Guarded<Resolved_SetValue>},
	{"GetoptResolved.SetAttribute", Guarded<Resolved_SetAttribute>},
	{"GetoptList.Add",         Guarded<List_Add>},
	{nullptr,                  nullptr},
};
