/* Copyright 2026 ETF2L Contributors, Dave
 *
 * What the spec, parser and help tests share: short ways to declare a raw
 * spec, to parse with it and to look at what came out, read through the
 * result's own rules. */
#ifndef GETOPT_TESTS_SUPPORT_H
#define GETOPT_TESTS_SUPPORT_H

#include "errors.h"
#include "help.h"
#include "parser.h"
#include "result.h"
#include "spec.h"
#include "text.h"
#include "types.h"

#include <map>
#include <string>
#include <vector>

namespace support {

using Keys = std::map<std::string, std::string>;
using Words = std::vector<std::string>;

/* 2026-10-06 12:00:00 UTC, the "now" of every test. */
const int64_t kNow = 1791288000;

inline smgetopt::RawSpec NewSpec()
{
	smgetopt::RawSpec spec;
	spec.programName = "tool";
	return spec;
}

inline smgetopt::RawEntry &AddOption(smgetopt::RawLevel &level, const std::string &key, Keys keys = {})
{
	level.options.push_back(std::make_unique<smgetopt::RawEntry>());
	level.options.back()->key = key;
	level.options.back()->keys = std::move(keys);
	return *level.options.back();
}

inline smgetopt::RawEntry &AddArg(smgetopt::RawLevel &level, const std::string &name, Keys keys = {})
{
	level.args.push_back(std::make_unique<smgetopt::RawEntry>());
	level.args.back()->key = name;
	level.args.back()->keys = std::move(keys);
	return *level.args.back();
}

inline smgetopt::RawLevel &AddCommand(smgetopt::RawLevel &level, const std::string &name, Keys keys = {})
{
	level.commands.emplace_back(name, std::make_unique<smgetopt::RawLevel>());
	level.commands.back().second->keys = std::move(keys);
	return *level.commands.back().second;
}

inline void SetValid(smgetopt::RawEntry &entry, std::vector<std::string> values)
{
	entry.hasValidList = true;
	entry.validList = std::move(values);
}

inline void AddDefault(smgetopt::RawEntry &entry, const std::string &value)
{
	entry.defaults.push_back({smgetopt::RawDefault::Kind::Value, 0, "", value});
}

inline void AddDefaultPair(smgetopt::RawEntry &entry, const std::string &key, const std::string &value)
{
	entry.defaults.push_back({smgetopt::RawDefault::Kind::Pair, 0, key, value});
}

inline void AddDefaultField(smgetopt::RawEntry &entry, int index, const std::string &key, const std::string &value)
{
	entry.defaults.push_back({smgetopt::RawDefault::Kind::Field, index, key, value});
}

inline const smgetopt::TypeRegistry &Types()
{
	static smgetopt::TypeRegistry types([] { return kNow; });
	return types;
}

inline std::shared_ptr<const smgetopt::Spec> Compile(const smgetopt::RawSpec &raw)
{
	return smgetopt::Spec::Compile(raw, Types());
}

/* The message of the spec error raw has, empty when it has none. */
inline std::string SpecErrorOf(const smgetopt::RawSpec &raw, const smgetopt::TypeRegistry &types = Types())
{
	try {
		smgetopt::Spec::Compile(raw, types);
	} catch (const smgetopt::SpecError &error) {
		return error.what();
	}
	return "";
}

inline smgetopt::ParseResult ParseWith(const smgetopt::RawSpec &raw, const Words &words)
{
	return smgetopt::Parse(Compile(raw), words);
}

/* The error of a rejected parse, "(parsed)" or "(answered)" otherwise. */
inline std::string ErrorOf(const smgetopt::RawSpec &raw, const Words &words)
{
	smgetopt::ParseResult result = ParseWith(raw, words);
	if (result.outcome == smgetopt::Outcome::Parsed)
		return "(parsed)";
	if (result.outcome == smgetopt::Outcome::Answered)
		return "(answered)";
	return result.error;
}

inline std::string Joined(const std::vector<std::string> &lines)
{
	return smgetopt::text::Join(lines, "\n");
}

/* The single value of name as text, "(unset)" when there is none. */
inline std::string TextOf(const smgetopt::LevelResult &result, const std::string &name)
{
	const smgetopt::Scalar *scalar = result.At(name);
	return scalar ? scalar->text : "(unset)";
}

/* The values of a list as "a|b|c". */
inline std::string ListOf(const smgetopt::LevelResult &result, const std::string &name)
{
	std::vector<std::string> items;
	for (size_t entry = 0; entry < result.Count(name); entry++)
		items.push_back(result.At(name, static_cast<int>(entry))->text);
	return smgetopt::text::Join(items, "|");
}

inline std::string MappingText(const smgetopt::Mapping &mapping)
{
	std::vector<std::string> pairs;
	for (const auto &pair : mapping)
		pairs.push_back(pair.first + "=" + pair.second.text);
	return smgetopt::text::Join(pairs, ",");
}

/* The pairs of a mapping as "k=v,k2=v2", in the order of the keys. */
inline std::string MappingOf(const smgetopt::LevelResult &result, const std::string &name)
{
	return MappingText(result.MappingAt(name));
}

/* The entries of an objectlist as "k=v,k2=v2|k=v". */
inline std::string ObjectsOf(const smgetopt::LevelResult &result, const std::string &name)
{
	std::vector<std::string> entries;
	for (size_t entry = 0; entry < result.Count(name); entry++)
		entries.push_back(MappingText(result.MappingAt(name, static_cast<int>(entry))));
	return smgetopt::text::Join(entries, "|");
}

/* The message of the read error code throws, empty when it throws none. */
template <typename Code>
std::string ReadErrorOf(Code code)
{
	try {
		code();
	} catch (const smgetopt::ReadError &error) {
		return error.what();
	}
	return "";
}

/* Whether the result has a value of an option or arg named name. */
inline bool Holds(const smgetopt::LevelResult &result, const std::string &name)
{
	return ReadErrorOf([&] { result.ValueNamed(name); }).empty();
}

inline bool Contains(const std::string &text, const std::string &part)
{
	return text.find(part) != std::string::npos;
}

/* Whether lines has a line that is exactly line. */
inline bool HasLine(const std::vector<std::string> &lines, const std::string &line)
{
	for (const std::string &candidate : lines) {
		if (candidate == line)
			return true;
	}
	return false;
}

} // namespace support

#endif // GETOPT_TESTS_SUPPORT_H
