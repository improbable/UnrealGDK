// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "UObject/NoExportTypes.h"

#include <WorkerSDK/improbable/c_schema.h>
#include <WorkerSDK/improbable/c_worker.h>

#include "EntityQueryHandler.h"
#include "Interop/EntityCommandHandler.h"
#include "Interop/SpatialCommandsHandler.h"
#include "Utils/SchemaUtils.h"

#include "GlobalStateManager.generated.h"

class USpatialNetDriver;
class USpatialActorChannel;
class USpatialStaticComponentView;
class USpatialSender;
class USpatialReceiver;

namespace SpatialGDK
{
class ViewCoordinator;
}

DECLARE_LOG_CATEGORY_EXTERN(LogGlobalStateManager, Log, All)

UCLASS()
class SPATIALGDK_API UGlobalStateManager : public UObject
{
	GENERATED_BODY()

public:
	void Init(USpatialNetDriver* InNetDriver);

	void ApplyDeploymentMapData(Schema_ComponentData* Data);
	void ApplySnapshotVersionData(Schema_ComponentData* Data);
	void ApplyStartupActorManagerData(Schema_ComponentData* Data);
	void WorkerEntityReady();

	void ApplySessionId(int32 InSessionId);

	void ApplyDeploymentMapUpdate(Schema_ComponentUpdate* Update);
	void ApplyStartupActorManagerUpdate(Schema_ComponentUpdate* Update);
	bool HasAuthority() const;

	DECLARE_DELEGATE_OneParam(QueryDelegate, const Worker_EntityQueryResponseOp&);
	void QueryGSM(const QueryDelegate& Callback);
	static bool GetAcceptingPlayersAndSessionIdFromQueryResponse(const Worker_EntityQueryResponseOp& Op, bool& OutAcceptingPlayers,
																 int32& OutSessionId);
	void ApplyVirtualWorkerMappingFromQueryResponse(const Worker_EntityQueryResponseOp& Op) const;
	void ApplyDataFromQueryResponse(const Worker_EntityQueryResponseOp& Op);

	void QueryTranslation();

	void SetDeploymentState();
	void SetAcceptingPlayers(bool bAcceptingPlayers);
	void IncrementSessionID();

	void SendCanBeginPlayUpdate(const bool bInCanBeginPlay);

	void Advance();

	FORCEINLINE FString GetDeploymentMapURL() const { return DeploymentMapURL; }
	FORCEINLINE bool GetAcceptingPlayers() const { return bAcceptingPlayers; }
	FORCEINLINE int32 GetSessionId() const { return DeploymentSessionId; }
	FORCEINLINE uint32 GetSchemaHash() const { return SchemaHash; }
	FORCEINLINE uint64 GetSnapshotVersion() const { return SnapshotVersion; }

	void AuthorityChanged(const Worker_ComponentSetAuthorityChangeOp& AuthChangeOp);

	void ResetGSM();

	void BeginDestroy() override;

	void TrySendWorkerReadyToBeginPlay();
	void TriggerBeginPlay();
	bool GetCanBeginPlay() const;

	bool IsReady() const;

	void HandleActorBasedOnLoadBalancer(AActor* ActorIterator) const;

	Worker_EntityId GetLocalServerWorkerEntityId() const;
	void ClaimSnapshotPartition();

	Worker_EntityId GlobalStateManagerEntityId;

	// Startup Actor Manager Component
	bool bCanSpawnWithAuthority;

private:
	// Deployment Map Component
	FString DeploymentMapURL;
	bool bAcceptingPlayers;
	int32 DeploymentSessionId = 0;
	uint32 SchemaHash;
	uint64 SnapshotVersion = 0;

	// Startup Actor Manager Component
	bool bHasReceivedStartupActorData;
	bool bWorkerEntityReady;
	bool bHasSentReadyForVirtualWorkerAssignment;
	bool bCanBeginPlay;

public:
#if WITH_EDITOR
	void OnPrePIEEnded(bool bValue);
	void ReceiveShutdownMultiProcessRequest();

	void OnReceiveShutdownCommand(const Worker_Op& Op, const Worker_CommandRequestOp& CommandRequestOp);

	void OnShutdownComponentUpdate(Schema_ComponentUpdate* Update);
	void ReceiveShutdownAdditionalServersEvent();
#endif // WITH_EDITOR

private:
	void SendSessionIdUpdate();

#if WITH_EDITOR
	void SendShutdownMultiProcessRequest();
	void SendShutdownAdditionalServersEvent();
#endif // WITH_EDITOR

private:
	UPROPERTY()
	USpatialNetDriver* NetDriver;

	SpatialGDK::ViewCoordinator* ViewCoordinator;

	SpatialGDK::FCommandsHandler CommandsHandler;

#if WITH_EDITOR
	SpatialGDK::EntityCommandRequestHandler RequestHandler;

	FDelegateHandle PrePIEEndedHandle;
#endif // WITH_EDITOR

	bool bTranslationQueryInFlight;
};
