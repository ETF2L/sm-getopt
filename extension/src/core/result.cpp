/* Copyright 2026 ETF2L Contributors, Dave */
#include "result.h"

#include "errors.h"
#include "text.h"

#include <math.h>

#include <iterator>
#include <utility>

namespace smgetopt {

using text::Format;

namespace {

const char *ShapeName(Shape shape)
{
	switch (shape) {
	case Shape::List:
		return "a list, read with the ...At methods";
	case Shape::Mapping:
		return "a mapping, read with the ...Of methods";
	case Shape::Objects:
		return "a list of mappings, read with the ...In methods";
	default:
		return "a single value, read with GetString, GetInt, GetFloat or GetBool";
	}
}

ReadError ShapeMismatch(const std::string &name, Shape shape)
{
	return ReadError(Format("'%s' is %s", name.c_str(), ShapeName(shape)));
}

/* The entry of a list or list of mappings a reader asks for. */
size_t CheckedIndex(const std::string &name, int entry, size_t count)
{
	if (entry < 0 || static_cast<size_t>(entry) >= count)
		throw ReadError(Format("'%s' has no entry %d (it has %zu)", name.c_str(), entry, count));
	return static_cast<size_t>(entry);
}

const Scalar *ScalarIn(const Mapping &mapping, const std::string &key)
{
	auto found = mapping.find(key);
	return found == mapping.end() ? nullptr : &found->second;
}

/* The shape an address is meant for. */
Shape AddressedShape(int entry, const std::string &key)
{
	if (entry >= 0)
		return key.empty() ? Shape::List : Shape::Objects;
	return key.empty() ? Shape::Single : Shape::Mapping;
}

/* Whole numbers this far from 0 do not fit 64 bits once they are a double. */
const double kWholeDoubleLimit = 9e18;

} // namespace

LevelResult::LevelResult(const Level *level, std::map<std::string, Value> values, std::optional<std::string> command,
                         std::shared_ptr<const LevelResult> subcommand)
	: level(level), command(std::move(command)), subcommand(std::move(subcommand)), m_values(std::move(values))
{
}

const Value &LevelResult::ValueNamed(const std::string &name) const
{
	auto found = m_values.find(name);
	if (found == m_values.end())
		throw ReadError(Format("This level has no option or arg '%s'", name.c_str()));
	return found->second;
}

bool LevelResult::IsSet(const std::string &name) const
{
	const Value &value = ValueNamed(name);
	return value.single || !value.list.empty() || !value.mapping.empty() || !value.objects.empty();
}

size_t LevelResult::Count(const std::string &name) const
{
	const Value &value = ValueNamed(name);
	switch (value.shape) {
	case Shape::List:
		return value.list.size();
	case Shape::Mapping:
		return value.mapping.size();
	case Shape::Objects:
		return value.objects.size();
	default:
		return value.single ? 1 : 0;
	}
}

const Scalar *LevelResult::At(const std::string &name, int entry, const std::string &key) const
{
	const Value &value = ValueNamed(name);
	if (AddressedShape(entry, key) != value.shape)
		throw ShapeMismatch(name, value.shape);

	switch (value.shape) {
	case Shape::List:
		return &value.list[CheckedIndex(name, entry, value.list.size())];
	case Shape::Mapping:
		return ScalarIn(value.mapping, key);
	case Shape::Objects:
		return ScalarIn(value.objects[CheckedIndex(name, entry, value.objects.size())], key);
	default:
		return value.single ? &*value.single : nullptr;
	}
}

const Mapping &LevelResult::MappingAt(const std::string &name, int entry) const
{
	const Value &value = ValueNamed(name);
	if (value.shape == Shape::Mapping && entry < 0)
		return value.mapping;
	if (value.shape == Shape::Objects && entry >= 0)
		return value.objects[CheckedIndex(name, entry, value.objects.size())];
	throw ShapeMismatch(name, value.shape);
}

const std::string &LevelResult::KeyAt(const std::string &name, int entry, int index) const
{
	const Mapping &mapping = MappingAt(name, entry);
	if (index < 0 || static_cast<size_t>(index) >= mapping.size())
		throw ReadError(Format("'%s' has no key %d (it has %zu)", name.c_str(), index, mapping.size()));
	return std::next(mapping.begin(), index)->first;
}

std::optional<int64_t> WholeNumberOf(const Scalar &scalar, const std::string &name)
{
	if (scalar.integer)
		return scalar.integer;
	if (text::IsInteger(scalar.text))
		return text::ToInt64(scalar.text);
	if (scalar.real && *scalar.real == floor(*scalar.real)) {
		if (fabs(*scalar.real) >= kWholeDoubleLimit)
			return std::nullopt;
		return static_cast<int64_t>(*scalar.real);
	}
	throw ReadError(Format("The value '%s' of '%s' is no whole number", scalar.text.c_str(), name.c_str()));
}

float FloatOf(const Scalar &scalar, const std::string &name)
{
	if (scalar.real)
		return static_cast<float>(*scalar.real);
	if (scalar.integer)
		return static_cast<float>(*scalar.integer);
	std::optional<double> number = text::ToDouble(scalar.text);
	if (!number)
		throw ReadError(Format("The value '%s' of '%s' is no number", scalar.text.c_str(), name.c_str()));
	return static_cast<float>(*number);
}

const temporal::Duration &DurationOf(const Scalar &scalar, const std::string &name)
{
	if (!scalar.duration)
		throw ReadError(Format("The value '%s' of '%s' is no duration", scalar.text.c_str(), name.c_str()));
	return *scalar.duration;
}

} // namespace smgetopt
