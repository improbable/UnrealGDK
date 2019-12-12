// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "Interop/SpatialStaticComponentView.h"
#include "SpatialCommonTypes.h"

#include <WorkerSDK/improbable/c_worker.h>

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialLoadBalanceEnforcer, Log, All)

class SpatialVirtualWorkerTranslator;
class USpatialSender;
class USpatialStaticComponentView;

class SpatialLoadBalanceEnforcer
{
public:
	SpatialLoadBalanceEnforcer();

	void Init(const PhysicalWorkerName &InWorkerId, USpatialStaticComponentView* InStaticComponentView, USpatialSender* InSpatialSender, SpatialVirtualWorkerTranslator* InVirtualWorkerTranslator);
	void Tick();

	void AuthorityChanged(const Worker_AuthorityChangeOp& AuthOp);
	void QueueAclAssignmentRequest(const Worker_EntityId EntityId);

	void OnAuthorityIntentComponentUpdated(const Worker_ComponentUpdateOp& Op);

private:

	PhysicalWorkerName WorkerId;
	TWeakObjectPtr<USpatialStaticComponentView> StaticComponentView;
	TWeakObjectPtr<USpatialSender> Sender;
	SpatialVirtualWorkerTranslator* VirtualWorkerTranslator;

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

	void ProcessQueuedAclAssignmentRequests();
};
