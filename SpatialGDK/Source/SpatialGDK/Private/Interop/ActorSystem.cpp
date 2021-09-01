// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Interop/ActorSystem.h"

#include "Algo/AnyOf.h"
#include "EngineClasses/SpatialFastArrayNetSerialize.h"
#include "EngineClasses/SpatialNetConnection.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialNetDriverAuthorityDebugger.h"
#include "GameFramework/PlayerState.h"
#include "Interop/InitialOnlyFilter.h"
#include "Interop/SpatialReceiver.h"
#include "Interop/SpatialSender.h"
#include "Schema/Restricted.h"
#include "Schema/Tombstone.h"
#include "SpatialConstants.h"
#include "SpatialView/EntityDelta.h"
#include "SpatialView/SubView.h"
#include "Utils/ComponentFactory.h"
#include "Utils/ComponentReader.h"
#include "Utils/EntityFactory.h"
#include "Utils/InterestFactory.h"
#include "Utils/RepLayoutUtils.h"
#include "Utils/SpatialActorUtils.h"

#include "ReplicationGraph.h"

DEFINE_LOG_CATEGORY(LogActorSystem);

DECLARE_CYCLE_STAT(TEXT("Actor System SendComponentUpdates"), STAT_ActorSystemSendComponentUpdates, STATGROUP_SpatialNet);
DECLARE_CYCLE_STAT(TEXT("Actor System UpdateInterestComponent"), STAT_ActorSystemUpdateInterestComponent, STATGROUP_SpatialNet);
DECLARE_CYCLE_STAT(TEXT("Actor System RemoveEntity"), STAT_ActorSystemRemoveEntity, STATGROUP_SpatialNet);
DECLARE_CYCLE_STAT(TEXT("Actor System ApplyData"), STAT_ActorSystemApplyData, STATGROUP_SpatialNet);
DECLARE_CYCLE_STAT(TEXT("Actor System ReceiveActor"), STAT_ActorSystemReceiveActor, STATGROUP_SpatialNet);
DECLARE_CYCLE_STAT(TEXT("Actor System RemoveActor"), STAT_ActorSystemRemoveActor, STATGROUP_SpatialNet);

namespace
{
struct FChangeListPropertyIterator
{
	const FRepChangeState* Changes;
	FChangelistIterator ChangeListIterator;
	FRepHandleIterator HandleIterator;
	bool bValid;
	FChangeListPropertyIterator(const FRepChangeState* Changes)
		: Changes(Changes)
		, ChangeListIterator(Changes->RepChanged, 0)
		, HandleIterator(static_cast<UStruct*>(Changes->RepLayout.GetOwner()), ChangeListIterator, Changes->RepLayout.Cmds,
						 Changes->RepLayout.BaseHandleToCmdIndex, /* InMaxArrayIndex */ 0, /* InMinCmdIndex */ 1, 0,
						 /* InMaxCmdIndex */ Changes->RepLayout.Cmds.Num() - 1)
		, bValid(HandleIterator.NextHandle())
	{
	}

	GDK_PROPERTY(Property) * operator*() const
	{
		if (bValid)
		{
			const FRepLayoutCmd& Cmd = Changes->RepLayout.Cmds[HandleIterator.CmdIndex];
			return Cmd.Property;
		}
		return nullptr;
	}

	operator bool() const { return bValid; }

	FChangeListPropertyIterator& operator++()
	{
		// Move forward
		if (bValid && Changes->RepLayout.Cmds[HandleIterator.CmdIndex].Type == ERepLayoutCmdType::DynamicArray)
		{
			bValid = !HandleIterator.JumpOverArray();
		}
		if (bValid)
		{
			bValid = HandleIterator.NextHandle();
		}
		return *this;
	}
};
} // namespace

namespace SpatialGDK
{
struct ActorSystem::RepStateUpdateHelper
{
	RepStateUpdateHelper(USpatialActorChannel& Channel, UObject& TargetObject)
		: ObjectPtr(MakeWeakObjectPtr(&TargetObject))
		, ObjectRepState(Channel.ObjectReferenceMap.Find(ObjectPtr))
	{
	}

	~RepStateUpdateHelper() { check(bUpdatePerfomed); }

	FObjectReferencesMap& GetRefMap()
	{
		if (ObjectRepState)
		{
			return ObjectRepState->ReferenceMap;
		}
		return TempRefMap;
	}

	void Update(ActorSystem& Actors, USpatialActorChannel& Channel, const bool bReferencesChanged)
	{
		check(!bUpdatePerfomed);

		if (bReferencesChanged)
		{
			if (ObjectRepState == nullptr && TempRefMap.Num() > 0)
			{
				ObjectRepState =
					&Channel.ObjectReferenceMap.Add(ObjectPtr, FSpatialObjectRepState(FChannelObjectPair(&Channel, ObjectPtr)));
				ObjectRepState->ReferenceMap = MoveTemp(TempRefMap);
			}

			if (ObjectRepState)
			{
				ObjectRepState->UpdateRefToRepStateMap(Actors.ObjectRefToRepStateMap);

				if (ObjectRepState->ReferencedObj.Num() == 0)
				{
					Channel.ObjectReferenceMap.Remove(ObjectPtr);
				}
			}
		}
#if DO_CHECK
		bUpdatePerfomed = true;
#endif
	}

private:
	FObjectReferencesMap TempRefMap;
	TWeakObjectPtr<UObject> ObjectPtr;
	FSpatialObjectRepState* ObjectRepState;
#if DO_CHECK
	bool bUpdatePerfomed = false;
#endif
};

struct ActorSystem::FEntitySubViewUpdate
{
	const TArray<EntityDelta>& EntityDeltas;
	ENetRole SubViewType;
};

void ActorSystem::ProcessUpdates(const FEntitySubViewUpdate& SubViewUpdate)
{
	for (const EntityDelta& Delta : SubViewUpdate.EntityDeltas)
	{
		if (Delta.Type == EntityDelta::UPDATE)
		{
			TArray<FWeakObjectPtr> ToResolveOps;
			for (const ComponentChange& Change : Delta.ComponentsAdded)
			{
				ApplyComponentAdd(Delta.EntityId, Change.ComponentId, Change.Data);
				ComponentAdded(Delta.EntityId, Change.ComponentId, Change.Data, ToResolveOps);
			}
			for (const ComponentChange& Change : Delta.ComponentsRemoved)
			{
				// NOTE: We have to deal the with dormant before ComponentUpdated
				// otherwise a SubViewUpdate that contains a component update that
				// also removes the dormant component will have its updates ignored.
				// Simon Sarginson 05-08-2021 UNR-5863 TODO: Address this properly
				// by making dormancy not a dynamic component but a fixed field.
				if (Change.ComponentId == SpatialConstants::DORMANT_COMPONENT_ID)
				{
					ComponentRemoved(Delta.EntityId, Change.ComponentId);
				}
			}
			for (const ComponentChange& Change : Delta.ComponentUpdates)
			{
				ComponentUpdated(Delta.EntityId, Change.ComponentId, Change.Update);
			}
			for (const ComponentChange& Change : Delta.ComponentsRefreshed)
			{
				ApplyComponentAdd(Delta.EntityId, Change.ComponentId, Change.CompleteUpdate.Data);
				ComponentAdded(Delta.EntityId, Change.ComponentId, Change.CompleteUpdate.Data, ToResolveOps);
			}
			for (const ComponentChange& Change : Delta.ComponentsRemoved)
			{
				// NOTE: We have to deal the with dormant before ComponentUpdated
				// otherwise a SubViewUpdate that contains a component update that
				// also removes the dormant component will have its updates ignored.
				if (Change.ComponentId != SpatialConstants::DORMANT_COMPONENT_ID)
				{
					ComponentRemoved(Delta.EntityId, Change.ComponentId);
				}
			}

			InvokePostNetReceives();
			ResolvePendingOpsFromEntityUpdate(ToResolveOps);
		}
	}
}

void ActorSystem::ProcessAdds(const FEntitySubViewUpdate& SubViewUpdate)
{
	for (const EntityDelta& Delta : SubViewUpdate.EntityDeltas)
	{
		if (Delta.Type == EntityDelta::ADD || Delta.Type == EntityDelta::TEMPORARILY_REMOVED)
		{
			const Worker_EntityId EntityId = Delta.EntityId;

			// Check if this entity is EntitiesToRetireOnAuthorityGain first,
			// to avoid creating an actor that might've been deleted before.
			if (SubViewUpdate.SubViewType == ENetRole::ROLE_Authority && HasEntityBeenRequestedForDelete(EntityId))
			{
				HandleEntityDeletedAuthority(EntityId);
				continue;
			}

			if (!PresentEntities.Contains(EntityId))
			{
				// Create new actor for the entity.
				EntityAdded(EntityId);

				PresentEntities.Emplace(EntityId);
			}
			else
			{
				RefreshEntity(EntityId);
			}
		}
	}
}

void ActorSystem::ProcessAuthorityGains(const FEntitySubViewUpdate& SubViewUpdate)
{
	for (const EntityDelta& Delta : SubViewUpdate.EntityDeltas)
	{
		if ((Delta.Type == EntityDelta::ADD || Delta.Type == EntityDelta::TEMPORARILY_REMOVED)
			&& SubViewUpdate.SubViewType != ENetRole::ROLE_SimulatedProxy)
		{
			const Worker_EntityId EntityId = Delta.EntityId;

			// Check if this entity is EntitiesToRetireOnAuthorityGain first,
			// to avoid authority gain on an actor that might've been deleted during a RepNotify.
			if (SubViewUpdate.SubViewType == ENetRole::ROLE_Authority && HasEntityBeenRequestedForDelete(EntityId))
			{
				HandleEntityDeletedAuthority(EntityId);
				continue;
			}

			const Worker_ComponentSetId AuthorityComponentSet = SubViewUpdate.SubViewType == ENetRole::ROLE_Authority
																	? SpatialConstants::SERVER_AUTH_COMPONENT_SET_ID
																	: SpatialConstants::CLIENT_AUTH_COMPONENT_SET_ID;
			AuthorityGained(EntityId, AuthorityComponentSet);
		}
	}
}

void ActorSystem::ProcessRemoves(const FEntitySubViewUpdate& SubViewUpdate)
{
	if (SubViewUpdate.SubViewType == ENetRole::ROLE_SimulatedProxy)
	{
		return;
	}

	for (const EntityDelta& Delta : SubViewUpdate.EntityDeltas)
	{
		if (Delta.Type == EntityDelta::REMOVE || Delta.Type == EntityDelta::TEMPORARILY_REMOVED)
		{
			const Worker_EntityId EntityId = Delta.EntityId;
			if (PresentEntities.Contains(EntityId))
			{
				const Worker_ComponentSetId AuthorityComponentSet = SubViewUpdate.SubViewType == ENetRole::ROLE_Authority
																		? SpatialConstants::SERVER_AUTH_COMPONENT_SET_ID
																		: SpatialConstants::CLIENT_AUTH_COMPONENT_SET_ID;
				AuthorityLost(EntityId, AuthorityComponentSet);
			}
		}
	}
}

ActorSystem::ActorSystem(const FSubView& InActorSubView, const FSubView& InAuthoritySubView, const FSubView& InOwnershipSubView,
						 const FSubView& InSimulatedSubView, const FSubView& InTombstoneSubView, USpatialNetDriver* InNetDriver,
						 SpatialEventTracer* InEventTracer)
	: ActorSubView(&InActorSubView)
	, AuthoritySubView(&InAuthoritySubView)
	, OwnershipSubView(&InOwnershipSubView)
	, SimulatedSubView(&InSimulatedSubView)
	, TombstoneSubView(&InTombstoneSubView)
	, NetDriver(InNetDriver)
	, EventTracer(InEventTracer)
	, ClientNetLoadActorHelper(*InNetDriver)
{
}

#if DO_CHECK
static void ValidateNoSubviewIntersections(const FSubView& Lhs, const FSubView& Rhs, const FString& SubviewDescription)
{
	TSet<Worker_EntityId_Key> LhsEntities, RhsEntities;
	Algo::Copy(Lhs.GetCompleteEntities(), LhsEntities);
	Algo::Copy(Rhs.GetCompleteEntities(), RhsEntities);
	for (const Worker_EntityId Overlapping : LhsEntities.Intersect(RhsEntities))
	{
		UE_LOG(LogActorSystem, Warning, TEXT("Entity %lld is doubly complete on %s"), Overlapping, *SubviewDescription);
	}
}
#endif // DO_CHECK

void ActorSystem::Advance()
{
	for (const EntityDelta& Delta : ActorSubView->GetViewDelta().EntityDeltas)
	{
		if (Delta.Type == EntityDelta::REMOVE)
		{
			EntityRemoved(Delta.EntityId);

			const int32 EntitiesRemoved = PresentEntities.Remove(Delta.EntityId);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (NetDriver->AuthorityDebugger != nullptr)
			{
				NetDriver->AuthorityDebugger->RemoveSpatialShadowActor(Delta.EntityId);
			}
#endif
		}
	}

	struct FEntitySubView
	{
		const FSubView* SubView;
		ENetRole Type;

		operator FEntitySubViewUpdate() const { return { SubView->GetViewDelta().EntityDeltas, Type }; }
	};

#if DO_CHECK
	{
		ValidateNoSubviewIntersections(*AuthoritySubView, *OwnershipSubView, TEXT("Authority and Ownership"));
		ValidateNoSubviewIntersections(*AuthoritySubView, *SimulatedSubView, TEXT("Authority and Simulated"));
		ValidateNoSubviewIntersections(*SimulatedSubView, *OwnershipSubView, TEXT("Simulated and Ownership"));
	}
#endif // DO_CHECK

	const FEntitySubView SubViews[]{
		{ AuthoritySubView, ENetRole::ROLE_Authority },
		{ OwnershipSubView, ENetRole::ROLE_AutonomousProxy },
		{ SimulatedSubView, ENetRole::ROLE_SimulatedProxy },
	};
	const FEntitySubView& EntityAuthSubView = SubViews[0];
	const FEntitySubView& EntityOwnershipSubView = SubViews[1];

	// First, we process updates; when receiving tear off updates, we want to
	// process them before a REMOVE if we receive it in the same ViewDelta.
	for (const FEntitySubView& SubView : SubViews)
	{
		ProcessUpdates(SubView);
	}

	for (const FEntitySubView& SubView : SubViews)
	{
		ProcessRemoves(SubView);
	}

	for (const FEntitySubView& SubView : SubViews)
	{
		ProcessAdds(SubView);
	}

	// Order here matters: Rep Notifies should be called before authority gains.
	// This is because it wouldn't make sense to be told a property was updated while you are authoritative over that actor, as while
	// authoritative you are supposed to be the only server that can update properties.
	InvokeRepNotifies();

	// And finally send clean up for channels that we got a tear off update for.
	// We don't do this earlier as Rep Notifies require the actor channel.
	CleanUpTornOffChannels();

	// No need to ProcessAuthorityGains on SimulatedSubView as we won't have gained authority on Entities in that SubView.
	ProcessAuthorityGains(EntityAuthSubView);
	ProcessAuthorityGains(EntityOwnershipSubView);

	ProcessClientInterestUpdates();

	for (const EntityDelta& Delta : TombstoneSubView->GetViewDelta().EntityDeltas)
	{
		if (Delta.Type == EntityDelta::ADD || Delta.Type == EntityDelta::TEMPORARILY_REMOVED)
		{
			AActor* EntityActor = TryGetActor(
				UnrealMetadata(TombstoneSubView->GetView()[Delta.EntityId]
								   .Components.FindByPredicate(ComponentIdEquality{ SpatialConstants::UNREAL_METADATA_COMPONENT_ID })
								   ->GetUnderlying()));
			if (EntityActor == nullptr)
			{
				continue;
			}
			UE_LOG(LogActorSystem, Verbose, TEXT("The received actor with entity ID %lld was tombstoned. The actor will be deleted."),
				   Delta.EntityId);
			// We must first Resolve the EntityId to the Actor in order for RemoveActor to succeed.
			NetDriver->PackageMap->ResolveEntityActorAndSubobjects(Delta.EntityId, EntityActor);
			RemoveActor(Delta.EntityId);
		}
	}

	CommandsHandler.ProcessOps(*ActorSubView->GetViewDelta().WorkerMessages);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (const EntityDelta& Delta : ActorSubView->GetViewDelta().EntityDeltas)
	{
		if (Delta.Type == EntityDelta::REMOVE) {}
		else if (Delta.Type == EntityDelta::ADD)
		{
			if (NetDriver->AuthorityDebugger != nullptr)
			{
				NetDriver->AuthorityDebugger->AddSpatialShadowActor(Delta.EntityId);
			}
		}
		else if (Delta.Type == EntityDelta::UPDATE)
		{
			if (NetDriver->AuthorityDebugger != nullptr)
			{
				NetDriver->AuthorityDebugger->UpdateSpatialShadowActor(Delta.EntityId);
			}
		}
	}
#endif
}

UnrealMetadata* ActorSystem::GetUnrealMetadata(const Worker_EntityId EntityId)
{
	if (ActorDataStore.Contains(EntityId))
	{
		return &ActorDataStore[EntityId].Metadata;
	}
	return nullptr;
}

void ActorSystem::PopulateDataStore(const Worker_EntityId EntityId)
{
	ActorData& Components = ActorDataStore.Emplace(EntityId, ActorData{});
	for (const ComponentData& Data : ActorSubView->GetView()[EntityId].Components)
	{
		switch (Data.GetComponentId())
		{
		case SpatialConstants::SPAWN_DATA_COMPONENT_ID:
			Components.Spawn = SpawnData(Data.GetUnderlying());
			break;
		case SpatialConstants::UNREAL_METADATA_COMPONENT_ID:
			Components.Metadata = UnrealMetadata(Data.GetUnderlying());
			break;
		default:
			break;
		}
	}
}

void ActorSystem::ApplyComponentAdd(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId, Schema_ComponentData* Data)
{
	switch (ComponentId)
	{
	case SpatialConstants::SPAWN_DATA_COMPONENT_ID:
		ActorDataStore[EntityId].Spawn = SpawnData(Data);
		break;
	case SpatialConstants::UNREAL_METADATA_COMPONENT_ID:
		ActorDataStore[EntityId].Metadata = UnrealMetadata(Data);
		break;
	default:
		break;
	}
}

void ActorSystem::AuthorityLost(const Worker_EntityId EntityId, const Worker_ComponentSetId ComponentSetId)
{
	if (ComponentSetId != SpatialConstants::SERVER_AUTH_COMPONENT_SET_ID
		&& ComponentSetId != SpatialConstants::CLIENT_AUTH_COMPONENT_SET_ID)
	{
		return;
	}

	HandleActorAuthority(EntityId, ComponentSetId, WORKER_AUTHORITY_NOT_AUTHORITATIVE);
}

void ActorSystem::AuthorityGained(Worker_EntityId EntityId, Worker_ComponentSetId ComponentSetId)
{
	if (ComponentSetId != SpatialConstants::SERVER_AUTH_COMPONENT_SET_ID
		&& ComponentSetId != SpatialConstants::CLIENT_AUTH_COMPONENT_SET_ID)
	{
		return;
	}

	HandleActorAuthority(EntityId, ComponentSetId, WORKER_AUTHORITY_AUTHORITATIVE);
}

void ActorSystem::HandleActorAuthority(const Worker_EntityId EntityId, const Worker_ComponentSetId ComponentSetId,
									   const Worker_Authority Authority)
{
	AActor* Actor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId));
	if (Actor == nullptr)
	{
		return;
	}

	// TODO - Using bActorHadAuthority should be replaced with better tracking system to Actor entity creation [UNR-3960]
	const bool bActorHadAuthority = Actor->HasAuthority();

	USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId);

	if (Channel != nullptr)
	{
		if (ComponentSetId == SpatialConstants::SERVER_AUTH_COMPONENT_SET_ID)
		{
			Channel->SetServerAuthority(Authority == WORKER_AUTHORITY_AUTHORITATIVE);
		}
		else if (ComponentSetId == SpatialConstants::CLIENT_AUTH_COMPONENT_SET_ID)
		{
			Channel->SetClientAuthority(Authority == WORKER_AUTHORITY_AUTHORITATIVE);
		}
	}

	if (NetDriver->IsServer())
	{
		// If we became authoritative over the server auth component set, set our role to be ROLE_Authority
		// and set our RemoteRole to be ROLE_AutonomousProxy if the actor has an owning connection.
		// Note: Pawn, PlayerController, and PlayerState for player-owned characters can arrive in
		// any order on non-authoritative servers, so it's possible that we don't yet know if a pawn
		// is player controlled when gaining authority over the pawn and need to wait for the player
		// state. Likewise, it's possible that the player state doesn't have a pointer to its pawn
		// yet, so we need to wait for the pawn to arrive.
		if (ComponentSetId == SpatialConstants::SERVER_AUTH_COMPONENT_SET_ID)
		{
			if (Authority == WORKER_AUTHORITY_AUTHORITATIVE)
			{
				const bool bDormantActor = (Actor->NetDormancy >= DORM_DormantAll);

				if (IsValid(Channel) || bDormantActor)
				{
					Actor->Role = ROLE_Authority;
					Actor->RemoteRole = ROLE_SimulatedProxy;

					// bReplicates is not replicated, but this actor is replicated.
					if (!Actor->GetIsReplicated())
					{
						Actor->SetReplicates(true);
					}

					if (Channel != nullptr && Channel->IsAutonomousProxyOnAuthority())
					{
						Actor->RemoteRole = ROLE_AutonomousProxy;

						// Flush PC interest on handover
						if (GetDefault<USpatialGDKSettings>()->bUseClientEntityInterestQueries
							&& GetDefault<USpatialGDKSettings>()->bRefreshClientInterestOnHandover)
						{
							Worker_EntityId ControllerEntityId =
								NetDriver->PackageMap->GetEntityIdFromObject(Actor->GetNetConnection()->PlayerController);
							if (ControllerEntityId != SpatialConstants::INVALID_ENTITY_ID)
							{
								MarkClientInterestDirty(ControllerEntityId, /*bOVerwrite*/ true);
							}
							else
							{
								UE_LOG(LogActorSystem, Warning, TEXT("Failed to get player controller to update client interest (%s)"),
									   *Actor->GetName());
							}
						}
					}

					if (!bDormantActor)
					{
						USpatialActorChannel* ActorChannel = NetDriver->GetActorChannelByEntityId(EntityId);
						ActorChannel->OnHandoverAuthorityGained();
					}

					// TODO - Using bActorHadAuthority should be replaced with better tracking system to Actor entity creation [UNR-3960]
					// When receiving AuthorityGained from SpatialOS, the Actor role will be ROLE_Authority iff this
					// worker is receiving entity data for the 1st time after spawning the entity. In all other cases,
					// the Actor role will have been explicitly set to ROLE_SimulatedProxy previously during the
					// entity creation flow.
					if (bActorHadAuthority)
					{
						Actor->SetActorReady(true);
					}

					// We still want to call OnAuthorityGained if the Actor migrated to this worker or was loaded from a snapshot.
					Actor->OnAuthorityGained();
				}
				else
				{
					UE_LOG(LogActorSystem, Verbose,
						   TEXT("Received authority over actor %s, with entity id %lld, which has no channel. This means it attempted to "
								"delete it earlier, when it had no authority. Retrying to delete now."),
						   *Actor->GetName(), EntityId);
					RetireEntity(EntityId, Actor->IsNetStartupActor());
				}
			}
			else if (Authority == WORKER_AUTHORITY_NOT_AUTHORITATIVE)
			{
				if (Channel != nullptr)
				{
					Channel->bCreatedEntity = false;
				}

				// With load-balancing enabled, we already set ROLE_SimulatedProxy and trigger OnAuthorityLost when we
				// set AuthorityIntent to another worker. This conditional exists to dodge calling OnAuthorityLost
				// twice.
				if (Actor->Role != ROLE_SimulatedProxy)
				{
					Actor->Role = ROLE_SimulatedProxy;
					Actor->RemoteRole = ROLE_Authority;

					Actor->OnAuthorityLost();
				}
			}
		}
	}
	else if (ComponentSetId == SpatialConstants::CLIENT_AUTH_COMPONENT_SET_ID)
	{
		if (Channel != nullptr)
		{
			Channel->ClientProcessOwnershipChange(Authority == WORKER_AUTHORITY_AUTHORITATIVE);

			if (Channel->IsAutonomousProxyOnAuthority() && Authority == WORKER_AUTHORITY_AUTHORITATIVE)
			{
				Actor->Role = ROLE_AutonomousProxy;
			}
		}
	}
}

