// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "SpatialConstants.h"
#include "SpatialView/ComponentData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInitialOnlyFilter, Log, All);

class USpatialWorkerConnection;
class USpatialReceiver;

namespace SpatialGDK
{
class InitialOnlyFilter
{
public:
	InitialOnlyFilter(USpatialWorkerConnection& InConnection, USpatialReceiver& InReceiver);

	bool HasInitialOnlyData(Worker_EntityId EntityId) const;
	bool HasInitialOnlyDataOrRequestIfAbsent(Worker_EntityId EntityId);
	void FlushRequests();
	void HandleInitialOnlyResponse(const Worker_EntityQueryResponseOp& Op);
	const TArray<ComponentData>* GetInitialOnlyData(Worker_EntityId EntityId) const;
	void RemoveInitialOnlyData(Worker_EntityId EntityId);

private:
	void ClearRequest(Worker_RequestId RequestId);

	USpatialWorkerConnection& Connection;
	USpatialReceiver& Receiver;

	TSet<Worker_EntityId_Key> PendingInitialOnlyEntities;
	TSet<Worker_EntityId_Key> InflightInitialOnlyEntities;
	TMap<Worker_RequestId_Key, TSet<Worker_EntityId_Key>> InflightInitialOnlyRequests;
	TMap<Worker_EntityId_Key, TArray<ComponentData>> RetrievedInitialOnlyData;
};
} // namespace SpatialGDK
