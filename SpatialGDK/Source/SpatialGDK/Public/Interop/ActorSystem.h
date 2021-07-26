// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once
#include "Schema/SpawnData.h"
#include "Schema/UnrealMetadata.h"
#include "Utils/RepDataUtils.h"

#include "Interop/ClientNetLoadActorHelper.h"
#include "Interop/SpatialCommandsHandler.h"

DECLARE_LOG_CATEGORY_EXTERN(LogActorSystem, Log, All);

class USpatialClassInfoManager;

struct FRepChangeState;
struct FPendingSubobjectAttachment;
class USpatialNetConnection;
class FSpatialObjectRepState;
class FRepLayout;
struct FClassInfo;
class USpatialNetDriver;

class SpatialActorChannel;

using FChannelsToUpdatePosition =
	TSet<TWeakObjectPtr<USpatialActorChannel>, TWeakObjectPtrKeyFuncs<TWeakObjectPtr<USpatialActorChannel>, false>>;

namespace SpatialGDK
{
class SpatialEventTracer;
class FSubView;

struct ActorData
{
	SpawnData Spawn;
	UnrealMetadata Metadata;
};

struct FObjectRepNotifies
{
	TArray<GDK_PROPERTY(Property)*> RepNotifies;
	TMap<GDK_PROPERTY(Property)*, FSpatialGDKSpanId> PropertySpanIds;
};
using FObjectToRepNotifies = TMap<FWeakObjectPtr, FObjectRepNotifies>;

class ActorSystem
{
public:
	ActorSystem(const FSubView& InActorSubView, const FSubView& InAuthoritySubView, const FSubView& InOwnershipSubView,
				const FSubView& InSimulatedSubView, const FSubView& InTombstoneSubView, USpatialNetDriver* InNetDriver,
				SpatialEventTracer* InEventTracer);

	void Advance();

	UnrealMetadata* GetUnrealMetadata(Worker_EntityId EntityId);

	void MoveMappedObjectToUnmapped(const FUnrealObjectRef& Ref);
	void CleanupRepStateMap(FSpatialObjectRepState& RepState);
	void ResolvePendingOpsFromEntityUpdate(TArray<FWeakObjectPtr> ToResolveOps);
	void ResolveAsyncPendingLoad(UObject* LoadedObject, const FUnrealObjectRef& ObjectRef);
	void ResolvePendingOperations(UObject* Object, const FUnrealObjectRef& ObjectRef);
	void RetireWhenAuthoritative(Worker_EntityId EntityId, Worker_ComponentId ActorClassId, bool bIsNetStartup, bool bNeedsTearOff);
	void RemoveActor(Worker_EntityId EntityId);

	static USpatialActorChannel* SetUpActorChannel(USpatialNetDriver* NetDriver, AActor* Actor, Worker_EntityId EntityId);
	USpatialActorChannel* SetUpActorChannel(AActor* Actor, Worker_EntityId EntityId);

	// Tombstones
	void CreateTombstoneEntity(AActor* Actor);
	void RetireEntity(Worker_EntityId EntityId, bool bIsNetStartupActor) const;

	// Updates
	void SendComponentUpdates(UObject* Object, const FClassInfo& Info, USpatialActorChannel* Channel, const FRepChangeState* RepChanges,
							  uint32& OutBytesWritten);
	void SendActorTornOffUpdate(Worker_EntityId EntityId, Worker_ComponentId ComponentId) const;
	void ProcessPositionUpdates();
	void RegisterChannelForPositionUpdate(USpatialActorChannel* Channel);
	void UpdateInterestComponent(AActor* Actor);
	void SendInterestBucketComponentChange(Worker_EntityId EntityId, Worker_ComponentId OldComponent,
										   Worker_ComponentId NewComponent) const;
	void SendAddComponentForSubobject(USpatialActorChannel* Channel, UObject* Subobject, const FClassInfo& SubobjectInfo,
									  uint32& OutBytesWritten);
	void SendRemoveComponentForClassInfo(Worker_EntityId EntityId, const FClassInfo& Info);