void ActorSystem::ComponentAdded(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId, Schema_ComponentData* Data,
								 TArray<FWeakObjectPtr>& OutToResolveOps)
{
	if (ComponentId == SpatialConstants::DORMANT_COMPONENT_ID)
	{
		HandleDormantComponentAdded(EntityId);
		return;
	}

	if (ComponentId < SpatialConstants::STARTING_GENERATED_COMPONENT_ID
		|| NetDriver->ClassInfoManager->IsGeneratedQBIMarkerComponent(ComponentId))
	{
		return;
	}

	USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId);
	if (!NetDriver->IsServer() && Channel == nullptr)
	{
		// Try to restore the channel if this is a stably named actor. This can happen if a sublevel
		// gets reloaded quickly and results in the entity components getting refreshed instead of
		// the entity getting removed and added again.
		if (AActor* StablyNamedActor = TryGetActor(ActorDataStore[EntityId].Metadata))
		{
			Channel = TryRestoreActorChannelForStablyNamedActor(StablyNamedActor, EntityId);
		}
	}

	if (Channel == nullptr)
	{
		UE_LOG(LogActorSystem, Error,
			   TEXT("Got an add component for an entity that doesn't have an associated actor channel."
					" Entity id: %lld, component id: %d."),
			   EntityId, ComponentId);
		return;
	}

	if (Channel->bCreatedEntity)
	{
		// Allows servers to change state if they are going to be authoritative, without us overwriting it with old data.
		// TODO: UNR-3457 to remove this workaround.
		return;
	}

	HandleIndividualAddComponent(EntityId, ComponentId, Data, OutToResolveOps);
}

void ActorSystem::ComponentUpdated(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId, Schema_ComponentUpdate* Update)
{
	if (ComponentId < SpatialConstants::STARTING_GENERATED_COMPONENT_ID
		|| NetDriver->ClassInfoManager->IsGeneratedQBIMarkerComponent(ComponentId))
	{
		return;
	}

	USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId);
	if (Channel == nullptr)
	{
		// If there is no actor channel as a result of the actor being dormant, then assume the actor is about to become active.
		if (ActorSubView->HasComponent(EntityId, SpatialConstants::DORMANT_COMPONENT_ID))
		{
			if (AActor* Actor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId)))
			{
				Channel = GetOrRecreateChannelForDormantActor(Actor, EntityId);

				// As we haven't removed the dormant component just yet, this might be a single replication update where the actor
				// remains dormant. Add it back to pending dormancy so the local worker can clean up the channel. If we do process
				// a dormant component removal later in this frame, we'll clear the channel from pending dormancy channel then.
				NetDriver->AddPendingDormantChannel(Channel);
			}
			else
			{
				UE_LOG(LogActorSystem, Warning,
					   TEXT("Worker: %s Dormant actor (entity: %lld) has been deleted on this worker but we have received a component "
							"update (id: %d) from the server."),
					   *NetDriver->Connection->GetWorkerId(), EntityId, ComponentId);
				return;
			}
		}
		else
		{
			UE_LOG(LogActorSystem, Log,
				   TEXT("Worker: %s Entity: %lld Component: %d - No actor channel for update. This most likely occured due to the "
						"component updates that are sent when authority is lost during entity deletion."),
				   *NetDriver->Connection->GetWorkerId(), EntityId, ComponentId);
			return;
		}
	}

	uint32 Offset;
	bool bFoundOffset = NetDriver->ClassInfoManager->GetOffsetByComponentId(ComponentId, Offset);
	if (!bFoundOffset)
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Worker: %s EntityId %d ComponentId %d - Could not find offset for component id when receiving a component update."),
			   *NetDriver->Connection->GetWorkerId(), EntityId, ComponentId);
		return;
	}

	UObject* TargetObject = nullptr;

	if (Offset == 0)
	{
		TargetObject = Channel->GetActor();
	}
	else
	{
		TargetObject = NetDriver->PackageMap->GetObjectFromUnrealObjectRef(FUnrealObjectRef(EntityId, Offset)).Get();
	}

	if (TargetObject == nullptr)
	{
		UE_LOG(LogActorSystem, Warning, TEXT("Entity: %d Component: %d - Couldn't find target object for update"), EntityId, ComponentId);
		return;
	}

	if (EventTracer != nullptr)
	{
		const AActor* Object = Channel->Actor;
		TArray<FSpatialGDKSpanId> CauseSpanIds = EventTracer->GetAndConsumeSpansForComponent(EntityComponentId(EntityId, ComponentId));
		const Trace_SpanIdType* Causes = (const Trace_SpanIdType*)CauseSpanIds.GetData();

		EventTracer->TraceEvent(COMPONENT_UPDATE_EVENT_NAME, "", Causes, CauseSpanIds.Num(),
								[Object, TargetObject, EntityId, ComponentId](FSpatialTraceEventDataBuilder& EventBuilder) {
									EventBuilder.AddObject(Object);
									EventBuilder.AddObject(TargetObject, "target_object");
									EventBuilder.AddEntityId(EntityId);
									EventBuilder.AddComponentId(ComponentId);
								});
	}

	ESchemaComponentType Category = NetDriver->ClassInfoManager->GetCategoryByComponentId(ComponentId);

	if (Category != SCHEMA_Invalid)
	{
		ensureAlways(Category != SCHEMA_ServerOnly || NetDriver->IsServer());
		SCOPE_CYCLE_COUNTER(STAT_ActorSystemApplyData);
		ApplyComponentUpdate(ComponentId, Update, *TargetObject, *Channel);
	}
	else
	{
		UE_LOG(LogActorSystem, Verbose,
			   TEXT("Entity: %d Component: %d - Skipping because it's an empty component update from an RPC component. (most likely as a "
					"result of gaining authority)"),
			   EntityId, ComponentId);
	}
}

