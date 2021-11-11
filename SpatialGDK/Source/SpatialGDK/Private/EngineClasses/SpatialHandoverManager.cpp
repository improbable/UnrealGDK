// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "EngineClasses/SpatialHandoverManager.h"

#include "EngineClasses/SpatialPackageMapClient.h"
#include "SpatialCommonTypes.h"
#include "SpatialConstants.h"
#include "SpatialView/EntityDelta.h"
#include "SpatialView/SpatialOSWorker.h"
#include "SpatialView/SubView.h"
#include "SpatialView/ViewDelta/ViewDelta.h"

namespace SpatialGDK
{
FSpatialHandoverManager::FSpatialHandoverManager(const FSubView& InActorView, const FSubView& InPartitionView)
	: ActorView(InActorView)
	, PartitionView(InPartitionView)
{
}

void FSpatialHandoverManager::Advance()
{
	PartitionsDelegated.Empty();
	DelegationLost.Empty();

	for (const EntityDelta& Delta : PartitionView.GetViewDelta().EntityDeltas)
	{
		switch (Delta.Type)
		{
		case EntityDelta::ADD:
			PartitionsToACK.Add(Delta.EntityId);
			OwnedPartitions.Add(Delta.EntityId);
			PartitionsDelegated.Add(Delta.EntityId);
			break;
		case EntityDelta::REMOVE:
			OwnedPartitions.Remove(Delta.EntityId);
			DelegationLost.Add(Delta.EntityId);
			break;
		case EntityDelta::TEMPORARILY_REMOVED:

			break;
		default:
			break;
		}
	}

	for (const EntityDelta& Delta : ActorView.GetViewDelta().EntityDeltas)
	{
		switch (Delta.Type)
		{
		case EntityDelta::UPDATE:
		{
			for (const ComponentChange& Change : Delta.ComponentUpdates)
			{
				ApplyComponentUpdate(Delta.EntityId, Change.ComponentId, Change.Update);
			}
			for (const ComponentChange& Change : Delta.ComponentsRefreshed)
			{
				ApplyComponentRefresh(Delta.EntityId, Change.ComponentId, Change.CompleteUpdate.Data);
			}
			break;
		}
		case EntityDelta::ADD:
			PopulateDataStore(Delta.EntityId);
			break;
		case EntityDelta::REMOVE:
			DataStore.Remove(Delta.EntityId);
			break;
		case EntityDelta::TEMPORARILY_REMOVED:
			DataStore.Remove(Delta.EntityId);
			PopulateDataStore(Delta.EntityId);
			break;
		default:
			break;
		}
	}
}

void FSpatialHandoverManager::PopulateDataStore(const Worker_EntityId EntityId)
{
	LBComponents2& Components = DataStore.Emplace(EntityId, LBComponents2{});
	for (const ComponentData& Data : ActorView.GetView()[EntityId].Components)
	{
		switch (Data.GetComponentId())
		{
		case SpatialConstants::AUTHORITY_INTENTV2_COMPONENT_ID:
			Components.Intent = AuthorityIntentV2(Data.GetUnderlying());
			break;
		case SpatialConstants::AUTHORITY_INTENT_ACK_COMPONENT_ID:
			Components.IntentACK = AuthorityIntentACK(Data.GetUnderlying());
			break;
		default:
			break;
		}
	}
	HandleChange(EntityId, Components);
}

void FSpatialHandoverManager::HandleChange(Worker_EntityId EntityId, const LBComponents2& Components)
{
	if (Components.Intent.AssignmentCounter != Components.IntentACK.AssignmentCounter)
	{
		if (OwnedPartitions.Contains(Components.Intent.PartitionId))
		{
			// TODO : Think about the need to maybe clear the handover array if there was a lock.
			// A locked actor means that it remains in the actor to handover array.
			ActorsToACK.Add(EntityId);
		}
		else
		{
			ActorsToHandover.Add(EntityId);
		}
	}
}

void FSpatialHandoverManager::ApplyComponentUpdate(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId,
												   Schema_ComponentUpdate* Update)
{
	LBComponents2& Components = DataStore[EntityId];
	switch (ComponentId)
	{
	case SpatialConstants::AUTHORITY_INTENTV2_COMPONENT_ID:
		Components.Intent.ApplyComponentUpdate(Update);
		break;
	default:
		break;
	}

	HandleChange(EntityId, Components);
}

void FSpatialHandoverManager::ApplyComponentRefresh(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId,
													Schema_ComponentData* Data)
{
	LBComponents2& Components = DataStore[EntityId];
	switch (ComponentId)
	{
	case SpatialConstants::AUTHORITY_INTENTV2_COMPONENT_ID:
		Components.Intent = AuthorityIntentV2(Data);
		break;
	default:
		break;
	}

	HandleChange(EntityId, Components);
}

void FSpatialHandoverManager::Flush(ISpatialOSWorker& Connection, const TSet<Worker_EntityId_Key>& ActorsReleased)
{
	for (auto Partition : PartitionsToACK)
	{
		ComponentUpdate Update(SpatialConstants::PARTITION_ACK_COMPONENT_ID);
		Schema_Object* ACKObj = Schema_GetComponentUpdateFields(Update.GetUnderlying());
		Schema_AddUint64(ACKObj, 1, 1);
		Connection.SendComponentUpdate(Partition, MoveTemp(Update));
	}
	PartitionsToACK.Empty();

	ActorsToHandover = ActorsToHandover.Difference(ActorsReleased);
	ActorsToACK.Append(ActorsReleased);

	for (auto ReleasedActor : ActorsToACK)
	{
		LBComponents2& Components = DataStore[ReleasedActor];
		Components.IntentACK.AssignmentCounter = Components.Intent.AssignmentCounter;

		OwningComponentUpdatePtr UpdateData(Components.IntentACK.CreateAuthorityIntentUpdate().schema_type);
		Connection.SendComponentUpdate(ReleasedActor,
									   ComponentUpdate(MoveTemp(UpdateData), SpatialConstants::AUTHORITY_INTENT_ACK_COMPONENT_ID));
	}
	ActorsToACK.Empty();
}

} // namespace SpatialGDK
