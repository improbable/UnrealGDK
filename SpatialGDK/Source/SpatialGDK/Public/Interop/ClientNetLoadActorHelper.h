// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once
#include "Schema/UnrealMetadata.h"
#include "SpatialConstants.h"
#include "Utils/RepDataUtils.h"

DECLARE_LOG_CATEGORY_EXTERN(LogClientNetLoadActorHelper, Log, All);

class USpatialNetDriver;

namespace SpatialGDK
{
class FClientNetLoadActorHelper
{
public:
	FClientNetLoadActorHelper(USpatialNetDriver& InNetDriver);
	void EntityRemoved(const Worker_EntityId EntityId, const AActor& Actor);
	UObject* GetReusableDynamicSubObject(const FUnrealObjectRef ObjectRef);

	// The runtime can remove components from a ClientNetLoad Actor while the actor is out of the client's interest
	// The client doesn't receive these updates when they happen, so the difference must be reconciled
	void RemoveRuntimeRemovedComponents(const Worker_EntityId EntityId, const TArray<ComponentData>& NewComponents);

private:
	USpatialNetDriver* NetDriver;

	// Stores subobjects from client net load actors that have gone out of the client's interest
	TMap<Worker_EntityId_Key, TMap<ObjectOffset, FNetworkGUID>> SpatialEntityRemovedSubobjectMetadata;

	// BNetLoadOnClient component edge case handling
	FNetworkGUID* GetSavedDynamicSubObjectNetGUID(const FUnrealObjectRef& ObjectRef);
	void SaveDynamicSubobjectMetadata(const FUnrealObjectRef& ObjectRef, const FNetworkGUID& NetGUID);
	void ClearDynamicSubobjectMetadata(const Worker_EntityId InEntityId);

	bool OffsetContainedInComponentArray(const TArray<ComponentData>& Components, const ObjectOffset OffsetToCheckIfContained) const;
};

} // namespace SpatialGDK