void ActorSystem::ComponentRemoved(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId) const
{
	// Early out if this isn't a generated component.
	if (ComponentId < SpatialConstants::STARTING_GENERATED_COMPONENT_ID && ComponentId != SpatialConstants::DORMANT_COMPONENT_ID)
	{
		return;
	}

	if (AActor* Actor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId).Get()))
	{
		const FUnrealObjectRef ObjectRef(EntityId, ComponentId);
		if (ComponentId == SpatialConstants::DORMANT_COMPONENT_ID)
		{
			GetOrRecreateChannelForDormantActor(Actor, EntityId);
		}
		else if (UObject* Object = NetDriver->PackageMap->GetObjectFromUnrealObjectRef(ObjectRef).Get())
		{
			DestroySubObject(ObjectRef, *Object);
		}
	}
}

void ActorSystem::DestroySubObject(const FUnrealObjectRef& ObjectRef, UObject& Object) const
{
	const Worker_EntityId EntityId = ObjectRef.Entity;
	if (AActor* Actor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId).Get()))
	{
		if (USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId))
		{
			UE_LOG(LogActorSystem, Verbose, TEXT("Destroying subobject with offset %u on entity %d"), ObjectRef.Offset, EntityId);

			Channel->OnSubobjectDeleted(ObjectRef, &Object, TWeakObjectPtr<UObject>(&Object));

			Actor->OnSubobjectDestroyFromReplication(&Object);

			Object.PreDestroyFromReplication();
			Object.MarkPendingKill();

			NetDriver->PackageMap->RemoveSubobject(ObjectRef);
		}
	}
}

void ActorSystem::MarkClientInterestDirty(Worker_EntityId EntityId, bool bOverwrite)
{
	bool& bMapOverwrite = ClientInterestDirty.FindOrAdd(EntityId);
	bMapOverwrite |= bOverwrite;
}

void ActorSystem::EntityAdded(const Worker_EntityId EntityId)
{
	PopulateDataStore(EntityId);
	ReceiveActor(EntityId);
}

void ActorSystem::EntityRemoved(const Worker_EntityId EntityId)
{
	SCOPE_CYCLE_COUNTER(STAT_ActorSystemRemoveEntity);

	RemoveActor(EntityId);

	if (NetDriver->InitialOnlyFilter != nullptr && NetDriver->InitialOnlyFilter->HasInitialOnlyData(EntityId))
	{
		NetDriver->InitialOnlyFilter->RemoveInitialOnlyData(EntityId);
	}

	// Stop tracking if the entity was deleted as a result of deleting the actor during creation.
	// This assumes that authority will be gained before interest is gained and lost.
	const int32 RetiredActorIndex = EntitiesToRetireOnAuthorityGain.IndexOfByPredicate([EntityId](const DeferredRetire& Retire) {
		return EntityId == Retire.EntityId;
	});
	if (RetiredActorIndex != INDEX_NONE)
	{
		EntitiesToRetireOnAuthorityGain.RemoveAtSwap(RetiredActorIndex);
	}

	ActorDataStore.Remove(EntityId);
}

void ActorSystem::RefreshActorDormancyOnEntityCreation(Worker_EntityId EntityId, bool bMakeDormant)
{
	EntitiesMapToRefreshDormancy.Emplace(EntityId, bMakeDormant);
}

bool ActorSystem::HasEntityBeenRequestedForDelete(Worker_EntityId EntityId) const
{
	return EntitiesToRetireOnAuthorityGain.ContainsByPredicate([EntityId](const DeferredRetire& Retire) {
		return EntityId == Retire.EntityId;
	});
}

void ActorSystem::HandleEntityDeletedAuthority(Worker_EntityId EntityId) const
{
	const int32 Index = EntitiesToRetireOnAuthorityGain.IndexOfByPredicate([EntityId](const DeferredRetire& Retire) {
		return Retire.EntityId == EntityId;
	});
	if (Index != INDEX_NONE)
	{
		HandleDeferredEntityDeletion(EntitiesToRetireOnAuthorityGain[Index]);
	}
}

void ActorSystem::HandleDeferredEntityDeletion(const DeferredRetire& Retire) const
{
	if (Retire.bNeedsTearOff)
	{
		SendActorTornOffUpdate(Retire.EntityId, Retire.ActorClassId);
		NetDriver->DelayedRetireEntity(Retire.EntityId, 1.0f, Retire.bIsNetStartupActor);
	}
	else
	{
		RetireEntity(Retire.EntityId, Retire.bIsNetStartupActor);
	}
}

void ActorSystem::RetireWhenAuthoritative(Worker_EntityId EntityId, Worker_ComponentId ActorClassId, bool bIsNetStartup, bool bNeedsTearOff)
{
	DeferredRetire DeferredObj = { EntityId, ActorClassId, bIsNetStartup, bNeedsTearOff };
	EntitiesToRetireOnAuthorityGain.Add(DeferredObj);
}

void ActorSystem::HandleDormantComponentAdded(const Worker_EntityId EntityId) const
{
	if (USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId))
	{
		NetDriver->AddPendingDormantChannel(Channel);
	}
	else
	{
		// This would normally get registered through the channel cleanup, but we don't have one for this entity
		NetDriver->RegisterDormantEntityId(EntityId);
	}
}

void ActorSystem::HandleIndividualAddComponent(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId,
											   Schema_ComponentData* Data, TArray<FWeakObjectPtr>& OutToResolveOps)
{
	uint32 Offset = 0;
	bool bFoundOffset = NetDriver->ClassInfoManager->GetOffsetByComponentId(ComponentId, Offset);
	if (!bFoundOffset)
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Could not find offset for component id when receiving dynamic AddComponent."
					" (EntityId %lld, ComponentId %d)"),
			   EntityId, ComponentId);
		return;
	}

	// Object already exists, we can apply data directly.
	if (UObject* Object = NetDriver->PackageMap->GetObjectFromUnrealObjectRef(FUnrealObjectRef(EntityId, Offset)).Get())
	{
		if (USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId))
		{
			ApplyComponentData(*Channel, *Object, ComponentId, Data);
		}
		return;
	}

	const FClassInfo& Info = NetDriver->ClassInfoManager->GetClassInfoByComponentId(ComponentId);
	AActor* Actor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId).Get());
	if (Actor == nullptr)
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Received an add component op for subobject of type %s on entity %lld but couldn't find Actor!"),
			   *Info.Class->GetName(), EntityId);
		return;
	}

	// Check if this is a static subobject that's been destroyed by the receiver.
	if (!IsDynamicSubObject(*NetDriver, *Actor, Offset))
	{
		UE_LOG(LogActorSystem, Verbose,
			   TEXT("Tried to apply component data on add component for a static subobject that's been deleted, will skip. Entity: %lld, "
					"Component: %d, Actor: %s"),
			   EntityId, ComponentId, *Actor->GetPathName());
		return;
	}

	// Otherwise this is a dynamically attached component. We need to make sure we have all related components before creation.
	TSet<Worker_ComponentId>& Components = PendingDynamicSubobjectComponents.FindOrAdd(EntityId);
	Components.Add(ComponentId);

	// Create filter for the components we expect to have in view.
	// Server - data/owner-only/handover
	// Owning client - data/owner-only
	// Non-owning client - data
	// If initial-only disabled + initial-only to all (counter-intuitive, but initial only is sent as normal if disabled and not sent at all
	// on dynamic components if enabled)
	const bool bIsServer = NetDriver->IsServer();
	const bool bIsAuthClient = NetDriver->HasClientAuthority(EntityId);
	const bool bInitialOnlyExpected = !GetDefault<USpatialGDKSettings>()->bEnableInitialOnlyReplicationCondition;

	Worker_ComponentId ComponentFilter[SCHEMA_Count];
	ComponentFilter[SCHEMA_Data] = true;
	ComponentFilter[SCHEMA_OwnerOnly] = bIsServer || bIsAuthClient;
	ComponentFilter[SCHEMA_ServerOnly] = bIsServer;
	ComponentFilter[SCHEMA_InitialOnly] = bInitialOnlyExpected;
	static_assert(SCHEMA_Count == 4, "Unexpected number of Schema type components, please check the enclosing function is still correct.");

	bool bComponentsComplete = true;
	for (int i = 0; i < SCHEMA_Count; ++i)
	{
		if (ComponentFilter[i] && Info.SchemaComponents[i] != SpatialConstants::INVALID_COMPONENT_ID
			&& Components.Find(Info.SchemaComponents[i]) == nullptr)
		{
			bComponentsComplete = false;
			break;
		}
	}

	UE_LOG(LogActorSystem, Log, TEXT("Processing add component, unreal component %s. Entity: %lld, Offset: %d, Component: %d, Actor: %s"),
		   bComponentsComplete ? TEXT("complete") : TEXT("not complete"), EntityId, Offset, ComponentId, *Actor->GetPathName());

	if (bComponentsComplete)
	{
		AttachDynamicSubobject(Actor, EntityId, Info, OutToResolveOps);
	}
}

void ActorSystem::AttachDynamicSubobject(AActor* Actor, Worker_EntityId EntityId, const FClassInfo& Info,
										 TArray<FWeakObjectPtr>& OutToResolveOps)
{
	USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId);
	if (Channel == nullptr)
	{
		UE_LOG(LogActorSystem, Verbose, TEXT("Tried to dynamically attach subobject of type %s to entity %lld but couldn't find Channel!"),
			   *Info.Class->GetName(), EntityId);
		return;
	}

	UObject* Subobject = NewObject<UObject>(Actor, Info.Class.Get());

	Actor->OnSubobjectCreatedFromReplication(Subobject);

	FUnrealObjectRef SubobjectRef(EntityId, Info.SchemaComponents[SCHEMA_Data]);
	NetDriver->PackageMap->ResolveSubobject(Subobject, SubobjectRef);

	Channel->CreateSubObjects.Add(Subobject);

	TSet<Worker_ComponentId>& Components = PendingDynamicSubobjectComponents.FindChecked(EntityId);
	ForAllSchemaComponentTypes([&](ESchemaComponentType Type) {
		const Worker_ComponentId ComponentId = Info.SchemaComponents[Type];

		if (ComponentId == SpatialConstants::INVALID_COMPONENT_ID)
		{
			return;
		}

		if (!Components.Contains(ComponentId))
		{
			return;
		}

		ApplyComponentData(
			*Channel, *Subobject, ComponentId,
			ActorSubView->GetView()[EntityId].Components.FindByPredicate(ComponentIdEquality{ ComponentId })->GetUnderlying());

		Components.Remove(ComponentId);
	});

	// Resolve things like RPCs and user object references after we have applied all other component updates for the entity.
	OutToResolveOps.Emplace(FWeakObjectPtr(Subobject));
}

void ActorSystem::ApplyComponentData(USpatialActorChannel& Channel, UObject& TargetObject, const Worker_ComponentId ComponentId,
									 Schema_ComponentData* Data)
{
	UClass* Class = NetDriver->ClassInfoManager->GetClassByComponentId(ComponentId);
	checkf(Class, TEXT("Component %d isn't hand-written and not present in ComponentToClassMap."), ComponentId);

	ESchemaComponentType ComponentType = NetDriver->ClassInfoManager->GetCategoryByComponentId(ComponentId);

	if (ComponentType != SCHEMA_Invalid)
	{
		if (ComponentType == SCHEMA_Data && TargetObject.IsA<UActorComponent>())
		{
			Schema_Object* ComponentObject = Schema_GetComponentDataFields(Data);
			bool bReplicates = !!Schema_IndexBool(ComponentObject, SpatialConstants::ACTOR_COMPONENT_REPLICATES_ID, 0);
			if (!bReplicates)
			{
				return;
			}
		}
		RepStateUpdateHelper RepStateHelper(Channel, TargetObject);

		ComponentReader Reader(NetDriver, RepStateHelper.GetRefMap(), NetDriver->Connection->GetEventTracer());
		bool bOutReferencesChanged = false;

		const bool bSuccessfullyPreNetReceived = InvokePreNetReceive(TargetObject);
		if (bSuccessfullyPreNetReceived)
		{
			FObjectRepNotifies& ObjectRepNotifiesOut = GetObjectRepNotifies(TargetObject);
			Reader.ApplyComponentData(ComponentId, Data, TargetObject, Channel, ObjectRepNotifiesOut, bOutReferencesChanged);
		}
		else
		{
			UE_LOG(LogActorSystem, Log,
				   TEXT("ApplyComponentData: Did not invoke PreNetReceive for object %s, entity id %lld, component id %u. No data will be "
						"applied."),
				   *TargetObject.GetName(), Channel.GetEntityId(), ComponentId);
		}

		RepStateHelper.Update(*this, Channel, bOutReferencesChanged);
	}
	else
	{
		UE_LOG(LogActorSystem, Verbose, TEXT("Entity: %d Component: %d - Skipping because RPC components don't have actual data."),
			   Channel.GetEntityId(), ComponentId);
	}
}

void ActorSystem::ResolvePendingOpsFromEntityUpdate(const TArray<FWeakObjectPtr>& ToResolveOps)
{
	// This should be called after all component updates and adds have been completed, and PostNetReceives have been called to avoid user
	// code from seeing inconsistent state
	for (const FWeakObjectPtr& WeakObjectPtr : ToResolveOps)
	{
		UObject* Object = WeakObjectPtr.Get();
		if (!Object)
		{
			UE_LOG(LogActorSystem, Log,
				   TEXT("ResolvePendingOpsFromEntityUpdate: Did not resolve pending ops for object %s as it was no longer valid."),
				   *GetNameSafe(Object));
			continue;
		}

		const FUnrealObjectRef ObjectRef = NetDriver->PackageMap->GetUnrealObjectRefFromObject(Object);

		if (!ObjectRef.IsValid())
		{
			UE_LOG(LogActorSystem, Error,
				   TEXT("ResolvePendingOpsFromEntityUpdate: Tried to resolve pending ops for %s but object ref was not valid."),
				   *Object->GetName());
		}

		ResolvePendingOperations(Object, ObjectRef);
	}
}

void ActorSystem::ResolveAsyncPendingLoad(UObject* LoadedObject, const FUnrealObjectRef& ObjectRef)
{
	ResolvePendingOperations(LoadedObject, ObjectRef);
	InvokeRepNotifies();
}

