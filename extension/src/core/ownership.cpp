/* Copyright 2026 ETF2L Contributors, Dave */
#include "ownership.h"

#include <algorithm>
#include <utility>

namespace smgetopt {

Ownership::Ownership(Clock clock) : m_clock(std::move(clock)), m_sharedTypes(std::make_unique<TypeRegistry>(m_clock))
{
}

const TypeRegistry &Ownership::TypesOf(OwnerKey owner)
{
	return RegistryOf(owner);
}

TypeRegistry &Ownership::RegistryOf(OwnerKey owner)
{
	std::unique_ptr<TypeRegistry> &types = m_types[owner];
	if (!types) {
		types = std::make_unique<TypeRegistry>(m_clock);
		types->SetParent(m_sharedTypes.get());
	}
	return *types;
}

void Ownership::RegisterType(OwnerKey owner, TypeDefinition definition, bool shared)
{
	if (!shared) {
		RegistryOf(owner).Register(std::move(definition));
		ForgetCompiledSpecsOf(owner);
		return;
	}

	m_sharedTypes->Register(definition);
	m_sharedDefinitions.push_back({owner, std::move(definition)});
}

void Ownership::TrackSpec(OwnerKey owner, std::weak_ptr<OwnedSpec> spec)
{
	PruneClosedSpecs();
	m_specs.push_back({owner, std::move(spec)});
}

void Ownership::Forget(OwnerKey owner)
{
	m_types.erase(owner);

	auto ofOwner = [owner](const TrackedSpec &tracked) { return tracked.owner == owner; };
	m_specs.erase(std::remove_if(m_specs.begin(), m_specs.end(), ofOwner), m_specs.end());

	auto sharedByOwner = [owner](const SharedDefinition &shared) { return shared.owner == owner; };
	auto firstShared = std::remove_if(m_sharedDefinitions.begin(), m_sharedDefinitions.end(), sharedByOwner);
	if (firstShared == m_sharedDefinitions.end())
		return;

	m_sharedDefinitions.erase(firstShared, m_sharedDefinitions.end());
	RebuildSharedTypes();
	ForgetCompiledSpecs();
}

void Ownership::RebuildSharedTypes()
{
	m_sharedTypes = std::make_unique<TypeRegistry>(m_clock);
	for (const SharedDefinition &shared : m_sharedDefinitions)
		m_sharedTypes->Register(shared.definition);
	for (auto &types : m_types)
		types.second->SetParent(m_sharedTypes.get());
}

void Ownership::ForgetCompiledSpecs()
{
	PruneClosedSpecs();
	for (const TrackedSpec &tracked : m_specs)
		tracked.spec.lock()->compiled.reset();
}

void Ownership::ForgetCompiledSpecsOf(OwnerKey owner)
{
	PruneClosedSpecs();
	for (const TrackedSpec &tracked : m_specs) {
		if (tracked.owner == owner)
			tracked.spec.lock()->compiled.reset();
	}
}

void Ownership::PruneClosedSpecs()
{
	auto closed = [](const TrackedSpec &tracked) { return tracked.spec.expired(); };
	m_specs.erase(std::remove_if(m_specs.begin(), m_specs.end(), closed), m_specs.end());
}

} // namespace smgetopt
