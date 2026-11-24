/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Tests of ownership: each owner's types, the shared types, and which
 * specs check themselves again when types come and go. */
#include "check.h"
#include "ownership.h"
#include "support.h"

#include <stdexcept>

using namespace smgetopt;

namespace {

struct PlainType : Type {
	TypeKind Kind() const override { return TypeKind::Value; }
};

TypeDefinition Definition(const std::string &name)
{
	return {{name}, {}, nullptr, [](const TypeKeys &) { return std::make_shared<PlainType>(); }};
}

/* Two owners; only compared, so any distinct addresses do. */
int g_first;
int g_second;
const OwnerKey kAlice = &g_first;
const OwnerKey kBob = &g_second;

/* A spec of owner that has compiled. */
std::shared_ptr<OwnedSpec> CompiledSpec(Ownership &ownership, OwnerKey owner)
{
	auto spec = std::make_shared<OwnedSpec>();
	spec->raw = support::NewSpec();
	spec->compiled = Spec::Compile(spec->raw, ownership.TypesOf(owner));
	ownership.TrackSpec(owner, spec);
	return spec;
}

std::string RegisterErrorOf(Ownership &ownership, OwnerKey owner, const std::string &name, bool shared)
{
	try {
		ownership.RegisterType(owner, Definition(name), shared);
	} catch (const std::invalid_argument &error) {
		return error.what();
	}
	return "";
}

} // namespace

TEST(ownership_own_types_come_before_shared_ones)
{
	Ownership ownership([] { return support::kNow; });
	ownership.RegisterType(kAlice, Definition("map"), true);
	ownership.RegisterType(kBob, Definition("map"), false);

	const TypeDefinition *shared = ownership.TypesOf(kAlice).Find("map");
	CHECK(shared != nullptr);
	CHECK(ownership.TypesOf(kBob).Find("map") != shared);
	CHECK(ownership.TypesOf(kBob).Find("string") != nullptr);

	/* A registry made before the shared type sees it as well. */
	Ownership early([] { return support::kNow; });
	CHECK(early.TypesOf(kBob).Find("map") == nullptr);
	early.RegisterType(kAlice, Definition("map"), true);
	CHECK(early.TypesOf(kBob).Find("map") != nullptr);
	CHECK(early.TypesOf(kBob).Find("map") == early.TypesOf(kAlice).Find("map"));
}

TEST(ownership_refuses_taken_names)
{
	Ownership ownership([] { return support::kNow; });
	ownership.RegisterType(kAlice, Definition("map"), true);
	CHECK_EQ(RegisterErrorOf(ownership, kBob, "map", true), "option type name 'map' is already registered");
	CHECK_EQ(RegisterErrorOf(ownership, kBob, "map", false), "");
	CHECK_EQ(RegisterErrorOf(ownership, kBob, "map", false), "option type name 'map' is already registered");
	CHECK_EQ(RegisterErrorOf(ownership, kAlice, "string", true), "option type name 'string' is already registered");
}

TEST(ownership_an_own_type_recompiles_its_owners_specs_only)
{
	Ownership ownership([] { return support::kNow; });
	std::shared_ptr<OwnedSpec> alices = CompiledSpec(ownership, kAlice);
	std::shared_ptr<OwnedSpec> bobs = CompiledSpec(ownership, kBob);

	ownership.RegisterType(kAlice, Definition("mine"), false);
	CHECK(alices->compiled == nullptr);
	CHECK(bobs->compiled != nullptr);

	/* A shared type takes no type away from a spec that compiled. */
	alices = CompiledSpec(ownership, kAlice);
	ownership.RegisterType(kAlice, Definition("ours"), true);
	CHECK(alices->compiled != nullptr);
	CHECK(bobs->compiled != nullptr);
}

TEST(ownership_a_sharing_owner_that_leaves_takes_its_types_along)
{
	Ownership ownership([] { return support::kNow; });
	ownership.RegisterType(kAlice, Definition("map"), true);
	ownership.RegisterType(kAlice, Definition("cvar"), false);
	ownership.RegisterType(kBob, Definition("key"), true);
	std::shared_ptr<OwnedSpec> bobs = CompiledSpec(ownership, kBob);
	CHECK(ownership.TypesOf(kBob).Find("map") != nullptr);

	ownership.Forget(kAlice);
	CHECK(ownership.TypesOf(kBob).Find("map") == nullptr);
	CHECK(ownership.TypesOf(kBob).Find("key") != nullptr);
	CHECK(bobs->compiled == nullptr);
	CHECK(ownership.TypesOf(kAlice).Find("cvar") == nullptr);

	/* The name is free again. */
	CHECK_EQ(RegisterErrorOf(ownership, kBob, "map", true), "");
}

TEST(ownership_an_owner_without_shared_types_leaves_the_rest_alone)
{
	Ownership ownership([] { return support::kNow; });
	ownership.RegisterType(kAlice, Definition("cvar"), false);
	std::shared_ptr<OwnedSpec> bobs = CompiledSpec(ownership, kBob);

	ownership.Forget(kAlice);
	ownership.Forget(kAlice);
	CHECK(bobs->compiled != nullptr);
	CHECK(ownership.TypesOf(kAlice).Find("cvar") == nullptr);

	/* A closed spec is no trouble. */
	bobs.reset();
	ownership.RegisterType(kBob, Definition("mine"), false);
}
