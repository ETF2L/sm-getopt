/* Copyright 2026 ETF2L Contributors, Dave
 *
 * What a parse hands out: single values, and the lists and mappings that
 * the multiple, hash and objectlist shapes collect them into. */
#ifndef GETOPT_CORE_VALUE_H
#define GETOPT_CORE_VALUE_H

#include "temporal.h"

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace smgetopt {

/* One checked value. The text is always there and is what a valid list is
 * compared with; a type adds the number it stands for where it has one,
 * and named attributes where a value means more than its text, such as the
 * players a target pattern matched. */
struct Scalar {
	std::string text;
	std::optional<int64_t> integer;
	std::optional<double> real;
	std::optional<temporal::Duration> duration;
	std::map<std::string, std::string> attributes;
};

using Mapping = std::map<std::string, Scalar>;

enum class Shape { Single, List, Mapping, Objects };

/* The value of one option or arg in its shape. Only the member of the
 * shape is used; a single value may be absent, the others are empty then. */
struct Value {
	Shape shape = Shape::Single;
	std::optional<Scalar> single;
	std::vector<Scalar> list;
	Mapping mapping;
	std::vector<Mapping> objects;
};

} // namespace smgetopt

#endif // GETOPT_CORE_VALUE_H
