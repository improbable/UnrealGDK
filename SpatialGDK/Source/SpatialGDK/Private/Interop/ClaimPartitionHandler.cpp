// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Interop/ClaimPartitionHandler.h"

#include "Schema/StandardLibrary.h"

#include "SpatialView/CommandRetryHandler.h"
#include "SpatialView/SpatialOSWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogClaimPartitionHandler, Log, All);

namespace SpatialGDK
{
	Worker_RequestId FClaimPartitionHandler::ClaimPartition(ISpatialOSWorker& WorkerInterface, Worker_EntityId SystemEntityId,
											Worker_PartitionId PartitionToClaim)
{
	UE_LOG(LogClaimPartitionHandler, Log,
		   TEXT("SendClaimPartitionRequest. SystemWorkerEntityId: %lld. "
				"PartitionId: %lld"),
		   SystemEntityId, PartitionToClaim);

	Worker_CommandRequest RequestData = Worker::CreateClaimPartitionRequest(PartitionToClaim);
	CommandRequest Request(OwningCommandRequestPtr(RequestData.schema_type), RequestData.component_id, RequestData.command_index);
	const Worker_RequestId ClaimEntityRequestId =
		WorkerInterface.SendEntityCommandRequest(SystemEntityId, MoveTemp(Request), RETRY_UNTIL_COMPLETE, {});
	FClaimPartitionRequest RequestEntry = { PartitionToClaim, FSystemEntityCommandDelegate() };
	ClaimPartitionRequestIds.Add(ClaimEntityRequestId, MoveTemp(RequestEntry));

	return ClaimEntityRequestId;
}

Worker_RequestId FClaimPartitionHandler::ClaimPartition(ISpatialOSWorker& WorkerInterface, Worker_EntityId SystemEntityId,
											Worker_PartitionId PartitionToClaim, FSystemEntityCommandDelegate Delegate)
{
	UE_LOG(LogClaimPartitionHandler, Log,
		   TEXT("SendClaimPartitionRequest. SystemWorkerEntityId: %lld. "
				"PartitionId: %lld"),
		   SystemEntityId, PartitionToClaim);

	Worker_CommandRequest RequestData = Worker::CreateClaimPartitionRequest(PartitionToClaim);
	CommandRequest Request(OwningCommandRequestPtr(RequestData.schema_type), RequestData.component_id, RequestData.command_index);
	const Worker_RequestId ClaimEntityRequestId =
		WorkerInterface.SendEntityCommandRequest(SystemEntityId, MoveTemp(Request), RETRY_UNTIL_COMPLETE, {});
	FClaimPartitionRequest RequestEntry = { PartitionToClaim, MoveTemp(Delegate) };
	ClaimPartitionRequestIds.Add(ClaimEntityRequestId, MoveTemp(RequestEntry));

	return ClaimEntityRequestId;
}

void FClaimPartitionHandler::ProcessOps(const TArray<Worker_Op>& Ops)
{
	for (const Worker_Op& Op : Ops)
	{
		if (Op.op_type == WORKER_OP_TYPE_COMMAND_RESPONSE)
		{
			const Worker_CommandResponseOp& CommandResponse = Op.op.command_response;
			FClaimPartitionRequest Request{ SpatialConstants::INVALID_PARTITION_ID, FSystemEntityCommandDelegate() };
			const bool bIsRequestHandled = ClaimPartitionRequestIds.RemoveAndCopyValue(CommandResponse.request_id, Request);
			if (bIsRequestHandled)
			{
				ensure(CommandResponse.response.component_id == SpatialConstants::WORKER_COMPONENT_ID);
				UE_CLOG(CommandResponse.status_code != WORKER_STATUS_CODE_SUCCESS, LogClaimPartitionHandler, Error,
						TEXT("Claim partition request for partition %lld finished, SDK returned code %d [%s]"), Request.PartitionId,
						(int)CommandResponse.status_code, UTF8_TO_TCHAR(CommandResponse.message));
				if (Request.Delegate)
				{
					Request.Delegate(CommandResponse);
				}
			}
		}
	}
}
} // namespace SpatialGDK
