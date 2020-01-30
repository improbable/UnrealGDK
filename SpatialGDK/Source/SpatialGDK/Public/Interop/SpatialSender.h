// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "EngineClasses/SpatialNetBitWriter.h"
#include "Interop/SpatialClassInfoManager.h"
#include "Interop/SpatialRPCService.h"
#include "Schema/RPCPayload.h"
#include "TimerManager.h"
#include "Utils/RepDataUtils.h"
#include "Utils/RPCContainer.h"

#include <WorkerSDK/improbable/c_schema.h>
#include <WorkerSDK/improbable/c_worker.h>

#include "SpatialSender.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialSender, Log, All);

class USpatialActorChannel;
class SpatialDispatcher;
class USpatialNetDriver;
class USpatialPackageMapClient;
class USpatialReceiver;
class USpatialStaticComponentView;
class USpatialClassInfoManager;
class SpatialActorGroupManager;
class USpatialWorkerConnection;

struct FReliableRPCForRetry
{
	FReliableRPCForRetry(UObject* InTargetObject, UFunction* InFunction, Worker_ComponentId InComponentId, Schema_FieldId InRPCIndex, const TArray<uint8>& InPayload, int InRetryIndex);

	TWeakObjectPtr<UObject> TargetObject;
	UFunction* Function;
	Worker_ComponentId ComponentId;
	Schema_FieldId RPCIndex;
	TArray<uint8> Payload;
	int Attempts; // For reliable RPCs

	int RetryIndex; // Index for ordering reliable RPCs on subsequent tries
};

struct FPendingRPC
{
	FPendingRPC() = default;
	FPendingRPC(FPendingRPC&& Other);

	uint32 Offset;
	Schema_FieldId Index;
	TArray<uint8> Data;
	Schema_EntityId Entity;
};

// TODO: Clear TMap entries when USpatialActorChannel gets deleted - UNR:100
// care for actor getting deleted before actor channel
using FChannelObjectPair = TPair<TWeakObjectPtr<USpatialActorChannel>, TWeakObjectPtr<UObject>>;
using FRPCsOnEntityCreationMap = TMap<TWeakObjectPtr<const UObject>, SpatialGDK::RPCsOnEntityCreation>;
using FUpdatesQueuedUntilAuthority = TMap<Worker_EntityId_Key, TArray<FWorkerComponentUpdate>>;
using FChannelsToUpdatePosition = TSet<TWeakObjectPtr<USpatialActorChannel>>;

UCLASS()
class SPATIALGDK_API USpatialSender : public UObject
{
	GENERATED_BODY()

public:
	void Init(USpatialNetDriver* InNetDriver, FTimerManager* InTimerManager, SpatialGDK::SpatialRPCService* InRPCService);

	// Actor Updates
	void SendComponentUpdates(UObject* Object, const FClassInfo& Info, USpatialActorChannel* Channel, const FRepChangeState* RepChanges, const FHandoverChangeState* HandoverChanges);
	void SendComponentInterestForActor(USpatialActorChannel* Channel, Worker_EntityId EntityId, bool bNetOwned);
	void SendComponentInterestForSubobject(const FClassInfo& Info, Worker_EntityId EntityId, bool bNetOwned);
	void SendPositionUpdate(Worker_EntityId EntityId, const FVector& Location);
	void SendAuthorityIntentUpdate(const AActor& Actor, VirtualWorkerId NewAuthoritativeVirtualWorkerId);
	void SetAclWriteAuthority(const Worker_EntityId EntityId, const FString& DestinationWorkerId);
	FRPCErrorInfo SendRPC(const FPendingRPCParams& Params);
	ERPCResult SendRPCInternal(UObject* TargetObject, UFunction* Function, const SpatialGDK::RPCPayload& Payload);
	void SendCommandResponse(Worker_RequestId RequestId, Worker_CommandResponse& Response);
	void SendEmptyCommandResponse(Worker_ComponentId ComponentId, Schema_FieldId CommandIndex, Worker_RequestId RequestId);
	void SendCommandFailure(Worker_RequestId RequestId, const FString& Message);
	void SendAddComponent(USpatialActorChannel* Channel, UObject* Subobject, const FClassInfo& Info);
	void SendRemoveComponent(Worker_EntityId EntityId, const FClassInfo& Info);
	void SendInterestBucketComponentChange(const Worker_EntityId EntityId, const Worker_ComponentId OldComponent, const Worker_ComponentId NewComponent);

	void SendCreateEntityRequest(USpatialActorChannel* Channel);
	void RetireEntity(const Worker_EntityId EntityId);

	/** Creates an entity containing just a tombstone component and the minimal data to resolve an actor. */
	void CreateTombstoneEntity(AActor* Actor);

	void SendRequestToClearRPCsOnEntityCreation(Worker_EntityId EntityId);
	void ClearRPCsOnEntityCreation(Worker_EntityId EntityId);

	void SendClientEndpointReadyUpdate(Worker_EntityId EntityId);
	void SendServerEndpointReadyUpdate(Worker_EntityId EntityId);

