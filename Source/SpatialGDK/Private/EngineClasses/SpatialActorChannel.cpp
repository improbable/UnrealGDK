// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialActorChannel.h"

#include "Engine/DemoNetDriver.h"
#include "GameFramework/PlayerState.h"
#include "Net/DataBunch.h"
#include "Net/NetworkProfiler.h"

#include "EngineClasses/SpatialNetConnection.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "Interop/SpatialSender.h"
#include "Interop/SpatialReceiver.h"
#include "Interop/GlobalStateManager.h"
#include "SpatialConstants.h"
#include "Utils/EntityRegistry.h"

DEFINE_LOG_CATEGORY(LogSpatialGDKActorChannel);

namespace
{
//This is a bookkeeping function that is similar to the one in RepLayout.cpp, modified for our needs (e.g. no NaKs)
// We can't use the one in RepLayout.cpp because it's private and it cannot account for our approach.
// In this function, we poll for any changes in Unreal properties compared to the last time we replicated this actor.
void UpdateChangelistHistory(FRepState * RepState)
{
	check(RepState->HistoryEnd >= RepState->HistoryStart);

	const int32 HistoryCount = RepState->HistoryEnd - RepState->HistoryStart;
	check(HistoryCount < FRepState::MAX_CHANGE_HISTORY);

	for (int32 i = RepState->HistoryStart; i < RepState->HistoryEnd; i++)
	{
		const int32 HistoryIndex = i % FRepState::MAX_CHANGE_HISTORY;

		FRepChangedHistory & HistoryItem = RepState->ChangeHistory[HistoryIndex];

		check(HistoryItem.Changed.Num() > 0);		// All active history items should contain a change list

		HistoryItem.Changed.Empty();
		HistoryItem.OutPacketIdRange = FPacketIdRange();
		RepState->HistoryStart++;
	}

	// Remove any tiling in the history markers to keep them from wrapping over time
	const int32 NewHistoryCount = RepState->HistoryEnd - RepState->HistoryStart;

	check(NewHistoryCount <= FRepState::MAX_CHANGE_HISTORY);

	RepState->HistoryStart = RepState->HistoryStart % FRepState::MAX_CHANGE_HISTORY;
	RepState->HistoryEnd = RepState->HistoryStart + NewHistoryCount;
}
}