void ActorSystem::ResolvePendingOperations(UObject* Object, const FUnrealObjectRef& ObjectRef)
{
	UE_LOG(LogActorSystem, Verbose, TEXT("Resolving pending object refs and RPCs which depend on object: %s %s."), *Object->GetName(),
		   *ObjectRef.ToString());

	ResolveIncomingOperations(Object, ObjectRef);

	// When resolving an Actor that should uniquely exist in a deployment, e.g. GameMode, GameState, LevelScriptActors, we also
	// resolve using class path (in case any properties were set from a server that hasn't resolved the Actor yet).
	if (FUnrealObjectRef::ShouldLoadObjectFromClassPath(Object))
	{
		FUnrealObjectRef ClassObjectRef = FUnrealObjectRef::GetRefFromObjectClassPath(Object, NetDriver->PackageMap);
		if (ClassObjectRef.IsValid())
		{
			ResolveIncomingOperations(Object, ClassObjectRef);
		}
	}

	// TODO: UNR-1650 We're trying to resolve all queues, which introduces more overhead.
	NetDriver->RPCService->ProcessIncomingRPCs();
}

void ActorSystem::ResolveIncomingOperations(UObject* Object, const FUnrealObjectRef& ObjectRef)
{
	// TODO: queue up resolved objects since they were resolved during process ops
	// and then resolve all of them at the end of process ops - UNR:582

	TSet<FChannelObjectPair>* TargetObjectSet = ObjectRefToRepStateMap.Find(ObjectRef);
	if (!TargetObjectSet)
	{
		return;
	}

	UE_LOG(LogActorSystem, Verbose, TEXT("Resolving incoming operations depending on object ref %s, resolved object: %s"),
		   *ObjectRef.ToString(), *Object->GetName());

	for (auto ChannelObjectIter = TargetObjectSet->CreateIterator(); ChannelObjectIter; ++ChannelObjectIter)
	{
		USpatialActorChannel* DependentChannel = ChannelObjectIter->Key.Get();
		if (!DependentChannel)
		{
			ChannelObjectIter.RemoveCurrent();
			continue;
		}

		UObject* ReplicatingObject = ChannelObjectIter->Value.Get();

		if (!ReplicatingObject)
		{
			if (DependentChannel->ObjectReferenceMap.Find(ChannelObjectIter->Value))
			{
				DependentChannel->ObjectReferenceMap.Remove(ChannelObjectIter->Value);
				ChannelObjectIter.RemoveCurrent();
			}
			continue;
		}

		FSpatialObjectRepState* RepState = DependentChannel->ObjectReferenceMap.Find(ChannelObjectIter->Value);
		if (!RepState || !RepState->UnresolvedRefs.Contains(ObjectRef))
		{
			continue;
		}

		// Check whether the resolved object has been torn off, or is on an actor that has been torn off.
		if (AActor* AsActor = Cast<AActor>(ReplicatingObject))
		{
			if (AsActor->GetTearOff())
			{
				UE_LOG(LogActorSystem, Log,
					   TEXT("Actor to be resolved was torn off, so ignoring incoming operations. Object ref: %s, resolved object: %s"),
					   *ObjectRef.ToString(), *Object->GetName());
				DependentChannel->ObjectReferenceMap.Remove(ChannelObjectIter->Value);
				continue;
			}
		}
		else if (AActor* OuterActor = ReplicatingObject->GetTypedOuter<AActor>())
		{
			if (OuterActor->GetTearOff())
			{
				UE_LOG(LogActorSystem, Log,
					   TEXT("Owning Actor of the object to be resolved was torn off, so ignoring incoming operations. Object ref: %s, "
							"resolved object: %s"),
					   *ObjectRef.ToString(), *Object->GetName());
				DependentChannel->ObjectReferenceMap.Remove(ChannelObjectIter->Value);
				continue;
			}
		}

		bool bSomeObjectsWereMapped = false;

		FRepLayout& RepLayout = DependentChannel->GetObjectRepLayout(ReplicatingObject);
		FRepStateStaticBuffer& ShadowData = DependentChannel->GetObjectStaticBuffer(ReplicatingObject);
		if (ShadowData.Num() == 0)
		{
			DependentChannel->ResetShadowData(RepLayout, ShadowData, ReplicatingObject);
		}

		FObjectRepNotifies& ObjectRepNotifiesOut = GetObjectRepNotifies(*ReplicatingObject);
		ResolveObjectReferences(RepLayout, ReplicatingObject, *RepState, RepState->ReferenceMap, ShadowData.GetData(),
								(uint8*)ReplicatingObject, ReplicatingObject->GetClass()->GetPropertiesSize(), ObjectRepNotifiesOut,
								bSomeObjectsWereMapped);

		if (bSomeObjectsWereMapped)
		{
			UE_LOG(LogActorSystem, Verbose, TEXT("Resolved for target object %s"), *ReplicatingObject->GetName());
			ReplicatingObject->PostNetReceive();
		}

		RepState->UnresolvedRefs.Remove(ObjectRef);
	}
}

void ActorSystem::ResolveObjectReferences(FRepLayout& RepLayout, UObject* ReplicatedObject, FSpatialObjectRepState& RepState,
										  FObjectReferencesMap& ObjectReferencesMap, uint8* RESTRICT StoredData, uint8* RESTRICT Data,
										  int32 MaxAbsOffset, FObjectRepNotifies& ObjectRepNotifiesOut, bool& bOutSomeObjectsWereMapped)
{
	for (auto It = ObjectReferencesMap.CreateIterator(); It; ++It)
	{
		int32 AbsOffset = It.Key();
		FObjectReferences& ObjectReferences = It.Value();
		GDK_PROPERTY(Property)* Property = ObjectReferences.Property;

		if (AbsOffset >= MaxAbsOffset)
		{
			// If you see this error, it is possible that there has been a non-auth modification of data containing object references.
			UE_LOG(LogActorSystem, Error,
				   TEXT("ResolveObjectReferences: Removed unresolved reference for property %s: AbsOffset >= MaxAbsOffset: %d > %d. This "
						"could indicate non-auth modification."),
				   *GetNameSafe(Property), AbsOffset, MaxAbsOffset);
			It.RemoveCurrent();
			continue;
		}

		const FRepParentCmd& Parent = RepLayout.Parents[ObjectReferences.ParentIndex];

		int32 StoredDataOffset = ObjectReferences.ShadowOffset;

		if (ObjectReferences.Array)
		{
			GDK_PROPERTY(ArrayProperty)* ArrayProperty = GDK_CASTFIELD<GDK_PROPERTY(ArrayProperty)>(Property);
			check(ArrayProperty != nullptr);

			Property->CopySingleValue(StoredData + StoredDataOffset, Data + AbsOffset);

			FScriptArray* StoredArray = (FScriptArray*)(StoredData + StoredDataOffset);
			FScriptArray* Array = (FScriptArray*)(Data + AbsOffset);

			int32 NewMaxOffset = Array->Num() * ArrayProperty->Inner->ElementSize;

			ResolveObjectReferences(RepLayout, ReplicatedObject, RepState, *ObjectReferences.Array, (uint8*)StoredArray->GetData(),
									(uint8*)Array->GetData(), NewMaxOffset, ObjectRepNotifiesOut, bOutSomeObjectsWereMapped);
			continue;
		}

		bool bResolvedSomeRefs = false;
		UObject* SinglePropObject = nullptr;
		FUnrealObjectRef SinglePropRef = FUnrealObjectRef::NULL_OBJECT_REF;

		for (auto UnresolvedIt = ObjectReferences.UnresolvedRefs.CreateIterator(); UnresolvedIt; ++UnresolvedIt)
		{
			FUnrealObjectRef& ObjectRef = *UnresolvedIt;

			bool bUnresolved = false;
			UObject* Object = FUnrealObjectRef::ToObjectPtr(ObjectRef, NetDriver->PackageMap, bUnresolved);
			if (!bUnresolved)
			{
				check(Object != nullptr);

				UE_LOG(LogActorSystem, Verbose,
					   TEXT("ResolveObjectReferences: Resolved object ref: Offset: %d, Object ref: %s, PropName: %s, ObjName: %s"),
					   AbsOffset, *ObjectRef.ToString(), *Property->GetNameCPP(), *Object->GetName());

				if (ObjectReferences.bSingleProp)
				{
					SinglePropObject = Object;
					SinglePropRef = ObjectRef;
				}

				UnresolvedIt.RemoveCurrent();

				bResolvedSomeRefs = true;
			}
		}

		if (bResolvedSomeRefs)
		{
			if (!bOutSomeObjectsWereMapped)
			{
				ReplicatedObject->PreNetReceive();
				bOutSomeObjectsWereMapped = true;
			}

			if (Parent.Property->HasAnyPropertyFlags(CPF_RepNotify))
			{
				Property->CopySingleValue(StoredData + StoredDataOffset, Data + AbsOffset);
			}

			if (ObjectReferences.bSingleProp)
			{
				GDK_PROPERTY(ObjectPropertyBase)* ObjectProperty = GDK_CASTFIELD<GDK_PROPERTY(ObjectPropertyBase)>(Property);
				check(ObjectProperty);

				ObjectProperty->SetObjectPropertyValue(Data + AbsOffset, SinglePropObject);
				ObjectReferences.MappedRefs.Add(SinglePropRef);
			}
			else if (ObjectReferences.bFastArrayProp)
			{
				TSet<FUnrealObjectRef> NewMappedRefs;
				TSet<FUnrealObjectRef> NewUnresolvedRefs;
				FSpatialNetBitReader ValueDataReader(NetDriver->PackageMap, ObjectReferences.Buffer.GetData(),
													 ObjectReferences.NumBufferBits, NewMappedRefs, NewUnresolvedRefs);

				check(Property->IsA<GDK_PROPERTY(ArrayProperty)>());
				UScriptStruct* NetDeltaStruct = GetFastArraySerializerProperty(GDK_CASTFIELD<GDK_PROPERTY(ArrayProperty)>(Property));

				FSpatialNetDeltaSerializeInfo::DeltaSerializeRead(NetDriver, ValueDataReader, ReplicatedObject, Parent.ArrayIndex,
																  Parent.Property, NetDeltaStruct);

				ObjectReferences.MappedRefs.Append(NewMappedRefs);
			}
			else
			{
				TSet<FUnrealObjectRef> NewMappedRefs;
				TSet<FUnrealObjectRef> NewUnresolvedRefs;
				FSpatialNetBitReader BitReader(NetDriver->PackageMap, ObjectReferences.Buffer.GetData(), ObjectReferences.NumBufferBits,
											   NewMappedRefs, NewUnresolvedRefs);
				check(Property->IsA<GDK_PROPERTY(StructProperty)>());

				bool bHasUnresolved = false;
				ReadStructProperty(BitReader, GDK_CASTFIELD<GDK_PROPERTY(StructProperty)>(Property), NetDriver, Data + AbsOffset,
								   bHasUnresolved);

				ObjectReferences.MappedRefs.Append(NewMappedRefs);
			}

			if (Parent.Property->HasAnyPropertyFlags(CPF_RepNotify))
			{
				if (Parent.RepNotifyCondition == REPNOTIFY_Always || !Property->Identical(StoredData + StoredDataOffset, Data + AbsOffset))
				{
					ObjectRepNotifiesOut.RepNotifies.AddUnique(Parent.Property);
				}
			}
		}
	}
}

USpatialActorChannel* ActorSystem::GetOrRecreateChannelForDormantActor(AActor* Actor, const Worker_EntityId EntityID) const
{
	// Receive would normally create channel in ReceiveActor - this function is used to recreate the channel after waking up a dormant actor
	USpatialActorChannel* Channel = NetDriver->GetOrCreateSpatialActorChannel(Actor);
	if (Channel == nullptr)
	{
		return nullptr;
	}
	check(!Channel->bCreatingNewEntity);
	check(Channel->GetEntityId() == EntityID);

	if (!ActorSubView->HasComponent(EntityID, SpatialConstants::DORMANT_COMPONENT_ID))
	{
		Actor->NetDormancy = DORM_Awake;
	}

	NetDriver->RemovePendingDormantChannel(Channel);
	NetDriver->UnregisterDormantEntityId(EntityID);

	return Channel;
}

void ActorSystem::ApplyComponentUpdate(const Worker_ComponentId ComponentId, Schema_ComponentUpdate* ComponentUpdate, UObject& TargetObject,
									   USpatialActorChannel& Channel)
{
	RepStateUpdateHelper RepStateHelper(Channel, TargetObject);

	ComponentReader Reader(NetDriver, RepStateHelper.GetRefMap(), NetDriver->Connection->GetEventTracer());
	bool bOutReferencesChanged = false;

	const bool bSuccessfullyPreNetReceived = InvokePreNetReceive(TargetObject);
	if (bSuccessfullyPreNetReceived)
	{
		FObjectRepNotifies& ObjectRepNotifiesOut = GetObjectRepNotifies(TargetObject);
		Reader.ApplyComponentUpdate(ComponentId, ComponentUpdate, TargetObject, Channel, ObjectRepNotifiesOut, bOutReferencesChanged);
	}
	else
	{
		UE_LOG(LogActorSystem, Log,
			   TEXT("ApplyComponentUpdate: Did not invoke PreNetReceive for object %s, entity id %lld, component id %u. No data will be "
					"applied."),
			   *TargetObject.GetName(), Channel.GetEntityId(), ComponentId);
	}
	RepStateHelper.Update(*this, Channel, bOutReferencesChanged);

	// This is a temporary workaround, see UNR-841:
	// If the update includes tearoff, close the channel and clean up the entity.
	if (TargetObject.IsA<AActor>() && NetDriver->ClassInfoManager->GetCategoryByComponentId(ComponentId) == SCHEMA_Data)
	{
		Schema_Object* ComponentObject = Schema_GetComponentUpdateFields(ComponentUpdate);

		// Check if bTearOff has been set to true
		if (GetBoolFromSchema(ComponentObject, SpatialConstants::ACTOR_TEAROFF_ID))
		{
			EntityChannelsToSetTornOff.Add(Channel.GetEntityId());
		}
	}
}

