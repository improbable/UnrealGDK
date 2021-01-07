﻿// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "RPCExecutorInterface.h"
#include "SpatialView/ViewCoordinator.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCrossServerRPCHandler, Log, All);

namespace SpatialGDK
{
DECLARE_DELEGATE_RetVal_OneParam(bool, FProcessCrossServerRPC, const FCrossServerRPCParams&);
DECLARE_DELEGATE_RetVal_OneParam(FCrossServerRPCParams, FTryRetrieveCrossServerRPCParams, const Worker_CommandRequestOp&);

static double CrossServerRPCGuidTimeout = 5.f;

class CrossServerRPCHandler
{
public:
	CrossServerRPCHandler(ViewCoordinator& Coordinator, TUniquePtr<RPCExecutorInterface> RPCExecutor);

	void ProcessMessages(const TArray<Worker_Op>& WorkerMessages, float DeltaTime);
	void ProcessPendingCrossServerRPCs();
	const TMap<Worker_EntityId_Key, TArray<FCrossServerRPCParams>>& GetQueuedCrossServerRPCs() const;
	int32 GetRPCGuidsInFlightCount() const;
	int32 GetRPCsToDeleteCount() const;

private:
	ViewCoordinator* Coordinator;
	TUniquePtr<RPCExecutorInterface> RPCExecutor;
	TSet<uint32> RPCGuidsInFlight;
	TArray<TTuple<double, uint32>> RPCsToDelete;

	double CurrentTime = 0.f;
	TMap<Worker_EntityId_Key, TArray<FCrossServerRPCParams>> QueuedCrossServerRPCs;
	FProcessCrossServerRPC ProcessCrossServerRPCDelegate;
	FTryRetrieveCrossServerRPCParams TryRetrieveCrossServerRPCParamsDelegate;

	void HandleWorkerOp(const Worker_Op& Op);
	bool TryExecuteCrossServerRPC(const FCrossServerRPCParams& Params) const;
	void DropQueueForEntity(const Worker_EntityId_Key EntityId);
};
} // namespace SpatialGDK