	void EnqueueRetryRPC(TSharedRef<FReliableRPCForRetry> RetryRPC);
	void FlushRetryRPCs();
	void RetryReliableRPC(TSharedRef<FReliableRPCForRetry> RetryRPC);

	void RegisterChannelForPositionUpdate(USpatialActorChannel* Channel);
	void ProcessPositionUpdates();

	bool UpdateEntityACLs(Worker_EntityId EntityId, const FString& OwnerWorkerAttribute);
	void UpdateInterestComponent(AActor* Actor);

	void ProcessOrQueueOutgoingRPC(const FUnrealObjectRef& InTargetObjectRef, SpatialGDK::RPCPayload&& InPayload);
	void ProcessUpdatesQueuedUntilAuthority(Worker_EntityId EntityId);

	void FlushPackedRPCs();

	void FlushRPCService();

	SpatialGDK::RPCPayload CreateRPCPayloadFromParams(UObject* TargetObject, const FUnrealObjectRef& TargetObjectRef, UFunction* Function, void* Params);
	void GainAuthorityThenAddComponent(USpatialActorChannel* Channel, UObject* Object, const FClassInfo* Info);

	// Creates an entity authoritative on this server worker, ensuring it will be able to receive updates for the GSM.
	void CreateServerWorkerEntity(int AttemptCounter = 1);

	void ClearPendingRPCs(const Worker_EntityId EntityId);

	bool ValidateOrExit_IsSupportedClass(const FString& PathName);

private:
	struct EntityComponentsDeleter
	{
		void operator()(TArray<Worker_ComponentData>* Components) const noexcept;
	};

	using EntityComponents = TUniquePtr<TArray<Worker_ComponentData>, EntityComponentsDeleter>;

	// Create a copy of an array of components. Deep copies all Schema_ComponentData.
	static TArray<Worker_ComponentData> CopyEntityComponentData(const EntityComponents& EntityComponents);

	// Create an entity given a set of components and an ID. Retries with the same component data and entity ID on timeout.
	// Requires that EntitiesBeingCreatedWithRetries contains the component data with the key of EntityId.
	void CreateEntityWithRetries(Worker_EntityId EntityId, FString EntityName);

	// Actor Lifecycle
	Worker_RequestId CreateEntity(USpatialActorChannel* Channel);
	Worker_ComponentData CreateLevelComponentData(AActor* Actor);

	void AddTombstoneToEntity(const Worker_EntityId EntityId);

	// RPC Construction
	FSpatialNetBitWriter PackRPCDataToSpatialNetBitWriter(UFunction* Function, void* Parameters) const;

	Worker_CommandRequest CreateRPCCommandRequest(UObject* TargetObject, const SpatialGDK::RPCPayload& Payload, Worker_ComponentId ComponentId, Schema_FieldId CommandIndex, Worker_EntityId& OutEntityId);
	Worker_CommandRequest CreateRetryRPCCommandRequest(const FReliableRPCForRetry& RPC, uint32 TargetObjectOffset);
	Worker_ComponentUpdate CreateRPCEventUpdate(UObject* TargetObject, const SpatialGDK::RPCPayload& Payload, Worker_ComponentId ComponentId, Schema_FieldId EventIndext);
	ERPCResult AddPendingRPC(UObject* TargetObject, UFunction* Function, const SpatialGDK::RPCPayload& Payload, Worker_ComponentId ComponentId, Schema_FieldId RPCIndext);

	TArray<Worker_InterestOverride> CreateComponentInterestForActor(USpatialActorChannel* Channel, bool bIsNetOwned);

	// RPC Tracking
#if !UE_BUILD_SHIPPING
	void TrackRPC(AActor* Actor, UFunction* Function, const SpatialGDK::RPCPayload& Payload, const ERPCType RPCType);
#endif
private:
	UPROPERTY()
	USpatialNetDriver* NetDriver;

	UPROPERTY()
	USpatialStaticComponentView* StaticComponentView;

	UPROPERTY()
	USpatialWorkerConnection* Connection;

	UPROPERTY()
	USpatialReceiver* Receiver;

	UPROPERTY()
	USpatialPackageMapClient* PackageMap;

	UPROPERTY()
	USpatialClassInfoManager* ClassInfoManager;

	// Stores the component data of entities being created for which we need to persist the component data between retries.
	// We can not have the response delegates own the data as they must be copyable.
	TMap<Worker_EntityId_Key, EntityComponents> EntitiesBeingCreatedWithRetries;

	SpatialActorGroupManager* ActorGroupManager;

	FTimerManager* TimerManager;

	SpatialGDK::SpatialRPCService* RPCService;

	FRPCContainer OutgoingRPCs;
	FRPCsOnEntityCreationMap OutgoingOnCreateEntityRPCs;

	TArray<TSharedRef<FReliableRPCForRetry>> RetryRPCs;

	FUpdatesQueuedUntilAuthority UpdatesQueuedUntilAuthorityMap;

	FChannelsToUpdatePosition ChannelsToUpdatePosition;

	TMap<Worker_EntityId_Key, TArray<FPendingRPC>> RPCsToPack;
};
