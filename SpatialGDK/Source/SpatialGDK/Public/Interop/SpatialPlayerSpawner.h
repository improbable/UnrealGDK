// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "GameFramework/OnlineReplStructs.h"
#include "Schema/PlayerSpawner.h"
#include "Templates/UniquePtr.h"
#include "UObject/NoExportTypes.h"

#include <WorkerSDK/improbable/c_worker.h>
#include <WorkerSDK/improbable/c_schema.h>

#include "SpatialPlayerSpawner.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialPlayerSpawner, Log, All);

class FTimerManager;
class USpatialNetDriver;

UCLASS()
class SPATIALGDK_API USpatialPlayerSpawner : public UObject
{
	GENERATED_BODY()

public:

	void Init(USpatialNetDriver* NetDriver, FTimerManager* TimerManager);

	// Client
	void SendPlayerSpawnRequest();
	void ReceivePlayerSpawnResponseOnClient(const Worker_CommandResponseOp& Op);

	// Authoritative server worker
	void ReceivePlayerSpawnRequestOnServer(const Worker_CommandRequestOp& Op);
	void ReceiveForwardPlayerSpawnResponse(const Worker_CommandResponseOp& Op);

	// Non-authoritative server worker
	void ReceiveForwardedPlayerSpawnRequest(const Worker_CommandRequestOp& Op);

private:
	struct ForwardSpawnRequestDeleter
	{
		void operator()(Schema_CommandRequest* Request) const noexcept
		{
			if (Request == nullptr)
			{
				return;
			}
			Schema_DestroyCommandRequest(Request);
		}
	};

	// Client
	SpatialGDK::SpawnPlayerRequest ObtainPlayerParams() const;

	// Authoritative server worker
	void FindPlayerStartAndProcessPlayerSpawn(Schema_Object* Request, const PhysicalWorkerName& ClientWorkerId);
	bool ForwardSpawnRequestToStrategizedServer(const Schema_Object* OriginalPlayerSpawnRequest, AActor* PlayerStart, const PhysicalWorkerName& ClientWorkerId);
	void RetryForwardSpawnPlayerRequest(const Worker_EntityId EntityId, const Worker_RequestId RequestId, const bool bShouldTryDifferentPlayerStart = false);

	// Any server
	void PassSpawnRequestToNetDriver(Schema_Object* PlayerSpawnData, AActor* PlayerStart);

	UPROPERTY()
	USpatialNetDriver* NetDriver;

	FTimerManager* TimerManager;
	int NumberOfAttempts;
	TMap<Worker_RequestId_Key, TUniquePtr<Schema_CommandRequest, ForwardSpawnRequestDeleter>> OutgoingForwardPlayerSpawnRequests;

	TSet<FString> WorkersWithPlayersSpawned;
};
