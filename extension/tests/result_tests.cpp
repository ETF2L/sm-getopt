/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Tests of reading a result: which address fits which shape, the checks of
 * entries and keys, and the numbers a value stands for, with every error
 * a reader gets. */
#include "check.h"
#include "support.h"

using namespace smgetopt;
using support::ReadErrorOf;

namespace {

Scalar TextScalar(const std::string &text)
{
	Scalar scalar;
	scalar.text = text;
	return scalar;
}

Value SingleValue(const std::string &text)
{
	Value value;
	value.single = TextScalar(text);
	return value;
}

/* A level with one value of every shape: "name" single, "unset" single
 * without a value, "tags" a list of a and b, "pairs" the mapping x=1,
 * y=2, "rows" a list of the mappings {a=1} and {b=2}. */
LevelResult EveryShape()
{
	Value tags;
	tags.shape = Shape::List;
	tags.list = {TextScalar("a"), TextScalar("b")};

	Value pairs;
	pairs.shape = Shape::Mapping;
	pairs.mapping = {{"x", TextScalar("1")}, {"y", TextScalar("2")}};

	Value rows;
	rows.shape = Shape::Objects;
	rows.objects = {{{"a", TextScalar("1")}}, {{"b", TextScalar("2")}}};

	std::map<std::string, Value> values = {
		{"name", SingleValue("cp_badlands")}, {"unset", Value()}, {"tags", tags}, {"pairs", pairs}, {"rows", rows}};
	return LevelResult(nullptr, std::move(values), std::nullopt, nullptr);
}

const char *const kSingle = "a single value, read with GetString, GetInt, GetFloat or GetBool";

} // namespace

TEST(result_addresses_fit_shapes)
{
	LevelResult result = EveryShape();
	CHECK_EQ(result.At("name")->text, "cp_badlands");
	CHECK(result.At("unset") == nullptr);
	CHECK_EQ(result.At("tags", 1)->text, "b");
	CHECK_EQ(result.At("pairs", kNoEntry, "y")->text, "2");
	CHECK(result.At("pairs", kNoEntry, "z") == nullptr);
	CHECK_EQ(result.At("rows", 1, "b")->text, "2");
	CHECK(result.At("rows", 0, "b") == nullptr);

	CHECK_EQ(ReadErrorOf([&] { result.At("name", 0); }), std::string("'name' is ") + kSingle);
	CHECK_EQ(ReadErrorOf([&] { result.At("tags"); }), "'tags' is a list, read with the ...At methods");
	CHECK_EQ(ReadErrorOf([&] { result.At("pairs", 0, "x"); }), "'pairs' is a mapping, read with the ...Of methods");
	CHECK_EQ(ReadErrorOf([&] { result.At("rows", kNoEntry, "a"); }), "'rows' is a list of mappings, read with the ...In methods");
	CHECK_EQ(ReadErrorOf([&] { result.At("missing"); }), "This level has no option or arg 'missing'");
}

TEST(result_entries_and_keys_are_checked)
{
	LevelResult result = EveryShape();
	CHECK_EQ(ReadErrorOf([&] { result.At("tags", 2); }), "'tags' has no entry 2 (it has 2)");
	CHECK_EQ(ReadErrorOf([&] { result.At("rows", 5, "a"); }), "'rows' has no entry 5 (it has 2)");

	CHECK_EQ(result.MappingAt("pairs").size(), 2u);
	CHECK_EQ(result.MappingAt("rows", 0).count("a"), 1u);
	CHECK_EQ(ReadErrorOf([&] { result.MappingAt("rows", 2); }), "'rows' has no entry 2 (it has 2)");
	CHECK_EQ(ReadErrorOf([&] { result.MappingAt("rows"); }), "'rows' is a list of mappings, read with the ...In methods");
	CHECK_EQ(ReadErrorOf([&] { result.MappingAt("name"); }), std::string("'name' is ") + kSingle);

	CHECK_EQ(result.KeyAt("pairs", kNoEntry, 1), "y");
	CHECK_EQ(result.KeyAt("rows", 1, 0), "b");
	CHECK_EQ(ReadErrorOf([&] { result.KeyAt("pairs", kNoEntry, 2); }), "'pairs' has no key 2 (it has 2)");
	CHECK_EQ(ReadErrorOf([&] { result.KeyAt("pairs", kNoEntry, -1); }), "'pairs' has no key -1 (it has 2)");
}

TEST(result_counts_every_shape)
{
	LevelResult result = EveryShape();
	CHECK_EQ(result.Count("name"), 1u);
	CHECK_EQ(result.Count("unset"), 0u);
	CHECK_EQ(result.Count("tags"), 2u);
	CHECK_EQ(result.Count("pairs"), 2u);
	CHECK_EQ(result.Count("rows"), 2u);
	CHECK(result.IsSet("name"));
	CHECK(!result.IsSet("unset"));
	CHECK(result.IsSet("rows"));

	LevelResult empty(nullptr, {{"tags", Value{Shape::List, std::nullopt, {}, {}, {}}}}, std::nullopt, nullptr);
	CHECK(!empty.IsSet("tags"));
	CHECK_EQ(ReadErrorOf([&] { empty.Count("other"); }), "This level has no option or arg 'other'");
}

TEST(result_whole_numbers)
{
	Scalar typed = TextScalar("five");
	typed.integer = 5;
	CHECK_EQ(*WholeNumberOf(typed, "n"), 5);
	CHECK_EQ(*WholeNumberOf(TextScalar("-12"), "n"), -12);
	CHECK(!WholeNumberOf(TextScalar("99999999999999999999"), "n"));

	/* A float default that is whole reads as an integer. */
	Scalar whole = TextScalar("3.0");
	whole.real = 3.0;
	CHECK_EQ(*WholeNumberOf(whole, "n"), 3);
	Scalar huge = TextScalar("1e19");
	huge.real = 1e19;
	CHECK(!WholeNumberOf(huge, "n"));

	Scalar fraction = TextScalar("2.5");
	fraction.real = 2.5;
	CHECK_EQ(ReadErrorOf([&] { WholeNumberOf(fraction, "n"); }), "The value '2.5' of 'n' is no whole number");
	CHECK_EQ(ReadErrorOf([&] { WholeNumberOf(TextScalar("abc"), "n"); }), "The value 'abc' of 'n' is no whole number");
}

TEST(result_floats)
{
	Scalar real = TextScalar("half");
	real.real = 0.5;
	CHECK_EQ(FloatOf(real, "f"), 0.5f);
	Scalar integer = TextScalar("seven");
	integer.integer = 7;
	CHECK_EQ(FloatOf(integer, "f"), 7.0f);
	CHECK_EQ(FloatOf(TextScalar("1.25"), "f"), 1.25f);
	CHECK_EQ(ReadErrorOf([&] { FloatOf(TextScalar("abc"), "f"); }), "The value 'abc' of 'f' is no number");
}

TEST(result_durations)
{
	Scalar length = TextScalar("2 days");
	length.duration = temporal::Duration{0, 2, 0};
	CHECK_EQ(DurationOf(length, "d").days, 2);
	CHECK_EQ(ReadErrorOf([&] { DurationOf(TextScalar("2"), "d"); }), "The value '2' of 'd' is no duration");
}
