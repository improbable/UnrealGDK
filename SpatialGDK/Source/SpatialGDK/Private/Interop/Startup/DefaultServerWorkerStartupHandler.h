﻿#pragma once

#include "EngineClasses/SpatialVirtualWorkerTranslator.h"
#include "Interop/ClaimPartitionHandler.h"
#include "Interop/Connection/SpatialWorkerConnection.h"
#include "Interop/CreateEntityHandler.h"
#include "Interop/SkeletonEntityCreationStep.h"
#include "SpatialCommonTypes.h"

class USpatialNetDriver;
class UGlobalStateManager;

namespace SpatialGDK
{
class ViewCoordinator;

struct FStartupStep
{
	using FOnStepStarted = TUniqueFunction<void()>;
	using FTryFinishStep = TUniqueFunction<bool()>;

	FOnStepStarted OnStepStarted;
	FTryFinishStep TryFinishStep;
};

class FStartupExecutor
{
public:
	explicit FStartupExecutor(TArray<FStartupStep> InSteps);
	bool TryFinishStartup();

private:
	TArray<FStartupStep> Steps;
};

class FSpatialServerStartupHandler
{
public:
	struct FInitialSetup
	{
		int32 ExpectedServerWorkersCount;
	};
	explicit FSpatialServerStartupHandler(USpatialNetDriver& InNetDriver, const FInitialSetup& InSetup);
	bool TryFinishStartup();
	FString GetStartupStateDescription() const;

private:
	FStartupExecutor Executor;
	void SpawnPartitionEntity(Worker_EntityId PartitionEntityId, VirtualWorkerId VirtualWorkerId);
	bool TryClaimingStartupPartition();

	bool bCalledCreateEntity = false;
	TOptional<ServerWorkerEntityCreator> WorkerEntityCreator;
	TOptional<Worker_EntityId> WorkerEntityId;

	TArray<Worker_EntityId_Key> WorkerEntityIds;

	bool bHasGSMAuth = false;

	bool bIsRecoveringOrSnapshot = false;

	bool bHasCalledPartitionEntityCreate = false;
	FCreateEntityHandler EntityHandler;
	TArray<Worker_PartitionId> WorkerPartitions;

	TMap<VirtualWorkerId, SpatialVirtualWorkerTranslator::WorkerInformation> WorkersToPartitions;
	TSet<Worker_PartitionId> PartitionsToCreate;

	TOptional<VirtualWorkerId> LocalVirtualWorkerId;
	TOptional<Worker_PartitionId> LocalPartitionId;

	FClaimPartitionHandler ClaimHandler;

	TOptional<FSkeletonEntityCreationStartupStep> SkeletonEntityStep;

	ViewCoordinator& GetCoordinator();
	const ViewCoordinator& GetCoordinator() const;
	const TArray<Worker_Op>& GetOps() const;
	UGlobalStateManager& GetGSM();

	enum class EStage : uint8
	{
		CreateWorkerEntity,
		WaitForWorkerEntities,
		WaitForGSMEntity,
		DeriveDeploymentRecoveryState,
		TryClaimingGSMEntityAuthority,
		WaitForGSMEntityAuthority,

		GsmAuthFillWorkerTranslationState,
		GsmAuthCreatePartitions,
		GsmAuthWaitForPartitionsVisibility,
		GsmAuthAssignPartitionsToVirtualWorkers,

		GetVirtualWorkerTranslationState,
		WaitForAssignedPartition,

		CreateSkeletonEntities,

		GsmAuthDispatchGSMStartPlay,
		GsmNonAuthWaitForGSMStartPlay,

		Finished,
		Initial = CreateWorkerEntity,
	};

	static TArray<FStartupStep> CreateSteps(USpatialNetDriver& InNetDriver, const FInitialSetup& InSetup,
											TOptional<Worker_EntityId>& WorkerEntityIdAddress, EStage& StageRef);

	EStage Stage = EStage::Initial;

	FInitialSetup Setup;

	USpatialNetDriver* NetDriver;
};
} // namespace SpatialGDK
