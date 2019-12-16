// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include <WorkerSDK/improbable/c_worker.h>

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialLoadBalanceEnforcer, Log, All)

class SpatialVirtualWorkerTranslator;
class USpatialStaticComponentView;

class SpatialLoadBalanceEnforcer
{
public:
	struct AclWriteAuthorityRequest
	{
		Worker_EntityId EntityId = 0;
		FString OwningWorkerId;
	};

	SpatialLoadBalanceEnforcer(const FString &InWorkerId, USpatialStaticComponentView* InStaticComponentView, SpatialVirtualWorkerTranslator* InVirtualWorkerTranslator);	

	void AuthorityChanged(const Worker_AuthorityChangeOp& AuthOp);
	void QueueAclAssignmentRequest(const Worker_EntityId EntityId);

	void OnAuthorityIntentComponentUpdated(const Worker_ComponentUpdateOp& Op);

	TArray<AclWriteAuthorityRequest> ProcessQueuedAclAssignmentRequests();

private:

	const FString WorkerId;
	TWeakObjectPtr<USpatialStaticComponentView> StaticComponentView;
	const SpatialVirtualWorkerTranslator* VirtualWorkerTranslator;

	struct WriteAuthAssignmentRequest
	{
		WriteAuthAssignmentRequest(Worker_EntityId InputEntityId)
			: EntityId(InputEntityId)
			, ProcessAttempts(0)
		{}
		Worker_EntityId EntityId;
		int32 ProcessAttempts;
	};

	TArray<WriteAuthAssignmentRequest> AclWriteAuthAssignmentRequests;
};
