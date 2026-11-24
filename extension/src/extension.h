/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The SourceMod side of the extension. A plugin declares a spec through
 * natives, which only collect the declaration; the first parse (or
 * Validate) compiles it with the core library. Everything a plugin holds is
 * either a handle (a spec, a result) or a plain number that names a part
 * of a spec, a type, or an object lent to a callback while it runs. */
#ifndef GETOPT_EXTENSION_H
#define GETOPT_EXTENSION_H

#include "smsdk_ext.h"

#include "errors.h"
#include "ownership.h"
#include "parser.h"
#include "spec.h"
#include "types.h"

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

/* A mistake in how a plugin calls a native. */
class NativeError : public std::runtime_error {
public:
	explicit NativeError(const std::string &message) : std::runtime_error(message) {}
};

/* The plugin that made a spec, a result or a type. Only that plugin may
 * use it: its callbacks are in there, and they end with the plugin. The
 * context is null once the plugin has unloaded. Its address is the key of
 * the plugin's types and specs in the core's ownership. */
struct Owner {
	IPluginContext *context;
};

/* A spec of a plugin, with what a plugin needs to change it by native. */
struct SpecState : smgetopt::OwnedSpec {
	/* Set while the declaration is read by the compiler, whose checks may
	 * call back into the plugin. */
	bool compiling = false;
	std::shared_ptr<Owner> owner;
	std::vector<cell_t> nodes;
};

enum class NodeKind { Level, Option, Arg };

/* A part of a spec a plugin names by number. */
struct Node {
	std::weak_ptr<SpecState> spec;
	NodeKind kind;
	smgetopt::RawLevel *level;
	smgetopt::RawEntry *entry;
};

/* What a result handle holds: one level of a parse. */
struct ResultState {
	std::shared_ptr<Owner> owner;
	std::shared_ptr<const smgetopt::Spec> spec;
	smgetopt::Outcome outcome = smgetopt::Outcome::Parsed;
	std::string error;
	std::vector<std::string> output;
	/* The level's values, when parsed. */
	std::shared_ptr<const smgetopt::LevelResult> values;
	/* The level's help, rendered when first asked for. */
	std::optional<std::vector<std::string>> help;
};

/* The callbacks of a type a plugin registered, in the order of the
 * include's GetoptTypeCallback. */
enum class TypeCallback { Check, Coerce, Verify, Prepare, CheckKeys, Notes, Resolve, Count };

struct CustomType {
	std::shared_ptr<Owner> owner;
	std::string name;
	smgetopt::TypeKind kind = smgetopt::TypeKind::Value;
	std::string label;
	/* Whether every plugin's specs can name the type, not only its own. */
	bool shared = false;
	IPluginFunction *callbacks[static_cast<size_t>(TypeCallback::Count)] = {};
};

class GetoptExtension : public SDKExtension, public IHandleTypeDispatch, public IPluginsListener {
public: // SDKExtension
	bool SDK_OnLoad(char *error, size_t maxlength, bool late) override;
	void SDK_OnUnload() override;

public: // IHandleTypeDispatch
	void OnHandleDestroy(HandleType_t type, void *object) override;

public: // IPluginsListener
	void OnPluginUnloaded(IPlugin *plugin) override;
	void OnPluginDestroyed(IPlugin *plugin) override;

public:
	/* Everything below that takes the calling plugin's context throws
	 * NativeError when what is asked for belongs to another plugin. */

	/* A new spec of the calling plugin and its handle. */
	Handle_t CreateSpec(IPluginContext *context, const std::string &name);
	std::shared_ptr<SpecState> ReadSpec(IPluginContext *context, Handle_t handle);

	/* The number under which a plugin names a part of spec. */
	cell_t AddNode(const std::shared_ptr<SpecState> &spec, NodeKind kind, smgetopt::RawLevel *level, smgetopt::RawEntry *entry);

	/* The part id names, which must be of one of the kinds. Throws
	 * NativeError for anything else, and for a part whose spec is closed. */
	Node &FindNode(IPluginContext *context, cell_t id, NodeKind kind, std::optional<NodeKind> alternative = std::nullopt);

	/* A new result of the calling plugin and its handle. Lines too long for
	 * a plugin's line buffer are broken. */
	Handle_t CreateResult(IPluginContext *context, ResultState state);
	ResultState &ReadResult(IPluginContext *context, Handle_t handle);
	void FreeResult(IPluginContext *context, Handle_t handle);

	/* The types the owner's specs can name: its own, then the shared ones. */
	const smgetopt::TypeRegistry &TypesOf(const Owner &owner);

	/* Registers a type of the calling plugin: for its own specs, or shared
	 * with every plugin's. Throws NativeError for a name that is taken. */
	cell_t RegisterType(IPluginContext *context, std::shared_ptr<CustomType> type, smgetopt::TypeDefinition definition);
	CustomType &FindCustomType(IPluginContext *context, cell_t id);

private:
	std::shared_ptr<Owner> OwnerOf(IPluginContext *context);