USpatialActorChannel::USpatialActorChannel(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
	, EntityId(0)
	, NetDriver(nullptr)
{
	bCoreActor = true;
	bCreatingNewEntity = false;
}

void USpatialActorChannel::Init(UNetConnection* InConnection, int32 ChannelIndex, bool bOpenedLocally)
{
	Super::Init(InConnection, ChannelIndex, bOpenedLocally);

	NetDriver = Cast<USpatialNetDriver>(Connection->Driver);
	check(NetDriver);
	Sender = NetDriver->Sender;
	Receiver = NetDriver->Receiver;
}

void USpatialActorChannel::DeleteEntityIfAuthoritative()
{
	if (NetDriver->Connection == nullptr)
	{
		return;
	}

	bool bHasAuthority = NetDriver->IsAuthoritativeDestructionAllowed() && NetDriver->View->GetAuthority(EntityId, Position::ComponentId) == WORKER_AUTHORITY_AUTHORITATIVE;

	UE_LOG(LogTemp, Log, TEXT("Delete entity request on %lld. Has authority: %d"), EntityId, (int)bHasAuthority);

	// If we have authority and aren't trying to delete a critical entity, delete it
	if (bHasAuthority && !IsSingletonEntity())
	{
		Sender->SendDeleteEntityRequest(EntityId);
		Receiver->CleanupDeletedEntity(EntityId);
	}
}

bool USpatialActorChannel::IsSingletonEntity()
{
	// Don't delete if singleton entity
	return NetDriver->GlobalStateManager->IsSingletonEntity(EntityId);
}

bool USpatialActorChannel::CleanUp(const bool bForDestroy)
{
#if WITH_EDITOR
	if (NetDriver->IsServer() &&
		NetDriver->GetWorld()->WorldType == EWorldType::PIE &&
		NetDriver->GetEntityRegistry()->GetActorFromEntityId(EntityId))
	{
		// If we're running in PIE, as a server worker, and the entity hasn't already been cleaned up, delete it on shutdown.
		DeleteEntityIfAuthoritative();
	}
#endif

	return UActorChannel::CleanUp(bForDestroy);
}

void USpatialActorChannel::Close()
{
	DeleteEntityIfAuthoritative();
	Super::Close();
}

bool USpatialActorChannel::IsDynamicArrayHandle(UObject* Object, uint16 Handle)
{
	check(ObjectHasReplicator(Object));
	FObjectReplicator& Replicator = FindOrCreateReplicator(Object).Get();
	TSharedPtr<FRepLayout>& RepLayout = Replicator.RepLayout;
	check(Handle - 1 < RepLayout->BaseHandleToCmdIndex.Num());
	return RepLayout->Cmds[RepLayout->BaseHandleToCmdIndex[Handle - 1].CmdIndex].Type == REPCMD_DynamicArray;
}

FRepChangeState USpatialActorChannel::CreateInitialRepChangeState(UObject* Object)
{
	FObjectReplicator& Replicator = FindOrCreateReplicator(Object).Get();

	TArray<uint16> InitialRepChanged;

	int32 DynamicArrayDepth = 0;
	const int32 CmdCount = Replicator.RepLayout->Cmds.Num();
	for (uint16 CmdIdx = 0; CmdIdx < CmdCount; ++CmdIdx)
	{
		const auto& Cmd = Replicator.RepLayout->Cmds[CmdIdx];

		InitialRepChanged.Add(Cmd.RelativeHandle);

		if (Cmd.Type == REPCMD_DynamicArray)
		{
			DynamicArrayDepth++;

			// For the first layer of each dynamic array encountered at the root level
			// add the number of array properties to conform to Unreal's RepLayout design and 
			// allow FRepHandleIterator to jump over arrays. Cmd.EndCmd is an index into 
			// RepLayout->Cmds[] that points to the value after the termination NULL of this array.
			if (DynamicArrayDepth == 1)
			{
				InitialRepChanged.Add((Cmd.EndCmd - CmdIdx) - 2);
			}
		}
		else if (Cmd.Type == REPCMD_Return)
		{
			DynamicArrayDepth--;
			checkf(DynamicArrayDepth >= 0 || CmdIdx == CmdCount - 1, TEXT("Encountered erroneous RepLayout"));
		}
	}

	return { InitialRepChanged, *Replicator.RepLayout };
}

FHandoverChangeState USpatialActorChannel::CreateInitialHandoverChangeState(FClassInfo* ClassInfo)
{
	FHandoverChangeState HandoverChanged;
	for (const FHandoverPropertyInfo& PropertyInfo : ClassInfo->HandoverProperties)
	{
		HandoverChanged.Add(PropertyInfo.Handle);
	}

	return HandoverChanged;
}

bool USpatialActorChannel::ReplicateActor()
{
	if (!IsReadyForReplication())
	{
		return false;
	}
	
	check(Actor);
	check(!Closing);
	check(Connection);
	check(Connection->PackageMap);
	
	const UWorld* const ActorWorld = Actor->GetWorld();

	// Time how long it takes to replicate this particular actor
	STAT(FScopeCycleCounterUObject FunctionScope(Actor));

	// Create an outgoing bunch (to satisfy some of the functions below).
	FOutBunch Bunch(this, 0);
	if (Bunch.IsError())
	{
		return false;
	}

	bIsReplicatingActor = true;
	FReplicationFlags RepFlags;

	// Send initial stuff.
	if (OpenPacketId.First == INDEX_NONE)
	{
		RepFlags.bNetInitial = true;
		Bunch.bClose = Actor->bNetTemporary;
		Bunch.bReliable = true; // Net temporary sends need to be reliable as well to force them to retry
	}

	//Here, Unreal would have determined if this connection belongs to this actor's Outer.
	//We don't have this concept when it comes to connections, our ownership-based logic is in the interop layer.
	//Setting this to true, but should not matter in the end.
	RepFlags.bNetOwner = true;

	// If initial, send init data.
	if (RepFlags.bNetInitial && OpenedLocally)
	{
		Actor->OnSerializeNewActor(Bunch);
	}

	RepFlags.bNetSimulated = (Actor->GetRemoteRole() == ROLE_SimulatedProxy);
	RepFlags.bRepPhysics = Actor->ReplicatedMovement.bRepPhysics;
	RepFlags.bReplay = ActorWorld && (ActorWorld->DemoNetDriver == Connection->GetDriver());

	UE_LOG(LogNetTraffic, Log, TEXT("Replicate %s, bNetInitial: %d, bNetOwner: %d"), *Actor->GetName(), RepFlags.bNetInitial, RepFlags.bNetOwner);

	FMemMark MemMark(FMemStack::Get());	// The calls to ReplicateProperties will allocate memory on FMemStack::Get(), and use it in ::PostSendBunch. we free it below

	// ----------------------------------------------------------
	// Replicate Actor and Component properties and RPCs
	// ----------------------------------------------------------

	// Epic does this at the net driver level, per connection. See UNetDriver::ServerReplicateActors().
	// However, we have many player controllers sharing one connection, so we do it at the actor level before replication.
	APlayerController* PlayerController = Cast<APlayerController>(Actor);
	if (PlayerController)
	{
		PlayerController->SendClientAdjustment();
	}

	// Update SpatialOS position.
	if (!PlayerController && !Cast<APlayerState>(Actor))
	{
		UpdateSpatialPosition();
	}
	
	// Update the replicated property change list.
	FRepChangelistState* ChangelistState = ActorReplicator->ChangelistMgr->GetRepChangelistState();
	bool bWroteSomethingImportant = false;
	ActorReplicator->ChangelistMgr->Update(Actor, Connection->Driver->ReplicationFrame, ActorReplicator->RepState->LastCompareIndex, RepFlags, bForceCompareProperties);

	const int32 PossibleNewHistoryIndex = ActorReplicator->RepState->HistoryEnd % FRepState::MAX_CHANGE_HISTORY;
	FRepChangedHistory& PossibleNewHistoryItem = ActorReplicator->RepState->ChangeHistory[PossibleNewHistoryIndex];
	TArray<uint16>& RepChanged = PossibleNewHistoryItem.Changed;

	// Gather all change lists that are new since we last looked, and merge them all together into a single CL
	for (int32 i = ActorReplicator->RepState->LastChangelistIndex; i < ChangelistState->HistoryEnd; i++)
	{
		const int32 HistoryIndex = i % FRepChangelistState::MAX_CHANGE_HISTORY;
		FRepChangedHistory& HistoryItem = ChangelistState->ChangeHistory[HistoryIndex];
		TArray<uint16> Temp = RepChanged;
		ActorReplicator->RepLayout->MergeChangeList((uint8*)Actor, HistoryItem.Changed, Temp, RepChanged);
	}

	ActorReplicator->RepState->LastCompareIndex = ChangelistState->CompareIndex;

	// Update the handover property change list.
	FHandoverChangeState HandoverChanged = GetHandoverChangeList(*ActorHandoverShadowData, Actor);

	//todo-giray: We currently don't take replication of custom delta properties into account here because it doesn't use changelists.
	// see ActorReplicator->ReplicateCustomDeltaProperties().

	// If any properties have changed, send a component update.
	if (bCreatingNewEntity || RepChanged.Num() > 0 || HandoverChanged.Num() > 0)
	{		
		if (bCreatingNewEntity)
		{
			// SAHIL: CHECK IF YOU PROPERLY FIX THIS
			//check(!Actor->IsFullNameStableForNetworking() || Interop->CanSpawnReplicatedStablyNamedActors());

			// When a player is connected, a FUniqueNetIdRepl is created with the players worker ID. This eventually gets stored
			// inside APlayerState::UniqueId when UWorld::SpawnPlayActor is called. If this actor channel is managing a pawn or a 
			// player controller, get the player state.
			FString PlayerWorkerId;
			APlayerState* PlayerState = Cast<APlayerState>(Actor);
			if (!PlayerState)
			{
				if (APawn* Pawn = Cast<APawn>(Actor))
				{
					PlayerState = Pawn->PlayerState;
				}
			}
			if (!PlayerState)
			{
				if (PlayerController)
				{
					PlayerState = PlayerController->PlayerState;
				}
			}
			if (PlayerState)
			{
				PlayerWorkerId = PlayerState->UniqueId.ToString();
			}
			else
			{
				UE_LOG(LogSpatialGDKActorChannel, Log, TEXT("Unable to find PlayerState for %s, this usually means that this actor is not owned by a player."), *Actor->GetClass()->GetName());
			}

			// Calculate initial spatial position (but don't send component update) and create the entity.
			LastSpatialPosition = GetActorSpatialPosition(Actor);
			Sender->SendCreateEntityRequest(this, LastSpatialPosition, PlayerWorkerId);
		}
		else
		{
			FRepChangeState RepChangeState = { RepChanged, GetObjectRepLayout(Actor) };
			Sender->SendComponentUpdates(Actor, this, &RepChangeState, &HandoverChanged);
		}

		bWroteSomethingImportant = true;
		if (RepChanged.Num() > 0)
		{
			ActorReplicator->RepState->HistoryEnd++;
		}
	}

	UpdateChangelistHistory(ActorReplicator->RepState);

	ActorReplicator->RepState->LastChangelistIndex = ChangelistState->HistoryEnd;

	if (bCreatingNewEntity)
	{
		bCreatingNewEntity = false;
	}
	else
	{
		FOutBunch DummyOutBunch;

		FClassInfo* ClassInfo = NetDriver->TypebindingManager->FindClassInfoByClass(Actor->GetClass());

		for (UActorComponent* ActorComponent : Actor->GetReplicatedComponents())
		{
			if (ClassInfo->SubobjectClasses.Contains(ActorComponent->GetClass()))
			{
				bWroteSomethingImportant |= ReplicateSubobject(ActorComponent, RepFlags);
				bWroteSomethingImportant |= ActorComponent->ReplicateSubobjects(this, &DummyOutBunch, &RepFlags);
			}
		}

		for (UObject* Subobject : NetDriver->TypebindingManager->GetHandoverSubobjects(Actor))
		{
			// Handover shadow data should already exist for this object. If it doesn't, it must have
			// started replicating after SetChannelActor was called on the owning actor.
			TArray<uint8>& SubobjectHandoverShadowData = HandoverShadowDataMap.FindChecked(Subobject).Get();
			FHandoverChangeState HandoverChanged = GetHandoverChangeList(SubobjectHandoverShadowData, Subobject);
			if (HandoverChanged.Num() > 0)
			{
				Sender->SendComponentUpdates(Subobject, this, nullptr, &HandoverChanged);
			}
		}
	}

	/*
	// Do we need to add support for deleted subobjects?
	for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
	{
	if (!RepComp.Key().IsValid())
	{
	// Write a deletion content header:
	WriteContentBlockForSubObjectDelete(Bunch, RepComp.Value()->ObjectNetGUID);
	bWroteSomethingImportant = true;
	Bunch.bReliable = true;
	RepComp.Value()->CleanUp();
	RepComp.RemoveCurrent();
	}
	}*/

	// If we evaluated everything, mark LastUpdateTime, even if nothing changed.
	LastUpdateTime = Connection->Driver->Time;

	MemMark.Pop();

	bIsReplicatingActor = false;

	bForceCompareProperties = false;		// Only do this once per frame when set

	return bWroteSomethingImportant;
}

bool USpatialActorChannel::ReplicateSubobject(UObject* Object, const FReplicationFlags& RepFlags)
{
	if (!NetDriver->TypebindingManager->IsSupportedClass(Object->GetClass()))
	{
		return false;
	}

	FObjectReplicator& Replicator = FindOrCreateReplicator(Object).Get();
	FRepChangelistState* ChangelistState = Replicator.ChangelistMgr->GetRepChangelistState();
	Replicator.ChangelistMgr->Update(Object, Replicator.Connection->Driver->ReplicationFrame, Replicator.RepState->LastCompareIndex, RepFlags, bForceCompareProperties);

	const int32 PossibleNewHistoryIndex = Replicator.RepState->HistoryEnd % FRepState::MAX_CHANGE_HISTORY;
	FRepChangedHistory& PossibleNewHistoryItem = Replicator.RepState->ChangeHistory[PossibleNewHistoryIndex];
	TArray<uint16>& RepChanged = PossibleNewHistoryItem.Changed;

	// Gather all change lists that are new since we last looked, and merge them all together into a single CL
	for (int32 i = Replicator.RepState->LastChangelistIndex; i < ChangelistState->HistoryEnd; i++)
	{
		const int32 HistoryIndex = i % FRepChangelistState::MAX_CHANGE_HISTORY;
		FRepChangedHistory& HistoryItem = ChangelistState->ChangeHistory[HistoryIndex];
		TArray<uint16> Temp = RepChanged;
		Replicator.RepLayout->MergeChangeList((uint8*)Object, HistoryItem.Changed, Temp, RepChanged);
	}

	Replicator.RepState->LastCompareIndex = ChangelistState->CompareIndex;

	if (RepChanged.Num() > 0)
	{
		FRepChangeState RepChangeState = { RepChanged, GetObjectRepLayout(Object) };
		Sender->SendComponentUpdates(Object, this, &RepChangeState, nullptr);
		Replicator.RepState->HistoryEnd++;
	}

	UpdateChangelistHistory(Replicator.RepState);
	Replicator.RepState->LastChangelistIndex = ChangelistState->HistoryEnd;

	return RepChanged.Num() > 0;
}

bool USpatialActorChannel::ReplicateSubobject(UObject* Obj, FOutBunch& Bunch, const FReplicationFlags& RepFlags)
{
	// Intentionally don't call Super::ReplicateSubobject() but rather call our custom version instead.
	return ReplicateSubobject(Obj, RepFlags);
}

void USpatialActorChannel::InitializeHandoverShadowData(TArray<uint8>& ShadowData, UObject* Object)
{
	FClassInfo* ClassInfo = NetDriver->TypebindingManager->FindClassInfoByClass(Object->GetClass());
	check(ClassInfo);

	uint32 Size = 0;
	for (const FHandoverPropertyInfo& PropertyInfo : ClassInfo->HandoverProperties)
	{
		if (PropertyInfo.ArrayIdx == 0) // For static arrays, the first element will handle the whole array
		{
			// Make sure we conform to Unreal's alignment requirements; this is matched below and in ReplicateActor()
			Size = Align(Size, PropertyInfo.Property->GetMinAlignment());
			Size += PropertyInfo.Property->GetSize();
		}
	}
	ShadowData.AddZeroed(Size);
	uint32 Offset = 0;
	for (const FHandoverPropertyInfo& PropertyInfo : ClassInfo->HandoverProperties)
	{
		if (PropertyInfo.ArrayIdx == 0)
		{
			Offset = Align(Offset, PropertyInfo.Property->GetMinAlignment());
			PropertyInfo.Property->InitializeValue(ShadowData.GetData() + Offset);
			Offset += PropertyInfo.Property->GetSize();
		}
	}
}

FHandoverChangeState USpatialActorChannel::GetHandoverChangeList(TArray<uint8>& ShadowData, UObject* Object)
{
	FHandoverChangeState HandoverChanged;

	FClassInfo* ClassInfo = NetDriver->TypebindingManager->FindClassInfoByClass(Object->GetClass());
	check(ClassInfo);

	uint32 ShadowDataOffset = 0;
	for (const FHandoverPropertyInfo& PropertyInfo : ClassInfo->HandoverProperties)
	{
		ShadowDataOffset = Align(ShadowDataOffset, PropertyInfo.Property->GetMinAlignment());

		const uint8* Data = (uint8*)Object + PropertyInfo.Offset;
		uint8* StoredData = ShadowData.GetData() + ShadowDataOffset;
		// Compare and assign.
		if (bCreatingNewEntity || !PropertyInfo.Property->Identical(StoredData, Data))
		{
			HandoverChanged.Add(PropertyInfo.Handle);
			PropertyInfo.Property->CopySingleValue(StoredData, Data);
		}
		ShadowDataOffset += PropertyInfo.Property->ElementSize;
	}

	return HandoverChanged;
}

void USpatialActorChannel::SetChannelActor(AActor* InActor)
{
	Super::SetChannelActor(InActor);

	if (!bCoreActor)
	{
		return;
	}

	// Set up the shadow data for the handover properties. This is used later to compare the properties and send only changed ones.
	check(!HandoverShadowDataMap.Contains(InActor));

	// Create the shadow map, and store a quick access pointer to it
	ActorHandoverShadowData = &HandoverShadowDataMap.Add(InActor, MakeShared<TArray<uint8>>()).Get();
	InitializeHandoverShadowData(*ActorHandoverShadowData, InActor);

	// Assume that all the replicated static components are already set as such. This is checked later in ReplicateSubobject.
	for (UObject* Subobject : NetDriver->TypebindingManager->GetHandoverSubobjects(InActor))
	{
		check(!HandoverShadowDataMap.Contains(Subobject));
		InitializeHandoverShadowData(HandoverShadowDataMap.Add(Subobject, MakeShared<TArray<uint8>>()).Get(), Subobject);
	}

	// Get the entity ID from the entity registry (or return 0 if it doesn't exist).
	check(NetDriver->GetEntityRegistry());
	EntityId = NetDriver->GetEntityRegistry()->GetEntityIdFromActor(InActor);

	// If the entity registry has no entry for this actor, this means we need to create it.
	if (EntityId == 0)
	{
		// If the actor is stably named, we only want to start the creation process on one server (the one that is authoritative
		// over the Global State Manager) to avoid having multiple copies of replicated stably named actors in SpatialOS
		if (InActor->IsFullNameStableForNetworking())
		{
			//SpatialNetDriver->GetSpatialInterop()->ReserveReplicatedStablyNamedActorChannel(this);
		}
		else
		{
			bCreatingNewEntity = true;
			Sender->SendReserveEntityIdRequest(this);
		}
	}
	else
	{
		UE_LOG(LogSpatialGDKActorChannel, Log, TEXT("Opened channel for actor %s with existing entity ID %lld."), *InActor->GetName(), EntityId);

		// Inform USpatialInterop of this new actor channel/entity pairing
		NetDriver->AddActorChannel(EntityId, this);
	}
}

FObjectReplicator& USpatialActorChannel::PreReceiveSpatialUpdate(UObject* TargetObject)
{
	FNetworkGUID ObjectNetGUID = Connection->Driver->GuidCache->GetOrAssignNetGUID(TargetObject);
	check(!ObjectNetGUID.IsDefault() && ObjectNetGUID.IsValid());

	TargetObject->PreNetReceive();
	FObjectReplicator& Replicator = FindOrCreateReplicator(TargetObject).Get();
	Replicator.RepLayout->InitShadowData(Replicator.RepState->StaticBuffer, TargetObject->GetClass(), (uint8*)TargetObject);

	return Replicator;
}

void USpatialActorChannel::PostReceiveSpatialUpdate(UObject* TargetObject, const TArray<UProperty*>& RepNotifies)
{
	FNetworkGUID ObjectNetGUID = Connection->Driver->GuidCache->GetOrAssignNetGUID(TargetObject);
	check(!ObjectNetGUID.IsDefault() && ObjectNetGUID.IsValid())

	FObjectReplicator& Replicator = FindOrCreateReplicator(TargetObject).Get();
	TargetObject->PostNetReceive();
	Replicator.RepNotifies = RepNotifies;
	Replicator.CallRepNotifies(false);
}

void USpatialActorChannel::RegisterEntityId(const Worker_EntityId& ActorEntityId)
{
	NetDriver->GetEntityRegistry()->AddToRegistry(ActorEntityId, GetActor());

	// Inform USpatialInterop of this new actor channel/entity pairing
	NetDriver->AddActorChannel(ActorEntityId, this);

	// If a Singleton was created, update the GSM with the proper Id.
	if (Actor->GetClass()->HasAnySpatialClassFlags(SPATIALCLASS_Singleton))
	{
		NetDriver->GlobalStateManager->UpdateSingletonEntityId(Actor->GetClass()->GetPathName(), ActorEntityId);
	}

	if (Actor->IsFullNameStableForNetworking())
	{
		USpatialPackageMapClient* PackageMap = Cast<USpatialPackageMapClient>(NetDriver->GetSpatialOSNetConnection()->PackageMap);

		uint32 CurrentOffset = 1;
		SubobjectToOffsetMap SubobjectNameToOffset;
		ForEachObjectWithOuter(Actor, [&CurrentOffset, &SubobjectNameToOffset](UObject* Object)
		{
			// Objects can only be allocated NetGUIDs if this is true.
			if (Object->IsSupportedForNetworking() && !Object->IsPendingKill() && !Object->IsEditorOnly())
			{
				SubobjectNameToOffset.Add(*Object->GetName(), CurrentOffset);
				CurrentOffset++;
			}
		});

		PackageMap->ResolveEntityActor(Actor, ActorEntityId, SubobjectNameToOffset);
	}
}

void USpatialActorChannel::OnReserveEntityIdResponse(const Worker_ReserveEntityIdResponseOp& Op)
{
	if (Op.status_code != WORKER_STATUS_CODE_SUCCESS)
	{
		UE_LOG(LogSpatialGDKActorChannel, Error, TEXT("Failed to reserve entity id. Reason: %s"), UTF8_TO_TCHAR(Op.message));
		// TODO: From now on, this actor channel will be useless. We need better error handling, or a retry mechanism here.
		return;
	}
	UE_LOG(LogSpatialGDKActorChannel, Log, TEXT("Received entity id (%lld) for: %s."), Op.entity_id, *Actor->GetName());
	EntityId = Op.entity_id;
	RegisterEntityId(EntityId);
}

void USpatialActorChannel::OnCreateEntityResponse(const Worker_CreateEntityResponseOp& Op)
{
	check(NetDriver->GetNetMode() < NM_Client);

	if (Op.status_code != WORKER_STATUS_CODE_SUCCESS)
	{
		UE_LOG(LogSpatialGDKActorChannel, Error, TEXT("!!! Failed to create entity for actor %s: %s"), *Actor->GetName(), UTF8_TO_TCHAR(Op.message));
		// TODO: From now on, this actor channel will be useless. We need better error handling, or a retry mechanism here.
		return;
	}
	UE_LOG(LogSpatialGDKActorChannel, Log, TEXT("!!! Created entity (%lld) for: %s."), EntityId, *Actor->GetName());

	// If a replicated stably named actor was created, update the GSM with the proper path and entity id
	// This ensures each stably named actor is only created once
	if (Actor->IsFullNameStableForNetworking())
	{
		//SpatialNetDriver->GetSpatialInterop()->AddReplicatedStablyNamedActorToGSM(FEntityId(ActorEntityId), Actor);
	}
}

void USpatialActorChannel::UpdateSpatialPosition()
{
	// PlayerController's and PlayerState's are a special case here. To ensure that they and their associated pawn are 
	// handed between workers at the same time (which is not guaranteed), we ensure that we update the position component 
	// of the PlayerController and PlayerState at the same time as the pawn.

	// Check that it has moved sufficiently far to be updated
	const float SpatialPositionThreshold = 100.0f * 100.0f; // 1m (100cm)
	FVector ActorSpatialPosition = GetActorSpatialPosition(Actor);
	if (FVector::DistSquared(ActorSpatialPosition, LastSpatialPosition) < SpatialPositionThreshold)
	{
		return;
	}

	LastSpatialPosition = ActorSpatialPosition;
	Sender->SendPositionUpdate(GetEntityId(), LastSpatialPosition);

	// If we're a pawn and are controlled by a player controller, update the player controller and the player state positions too.
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()))
		{
			USpatialActorChannel* ControllerActorChannel = Cast<USpatialActorChannel>(Connection->ActorChannels.FindRef(PlayerController));
			if (ControllerActorChannel)
			{
				Sender->SendPositionUpdate(ControllerActorChannel->GetEntityId(), LastSpatialPosition);
			}
			USpatialActorChannel* PlayerStateActorChannel = Cast<USpatialActorChannel>(Connection->ActorChannels.FindRef(PlayerController->PlayerState));
			if (PlayerStateActorChannel)
			{
				Sender->SendPositionUpdate(PlayerStateActorChannel->GetEntityId(), LastSpatialPosition);
			}
		}
	}
}

FVector USpatialActorChannel::GetActorSpatialPosition(AActor* Actor)
{
	// Preferentially uses the owner location over the origin
	// This is to enable actors like PlayerState to follow their corresponding character

	// If the actor has a well defined location then use that
	// Otherwise if it has a parent use its location
	// Otherwise use the origin
	if (Actor->GetRootComponent()) 
	{
		return Actor->GetRootComponent()->GetComponentLocation();
	}
	else if (Actor->GetOwner())
	{
		return GetActorSpatialPosition(Actor->GetOwner());
	}
	else
	{
		return FVector::ZeroVector;
	}
}
