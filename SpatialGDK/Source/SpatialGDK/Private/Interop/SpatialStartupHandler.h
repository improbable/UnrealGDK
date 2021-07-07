﻿#pragma once

#include "EngineClasses/SpatialVirtualWorkerTranslator.h"
#include "Interop/Connection/SpatialWorkerConnection.h"
#include "Interop/GlobalStateManager.h"
#include "SpatialView/ViewCoordinator.h"

class USpatialNetDriver;

namespace SpatialGDK
{
class FSpatialStartupHandler
{
public:
	struct FInitialSetup
	{
		int32 ExpectedServerWorkersCount;
	};
	explicit FSpatialStartupHandler(USpatialNetDriver& InNetDriver, const FInitialSetup& InSetup);
	bool TryFinishStartup();
	void SpawnPartitionEntity(Worker_EntityId PartitionEntityId, VirtualWorkerId VirtualWorkerId);

private:
	bool TryClaimingStartupPartition();

	bool bCalledCreateEntity = false;
	TOptional<ServerWorkerEntityCreator> ServerWorkerEntityCreator1;
	Worker_EntityId WorkerEntityId;

	TArray<Worker_EntityId_Key> WorkerEntityIds;

	bool bHasGSMAuth = false;

	bool bHasCalledPartitionEntityCreate = false;
	CreateEntityHandler EntityHandler;
	TArray<Worker_PartitionId> WorkerPartitions;

	TMap<VirtualWorkerId, SpatialVirtualWorkerTranslator::WorkerInformation> WorkersToPartitions;
	TSet<Worker_PartitionId> PartitionsToCreate;

	VirtualWorkerId LocalVirtualWorkerId;
	Worker_PartitionId LocalPartitionId;

	ClaimPartitionHandler ClaimHandler;

	ViewCoordinator& GetCoordinator();
	const ViewCoordinator& GetCoordinator() const;
	const TArray<Worker_Op>& GetOps() const;
	UGlobalStateManager& GetGSM();

	enum class EStage : uint8
	{
		CreateWorkerEntity,
		WaitForWorkerEntities,
		WaitForGSMEntity,
		TryClaimingGSMEntityAuthority,
		WaitForGSMEntityAuthority,
		GetVirtualWorkerTranslationState,

		FillWorkerTranslationState,

		DispatchGSMStartPlay,

		WaitForAssignedPartition,
		WaitForAssignedPartition2,
		WaitForGSMStartPlay,

		Finished,
		Initial = CreateWorkerEntity,
	};

	EStage Stage = EStage::Initial;

	FInitialSetup Setup;

	USpatialNetDriver* NetDriver;
};

class FSpatialClientStartupHandler
{
public:
	struct FInitialSetup
	{
		int32 ExpectedServerWorkersCount;
	};
	explicit FSpatialClientStartupHandler(USpatialNetDriver& InNetDriver, UGameInstance& InGameInstance, const FInitialSetup& InSetup);
	bool TryFinishStartup();
	void QueryGSM();
	ViewCoordinator& GetCoordinator();
	const ViewCoordinator& GetCoordinator() const;
	const TArray<Worker_Op>& GetOps() const;

private:
	bool bQueriedGSM = false;

	EntityQueryHandler QueryHandler;
	struct FDeploymentMapData
	{
		FString DeploymentMapURL;

		bool bAcceptingPlayers;

		int32 DeploymentSessionId;

		uint32 SchemaHash;
	};
	TOptional<FDeploymentMapData> GSMData;
	static bool GetFromComponentData(const Worker_ComponentData& Component, FDeploymentMapData& OutData);

	struct FSnapshotData
	{
		uint64 SnapshotVersion;
	};
	TOptional<FSnapshotData> SnapshotData;
	static bool GetFromComponentData(const Worker_ComponentData& Component, FSnapshotData& OutData);

	bool bFinishedMapLoad = false;
	FDelegateHandle PostMapLoadedDelegateHandle;
	void OnMapLoaded(UWorld* LoadedWorld);

	enum class EStage : uint8
	{
		QueryGSM,
		WaitForMapLoad,
		SendPlayerSpawnRequest,
		Finished,
		Initial = QueryGSM,
	};

	FInitialSetup Setup;

	EStage Stage = EStage::Initial;

	USpatialNetDriver* NetDriver;
	UGameInstance* GameInstance;
};

} // namespace SpatialGDK