	// Creating entities for actor channels
	void SendCreateEntityRequest(USpatialActorChannel& ActorChannel, uint32& OutBytesWritten);
	void OnEntityCreated(const Worker_CreateEntityResponseOp& Op, FSpatialGDKSpanId CreateOpSpan);
	bool HasPendingOpsForChannel(const USpatialActorChannel& ActorChannel) const;

	static Worker_ComponentData CreateLevelComponentData(const AActor& Actor, const UWorld& NetDriverWorld,
														 const USpatialClassInfoManager& ClassInfoManager);

	void DestroySubObject(const FUnrealObjectRef& ObjectRef, UObject& Object) const;

private:
	// Helper struct to manage FSpatialObjectRepState update cycle.
	// TODO: move into own class.
	struct RepStateUpdateHelper;

	struct DeferredRetire
	{
		Worker_EntityId EntityId;
		Worker_ComponentId ActorClassId;
		bool bIsNetStartupActor;
		bool bNeedsTearOff;
	};
	TArray<DeferredRetire> EntitiesToRetireOnAuthorityGain;

	// Map from references to replicated objects to properties using these references.
	// Useful to manage entities going in and out of interest, in order to recover references to actors.
	FObjectToRepStateMap ObjectRefToRepStateMap;

	void PopulateDataStore(Worker_EntityId EntityId);

	struct FEntitySubViewUpdate;

	void ProcessUpdates(const FEntitySubViewUpdate& SubViewUpdate);
	void ProcessAdds(const FEntitySubViewUpdate& SubViewUpdate);
	void ProcessAuthorityGains(const FEntitySubViewUpdate& SubViewUpdate);
	void ProcessRemoves(const FEntitySubViewUpdate& SubViewUpdate);

	void ApplyComponentAdd(Worker_EntityId EntityId, Worker_ComponentId ComponentId, Schema_ComponentData* Data);

	void AuthorityLost(Worker_EntityId EntityId, Worker_ComponentSetId ComponentSetId);
	void AuthorityGained(Worker_EntityId EntityId, Worker_ComponentSetId ComponentSetId);
	void HandleActorAuthority(Worker_EntityId EntityId, Worker_ComponentSetId ComponentSetId, Worker_Authority Authority);

	void ComponentAdded(Worker_EntityId EntityId, Worker_ComponentId ComponentId, Schema_ComponentData* Data,
						TArray<FWeakObjectPtr>& OutToResolveOps);
	void ComponentUpdated(Worker_EntityId EntityId, Worker_ComponentId ComponentId, Schema_ComponentUpdate* Update);
	void ComponentRemoved(Worker_EntityId EntityId, Worker_ComponentId ComponentId) const;

	void EntityAdded(Worker_EntityId EntityId);
	void EntityRemoved(Worker_EntityId EntityId);
	void RefreshEntity(const Worker_EntityId EntityId);
	void ApplyFullState(const Worker_EntityId EntityId, USpatialActorChannel& EntityActorChannel, AActor& EntityActor);

	// Invokes RepNotifies queued inside ActorRepNotifiesToSend/SubobjectRepNotifiesToSend.
	void InvokeRepNotifies();
	void TryInvokeRepNotifiesForObject(FWeakObjectPtr& Object, FObjectRepNotifies& ObjectRepNotifies) const;
	static void RemoveRepNotifiesWithUnresolvedObjs(UObject& Object, const USpatialActorChannel& Channel,
													TArray<GDK_PROPERTY(Property) *>& RepNotifies);

	// Authority
	bool HasEntityBeenRequestedForDelete(Worker_EntityId EntityId) const;
	void HandleEntityDeletedAuthority(Worker_EntityId EntityId) const;
	void HandleDeferredEntityDeletion(const DeferredRetire& Retire) const;

	// Component add
	void HandleDormantComponentAdded(Worker_EntityId EntityId) const;
	void HandleIndividualAddComponent(Worker_EntityId EntityId, Worker_ComponentId ComponentId, Schema_ComponentData* Data,
									  TArray<FWeakObjectPtr>& OutToResolveOps);
	void AttachDynamicSubobject(AActor* Actor, Worker_EntityId EntityId, const FClassInfo& Info,
								TArray<FWeakObjectPtr>& OutToResolveOps);
	void ApplyComponentData(USpatialActorChannel& Channel, UObject& TargetObject, const Worker_ComponentId ComponentId,
							Schema_ComponentData* Data);

