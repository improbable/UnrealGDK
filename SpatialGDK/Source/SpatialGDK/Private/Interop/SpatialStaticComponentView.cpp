// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Interop/SpatialStaticComponentView.h"

#include "Schema/AuthorityIntent.h"
#include "Schema/Component.h"
#include "Schema/Interest.h"
#include "Schema/NetOwningClientWorker.h"
#include "Schema/Restricted.h"
#include "Schema/SpatialDebugging.h"
#include "Schema/SpawnData.h"
#include "Schema/StandardLibrary.h"
#include "Schema/UnrealMetadata.h"

Worker_Authority USpatialStaticComponentView::GetAuthority(Worker_EntityId EntityId, Worker_ComponentId ComponentId) const
{
	if (const TMap<Worker_ComponentId, Worker_Authority>* ComponentAuthorityMap = EntityComponentAuthorityMap.Find(EntityId))
	{
		if (const Worker_Authority* Authority = ComponentAuthorityMap->Find(ComponentId))
		{
			return *Authority;
		}
	}

	return WORKER_AUTHORITY_NOT_AUTHORITATIVE;
}

bool USpatialStaticComponentView::HasAuthority(Worker_EntityId EntityId, Worker_ComponentId ComponentId) const
{
	return GetAuthority(EntityId, ComponentId) == WORKER_AUTHORITY_AUTHORITATIVE;
}

bool USpatialStaticComponentView::HasEntity(Worker_EntityId EntityId) const
{
	return EntityComponentMap.Find(EntityId) != nullptr;
}

bool USpatialStaticComponentView::HasComponent(Worker_EntityId EntityId, Worker_ComponentId ComponentId) const
{
	if (auto* EntityComponentStorage = EntityComponentMap.Find(EntityId))
	{
		return EntityComponentStorage->Contains(ComponentId);
	}

	return false;
}

void USpatialStaticComponentView::OnAddComponent(const Worker_AddComponentOp& Op)
{
	TUniquePtr<SpatialGDK::Component> Data;
	switch (Op.data.component_id)
	{
	case SpatialConstants::METADATA_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::Metadata>(Op.data);
		break;
	case SpatialConstants::POSITION_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::Position>(Op.data);
		break;
	case SpatialConstants::PERSISTENCE_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::Persistence>(Op.data);
		break;
	case SpatialConstants::WORKER_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::Worker>(Op.data);
		break;
	case SpatialConstants::SPAWN_DATA_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::SpawnData>(Op.data);
		break;
	case SpatialConstants::UNREAL_METADATA_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::UnrealMetadata>(Op.data);
		break;
	case SpatialConstants::INTEREST_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::Interest>(Op.data);
		break;
	case SpatialConstants::AUTHORITY_INTENT_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::AuthorityIntent>(Op.data);
		break;
	case SpatialConstants::SPATIAL_DEBUGGING_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::SpatialDebugging>(Op.data);
		break;
	case SpatialConstants::NET_OWNING_CLIENT_WORKER_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::NetOwningClientWorker>(Op.data);
		break;
	case SpatialConstants::AUTHORITY_DELEGATION_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::AuthorityDelegation>(Op.data);
		break;
	case SpatialConstants::PARTITION_COMPONENT_ID:
		Data = MakeUnique<SpatialGDK::Partition>(Op.data);
		break;
	default:
		// Component is not hand written, but we still want to know the existence of it on this entity.
		Data = nullptr;
	}
	EntityComponentMap.FindOrAdd(Op.entity_id).FindOrAdd(Op.data.component_id) = MoveTemp(Data);
}

void USpatialStaticComponentView::OnRemoveComponent(const Worker_RemoveComponentOp& Op)
{
	if (auto* ComponentMap = EntityComponentMap.Find(Op.entity_id))
	{
		ComponentMap->Remove(Op.component_id);
	}
}

void USpatialStaticComponentView::OnRemoveEntity(Worker_EntityId EntityId)
{
	EntityComponentMap.Remove(EntityId);
	EntityComponentAuthorityMap.Remove(EntityId);
}

void USpatialStaticComponentView::OnComponentUpdate(const Worker_ComponentUpdateOp& Op)
{
	SpatialGDK::Component* Component = nullptr;

	switch (Op.update.component_id)
	{
	case SpatialConstants::POSITION_COMPONENT_ID:
		Component = GetComponentData<SpatialGDK::Position>(Op.entity_id);
		break;
	case SpatialConstants::AUTHORITY_INTENT_COMPONENT_ID:
		Component = GetComponentData<SpatialGDK::AuthorityIntent>(Op.entity_id);
		break;
	case SpatialConstants::SPATIAL_DEBUGGING_COMPONENT_ID:
		Component = GetComponentData<SpatialGDK::SpatialDebugging>(Op.entity_id);
		break;
	case SpatialConstants::NET_OWNING_CLIENT_WORKER_COMPONENT_ID:
		Component = GetComponentData<SpatialGDK::NetOwningClientWorker>(Op.entity_id);
		break;
	case SpatialConstants::AUTHORITY_DELEGATION_COMPONENT_ID:
		Component = GetComponentData<SpatialGDK::AuthorityDelegation>(Op.entity_id);
		break;
	case SpatialConstants::PARTITION_COMPONENT_ID:
		Component = GetComponentData<SpatialGDK::Partition>(Op.entity_id);
		break;
	default:
		return;
	}

	if (Component)
	{
		Component->ApplyComponentUpdate(Op.update);
	}
}

void USpatialStaticComponentView::OnAuthorityChange(const Worker_ComponentSetAuthorityChangeOp& Op)
{
	EntityComponentAuthorityMap.FindOrAdd(Op.entity_id).FindOrAdd(Op.component_set_id) = (Worker_Authority)Op.authority;
}