void ActorSystem::ReceiveActor(Worker_EntityId EntityId)
{
	SCOPE_CYCLE_COUNTER(STAT_ActorSystemReceiveActor);

	checkf(NetDriver, TEXT("We should have a NetDriver whilst processing ops."));
	checkf(NetDriver->GetWorld(), TEXT("We should have a World whilst processing ops."));

	ActorData& ActorComponents = ActorDataStore[EntityId];

	{
		AActor* EntityActor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId).Get(/*bEvenIfPendingKill =*/true));
		if (EntityActor != nullptr)
		{
			if (!EntityActor->IsActorReady())
			{
				UE_LOG(LogActorSystem, Verbose, TEXT("%s: Entity %lld for Actor %s has been checked out on the worker which spawned it."),
					   *NetDriver->Connection->GetWorkerId(), EntityId, *EntityActor->GetName());
			}

			return;
		}
	}

	UClass* Class = ActorComponents.Metadata.GetNativeEntityClass();
	if (Class == nullptr)
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("The received actor with entity ID %lld couldn't be loaded. The actor (%s) will not be spawned."), EntityId,
			   *ActorComponents.Metadata.ClassPath);
		return;
	}

	// Make sure ClassInfo exists
	NetDriver->ClassInfoManager->GetOrCreateClassInfoByClass(Class);

	// If the received actor is torn off, don't bother spawning it.
	// (This is only needed due to the delay between tearoff and deleting the entity. See https://improbableio.atlassian.net/browse/UNR-841)
	if (IsReceivedEntityTornOff(EntityId))
	{
		UE_LOG(LogActorSystem, Verbose, TEXT("The received actor with entity ID %lld was already torn off. The actor will not be spawned."),
			   EntityId);
		return;
	}

	AActor* EntityActor = TryGetOrCreateActor(ActorComponents, EntityId);

	if (EntityActor == nullptr)
	{
		// This could be nullptr if:
		// a stably named actor could not be found
		// the class couldn't be loaded
		return;
	}

	if (!NetDriver->PackageMap->ResolveEntityActorAndSubobjects(EntityId, EntityActor))
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Failed to resolve entity actor when receiving entity. Actor will not be spawned. Entity: %lld, actor: %s"), EntityId,
			   *EntityActor->GetPathName());
		EntityActor->Destroy(true);
		return;
	}

	USpatialActorChannel* Channel = SetUpActorChannel(EntityActor, EntityId);
	if (Channel == nullptr)
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Failed to create an actor channel when receiving entity. Actor will not be spawned. Entity: %lld, actor: %s"),
			   EntityId, *EntityActor->GetPathName());
		EntityActor->Destroy(true);
		return;
	}

	ApplyFullState(EntityId, *Channel, *EntityActor);

	const UNetConnection* ActorNetConnection = EntityActor->GetNetConnection();
	if (IsValid(ActorNetConnection) && NetDriver->ServerConnection == ActorNetConnection)
	{
		if (ensureMsgf(NetDriver->OwnershipCompletenessHandler,
					   TEXT("OwnershipCompletenessHandler must be valid throughout ActorSystem's lifetime")))
		{
			NetDriver->OwnershipCompletenessHandler->AddPlayerEntity(EntityId);
		}
	}

	UE_LOG(LogActorSystem, Verbose,
		   TEXT("%s: Entity has been checked out on a worker which didn't spawn it. "
				"Entity ID: %lld, actor: %s"),
		   *NetDriver->Connection->GetWorkerId(), EntityId, *EntityActor->GetPathName());
}

void ActorSystem::RefreshEntity(const Worker_EntityId EntityId)
{
	AActor* EntityActor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId));

	checkf(IsValid(EntityActor), TEXT("RefreshEntity must have an actor for entity %lld"), EntityId);

	checkf(NetDriver, TEXT("We should have a NetDriver whilst processing ops."));
	checkf(NetDriver->GetWorld(), TEXT("We should have a World whilst processing ops."));

	USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId);
	check(IsValid(Channel));
	check(Channel->Actor == EntityActor);

	ApplyFullState(EntityId, *Channel, *EntityActor);
}

void ActorSystem::ApplyFullState(const Worker_EntityId EntityId, USpatialActorChannel& EntityActorChannel, AActor& EntityActor)
{
	TArray<ObjectPtrRefPair> ObjectsToResolvePendingOpsFor;

	const TArray<ComponentData>& EntityComponents = ActorSubView->GetView()[EntityId].Components;

	// Apply initial replicated properties.
	// This was moved to after FinishingSpawning because components existing only in blueprints aren't added until spawning is complete
	// Potentially we could split out the initial actor state and the initial component state
	for (const ComponentData& Component : EntityComponents)
	{
		if (NetDriver->ClassInfoManager->IsGeneratedQBIMarkerComponent(Component.GetComponentId())
			|| Component.GetComponentId() < SpatialConstants::STARTING_GENERATED_COMPONENT_ID)
		{
			continue;
		}
		ApplyComponentDataOnActorCreation(EntityId, Component.GetComponentId(), Component.GetUnderlying(), EntityActorChannel,
										  ObjectsToResolvePendingOpsFor);
	}

	if (NetDriver->InitialOnlyFilter != nullptr)
	{
		if (const TArray<ComponentData>* InitialOnlyComponents = NetDriver->InitialOnlyFilter->GetInitialOnlyData(EntityId))
		{
			for (const ComponentData& Component : *InitialOnlyComponents)
			{
				ApplyComponentDataOnActorCreation(EntityId, Component.GetComponentId(), Component.GetUnderlying(), EntityActorChannel,
												  ObjectsToResolvePendingOpsFor);
			}
		}
	}

	if (EntityActor.IsFullNameStableForNetworking())
	{
		// bNetLoadOnClient actors could have components removed while out of the client's interest
		ClientNetLoadActorHelper.RemoveRuntimeRemovedComponents(EntityId, EntityComponents, EntityActor);
	}

	InvokePostNetReceives();

	// Resolve things like RPCs and unresolved references after applying component data.
	for (const ObjectPtrRefPair& ObjectToResolve : ObjectsToResolvePendingOpsFor)
	{
		ResolvePendingOperations(ObjectToResolve.Key, ObjectToResolve.Value);
	}

	if (!NetDriver->IsServer())
	{
		// Update interest on the entity's components after receiving initial component data (so Role and RemoteRole are properly set).

		// This is a bit of a hack unfortunately, among the core classes only PlayerController implements this function and it requires
		// a player index. For now we don't support split screen, so the number is always 0.
		if (EntityActor.IsA(APlayerController::StaticClass()))
		{
			uint8 PlayerIndex = 0;
			// FInBunch takes size in bits not bytes
			FInBunch Bunch(NetDriver->ServerConnection, &PlayerIndex, sizeof(PlayerIndex) * 8);
			EntityActor.OnActorChannelOpen(Bunch, NetDriver->ServerConnection);
		}
		else
		{
			FInBunch Bunch(NetDriver->ServerConnection);
			EntityActor.OnActorChannelOpen(Bunch, NetDriver->ServerConnection);
		}
	}

	// Any Actor created here will have been received over the wire as an entity so we can mark it ready.
	EntityActor.SetActorReady(NetDriver->IsServer() && EntityActor.bNetStartup);

	// When we check out a startup Actor entity spawned by another server, call the notify function so relevant
	// rep graph nodes can have their interest flagged as dirty.
	if (NetDriver->IsServer() && GetDefault<USpatialGDKSettings>()->bUseClientEntityInterestQueries && !EntityActor.HasAuthority()
		&& EntityActor.bNetStartup)
	{
		if (UReplicationGraph* RepGraph = Cast<UReplicationGraph>(NetDriver->GetReplicationDriver()))
		{
			RepGraph->NotifyActorEntityCreation(&EntityActor);
		}
	}

	// Taken from PostNetInit
	if (NetDriver->GetWorld()->HasBegunPlay() && !EntityActor.HasActorBegunPlay())
	{
		EntityActor.DispatchBeginPlay();
	}

	EntityActor.UpdateOverlaps();

	if (ActorSubView->HasComponent(EntityId, SpatialConstants::DORMANT_COMPONENT_ID))
	{
		NetDriver->AddPendingDormantChannel(&EntityActorChannel);
	}
}

bool ActorSystem::IsReceivedEntityTornOff(const Worker_EntityId EntityId) const
{
	// Check the pending add components, to find the root component for the received entity.
	for (const ComponentData& Data : ActorSubView->GetView()[EntityId].Components)
	{
		if (NetDriver->ClassInfoManager->GetCategoryByComponentId(Data.GetComponentId()) != SCHEMA_Data)
		{
			continue;
		}

		UClass* Class = NetDriver->ClassInfoManager->GetClassByComponentId(Data.GetComponentId());
		if (!Class->IsChildOf<AActor>())
		{
			continue;
		}

		Schema_Object* ComponentObject = Schema_GetComponentDataFields(Data.GetUnderlying());
		return GetBoolFromSchema(ComponentObject, SpatialConstants::ACTOR_TEAROFF_ID);
	}

	return false;
}

AActor* ActorSystem::TryGetActor(const UnrealMetadata& Metadata) const
{
	if (Metadata.StablyNamedRef.IsSet())
	{
		if (NetDriver->IsServer() || Metadata.bNetStartup.GetValue())
		{
			// This Actor already exists in the map, get it from the package map.
			const FUnrealObjectRef& StablyNamedRef = Metadata.StablyNamedRef.GetValue();
			AActor* StaticActor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromUnrealObjectRef(StablyNamedRef));
			// An unintended side effect of GetObjectFromUnrealObjectRef is that this ref
			// will be registered with this Actor. It can be the case that this Actor is not
			// stably named (due to bNetLoadOnClient = false) so we should let
			// SpatialPackageMapClient::ResolveEntityActor handle it properly.
			NetDriver->PackageMap->UnregisterActorObjectRefOnly(StablyNamedRef);

			return StaticActor;
		}
	}
	return nullptr;
}

AActor* ActorSystem::TryGetOrCreateActor(ActorData& ActorComponents, const Worker_EntityId EntityId)
{
	if (ActorComponents.Metadata.StablyNamedRef.IsSet())
	{
		if (NetDriver->IsServer() || ActorComponents.Metadata.bNetStartup.GetValue())
		{
			// This Actor already exists in the map, get it from the package map.
			const FUnrealObjectRef& StablyNamedRef = ActorComponents.Metadata.StablyNamedRef.GetValue();
			AActor* StaticActor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromUnrealObjectRef(StablyNamedRef));
			// An unintended side effect of GetObjectFromUnrealObjectRef is that this ref
			// will be registered with this Actor. It can be the case that this Actor is not
			// stably named (due to bNetLoadOnClient = false) so we should let
			// SpatialPackageMapClient::ResolveEntityActor handle it properly.
			NetDriver->PackageMap->UnregisterActorObjectRefOnly(StablyNamedRef);

			return StaticActor;
		}
	}

	// Handle linking received unique Actors (e.g. game state, game mode) to instances already spawned on this worker.
	UClass* ActorClass = ActorComponents.Metadata.GetNativeEntityClass();
	if (FUnrealObjectRef::IsUniqueActorClass(ActorClass) && NetDriver->IsServer())
	{
		return NetDriver->PackageMap->GetUniqueActorInstanceByClass(ActorClass);
	}

	return CreateActor(ActorComponents, EntityId);
}

// This function is only called for client and server workers who did not spawn the Actor
AActor* ActorSystem::CreateActor(ActorData& ActorComponents, const Worker_EntityId EntityId)
{
	UClass* ActorClass = ActorComponents.Metadata.GetNativeEntityClass();

	if (ActorClass == nullptr)
	{
		UE_LOG(LogActorSystem, Error, TEXT("Could not load class %s when spawning entity!"), *ActorComponents.Metadata.ClassPath);
		return nullptr;
	}

	UE_LOG(LogActorSystem, Verbose, TEXT("Spawning a %s whilst checking out an entity."), *ActorClass->GetFullName());

	const bool bCreatingPlayerController = ActorClass->IsChildOf(APlayerController::StaticClass());

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.bRemoteOwned = true;
	SpawnInfo.bNoFail = true;

	FVector SpawnLocation = FRepMovement::RebaseOntoLocalOrigin(ActorComponents.Spawn.Location, NetDriver->GetWorld()->OriginLocation);

	AActor* NewActor =
		NetDriver->GetWorld()->SpawnActorAbsolute(ActorClass, FTransform(ActorComponents.Spawn.Rotation, SpawnLocation), SpawnInfo);
	check(NewActor);

	if (NetDriver->IsServer() && bCreatingPlayerController)
	{
		// Grab the client system entity ID from the partition component in order to correctly link this
		// connection to the client it corresponds to.
		const Worker_EntityId ClientSystemEntityId =
			Partition(ActorSubView->GetView()[EntityId]
						  .Components.FindByPredicate(ComponentIdEquality{ SpatialConstants::PARTITION_COMPONENT_ID })
						  ->GetUnderlying())
				.WorkerConnectionId;

		NetDriver->PostSpawnPlayerController(Cast<APlayerController>(NewActor), ClientSystemEntityId);
	}

	// Imitate the behavior in UPackageMapClient::SerializeNewActor.
	const float Epsilon = 0.001f;
	if (ActorComponents.Spawn.Velocity.Equals(FVector::ZeroVector, Epsilon))
	{
		NewActor->PostNetReceiveVelocity(ActorComponents.Spawn.Velocity);
	}
	if (!ActorComponents.Spawn.Scale.Equals(FVector::OneVector, Epsilon))
	{
		NewActor->SetActorScale3D(ActorComponents.Spawn.Scale);
	}

	// Don't have authority over Actor until SpatialOS delegates authority
	NewActor->Role = ROLE_SimulatedProxy;
	NewActor->RemoteRole = ROLE_Authority;
	NewActor->NetDormancy = ActorSubView->HasComponent(EntityId, SpatialConstants::DORMANT_COMPONENT_ID) ? DORM_DormantAll : DORM_Awake;

	return NewActor;
}

void ActorSystem::ApplyComponentDataOnActorCreation(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId,
													Schema_ComponentData* Data, USpatialActorChannel& Channel,
													TArray<ObjectPtrRefPair>& OutObjectsToResolve)
{
	AActor* Actor = Channel.GetActor();

	uint32 Offset = 0;
	const bool bFoundOffset = NetDriver->ClassInfoManager->GetOffsetByComponentId(ComponentId, Offset);
	if (!bFoundOffset)
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Worker: %s EntityId: %lld, ComponentId: %d - Could not find offset for component id when applying component data to "
					"Actor %s!"),
			   *NetDriver->Connection->GetWorkerId(), EntityId, ComponentId, *Actor->GetPathName());
		return;
	}

	FUnrealObjectRef TargetObjectRef(EntityId, Offset);
	TWeakObjectPtr<UObject> TargetObject = NetDriver->PackageMap->GetObjectFromUnrealObjectRef(TargetObjectRef);
	if (!TargetObject.IsValid())
	{
		if (!IsDynamicSubObject(*NetDriver, *Actor, Offset))
		{
			UE_LOG(LogActorSystem, Verbose,
				   TEXT("Tried to apply component data on actor creation for a static subobject that's been deleted, will skip. Entity: "
						"%lld, Component: %d, Actor: %s"),
				   EntityId, ComponentId, *Actor->GetPathName());
			return;
		}

		// If we can't find this subobject, it's a dynamically attached object. Check if we created previously.
		if (UObject* DynamicSubObject = ClientNetLoadActorHelper.GetReusableDynamicSubObject(TargetObjectRef))
		{
			ApplyComponentData(Channel, *DynamicSubObject, ComponentId, Data);
			OutObjectsToResolve.Add(ObjectPtrRefPair(DynamicSubObject, TargetObjectRef));
			return;
		}

		// If the dynamically attached object was not created before. Create it now.
		TargetObject = NewObject<UObject>(Actor, NetDriver->ClassInfoManager->GetClassByComponentId(ComponentId));

		Actor->OnSubobjectCreatedFromReplication(TargetObject.Get());

		NetDriver->PackageMap->ResolveSubobject(TargetObject.Get(), TargetObjectRef);

		Channel.CreateSubObjects.Add(TargetObject.Get());
	}

	FString TargetObjectPath = TargetObject->GetPathName();
	ApplyComponentData(Channel, *TargetObject, ComponentId, Data);

	if (TargetObject.IsValid())
	{
		OutObjectsToResolve.Add(ObjectPtrRefPair(TargetObject.Get(), TargetObjectRef));
	}
	else
	{
		// TODO: remove / downgrade this to a log after verifying we handle this properly - UNR-4379
		UE_LOG(LogActorSystem, Warning, TEXT("Actor subobject got invalidated after applying component data! Subobject: %s"),
			   *TargetObjectPath);
	}
}

