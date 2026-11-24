/* Copyright 2026 ETF2L Contributors, Dave */
#include "extension.h"

#include "text.h"

#include <string.h>

#include <utility>

GetoptExtension g_Getopt;
SMEXT_LINK(&g_Getopt);

bool GetoptExtension::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	HandleError handleError;
	m_specType = handlesys->CreateType("GetoptSpec", this, 0, nullptr, nullptr, myself->GetIdentity(), &handleError);
	m_resultType = handlesys->CreateType("GetoptResult", this, 0, nullptr, nullptr, myself->GetIdentity(), &handleError);
	if (!m_specType || !m_resultType) {
		snprintf(error, maxlength, "Could not create the handle types (error %d)", handleError);
		return false;
	}

	m_ownership = std::make_unique<smgetopt::Ownership>();
	sharesys->AddNatives(myself, g_SpecNatives);
	sharesys->AddNatives(myself, g_TypeNatives);
	sharesys->AddNatives(myself, g_ResultNatives);
	sharesys->RegisterLibrary(myself, "getopt");
	plsys->AddPluginsListener(this);
	return true;
}

void GetoptExtension::SDK_OnUnload()
{
	plsys->RemovePluginsListener(this);
	handlesys->RemoveType(m_specType, myself->GetIdentity());
	handlesys->RemoveType(m_resultType, myself->GetIdentity());
	m_nodes.clear();
	m_owners.clear();
	m_customTypes.clear();
	m_ownership.reset();
}

void GetoptExtension::OnHandleDestroy(HandleType_t type, void *object)
{
	if (type == m_resultType) {
		delete static_cast<ResultState *>(object);
		return;
	}

	/* A native working on the spec keeps it alive until it returns; the
	 * numbers that name its parts end with the handle. */
	auto *spec = static_cast<std::shared_ptr<SpecState> *>(object);
	for (cell_t id : (*spec)->nodes)
		m_nodes.erase(id);
	delete spec;
}

void GetoptExtension::OnPluginUnloaded(IPlugin *plugin)
{
	ForgetPlugin(plugin->GetBaseContext());
}

void GetoptExtension::OnPluginDestroyed(IPlugin *plugin)
{
	ForgetPlugin(plugin->GetBaseContext());
}

void GetoptExtension::ForgetPlugin(IPluginContext *context)
{
	auto owner = m_owners.find(context);
	if (owner == m_owners.end())
		return;

	/* What the plugin made and another plugin still holds a handle to
	 * answers to nobody from here on. */
	owner->second->context = nullptr;
	for (auto it = m_customTypes.begin(); it != m_customTypes.end();) {
		if (it->second->owner == owner->second)
			it = m_customTypes.erase(it);
		else
			++it;
	}

	m_ownership->Forget(owner->second.get());
	m_owners.erase(owner);
}

std::shared_ptr<Owner> GetoptExtension::OwnerOf(IPluginContext *context)
{
	std::shared_ptr<Owner> &owner = m_owners[context];
	if (!owner)
		owner = std::make_shared<Owner>(Owner{context});
	return owner;
}

Handle_t GetoptExtension::CreateSpec(IPluginContext *context, const std::string &name)
{
	auto state = std::make_shared<SpecState>();
	state->owner = OwnerOf(context);
	state->raw.programName = name;

	/* --version shows the plugin's own version unless the spec sets one. */
	IPlugin *plugin = plsys->FindPluginByContext(context->GetContext());
	const sm_plugininfo_t *info = plugin ? plugin->GetPublicInfo() : nullptr;
	if (info && info->version)
		state->raw.fallbackVersion = info->version;

	m_ownership->TrackSpec(state->owner.get(), state);
	auto *object = new std::shared_ptr<SpecState>(state);
	HandleError error;
	Handle_t handle = handlesys->CreateHandle(m_specType, object, context->GetIdentity(), myself->GetIdentity(), &error);
	if (handle == BAD_HANDLE) {
		delete object;
		throw NativeError(smgetopt::text::Format("Could not create the spec handle (error %d)", error));
	}
	return handle;
}

std::shared_ptr<SpecState> GetoptExtension::ReadSpec(IPluginContext *context, Handle_t handle)
{
	HandleSecurity security(nullptr, myself->GetIdentity());
	std::shared_ptr<SpecState> *spec;
	HandleError error = handlesys->ReadHandle(handle, m_specType, &security, reinterpret_cast<void **>(&spec));
	if (error != HandleError_None)
		throw NativeError(smgetopt::text::Format("Invalid GetoptSpec handle %x (error %d)", handle, error));
	if ((*spec)->owner->context != context)
		throw NativeError("This spec belongs to another plugin");
	return *spec;
}

cell_t GetoptExtension::AddNode(const std::shared_ptr<SpecState> &spec, NodeKind kind, smgetopt::RawLevel *level,
                                smgetopt::RawEntry *entry)
{
	cell_t id = m_nextId++;
	m_nodes[id] = Node{spec, kind, level, entry};
	spec->nodes.push_back(id);
	return id;
}

Node &GetoptExtension::FindNode(IPluginContext *context, cell_t id, NodeKind kind, std::optional<NodeKind> alternative)
{
	auto found = m_nodes.find(id);
	bool fits = found != m_nodes.end() && (found->second.kind == kind || (alternative && found->second.kind == *alternative));
	std::shared_ptr<SpecState> spec = fits ? found->second.spec.lock() : nullptr;
	if (!spec) {
		const char *expected = kind == NodeKind::Level ? "level" : alternative ? "option or arg" : kind == NodeKind::Option ? "option" : "arg";
		throw NativeError(smgetopt::text::Format("%d is no %s of an open spec", id, expected));
	}
	if (spec->owner->context != context)
		throw NativeError("This spec belongs to another plugin");
	return found->second;
}

