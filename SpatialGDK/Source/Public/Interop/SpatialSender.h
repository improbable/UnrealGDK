// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "Utils/RepDataUtils.h"

#include <improbable/c_schema.h>
#include <improbable/c_worker.h>

#include "SpatialSender.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialSender, Log, All);

class USpatialNetDriver;
class USpatialWorkerConnection;
class USpatialActorChannel;
class USpatialPackageMapClient;
class USpatialTypebindingManager;
class USpatialReceiver;

struct FPendingRPCParams
{
	FPendingRPCParams(UObject* InTargetObject, UFunction* InFunction, void* InParameters);
	~FPendingRPCParams();

	TWeakObjectPtr<UObject> TargetObject;
	UFunction* Function;
	TArray<uint8> Parameters;
	int Attempts; // For reliable RPCs
};

// TODO: Clear TMap entries when USpatialActorChannel gets deleted
// care for actor getting deleted before actor channel
using FChannelObjectPair = TPair<TWeakObjectPtr<USpatialActorChannel>, TWeakObjectPtr<UObject>>;
using FOutgoingRPCMap = TMap<const UObject*, TArray<TSharedRef<FPendingRPCParams>>>;
using FReliableRPCMap = TMap<Worker_RequestId, TSharedRef<FPendingRPCParams>>;
using FUnresolvedEntry = TSharedPtr<TSet<const UObject*>>;
using FHandleToUnresolved = TMap<uint16, FUnresolvedEntry>;
using FChannelToHandleToUnresolved = TMap<FChannelObjectPair, FHandleToUnresolved>;
using FOutgoingRepUpdates = TMap<const UObject*, FChannelToHandleToUnresolved>;

UCLASS()
class SPATIALGDK_API USpatialSender : public UObject
{
	GENERATED_BODY()

public:
	void Init(USpatialNetDriver* InNetDriver, FTimerManager* InTimerManager);

	// Actor Updates
	void SendComponentUpdates(UObject* Object, USpatialActorChannel* Channel, const FRepChangeState* RepChanges, const FHandoverChangeState* HandoverChanges);
	void SendPositionUpdate(Worker_EntityId EntityId, const FVector& Location);
	void SendRotationUpdate(Worker_EntityId EntityId, const FRotator& Rotation);
	void SendRPC(TSharedRef<FPendingRPCParams> Params);
	void ReceiveCommandResponse(Worker_CommandResponseOp& Op);
	void SendCommandResponse(Worker_RequestId request_id, Worker_CommandResponse& Response);

	void SendReserveEntityIdRequest(USpatialActorChannel* Channel);
	void SendCreateEntityRequest(USpatialActorChannel* Channel, const FString& PlayerWorkerId);
	void SendDeleteEntityRequest(Worker_EntityId EntityId);

	void ResolveOutgoingOperations(UObject* Object, bool bIsHandover);
	void ResolveOutgoingRPCs(UObject* Object);

private:
	// Actor Lifecycle
	Worker_RequestId CreateEntity(const FString& ClientWorkerId, const FString& Metadata, USpatialActorChannel* Channel);

	// Queuing
	void ResetOutgoingUpdate(USpatialActorChannel* DependentChannel, UObject* ReplicatedObject, int16 Handle, bool bIsHandover);
	void QueueOutgoingUpdate(USpatialActorChannel* DependentChannel, UObject* ReplicatedObject, int16 Handle, const TSet<const UObject*>& UnresolvedObjects, bool bIsHandover);
	void QueueOutgoingRPC(const UObject* UnresolvedObject, TSharedRef<FPendingRPCParams> Params);

	// RPC Construction
	Worker_CommandRequest CreateRPCCommandRequest(UObject* TargetObject, UFunction* Function, void* Parameters, Worker_ComponentId ComponentId, Schema_FieldId CommandIndex, Worker_EntityId& OutEntityId, const UObject*& OutUnresolvedObject);
	Worker_ComponentUpdate CreateMulticastUpdate(UObject* TargetObject, UFunction* Function, void* Parameters, Worker_ComponentId ComponentId, Schema_FieldId EventIndex, Worker_EntityId& OutEntityId, const UObject*& OutUnresolvedObject);

private:
	USpatialNetDriver* NetDriver;
	USpatialWorkerConnection* Connection;
	USpatialReceiver* Receiver;
	USpatialPackageMapClient* PackageMap;
	USpatialTypebindingManager* TypebindingManager;
	FTimerManager* TimerManager;

	FChannelToHandleToUnresolved RepPropertyToUnresolved;
	FOutgoingRepUpdates RepObjectToUnresolved;

	FChannelToHandleToUnresolved HandoverPropertyToUnresolved;
	FOutgoingRepUpdates HandoverObjectToUnresolved;

	FOutgoingRPCMap OutgoingRPCs;
	FReliableRPCMap OutgoingReliableRPCs;

	TMap<Worker_RequestId, USpatialActorChannel*> PendingActorRequests;
};
