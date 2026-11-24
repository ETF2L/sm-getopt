/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The values of one level a parse selected, and the rules by which they
 * are read: which address fits which shape, which entries and keys there
 * are, and which numbers a value stands for. Reading what is not there in
 * that way is a mistake of the reader and throws ReadError. */
#ifndef GETOPT_CORE_RESULT_H
#define GETOPT_CORE_RESULT_H

#include "spec.h"
#include "temporal.h"
#include "value.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace smgetopt {

/* The entry of an address that names none: a single value or a mapping. */
const int kNoEntry = -1;

class LevelResult {
public:
	/* values holds the value of every declared option and arg of level, by
	 * primary name or arg name. */
	LevelResult(const Level *level, std::map<std::string, Value> values, std::optional<std::string> command,
	            std::shared_ptr<const LevelResult> subcommand);

	const Level *level;

	/* The command selected on this level and its values, when the level has
	 * commands and one was given. */
	std::optional<std::string> command;
	std::shared_ptr<const LevelResult> subcommand;

	/* The value of an option or arg of this level. */
	const Value &ValueNamed(const std::string &name) const;

	/* Whether the value holds anything: a single value, or an entry. */
	bool IsSet(const std::string &name) const;

	/* The single values a list holds, the keys of a mapping, the entries of
	 * a list of mappings, or 1 for a single value that is set. */
	size_t Count(const std::string &name) const;

	/* The single value at an address: no entry and no key for a single
	 * value, an entry for a list, a key for a mapping, both for a list of
	 * mappings. An entry below 0 is none, an empty key is none. Null when a
	 * single value is unset or a mapping lacks the key. Throws when the
	 * address does not fit the value's shape or the entry is not there. */
	const Scalar *At(const std::string &name, int entry = kNoEntry, const std::string &key = "") const;

	/* A mapping (no entry), or one entry of a list of mappings. */
	const Mapping &MappingAt(const std::string &name, int entry = kNoEntry) const;

	/* The key at index of what MappingAt gives, in the order of the keys. */
	const std::string &KeyAt(const std::string &name, int entry, int index) const;

private:
	std::map<std::string, Value> m_values;
};

/* The whole number a value of name stands for: the one its type gave it,
 * or the one its text or its number is. Nothing when it is whole but does
 * not fit 64 bits. Throws when it is no whole number. */
std::optional<int64_t> WholeNumberOf(const Scalar &scalar, const std::string &name);

/* The number a value of name stands for, in a plugin's precision. Throws
 * when it is no number. */
float FloatOf(const Scalar &scalar, const std::string &name);

/* The duration a value of name stands for. Throws when its type gave it
 * none. */
const temporal::Duration &DurationOf(const Scalar &scalar, const std::string &name);

} // namespace smgetopt

#endif // GETOPT_CORE_RESULT_H