Handle_t GetoptExtension::CreateResult(IPluginContext *context, ResultState state)
{
	state.owner = OwnerOf(context);
	state.output = smgetopt::text::FitLines(state.output, kMaxLineBytes);
	auto *object = new ResultState(std::move(state));
	HandleError error;
	Handle_t handle = handlesys->CreateHandle(m_resultType, object, context->GetIdentity(), myself->GetIdentity(), &error);
	if (handle == BAD_HANDLE) {
		delete object;
		throw NativeError(smgetopt::text::Format("Could not create the result handle (error %d)", error));
	}
	return handle;
}

ResultState &GetoptExtension::ReadResult(IPluginContext *context, Handle_t handle)
{
	HandleSecurity security(nullptr, myself->GetIdentity());
	ResultState *result;
	HandleError error = handlesys->ReadHandle(handle, m_resultType, &security, reinterpret_cast<void **>(&result));
	if (error != HandleError_None)
		throw NativeError(smgetopt::text::Format("Invalid GetoptResult handle %x (error %d)", handle, error));
	if (result->owner->context != context)
		throw NativeError("This result belongs to another plugin");
	return *result;
}

void GetoptExtension::FreeResult(IPluginContext *context, Handle_t handle)
{
	HandleSecurity security(context->GetIdentity(), myself->GetIdentity());
	handlesys->FreeHandle(handle, &security);
}

const smgetopt::TypeRegistry &GetoptExtension::TypesOf(const Owner &owner)
{
	return m_ownership->TypesOf(&owner);
}

cell_t GetoptExtension::RegisterType(IPluginContext *context, std::shared_ptr<CustomType> type, smgetopt::TypeDefinition definition)
{
	type->owner = OwnerOf(context);
	try {
		m_ownership->RegisterType(type->owner.get(), std::move(definition), type->shared);
	} catch (const std::invalid_argument &error) {
		throw NativeError(std::string("Getopt: ") + error.what());
	}

	cell_t id = m_nextId++;
	m_customTypes[id] = std::move(type);
	return id;
}

CustomType &GetoptExtension::FindCustomType(IPluginContext *context, cell_t id)
{
	auto found = m_customTypes.find(id);
	if (found == m_customTypes.end())
		throw NativeError(smgetopt::text::Format("%d is no registered option type", id));
	if (found->second->owner->context != context)
		throw NativeError("This option type belongs to another plugin");
	return *found->second;
}

/* --------------------------------------------------- shared by the natives */

std::string ReadString(IPluginContext *context, cell_t address)
{
	char *text;
	if (context->LocalToString(address, &text) != SP_ERROR_NONE)
		throw NativeError("Invalid string argument");
	return text;
}

cell_t &ReadReference(IPluginContext *context, cell_t address)
{
	cell_t *cell;
	if (context->LocalToPhysAddr(address, &cell) != SP_ERROR_NONE)
		throw NativeError("Invalid by-reference argument");
	return *cell;
}

cell_t NextLentId()
{
	static cell_t next = 0;
	return ++next;
}

void WriteString(IPluginContext *context, cell_t address, cell_t maxlength, const std::string &text)
{
	if (maxlength < 0)
		throw NativeError(smgetopt::text::Format("Invalid buffer size %d", maxlength));
	if (context->StringToLocalUTF8(address, static_cast<size_t>(maxlength), text.c_str(), nullptr) != SP_ERROR_NONE)
		throw NativeError("Invalid string buffer");
}

IPluginFunction *ReadFunction(IPluginContext *context, cell_t id)
{
	IPluginFunction *function = context->GetFunctionById(static_cast<funcid_t>(id));
	if (!function)
		throw NativeError(smgetopt::text::Format("Invalid callback function %x", id));
	return function;
}

PluginCall::PluginCall(IPluginFunction *function, std::string what) : m_function(function), m_what(std::move(what))
{
}

/* Arguments pushed for a call that never ran must not reach the next one. */
PluginCall::~PluginCall()
{
	if (!m_ran)
		m_function->Cancel();
}

PluginCall &PluginCall::Cell(cell_t value)
{
	m_function->PushCell(value);
	return *this;
}

PluginCall &PluginCall::Text(const std::string &text)
{
	m_function->PushString(text.c_str());
	return *this;
}

PluginCall &PluginCall::Buffer(std::string &text)
{
	auto bytes = std::make_unique<char[]>(kCallbackTextSize);
	strncpy(bytes.get(), text.c_str(), kCallbackTextSize - 1);
	m_function->PushStringEx(bytes.get(), kCallbackTextSize, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_function->PushCell(static_cast<cell_t>(kCallbackTextSize));
	m_buffers.push_back({std::move(bytes), &text});
	return *this;
}

cell_t PluginCall::Run()
{
	cell_t result = 0;
	m_ran = true;
	int error = m_function->Execute(&result);
	if (error != SP_ERROR_NONE)
		throw smgetopt::CallbackError("The " + m_what + " could not be run");

	for (OutBuffer &buffer : m_buffers) {
		buffer.bytes[kCallbackTextSize - 1] = '\0';
		*buffer.target = buffer.bytes.get();
	}
	return result;
}
