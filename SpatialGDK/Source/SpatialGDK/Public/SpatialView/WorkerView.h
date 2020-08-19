// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "SpatialView/MessagesToSend.h"
#include "SpatialView/OpList/OpList.h"
#include "SpatialView/ViewDelta.h"

namespace SpatialGDK
{
class WorkerView
{
public:
	WorkerView();

	// Process op lists to create a new view delta.
	// The view delta will exist until the next call to AdvanceViewDelta.
	void AdvanceViewDelta(TArray<OpList> OpLists);

	ViewDelta& GetViewDelta();
	const ViewDelta& GetViewDelta() const;
	const EntityView& GetView() const;

	// Ensure all local changes have been applied and return the resulting MessagesToSend.
	TUniquePtr<MessagesToSend> FlushLocalChanges();

	void SendAddComponent(Worker_EntityId EntityId, ComponentData Data, const TOptional<Trace_SpanId>& SpanId);
	void SendComponentUpdate(Worker_EntityId EntityId, ComponentUpdate Update, const TOptional<Trace_SpanId>& SpanId);
	void SendRemoveComponent(Worker_EntityId EntityId, Worker_ComponentId ComponentId, const TOptional<Trace_SpanId>& SpanId);
	void SendReserveEntityIdsRequest(ReserveEntityIdsRequest Request);
	void SendCreateEntityRequest(CreateEntityRequest Request);
	void SendDeleteEntityRequest(DeleteEntityRequest Request);
	void SendEntityQueryRequest(EntityQueryRequest Request);
	void SendEntityCommandRequest(EntityCommandRequest Request);
	void SendEntityCommandResponse(EntityCommandResponse Response);
	void SendEntityCommandFailure(EntityCommandFailure Failure);
	void SendMetrics(SpatialMetrics Metrics);
	void SendLogMessage(LogMessage Log);

private:
	EntityView View;
	ViewDelta Delta;

	TUniquePtr<MessagesToSend> LocalChanges;
};
} // namespace SpatialGDK