USpatialActorChannel* ActorSystem::SetUpActorChannel(USpatialNetDriver* NetDriver, AActor* Actor, const Worker_EntityId EntityId)
{
	UNetConnection* Connection = NetDriver->GetSpatialOSNetConnection();

	if (Connection == nullptr)
	{
		UE_LOG(LogActorSystem, Error,
			   TEXT("Unable to find SpatialOSNetConnection! Has this worker been disconnected from SpatialOS due to a timeout?"));
		return nullptr;
	}

	// Set up actor channel.
	USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId);
	if (Channel == nullptr)
	{
		Channel = Cast<USpatialActorChannel>(Connection->CreateChannelByName(
			NAME_Actor, NetDriver->IsServer() ? EChannelCreateFlags::OpenedLocally : EChannelCreateFlags::None));
	}

	if (Channel != nullptr && Channel->Actor == nullptr)
	{
		Channel->SetChannelActor(Actor, ESetChannelActorFlags::None);
	}

	return Channel;
}

USpatialActorChannel* ActorSystem::SetUpActorChannel(AActor* Actor, Worker_EntityId EntityId)
{
	return SetUpActorChannel(NetDriver, Actor, EntityId);
}

USpatialActorChannel* ActorSystem::TryRestoreActorChannelForStablyNamedActor(AActor* StablyNamedActor, const Worker_EntityId EntityId)
{
	if (!NetDriver->PackageMap->ResolveEntityActorAndSubobjects(EntityId, StablyNamedActor))
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Failed to restore actor channel for stably named actor: failed to resolve actor. Entity: %lld, actor: %s"), EntityId,
			   *StablyNamedActor->GetPathName());
		return nullptr;
	}

	USpatialActorChannel* Channel = SetUpActorChannel(StablyNamedActor, EntityId);
	if (Channel == nullptr)
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Failed to restore actor channel for stably named actor: failed to create channel. Entity: %lld, actor: %s"), EntityId,
			   *StablyNamedActor->GetPathName());
	}

	return Channel;
}

bool ActorSystem::InvokePreNetReceive(UObject& Object)
{
	if (Object.IsPendingKill())
	{
		UE_LOG(LogActorSystem, Log, TEXT("InvokePreNetReceive: Did not invoke PreNetReceive for object %s, as object is pending kill."),
			   *Object.GetName());
		return false;
	}

	// We can have multiple spatial components receiving updates per unreal object but we only want to call PreNetReceive a single time for
	// an object for each tick.
	if (!PostNetReceivesToSend.Contains(FWeakObjectPtr(&Object)))
	{
		UE_LOG(LogActorSystem, VeryVerbose, TEXT("InvokePreNetReceive: Invoking PreNetReceive for object %s."), *Object.GetName());

		Object.PreNetReceive();
		PostNetReceivesToSend.Emplace(FWeakObjectPtr(&Object));
	}
	else
	{
		UE_LOG(LogActorSystem, VeryVerbose,
			   TEXT("InvokePreNetReceive: Not invoking PreNetReceive for object %s as it is already contained within "
					"PostNetReceivesToSend."),
			   *Object.GetName());
	}

	return true;
}

void ActorSystem::InvokePostNetReceives()
{
	for (const FWeakObjectPtr& WeakPtr : PostNetReceivesToSend)
	{
		UObject* Object = WeakPtr.Get();
		if (!Object)
		{
			// An object could have been set to pending kill as a result of the user callback PreNetReceive.
			UE_LOG(LogActorSystem, Log, TEXT("Not sending PostNetReceive for object: %s as it is not valid."), *GetNameSafe(Object));
			continue;
		}

		UE_LOG(LogActorSystem, VeryVerbose, TEXT("Sending PostNetReceive for object %s."), *Object->GetName());

		Object->PostNetReceive();
	}

	PostNetReceivesToSend.Empty();
}

FObjectRepNotifies& ActorSystem::GetObjectRepNotifies(UObject& Object)
{
	return Object.IsA<AActor>() ? ActorRepNotifiesToSend.FindOrAdd(FWeakObjectPtr(&Object))
								: SubobjectRepNotifiesToSend.FindOrAdd(FWeakObjectPtr(&Object));
}

void ActorSystem::InvokeRepNotifies()
{
	// We have two lists of ObjectRepNotifies, one for Actors and one for Subobjects.
	// The reason for this is native always calls Rep Notifies on an actor and then on its subobjects.
	// However, we call Rep Notifies after data on all actors and subobjects has been applied.
	// So, to match native functionality, we simply need to call all Subobject Rep Notifies after all Actor Rep Notifies.
	for (auto& RepNotifiesPair : ActorRepNotifiesToSend)
	{
		TryInvokeRepNotifiesForObject(RepNotifiesPair.Key, RepNotifiesPair.Value);
	}
	for (auto& RepNotifiesPair : SubobjectRepNotifiesToSend)
	{
		TryInvokeRepNotifiesForObject(RepNotifiesPair.Key, RepNotifiesPair.Value);
	}

	ActorRepNotifiesToSend.Empty();
	SubobjectRepNotifiesToSend.Empty();
}

void ActorSystem::TryInvokeRepNotifiesForObject(FWeakObjectPtr& WeakObjectPtr, FObjectRepNotifies& ObjectRepNotifies) const
{
	// Object could have been killed during a RepNotify
	UObject* Object = WeakObjectPtr.Get();
	if (!Object)
	{
		return;
	}
	const Worker_EntityId EntityId = NetDriver->PackageMap->GetEntityIdFromObject(Object);
	if (EntityId == SpatialConstants::INVALID_ENTITY_ID)
	{
		UE_LOG(LogActorSystem, Warning, TEXT("Failed to invoke rep notifies for an object as its entity id was invalid. Object: %s"),
			   *Object->GetName());
		return;
	}
	USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId);
	if (!IsValid(Channel))
	{
		UE_LOG(LogActorSystem, Warning,
			   TEXT("Failed to invoke rep notifies for an object as its channel was invalid. Object: %s, Entity: %lld"), *Object->GetName(),
			   EntityId);
		return;
	}

	ObjectRepNotifies.RepNotifies.Sort([](GDK_PROPERTY(Property) & A, GDK_PROPERTY(Property) & B) -> bool {
		// We want to call RepNotifies on properties with a lower RepIndex earlier.
		return A.RepIndex < B.RepIndex;
	});

	RemoveRepNotifiesWithUnresolvedObjs(*Object, *Channel, ObjectRepNotifies.RepNotifies);
	Channel->InvokeRepNotifies(Object, ObjectRepNotifies.RepNotifies, ObjectRepNotifies.PropertySpanIds);
}

void ActorSystem::RemoveRepNotifiesWithUnresolvedObjs(UObject& Object, const USpatialActorChannel& Channel,
													  TArray<GDK_PROPERTY(Property) *>& RepNotifies)
{
	if (const TSharedRef<FObjectReplicator>* ReplicatorRef = Channel.ReplicationMap.Find(&Object))
	{
		if (const FSpatialObjectRepState* ObjectRepState = Channel.ObjectReferenceMap.Find(&Object))
		{
			FObjectReplicator& Replicator = ReplicatorRef->Get();
			Channel.RemoveRepNotifiesWithUnresolvedObjs(RepNotifies, *Replicator.RepLayout, ObjectRepState->ReferenceMap, &Object);
		}
	}
}

void ActorSystem::CleanUpTornOffChannels()
{
	for (const Worker_EntityId EntityId : EntityChannelsToSetTornOff)
	{
		if (USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(EntityId))
		{
			Channel->ConditionalCleanUp(false, EChannelCloseReason::TearOff);
		}
	}
	EntityChannelsToSetTornOff.Empty();
}

void ActorSystem::ProcessClientInterestUpdates()
{
	for (auto Pair : ClientInterestDirty)
	{
		if (AActor* Actor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(Pair.Key)))
		{
			UpdateClientInterest(Actor, Pair.Value);
		}
		else
		{
			UE_LOG(LogActorSystem, Warning, TEXT("Failed to get actor for entity id to update client interest (%lld)"), Pair.Key);
		}
	}
	ClientInterestDirty.Empty();
}

void ActorSystem::RemoveActor(const Worker_EntityId EntityId)
{
	SCOPE_CYCLE_COUNTER(STAT_ActorSystemRemoveActor);

	TWeakObjectPtr<UObject> WeakActor = NetDriver->PackageMap->GetObjectFromEntityId(EntityId);

	if (ensureMsgf(NetDriver->OwnershipCompletenessHandler,
				   TEXT("OwnershipCompletenessHandler must be valid throughout ActorSystem's lifetime")))
	{
		NetDriver->OwnershipCompletenessHandler->TryRemovePlayerEntity(EntityId);
	}

	// Actor has not been resolved yet or has already been destroyed. Clean up surrounding bookkeeping.
	if (!WeakActor.IsValid())
	{
		DestroyActor(nullptr, EntityId);
		return;
	}

	AActor* Actor = Cast<AActor>(WeakActor.Get());

	UE_LOG(LogActorSystem, Verbose, TEXT("Worker %s Remove Actor: %s %lld"), *NetDriver->Connection->GetWorkerId(),
		   Actor && !Actor->IsPendingKill() ? *Actor->GetName() : TEXT("nullptr"), EntityId);

	// Cleanup pending add components if any exist.
	if (USpatialActorChannel* ActorChannel = NetDriver->GetActorChannelByEntityId(EntityId))
	{
		// If we have any pending subobjects on the channel, remove them
		if (ActorChannel->PendingDynamicSubobjects.Num() > 0)
		{
			PendingDynamicSubobjectComponents.Remove(EntityId);
		}
	}

	// Actor already deleted (this worker was most likely authoritative over it and deleted it earlier).
	if (Actor == nullptr || Actor->IsPendingKill())
	{
		if (USpatialActorChannel* ActorChannel = NetDriver->GetActorChannelByEntityId(EntityId))
		{
			UE_LOG(LogActorSystem, Warning,
				   TEXT("RemoveActor: actor for entity %lld was already deleted (likely on the authoritative worker) but still has an open "
						"actor channel."),
				   EntityId);
			ActorChannel->ConditionalCleanUp(false, EChannelCloseReason::Destroyed);
		}
		return;
	}

	if (USpatialActorChannel* ActorChannel = NetDriver->GetActorChannelByEntityId(EntityId))
	{
		if (NetDriver->GetWorld() != nullptr && !NetDriver->GetWorld()->IsPendingKillOrUnreachable())
		{
			for (UObject* SubObject : ActorChannel->CreateSubObjects)
			{
				if (SubObject)
				{
					FUnrealObjectRef ObjectRef =
						FUnrealObjectRef::FromObjectPtr(SubObject, Cast<USpatialPackageMapClient>(NetDriver->PackageMap));
					// Unmap this object so we can remap it if it becomes relevant again in the future
					MoveMappedObjectToUnmapped(ObjectRef);
				}
			}

			FUnrealObjectRef ObjectRef = FUnrealObjectRef::FromObjectPtr(Actor, Cast<USpatialPackageMapClient>(NetDriver->PackageMap));
			// Unmap this object so we can remap it if it becomes relevant again in the future
			MoveMappedObjectToUnmapped(ObjectRef);
		}

		for (auto& ChannelRefs : ActorChannel->ObjectReferenceMap)
		{
			CleanupRepStateMap(ChannelRefs.Value);
		}

		ActorChannel->ObjectReferenceMap.Empty();

		// If the entity is to be deleted after having been torn off, ignore the request (but clean up the channel if it has not been
		// cleaned up already).
		if (Actor->GetTearOff())
		{
			ActorChannel->ConditionalCleanUp(false, EChannelCloseReason::TearOff);
			return;
		}
	}

	// If the actor was torn off and we don't have a channel, don't destroy the actor.
	if (Actor->GetTearOff())
	{
		return;
	}

	// Actor is a startup actor that is a part of the level. If it's not Tombstone-d, then it
	// has just fallen out of our view and we should only remove the entity.
	if (Actor->IsFullNameStableForNetworking() && !ActorSubView->HasComponent(EntityId, SpatialConstants::TOMBSTONE_COMPONENT_ID))
	{
		ClientNetLoadActorHelper.EntityRemoved(EntityId, *Actor);
		// We can't call CleanupDeletedEntity here as we need the NetDriver to maintain the EntityId
		// to Actor Channel mapping for the DestroyActor to function correctly
		NetDriver->PackageMap->RemoveEntityActor(EntityId);
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(Actor))
	{
		// Force APlayerController::DestroyNetworkActorHandled to return false
		PC->Player = nullptr;
	}

	// Workaround for camera loss on handover: prevent UnPossess() (non-authoritative destruction of pawn, while being authoritative over
	// the controller)
	// TODO: Check how AI controllers are affected by this (UNR-430)
	// TODO: This should be solved properly by working sets (UNR-411)
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		AController* Controller = Pawn->Controller;

		if (Controller && Controller->HasAuthority())
		{
			Pawn->Controller = nullptr;
		}
	}

	DestroyActor(Actor, EntityId);
}

Worker_ComponentData ActorSystem::CreateLevelComponentData(const AActor& Actor, const UWorld& NetDriverWorld,
														   const USpatialClassInfoManager& ClassInfoManager)
{
	UWorld* ActorWorld = Actor.GetTypedOuter<UWorld>();
	if (ActorWorld != &NetDriverWorld)
	{
		const uint32 ComponentId = ClassInfoManager.GetComponentIdFromLevelPath(ActorWorld->GetOuter()->GetPathName());
		if (ComponentId != SpatialConstants::INVALID_COMPONENT_ID)
		{
			return SpatialGDK::ComponentFactory::CreateEmptyComponentData(ComponentId);
		}
		UE_LOG(LogActorSystem, Error,
			   TEXT("Could not find Streaming Level Component for Level %s, processing Actor %s. Have you generated schema?"),
			   *ActorWorld->GetOuter()->GetPathName(), *Actor.GetPathName());
	}

	return SpatialGDK::ComponentFactory::CreateEmptyComponentData(SpatialConstants::NOT_STREAMED_COMPONENT_ID);
}

