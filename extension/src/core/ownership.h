/* Copyright 2026 ETF2L Contributors, Dave
 *
 * What each owner (a plugin) made that others depend on: its types, the
 * types it shares with every owner, and its specs, which must check
 * themselves again when the types they name may have changed.
 * An owner is an opaque key; whoever uses this module decides what it
 * stands for. */
#ifndef GETOPT_CORE_OWNERSHIP_H
#define GETOPT_CORE_OWNERSHIP_H

#include "spec.h"
#include "types.h"

#include <map>
#include <memory>
#include <vector>

namespace smgetopt {

/* Who made a type or a spec. Only compared, never followed. */
using OwnerKey = const void *;

/* A spec as its owner declares it, and what it compiled to since its last
 * change or since the types it names last changed. */
struct OwnedSpec {
	RawSpec raw;
	std::shared_ptr<const Spec> compiled;
};

class Ownership {
public:
	/* clock is what the date and duration types of every registry count
	 * from. */
	explicit Ownership(Clock clock = SystemClock);

	Ownership(const Ownership &) = delete;
	Ownership &operator=(const Ownership &) = delete;

	/* The types the owner's specs can name: its own, then the shared ones.
	 * Lives until the owner is forgotten. */
	const TypeRegistry &TypesOf(OwnerKey owner);

	/* Registers a type of the owner: for its own specs, or shared with
	 * every owner's. An own type comes before a shared one of its name, so
	 * the owner's specs check themselves again. A new shared name takes no
	 * type away from a spec that compiled, so no spec has to. Throws
	 * std::invalid_argument for a name that is taken where the type goes:
	 * among the owner's own types, or among the shared ones. */
	void RegisterType(OwnerKey owner, TypeDefinition definition, bool shared);

	/* Keeps track of a spec of the owner for as long as it lives. */
	void TrackSpec(OwnerKey owner, std::weak_ptr<OwnedSpec> spec);

	/* Lets go of everything of the owner. When it shared types, they leave
	 * every owner's registry and every spec checks itself again. Does
	 * nothing for an owner it knows nothing of. */
	void Forget(OwnerKey owner);

private:
	struct SharedDefinition {
		OwnerKey owner;
		TypeDefinition definition;
	};

	struct TrackedSpec {
		OwnerKey owner;
		std::weak_ptr<OwnedSpec> spec;
	};

	TypeRegistry &RegistryOf(OwnerKey owner);

	/* Builds the shared registry from the shared types left and points
	 * every owner's registry at it. */
	void RebuildSharedTypes();

	/* Every spec, or those of one owner, checks itself again on its next
	 * use. */
	void ForgetCompiledSpecs();
	void ForgetCompiledSpecsOf(OwnerKey owner);

	/* Drops the specs that are gone, so the list does not grow with every
	 * spec an owner creates and deletes. */
	void PruneClosedSpecs();

	Clock m_clock;
	std::unique_ptr<TypeRegistry> m_sharedTypes;
	std::vector<SharedDefinition> m_sharedDefinitions;
	std::map<OwnerKey, std::unique_ptr<TypeRegistry>> m_types;
	std::vector<TrackedSpec> m_specs;
};

} // namespace smgetopt

#endif // GETOPT_CORE_OWNERSHIP_H