	void ResolveIncomingOperations(UObject* Object, const FUnrealObjectRef& ObjectRef);
	void ResolveObjectReferences(FRepLayout& RepLayout, UObject* ReplicatedObject, FSpatialObjectRepState& RepState,
								 FObjectReferencesMap& ObjectReferencesMap, uint8* RESTRICT StoredData, uint8* RESTRICT Data,
								 int32 MaxAbsOffset, FObjectRepNotifies& ObjectRepNotifiesOut, bool& bOutSomeObjectsWereMapped);

	// Component update
	USpatialActorChannel* GetOrRecreateChannelForDormantActor(AActor* Actor, Worker_EntityId EntityID) const;
	void ApplyComponentUpdate(Worker_ComponentId ComponentId, Schema_ComponentUpdate* ComponentUpdate, UObject& TargetObject,
							  USpatialActorChannel& Channel);

	// Entity add
	void ReceiveActor(Worker_EntityId EntityId);
	bool IsReceivedEntityTornOff(Worker_EntityId EntityId) const;
	AActor* TryGetActor(const UnrealMetadata& Metadata) const;
	AActor* TryGetOrCreateActor(ActorData& ActorComponents, Worker_EntityId EntityId);
	AActor* CreateActor(ActorData& ActorComponents, Worker_EntityId EntityId);
	void ApplyComponentDataOnActorCreation(Worker_EntityId EntityId, Worker_ComponentId ComponentId, Schema_ComponentData* Data,
										   USpatialActorChannel& Channel, TArray<ObjectPtrRefPair>& OutObjectsToResolve);

	USpatialActorChannel* TryRestoreActorChannelForStablyNamedActor(AActor* StablyNamedActor, Worker_EntityId EntityId);
	void InvokePostNetReceives(Worker_EntityId EntityId) const;
	FObjectRepNotifies& GetObjectRepNotifies(UObject& Object);

	// Entity remove
	void DestroyActor(AActor* Actor, Worker_EntityId EntityId);
	static FString GetObjectNameFromRepState(const FSpatialObjectRepState& RepState);

	void CreateEntityWithRetries(Worker_EntityId EntityId, FString EntityName, TArray<FWorkerComponentData> EntityComponents);
	static TArray<FWorkerComponentData> CopyEntityComponentData(const TArray<FWorkerComponentData>& EntityComponents);
	static void DeleteEntityComponentData(TArray<FWorkerComponentData>& EntityComponents);
	void AddTombstoneToEntity(Worker_EntityId EntityId) const;

	// Updates
	void SendAddComponents(Worker_EntityId EntityId, TArray<FWorkerComponentData> ComponentDatas) const;
	void SendRemoveComponents(Worker_EntityId EntityId, TArray<Worker_ComponentId> ComponentIds) const;

	const FSubView* ActorSubView;
	const FSubView* AuthoritySubView;
	const FSubView* OwnershipSubView;
	const FSubView* SimulatedSubView;
	const FSubView* TombstoneSubView;

	USpatialNetDriver* NetDriver;
	SpatialEventTracer* EventTracer;
	FClientNetLoadActorHelper ClientNetLoadActorHelper;

	FCommandsHandler CommandsHandler;

	TSet<Worker_EntityId_Key> PresentEntities;

	TMap<Worker_RequestId_Key, TWeakObjectPtr<USpatialActorChannel>> CreateEntityRequestIdToActorChannel;

	TMap<Worker_EntityId_Key, TSet<Worker_ComponentId>> PendingDynamicSubobjectComponents;

	FChannelsToUpdatePosition ChannelsToUpdatePosition;

	// RepNotifies are stored here then sent after all updates we have are applied
	FObjectToRepNotifies ActorRepNotifiesToSend;
	FObjectToRepNotifies SubobjectRepNotifiesToSend;

	// Deserialized state store for Actor relevant components.
	TMap<Worker_EntityId_Key, ActorData> ActorDataStore;
};

} // namespace SpatialGDK
