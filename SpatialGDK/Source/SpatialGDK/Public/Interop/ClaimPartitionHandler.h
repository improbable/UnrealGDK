// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "SpatialCommonTypes.h"
#include "Templates/Function.h"

using FSystemEntityCommandDelegate = TFunction<void(const Worker_CommandResponseOp&)>;

namespace SpatialGDK
{
class ISpatialOSWorker;
class FClaimPartitionHandler
{
public:
	Worker_RequestId ClaimPartition(ISpatialOSWorker& WorkerInterface, Worker_EntityId SystemEntityId, Worker_PartitionId PartitionToClaim);
	Worker_RequestId ClaimPartition(ISpatialOSWorker& WorkerInterface, Worker_EntityId SystemEntityId, Worker_PartitionId PartitionToClaim,
									FSystemEntityCommandDelegate Delegate);

	void ProcessOps(const TArray<Worker_Op>& Ops);

private:
	struct FClaimPartitionRequest
	{
		Worker_PartitionId PartitionId;
		FSystemEntityCommandDelegate Delegate;
	};

	TMap<Worker_RequestId_Key, FClaimPartitionRequest> ClaimPartitionRequestIds;
};
} // namespace SpatialGDK