void ActorSystem::CreateTombstoneEntity(AActor* Actor)
{
	check(Actor->IsNetStartupActor());

	const Worker_EntityId EntityId = NetDriver->PackageMap->AllocateEntityIdAndResolveActor(Actor);

	if (EntityId == SpatialConstants::INVALID_ENTITY_ID)
	{
		// This shouldn't happen, but as a precaution, error and return instead of attempting to create an entity with ID 0.
		UE_LOG(LogActorSystem, Error, TEXT("Failed to tombstone actor, no entity ids available. Actor: %s."), *Actor->GetName());
		return;
	}

	EntityFactory DataFactory(NetDriver, NetDriver->PackageMap, NetDriver->ClassInfoManager, NetDriver->GetRPCService());
	TArray<FWorkerComponentData> Components = DataFactory.CreateTombstoneEntityComponents(Actor);

	Components.Add(CreateLevelComponentData(*Actor, *NetDriver->GetWorld(), *NetDriver->ClassInfoManager));

	CreateEntityWithRetries(EntityId, Actor->GetName(), MoveTemp(Components));

	UE_LOG(LogActorSystem, Log,
		   TEXT("Creating tombstone entity for actor. "
				"Actor: %s. Entity ID: %d."),
		   *Actor->GetName(), EntityId);

#if WITH_EDITOR
	NetDriver->TrackTombstone(EntityId);
#endif
}

void ActorSystem::RetireEntity(Worker_EntityId EntityId, bool bIsNetStartupActor) const
{
	if (bIsNetStartupActor)
	{
		NetDriver->ActorSystem->RemoveActor(EntityId);
		// In the case that this is a startup actor, we won't actually delete the entity in SpatialOS.  Instead we'll Tombstone it.
		if (!ActorSubView->HasComponent(EntityId, SpatialConstants::TOMBSTONE_COMPONENT_ID))
		{
			UE_LOG(LogActorSystem, Log, TEXT("Adding tombstone to entity: %lld"), EntityId);
			AddTombstoneToEntity(EntityId);
		}
		else
		{
			UE_LOG(LogActorSystem, Verbose, TEXT("RetireEntity called on already retired entity: %lld"), EntityId);
		}
	}
	else
	{
		// Actor no longer guaranteed to be in package map, but still useful for additional logging info
		AActor* Actor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId));

		UE_LOG(LogActorSystem, Log, TEXT("Sending delete entity request for %s with EntityId %lld, HasServerAuthority: %d"),
			   *GetPathNameSafe(Actor), EntityId, NetDriver->HasServerAuthority(EntityId));

		if (EventTracer != nullptr)
		{
			FSpatialGDKSpanId SpanId = EventTracer->TraceEvent(SEND_RETIRE_ENTITY_EVENT_NAME, "", /* Causes */ nullptr, /* NumCauses */ 0,
															   [Actor, EntityId](FSpatialTraceEventDataBuilder& EventBuilder) {
																   EventBuilder.AddObject(Actor);
																   EventBuilder.AddEntityId(EntityId);
															   });
		}

		NetDriver->Connection->SendDeleteEntityRequest(EntityId, RETRY_UNTIL_COMPLETE);
	}
}

void ActorSystem::SendComponentUpdates(UObject* Object, const FClassInfo& Info, USpatialActorChannel* Channel,
									   const FRepChangeState* RepChanges, uint32& OutBytesWritten)
{
	SCOPE_CYCLE_COUNTER(STAT_ActorSystemSendComponentUpdates);
	const Worker_EntityId EntityId = Channel->GetEntityId();

	// It's not clear if this is ever valid for authority to not be true anymore (since component sets), but still possible if we attempt
	// to process updates whilst an entity creation is in progress, or after the entity has been deleted or removed from view. So in the
	// meantime we've kept the checking with an error message.
	if (!NetDriver->HasServerAuthority(EntityId))
	{
		UE_LOG(LogActorSystem, Error, TEXT("Trying to send component update but don't have authority! entity: %lld"), EntityId);
		return;
	}

	UE_LOG(LogActorSystem, Verbose, TEXT("Sending component update (object: %s, entity: %lld)"), *Object->GetName(), EntityId);

	ComponentFactory UpdateFactory(Channel->GetInterestDirty(), NetDriver);

	TArray<FWorkerComponentUpdate> ComponentUpdates =
		UpdateFactory.CreateComponentUpdates(Object, Info, EntityId, RepChanges, OutBytesWritten);

	TArray<FSpatialGDKSpanId> PropertySpans;
	if (EventTracer != nullptr && RepChanges != nullptr
		&& RepChanges->RepChanged.Num() > 0) // Only need to add these if they are actively being traced
	{
		// If the stack is empty we want to create a trace event such that it is a root event. This means giving it no causes.
		// If the stack has an item, an event was created in project space and should be used as the cause of these "send property" events.
		const bool bStackEmpty = EventTracer->IsObjectStackEmpty();
		const int32 NumCauses = bStackEmpty ? 0 : 1;
		const Trace_SpanIdType* Causes = bStackEmpty ? nullptr : EventTracer->PopLatentPropertyUpdateSpanId(Object).GetConstId();
		for (FChangeListPropertyIterator Itr(RepChanges); Itr; ++Itr)
		{
			FSpatialGDKSpanId PropertySpan = EventTracer->TraceEvent(
				PROPERTY_CHANGED_EVENT_NAME, "", Causes, NumCauses, [Object, EntityId, Itr](FSpatialTraceEventDataBuilder& EventBuilder) {
					GDK_PROPERTY(Property)* Property = *Itr;
					EventBuilder.AddObject(Object);
					EventBuilder.AddEntityId(EntityId);
					EventBuilder.AddKeyValue("property_name", Property->GetName());
					EventBuilder.AddLinearTraceId(EventTraceUniqueId::GenerateForProperty(EntityId, Property));
				});
			PropertySpans.Push(PropertySpan);
		}
	}

	for (int i = 0; i < ComponentUpdates.Num(); i++)
	{
		FWorkerComponentUpdate& Update = ComponentUpdates[i];

		FSpatialGDKSpanId SpanId;
		if (EventTracer != nullptr)
		{
			const Trace_SpanIdType* Causes = (const Trace_SpanIdType*)PropertySpans.GetData();
			SpanId = EventTracer->TraceEvent(SEND_PROPERTY_UPDATE_EVENT_NAME, "", Causes, PropertySpans.Num(),
											 [Object, EntityId, Update](FSpatialTraceEventDataBuilder& EventBuilder) {
												 EventBuilder.AddObject(Object);
												 EventBuilder.AddEntityId(EntityId);
												 EventBuilder.AddComponentId(Update.component_id);
											 });
		}

		NetDriver->Connection->SendComponentUpdate(EntityId, &Update, SpanId);
	}
}

void ActorSystem::SendActorTornOffUpdate(const Worker_EntityId EntityId, const Worker_ComponentId ComponentId) const
{
	FWorkerComponentUpdate ComponentUpdate = {};

	ComponentUpdate.component_id = ComponentId;
	ComponentUpdate.schema_type = Schema_CreateComponentUpdate();
	Schema_Object* ComponentObject = Schema_GetComponentUpdateFields(ComponentUpdate.schema_type);

	Schema_AddBool(ComponentObject, SpatialConstants::ACTOR_TEAROFF_ID, 1);

	NetDriver->Connection->SendComponentUpdate(EntityId, &ComponentUpdate);
}

void ActorSystem::ProcessPositionUpdates()
{
	for (auto& Channel : ChannelsToUpdatePosition)
	{
		if (Channel.IsValid())
		{
			Channel->UpdateSpatialPosition();
		}
	}

	ChannelsToUpdatePosition.Empty();
}

void ActorSystem::RegisterChannelForPositionUpdate(USpatialActorChannel* Channel)
{
	ChannelsToUpdatePosition.Add(Channel);
}

void ActorSystem::UpdateInterestComponent(AActor* Actor)
{
	SCOPE_CYCLE_COUNTER(STAT_ActorSystemUpdateInterestComponent);

	Worker_EntityId EntityId = NetDriver->PackageMap->GetEntityIdFromObject(Actor);
	if (EntityId == SpatialConstants::INVALID_ENTITY_ID)
	{
		UE_LOG(LogActorSystem, Verbose, TEXT("Attempted to update interest for non replicated actor: %s"), *GetNameSafe(Actor));
		return;
	}

	FWorkerComponentUpdate Update =
		NetDriver->InterestFactory->CreateInterestUpdate(Actor, NetDriver->ClassInfoManager->GetOrCreateClassInfoByObject(Actor), EntityId);

	NetDriver->Connection->SendComponentUpdate(EntityId, &Update);
}

void ActorSystem::UpdateClientInterest(AActor* Actor, const bool bOverwrite)
{
	if (Actor->IsA(APlayerController::StaticClass()) && GetDefault<USpatialGDKSettings>()->bUseClientEntityInterestQueries)
	{
		Worker_CommandRequest CommandRequest{};
		const bool bRequestValid = NetDriver->InterestFactory->CreateClientInterestDiff(Actor, CommandRequest, bOverwrite);

		if (bRequestValid)
		{
			const Worker_EntityId SystemEntityId =
				Cast<USpatialNetConnection>(Actor->GetNetConnection())->ConnectionClientWorkerSystemEntityId;

			NetDriver->Connection->SendCommandRequest(SystemEntityId, &CommandRequest, RETRY_MAX_TIMES, {});

			UE_LOG(LogActorSystem, Log, TEXT("Interest diff: worker entity id %lld"), SystemEntityId);
		}
		else
		{
			UE_LOG(LogActorSystem, Warning, TEXT("Interest diff: refresh request but no diff detected (%s)"), *Actor->GetName());
		}
	}
	else
	{
		UE_LOG(LogActorSystem, Warning, TEXT("Called UpdateClientInterst on non-PlayerController (%s)"), *Actor->GetName());
	}
}

void ActorSystem::SendInterestBucketComponentChange(Worker_EntityId EntityId, Worker_ComponentId OldComponent,
													Worker_ComponentId NewComponent) const
{
	if (OldComponent != SpatialConstants::INVALID_COMPONENT_ID)
	{
		NetDriver->Connection->SendRemoveComponent(EntityId, OldComponent);
	}

	if (NewComponent != SpatialConstants::INVALID_COMPONENT_ID)
	{
		FWorkerComponentData Data = ComponentFactory::CreateEmptyComponentData(NewComponent);
		NetDriver->Connection->SendAddComponent(EntityId, &Data);
	}
}

void ActorSystem::SendAddComponentForSubobject(USpatialActorChannel* Channel, UObject* Subobject, const FClassInfo& SubobjectInfo,
											   uint32& OutBytesWritten)
{
	FRepChangeState SubobjectRepChanges = Channel->CreateInitialRepChangeState(Subobject);

	ComponentFactory DataFactory(false, NetDriver);

	TArray<FWorkerComponentData> SubobjectDatas =
		DataFactory.CreateComponentDatas(Subobject, SubobjectInfo, SubobjectRepChanges, OutBytesWritten);
	SendAddComponents(Channel->GetEntityId(), SubobjectDatas);

	Channel->PendingDynamicSubobjects.Remove(TWeakObjectPtr<UObject>(Subobject));
}

void ActorSystem::SendRemoveComponentForClassInfo(Worker_EntityId EntityId, const FClassInfo& Info)
{
	TArray<Worker_ComponentId> ComponentsToRemove;
	ComponentsToRemove.Reserve(SCHEMA_Count);
	for (Worker_ComponentId SubobjectComponentId : Info.SchemaComponents)
	{
		if (ActorSubView->GetView()[EntityId].Components.ContainsByPredicate(ComponentIdEquality{ SubobjectComponentId }))
		{
			ComponentsToRemove.Add(SubobjectComponentId);
		}
	}

	SendRemoveComponents(EntityId, ComponentsToRemove);

	NetDriver->PackageMap->RemoveSubobject(FUnrealObjectRef(EntityId, Info.SchemaComponents[SCHEMA_Data]));
}

void ActorSystem::SendCreateEntityRequest(USpatialActorChannel& ActorChannel, uint32& OutBytesWritten)
{
	AActor* Actor = ActorChannel.Actor;
	const Worker_EntityId EntityId = ActorChannel.GetEntityId();
	UE_LOG(LogActorSystem, Log, TEXT("Sending create entity request for %s with EntityId %lld, HasAuthority: %d"), *Actor->GetName(),
		   ActorChannel.GetEntityId(), Actor->HasAuthority());

	EntityFactory DataFactory(NetDriver, NetDriver->PackageMap, NetDriver->ClassInfoManager, NetDriver->RPCService.Get());
	TArray<FWorkerComponentData> ComponentDatas;
	TArray<FWorkerComponentUpdate> ComponentUpdates;

	const EntityViewElement* ExistingEntity = ActorSubView->GetView().Find(ActorChannel.GetEntityId());

	const bool bIsSkeletonEntity =
		ExistingEntity != nullptr
		&& ExistingEntity->Components.ContainsByPredicate(ComponentIdEquality{ SpatialConstants::SKELETON_ENTITY_QUERY_TAG_COMPONENT_ID });

	// If we're not populating a skeleton entity, we're creating a new one.
	const bool bIsCreatingNewEntity = !bIsSkeletonEntity;

	if (bIsCreatingNewEntity)
	{
		ComponentDatas = DataFactory.CreateEntityComponents(&ActorChannel, OutBytesWritten);
	}
	else
	{
		DataFactory.CreatePopulateSkeletonComponents(ActorChannel, ComponentDatas, ComponentUpdates, OutBytesWritten);
	}

	// If the Actor was loaded rather than dynamically spawned, associate it with its owning sublevel.
	ComponentDatas.Add(CreateLevelComponentData(*Actor, *NetDriver->GetWorld(), *NetDriver->ClassInfoManager));

	FSpatialGDKSpanId SpanId;
	if (EventTracer != nullptr)
	{
		SpanId = EventTracer->TraceEvent(SEND_CREATE_ENTITY_EVENT_NAME, "", /* Causes */ nullptr, /* NumCauses */ 0,
										 [Actor, EntityId](FSpatialTraceEventDataBuilder& EventBuilder) {
											 EventBuilder.AddObject(Actor);
											 EventBuilder.AddEntityId(EntityId);
										 });
	}

	ViewCoordinator& Coordinator = NetDriver->Connection->GetCoordinator();

	if (bIsCreatingNewEntity)
	{
		const Worker_RequestId CreateEntityRequestId =
			NetDriver->Connection->SendCreateEntityRequest(MoveTemp(ComponentDatas), &EntityId, SpatialGDK::RETRY_UNTIL_COMPLETE, SpanId);

		CommandsHandler.AddRequest(CreateEntityRequestId, [this, SpanId](const Worker_CreateEntityResponseOp& Op) {
			OnEntityCreated(Op, SpanId);
		});

		if (ensure(EntityId != SpatialConstants::INVALID_ENTITY_ID))
		{
			CreateEntityRequestsInFlight.Add(EntityId);
		}
		CreateEntityRequestIdToActorChannel.Emplace(CreateEntityRequestId, MakeWeakObjectPtr(&ActorChannel));
	}
	else
	{
		for (const FWorkerComponentUpdate& ComponentToUpdate : ComponentUpdates)
		{
			ComponentUpdate GeneratedUpdate(OwningComponentUpdatePtr(ComponentToUpdate.schema_type), ComponentToUpdate.component_id);
			Coordinator.SendComponentUpdate(EntityId, MoveTemp(GeneratedUpdate), {});
		}
		for (const FWorkerComponentData& ComponentToAdd : ComponentDatas)
		{
			ComponentData GeneratedAdd = ComponentData(OwningComponentDataPtr(ComponentToAdd.schema_type), ComponentToAdd.component_id);
			Coordinator.SendAddComponent(EntityId, MoveTemp(GeneratedAdd), {});
		}

		// Add this last; due to AddComponent ordering, seeing the POPULATION_FINISHED tag would mean that all the previous ComponentAdds
		// have been observed.
		Coordinator.SendAddComponent(EntityId, ComponentData(SpatialConstants::SKELETON_ENTITY_POPULATION_FINISHED_TAG_COMPONENT_ID));

		Coordinator.RefreshEntityCompleteness(EntityId);
	}
}