	/* Lets go of everything a plugin made. SourceMod tells a plugin's end
	 * by OnPluginUnloaded only when it got as far as OnPluginStart, and by
	 * OnPluginDestroyed always; this runs on the first and does nothing on
	 * the second. */
	void ForgetPlugin(IPluginContext *context);

	HandleType_t m_specType = 0;
	HandleType_t m_resultType = 0;
	cell_t m_nextId = 1;
	std::unordered_map<IPluginContext *, std::shared_ptr<Owner>> m_owners;
	std::unordered_map<cell_t, Node> m_nodes;
	std::unordered_map<cell_t, std::shared_ptr<CustomType>> m_customTypes;
	std::unique_ptr<smgetopt::Ownership> m_ownership;
};

extern GetoptExtension g_Getopt;
extern const sp_nativeinfo_t g_SpecNatives[];
extern const sp_nativeinfo_t g_TypeNatives[];
extern const sp_nativeinfo_t g_ResultNatives[];

/* --------------------------------------------------- shared by the natives */

/* Runs a native's body; what it throws becomes the native's error. */
template <cell_t (*Body)(IPluginContext *, const cell_t *)>
cell_t Guarded(IPluginContext *context, const cell_t *params)
{
	try {
		return Body(context, params);
	} catch (const smgetopt::SpecError &error) {
		return context->ThrowNativeError("Getopt spec: %s", error.what());
	} catch (const std::exception &error) {
		return context->ThrowNativeError("%s", error.what());
	}
}

std::string ReadString(IPluginContext *context, cell_t address);

/* Writes text into a plugin's buffer, cut at a character boundary. Throws
 * NativeError for a negative size or a buffer outside the plugin's memory. */
void WriteString(IPluginContext *context, cell_t address, cell_t maxlength, const std::string &text);

/* The function a plugin passed as a callback. Throws NativeError when it is
 * none. */
IPluginFunction *ReadFunction(IPluginContext *context, cell_t id);

/* The cell a plugin passed by reference. */
cell_t &ReadReference(IPluginContext *context, cell_t address);

/* The longest line a plugin is handed, which fits GETOPT_LINE_LENGTH of
 * the include. */
const size_t kMaxLineBytes = 500;

/* The size of the buffers callbacks write text into. */
const size_t kCallbackTextSize = 1024;

/* One call of a plugin callback: push the arguments in order, then Run. */
class PluginCall {
public:
	/* what names the callback in the error of a call that fails. */
	PluginCall(IPluginFunction *function, std::string what);
	~PluginCall();
	PluginCall(const PluginCall &) = delete;
	PluginCall &operator=(const PluginCall &) = delete;

	PluginCall &Cell(cell_t value);
	PluginCall &Text(const std::string &text);
	/* A buffer of kCallbackTextSize that starts as text and is read back
	 * into text after the call, followed by its size. */
	PluginCall &Buffer(std::string &text);

	/* Throws CallbackError when the callback cannot run or fails. */
	cell_t Run();

private:
	struct OutBuffer {
		std::unique_ptr<char[]> bytes;
		std::string *target;
	};

	IPluginFunction *m_function;
	std::string m_what;
	std::vector<OutBuffer> m_buffers;
	bool m_ran = false;
};

/* A number no other lent object has had. */
cell_t NextLentId();

/* An object lent to the callback of one plugin under a number, for as long
 * as the scope lives: the keys of a type, or a list the callback fills. */
template <typename T>
class Lent {
public:
	/* borrower is the context of the plugin whose callback gets the object. */
	Lent(T &object, IPluginContext *borrower) : m_id(NextLentId()) { Objects()[m_id] = {&object, borrower}; }
	~Lent() { Objects().erase(m_id); }
	Lent(const Lent &) = delete;
	Lent &operator=(const Lent &) = delete;

	cell_t Id() const { return m_id; }

	/* Throws NativeError when id names nothing that is lent right now, or
	 * something lent to another plugin's callback. */
	static T &Find(IPluginContext *context, cell_t id, const char *what)
	{
		auto found = Objects().find(id);
		if (found == Objects().end())
			throw NativeError(std::string(what) + " can only be used inside the callback it was passed to");
		if (found->second.borrower != context)
			throw NativeError(std::string(what) + " was passed to another plugin's callback");
		return *found->second.object;
	}

private:
	struct Loan {
		T *object;
		IPluginContext *borrower;
	};

	static std::unordered_map<cell_t, Loan> &Objects()
	{
		static std::unordered_map<cell_t, Loan> objects;
		return objects;
	}

	cell_t m_id;
};

/* What a type's callback is lent as its GetoptKeys: the option's spec keys
 * and the client the parse runs for, -1 outside a parse's steps. */
struct KeysView {
	const smgetopt::TypeKeys &keys;
	cell_t caller;
};

using LentKeys = Lent<const KeysView>;
using LentList = Lent<std::vector<std::string>>;
using LentResolved = Lent<smgetopt::Scalar>;

#endif // GETOPT_EXTENSION_H
