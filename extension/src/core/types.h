/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Option types: what decides whether an option takes a value, how the
 * value is checked and converted, and how the help labels it. A type
 * definition is registered under its names; every option and arg that uses
 * it gets a type of its own, built from the spec keys the definition
 * declares. */
#ifndef GETOPT_CORE_TYPES_H
#define GETOPT_CORE_TYPES_H

#include "value.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace smgetopt {

/* How an option of a type appears on the command line. */
enum class TypeKind {
	Flag,     // --name
	Bool,     // --name, --no-name
	Counter,  // --name --name, -nn
	Value,    // --name VALUE
};

/* A problem with a value or a spec key, worded without the name of the
 * option; nothing when there is none. */
using Problem = std::optional<std::string>;

/* The spec keys an option or arg gave its type, such as min and max. */
using TypeKeys = std::map<std::string, std::string>;

/* What a parse knows about the one it runs for, handed to the steps of a
 * type that run during the parse. */
struct ParseContext {
	/* Who runs the command. What the number means is the embedder's: the
	 * SourceMod extension passes the client, 0 for the server. */
	int64_t caller = 0;
};

/* One value checked and converted: the problem with it, or the value a
 * reader returns. */
struct Conversion {
	Problem problem;
	Scalar scalar;
};

/* The type of one option or arg. */
class Type {
public:
	virtual ~Type() = default;

	virtual TypeKind Kind() const = 0;

	/* Whether value is acceptable. A value that fails is passed to no other
	 * method. */
	virtual Problem Check(const std::string &value) const { return std::nullopt; }

	/* The value a reader returns for a value that passed Check. */
	virtual Scalar Coerce(const std::string &value) const { return Scalar{value, {}, {}, {}, {}}; }

	/* Check and Coerce of one value, which is how the value pipeline asks.
	 * A type whose check and conversion must agree on one reading of the
	 * clock overrides it and reads the clock once. */
	virtual Conversion Convert(const std::string &value) const
	{
		if (Problem problem = Check(value))
			return Conversion{problem, {}};
		return Conversion{std::nullopt, Coerce(value)};
	}

	/* Whether what a value means depends on the moment it is converted, as
	 * "tomorrow" does: a default of such a type is converted anew for every
	 * parse. */
	virtual bool FollowsClock() const { return false; }

	/* Looks a value up in what the parse runs in, such as the players on a
	 * server, and may replace it with what it found. Runs on the values a
	 * parse settles on, defaults included, before any is verified. */
	virtual Problem Resolve(Scalar &value, const ParseContext &context) const { return std::nullopt; }

	/* Checks that depend on the machine, not on the value. Runs on the
	 * values a parse settles on, once all are resolved and before anything
	 * is prepared. */
	virtual Problem Verify(const Scalar &value, const ParseContext &context) const { return std::nullopt; }

	/* Arranges what a value needs. Runs only when every value of the parse
	 * passed Verify. */
	virtual Problem Prepare(const Scalar &value, const ParseContext &context) const { return std::nullopt; }

	/* The tag the help shows after the help text, empty for none. */
	virtual std::string Label() const { return ""; }

	/* Notes the help shows in brackets before the help text. */
	virtual std::vector<std::string> ConstraintNotes() const { return {}; }

	bool TakesValue() const { return Kind() == TypeKind::Value; }
};

struct TypeDefinition {
	/* The names the type answers to in a spec, matched without case. */
	std::vector<std::string> names;

	/* The extra spec keys options and args of this type may use. */
	std::vector<std::string> specKeys;

	/* Checks the spec keys one option or arg gave, before Create. */
	std::function<Problem(const TypeKeys &)> checkSpecKeys;

	std::function<std::shared_ptr<const Type>(const TypeKeys &)> create;
};

/* The current time in seconds since the epoch. */
using Clock = std::function<int64_t()>;

int64_t SystemClock();

/* The types one program (one plugin) can name in its specs: the built-in
 * ones and those it registered, and after them those of a parent registry,
 * such as types shared by several programs. */
class TypeRegistry {
public:
	/* clock is what the date and duration types count from. */
	explicit TypeRegistry(Clock clock = SystemClock);

	/* Throws std::invalid_argument when the definition has no names, an
	 * empty name, or a name that is taken in this registry. A name of the
	 * parent is not taken: this registry's type comes first. */
	void Register(TypeDefinition definition);

	/* The registry asked for names this one does not have; null for none.
	 * It must outlive this registry or be replaced first. */
	void SetParent(const TypeRegistry *parent);

	const TypeDefinition *Find(const std::string &name) const;

	/* Every name this registry or its parent knows, sorted. */
	std::vector<std::string> KnownNames() const;

private:
	std::map<std::string, std::shared_ptr<const TypeDefinition>> m_byName;
	const TypeRegistry *m_parent = nullptr;
};

} // namespace smgetopt

#endif // GETOPT_CORE_TYPES_H