bool ActorSystem::HasPendingOpsForChannel(const USpatialActorChannel& ActorChannel) const
{
	const bool bHasUnresolvedObjects = Algo::AnyOf(
		ActorChannel.ObjectReferenceMap, [&ActorChannel](const TPair<TWeakObjectPtr<UObject>, FSpatialObjectRepState>& ObjectReference) {
			return ObjectReference.Value.HasUnresolved();
		});

	if (bHasUnresolvedObjects)
	{
		return true;
	}

	const bool bHasPendingCreateEntityRequests = Algo::AnyOf(
		CreateEntityRequestIdToActorChannel, [&ActorChannel](const TPair<Worker_RequestId_Key, TWeakObjectPtr<USpatialActorChannel>>& It) {
			return It.Value == &ActorChannel;
		});

	return bHasPendingCreateEntityRequests;
}

bool ActorSystem::IsCreateEntityRequestInFlight(Worker_EntityId EntityId) const
{
	return CreateEntityRequestsInFlight.Contains(EntityId);
}

void ActorSystem::OnEntityCreated(const Worker_CreateEntityResponseOp& Op, FSpatialGDKSpanId CreateOpSpan)
{
	CreateEntityRequestsInFlight.Remove(Op.entity_id);

	TWeakObjectPtr<USpatialActorChannel> BoundActorChannel;

	if (!ensure(CreateEntityRequestIdToActorChannel.RemoveAndCopyValue(Op.request_id, BoundActorChannel)))
	{
		return;
	}

	if (!ensure(BoundActorChannel.IsValid()))
	{
		// The channel was destroyed before the response reached this worker.
		return;
	}

	USpatialActorChannel& Channel = *BoundActorChannel.Get();

	AActor* Actor = Channel.Actor;
	const Worker_EntityId EntityId = Channel.GetEntityId();
	ensure(EntityId != SpatialConstants::INVALID_ENTITY_ID);

	if (EventTracer != nullptr)
	{
		EventTracer->TraceEvent(RECEIVE_CREATE_ENTITY_SUCCESS_EVENT_NAME, "", CreateOpSpan.GetConstId(), /* NumCauses */ 1,
								[Actor, EntityId](FSpatialTraceEventDataBuilder& EventBuilder) {
									EventBuilder.AddObject(Actor);
									EventBuilder.AddEntityId(EntityId);
								});
	}

	check(NetDriver->GetNetMode() < NM_Client);

	if (Actor == nullptr || Actor->IsPendingKill())
	{
		UE_LOG(LogActorSystem, Log, TEXT("Actor is invalid after trying to create entity"));
		return;
	}

	// True if the entity is in the worker's view.
	// If this is the case then we know the entity was created and do not need to retry if the request timed-out.
	const bool bEntityIsInView = ActorSubView->HasEntity(EntityId);

	switch (static_cast<Worker_StatusCode>(Op.status_code))
	{
	case WORKER_STATUS_CODE_SUCCESS:
		UE_LOG(LogActorSystem, Verbose,
			   TEXT("Create entity request succeeded. "
					"Actor %s, request id: %d, entity id: %lld, message: %s"),
			   *Actor->GetName(), Op.request_id, Op.entity_id, UTF8_TO_TCHAR(Op.message));
		ensure(EntityId == Op.entity_id);
		break;
	case WORKER_STATUS_CODE_TIMEOUT:
		if (bEntityIsInView)
		{
			UE_LOG(LogActorSystem, Log,
				   TEXT("Create entity request failed but the entity was already in view. "
						"Actor %s, request id: %d, entity id: %lld, message: %s"),
				   *Actor->GetName(), Op.request_id, Op.entity_id, UTF8_TO_TCHAR(Op.message));
		}
		else
		{
			UE_LOG(LogActorSystem, Warning,
				   TEXT("Create entity request timed out. Retrying. "
						"Actor %s, request id: %d, entity id: %lld, message: %s"),
				   *Actor->GetName(), Op.request_id, Op.entity_id, UTF8_TO_TCHAR(Op.message));

			// TODO: UNR-664 - Track these bytes written to use in saturation.
			uint32 BytesWritten = 0;
			SendCreateEntityRequest(Channel, BytesWritten);
		}
		break;
	case WORKER_STATUS_CODE_APPLICATION_ERROR:
		if (bEntityIsInView)
		{
			UE_LOG(LogActorSystem, Log,
				   TEXT("Create entity request failed as the entity already exists and is in view. "
						"Actor %s, request id: %d, entity id: %lld, message: %s"),
				   *Actor->GetName(), Op.request_id, Op.entity_id, UTF8_TO_TCHAR(Op.message));
		}
		else
		{
			UE_LOG(LogActorSystem, Warning,
				   TEXT("Create entity request failed."
						"Either the reservation expired, the entity already existed, or the entity was invalid. "
						"Actor %s, request id: %d, entity id: %lld, message: %s"),
				   *Actor->GetName(), Op.request_id, Op.entity_id, UTF8_TO_TCHAR(Op.message));
		}
		break;
	default:
		UE_LOG(LogActorSystem, Error,
			   TEXT("Create entity request failed. This likely indicates a bug in the Unreal GDK and should be reported."
					"Actor %s, request id: %d, entity id: %lld, message: %s"),
			   *Actor->GetName(), Op.request_id, Op.entity_id, UTF8_TO_TCHAR(Op.message));
		break;
	}

	if (static_cast<Worker_StatusCode>(Op.status_code) == WORKER_STATUS_CODE_SUCCESS && Actor->IsA<APlayerController>())
	{
		// With USLB, we want the client worker that results in the spawning of a PlayerController to claim the
		// PlayerController entity as a partition entity so the client can become authoritative over necessary
		// components (such as client RPC endpoints, player controller component, etc).
		const Worker_EntityId ClientSystemEntityId = SpatialGDK::GetConnectionOwningClientSystemEntityId(Cast<APlayerController>(Actor));
		check(ClientSystemEntityId != SpatialConstants::INVALID_ENTITY_ID);
		CommandsHandler.ClaimPartition(NetDriver->Connection->GetCoordinator(), ClientSystemEntityId, Op.entity_id);
	}

	if (static_cast<Worker_StatusCode>(Op.status_code) == WORKER_STATUS_CODE_SUCCESS && EntitiesMapToRefreshDormancy.Contains(EntityId))
	{
		bool bMakeDormant = EntitiesMapToRefreshDormancy[EntityId];
		EntitiesMapToRefreshDormancy.Remove(EntityId);
		NetDriver->RefreshActorDormancy(Actor, bMakeDormant);
	}
}

void ActorSystem::DestroyActor(AActor* Actor, const Worker_EntityId EntityId)
{
	// Clean up the actor channel. For clients, this will also call destroy on the actor.
	if (USpatialActorChannel* ActorChannel = NetDriver->GetActorChannelByEntityId(EntityId))
	{
		ActorChannel->ConditionalCleanUp(false, EChannelCloseReason::Destroyed);
	}
	else
	{
		if (NetDriver->IsDormantEntity(EntityId))
		{
			NetDriver->PackageMap->RemoveEntityActor(EntityId);
		}
		else
		{
			UE_LOG(LogActorSystem, Verbose,
				   TEXT("Removing actor as a result of a remove entity op, which has a missing actor channel. Actor: %s EntityId: %lld"),
				   *GetNameSafe(Actor), EntityId);
		}
	}

	if (APlayerController* PC = Cast<APlayerController>(Actor))
	{
		NetDriver->CleanUpServerConnectionForPC(PC);
	}

	// It is safe to call AActor::Destroy even if the destruction has already started.
	if (Actor != nullptr && !Actor->Destroy(true))
	{
		UE_LOG(LogActorSystem, Error, TEXT("Failed to destroy actor in RemoveActor %s %lld"), *Actor->GetName(), EntityId);
	}

	check(NetDriver->PackageMap->GetObjectFromEntityId(EntityId) == nullptr);
}

void ActorSystem::MoveMappedObjectToUnmapped(const FUnrealObjectRef& Ref)
{
	if (TSet<FChannelObjectPair>* RepStatesWithMappedRef = ObjectRefToRepStateMap.Find(Ref))
	{
		for (const FChannelObjectPair& ChannelObject : *RepStatesWithMappedRef)
		{
			if (USpatialActorChannel* Channel = ChannelObject.Key.Get())
			{
				if (FSpatialObjectRepState* RepState = Channel->ObjectReferenceMap.Find(ChannelObject.Value))
				{
					RepState->MoveMappedObjectToUnmapped(Ref);
				}
			}
		}
	}
}

void ActorSystem::CleanupRepStateMap(FSpatialObjectRepState& RepState)
{
	for (const FUnrealObjectRef& Ref : RepState.ReferencedObj)
	{
		TSet<FChannelObjectPair>* RepStatesWithMappedRef = ObjectRefToRepStateMap.Find(Ref);
		if (ensureMsgf(RepStatesWithMappedRef,
					   TEXT("Ref to entity %lld on object %s is missing its referenced entry in the Ref/RepState map"), Ref.Entity,
					   *GetObjectNameFromRepState(RepState)))
		{
			checkf(RepStatesWithMappedRef->Contains(RepState.GetChannelObjectPair()),
				   TEXT("Ref to entity %lld on object %s is missing its referenced entry in the Ref/RepState map"), Ref.Entity,
				   *GetObjectNameFromRepState(RepState));
			RepStatesWithMappedRef->Remove(RepState.GetChannelObjectPair());
			if (RepStatesWithMappedRef->Num() == 0)
			{
				ObjectRefToRepStateMap.Remove(Ref);
			}
		}
	}
}

FString ActorSystem::GetObjectNameFromRepState(const FSpatialObjectRepState& RepState)
{
	if (UObject* Obj = RepState.GetChannelObjectPair().Value.Get())
	{
		return Obj->GetName();
	}
	return TEXT("<unknown>");
}

void ActorSystem::CreateEntityWithRetries(Worker_EntityId EntityId, FString EntityName, TArray<FWorkerComponentData> EntityComponents)
{
	const Worker_RequestId RequestId =
		NetDriver->Connection->SendCreateEntityRequest(CopyEntityComponentData(EntityComponents), &EntityId, RETRY_UNTIL_COMPLETE);

	FCreateEntityDelegate Delegate = [this, EntityId, Name = MoveTemp(EntityName),
									  Components = MoveTemp(EntityComponents)](const Worker_CreateEntityResponseOp& Op) mutable {
		switch (Op.status_code)
		{
		case WORKER_STATUS_CODE_SUCCESS:
			UE_LOG(LogActorSystem, Log,
				   TEXT("Created entity. "
						"Entity name: %s, entity id: %lld"),
				   *Name, EntityId);
			DeleteEntityComponentData(Components);
			break;
		case WORKER_STATUS_CODE_TIMEOUT:
			UE_LOG(LogActorSystem, Log,
				   TEXT("Timed out creating entity. Retrying. "
						"Entity name: %s, entity id: %lld"),
				   *Name, EntityId);
			CreateEntityWithRetries(EntityId, MoveTemp(Name), MoveTemp(Components));
			break;
		default:
			UE_LOG(LogActorSystem, Log,
				   TEXT("Failed to create entity. It might already be created. Not retrying. "
						"Entity name: %s, entity id: %lld"),
				   *Name, EntityId);
			DeleteEntityComponentData(Components);
			break;
		}
	};

	CommandsHandler.AddRequest(RequestId, MoveTemp(Delegate));
}

TArray<FWorkerComponentData> ActorSystem::CopyEntityComponentData(const TArray<FWorkerComponentData>& EntityComponents)
{
	TArray<FWorkerComponentData> Copy;
	Copy.Reserve(EntityComponents.Num());
	for (const FWorkerComponentData& Component : EntityComponents)
	{
		Copy.Emplace(
			Worker_ComponentData{ Component.reserved, Component.component_id, Schema_CopyComponentData(Component.schema_type), nullptr });
	}

	return Copy;
}

void ActorSystem::DeleteEntityComponentData(TArray<FWorkerComponentData>& EntityComponents)
{
	for (FWorkerComponentData& Component : EntityComponents)
	{
		Schema_DestroyComponentData(Component.schema_type);
	}

	EntityComponents.Empty();
}

void ActorSystem::AddTombstoneToEntity(Worker_EntityId EntityId) const
{
	if (!ensureAlwaysMsgf(ActorSubView->HasAuthority(EntityId, SpatialConstants::SERVER_AUTH_COMPONENT_SET_ID),
						  TEXT("Trying to add tombstone to entity without authority")))
	{
		return;
	}

	FWorkerComponentData TombstoneData = Tombstone().CreateComponentData();
	NetDriver->Connection->SendAddComponent(EntityId, &TombstoneData);

	NetDriver->Connection->GetCoordinator().RefreshEntityCompleteness(EntityId);

#if WITH_EDITOR
	NetDriver->TrackTombstone(EntityId);
#endif
}

void ActorSystem::SendAddComponents(Worker_EntityId EntityId, TArray<FWorkerComponentData> ComponentDatas) const
{
	if (ComponentDatas.Num() == 0)
	{
		return;
	}

	for (FWorkerComponentData& ComponentData : ComponentDatas)
	{
		NetDriver->Connection->SendAddComponent(EntityId, &ComponentData);
	}
}

void ActorSystem::SendRemoveComponents(Worker_EntityId EntityId, TArray<Worker_ComponentId> ComponentIds) const
{
	for (auto ComponentId : ComponentIds)
	{
		NetDriver->Connection->SendRemoveComponent(EntityId, ComponentId);
	}
}

} // namespace SpatialGDK
