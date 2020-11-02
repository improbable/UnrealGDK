﻿// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialView/OpList/EntityComponentOpList.h"

#include "SpatialView/OpList/StringStorage.h"

namespace SpatialGDK
{
EntityComponentOpListBuilder::EntityComponentOpListBuilder()
	: OpListData(MakeUnique<EntityComponentOpListData>())
{
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::AddEntity(Worker_EntityId EntityId)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_ADD_ENTITY;
	Op.op.add_entity.entity_id = EntityId;

	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::RemoveEntity(Worker_EntityId EntityId)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_REMOVE_ENTITY;
	Op.op.remove_entity.entity_id = EntityId;

	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::AddComponent(Worker_EntityId EntityId, ComponentData Data)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_ADD_COMPONENT;
	Op.op.add_component.entity_id = EntityId;
	Op.op.add_component.data = Data.GetWorkerComponentData();
	OpListData->DataStorage.Emplace(MoveTemp(Data));

	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::UpdateComponent(Worker_EntityId EntityId, ComponentUpdate Update)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_COMPONENT_UPDATE;
	Op.op.component_update.entity_id = EntityId;
	Op.op.component_update.update = Update.GetWorkerComponentUpdate();
	OpListData->UpdateStorage.Emplace(MoveTemp(Update));

	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::RemoveComponent(Worker_EntityId EntityId, Worker_ComponentId ComponentId)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_REMOVE_COMPONENT;
	Op.op.remove_component.entity_id = EntityId;
	Op.op.remove_component.component_id = ComponentId;

	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::SetAuthority(Worker_EntityId EntityId, Worker_ComponentId ComponentId,
																		 Worker_Authority Authority)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_AUTHORITY_CHANGE;
	Op.op.authority_change.entity_id = EntityId;
	Op.op.authority_change.component_id = ComponentId;
	Op.op.authority_change.authority = Authority;

	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::SetDisconnect(Worker_ConnectionStatusCode StatusCode,
																		  const FString& DisconnectReason)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_DISCONNECT;
	Op.op.disconnect.connection_status_code = StatusCode;
	Op.op.disconnect.reason = StoreString(DisconnectReason);
	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::AddCreateEntityCommandResponse(Worker_EntityId EntityID,
																						   Worker_RequestId RequestId,
																						   Worker_StatusCode StatusCode,
																						   const FString& Message)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_CREATE_ENTITY_RESPONSE;
	Op.op.create_entity_response.entity_id = EntityID;
	Op.op.create_entity_response.request_id = RequestId;
	Op.op.create_entity_response.status_code = StatusCode;
	Op.op.create_entity_response.message = StoreString(Message);
	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::AddReserveEntityIdsCommandResponse(
	Worker_EntityId EntityID, uint32 NumberOfEntities, Worker_RequestId RequestId, Worker_StatusCode StatusCode, const FString& Message)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_RESERVE_ENTITY_IDS_RESPONSE;
	Op.op.reserve_entity_ids_response.first_entity_id = EntityID;
	Op.op.reserve_entity_ids_response.number_of_entity_ids = NumberOfEntities;
	Op.op.reserve_entity_ids_response.request_id = RequestId;
	Op.op.reserve_entity_ids_response.status_code = StatusCode;
	Op.op.reserve_entity_ids_response.message = StoreString(Message);
	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::AddDeleteEntityCommandResponse(Worker_EntityId EntityID,
																						   Worker_RequestId RequestId,
																						   Worker_StatusCode StatusCode,
																						   const FString& Message)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_DELETE_ENTITY_RESPONSE;
	Op.op.delete_entity_response.entity_id = EntityID;
	Op.op.delete_entity_response.request_id = RequestId;
	Op.op.delete_entity_response.status_code = StatusCode;
	Op.op.delete_entity_response.message = StoreString(Message);
	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::AddEntityQueryCommandResponse(Worker_RequestId RequestId,
																						  TArray<Worker_Entity> Results,
																						  Worker_StatusCode StatusCode,
																						  const FString& Message)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_ENTITY_QUERY_RESPONSE;
	Op.op.entity_query_response.result_count = Results.Num();
	Op.op.entity_query_response.results = StoreQueriedEntities(MoveTemp(Results));
	Op.op.entity_query_response.request_id = RequestId;
	Op.op.entity_query_response.status_code = StatusCode;
	Op.op.entity_query_response.message = StoreString(Message);
	OpListData->Ops.Add(Op);
	return *this;
}

EntityComponentOpListBuilder& EntityComponentOpListBuilder::AddEntityCommandResponse(Worker_EntityId EntityID, Worker_RequestId RequestId,
																					 Worker_StatusCode StatusCode, const FString& Message)
{
	Worker_Op Op = {};
	Op.op_type = WORKER_OP_TYPE_COMMAND_RESPONSE;
	Op.op.command_response.entity_id = EntityID;
	Op.op.command_response.request_id = RequestId;
	Op.op.command_response.status_code = StatusCode;
	Op.op.command_response.message = StoreString(Message);
	OpListData->Ops.Add(Op);
	return *this;
}

const char* EntityComponentOpListBuilder::StoreString(FString Message) const
{
	OpListData->MessageStorage.Add(StringStorage(Message));
	return OpListData->MessageStorage.Last().Get();
}

const Worker_Entity* EntityComponentOpListBuilder::StoreQueriedEntities(TArray<Worker_Entity> Entities) const
{
	OpListData->QueriedEntities.Push(TArray<Worker_Entity>());
	for (auto& Entity : Entities)
	{
		Worker_Entity CurrentEntity;
		CurrentEntity.entity_id = Entity.entity_id;
		OpListData->QueriedComponents.Push(TArray<Worker_ComponentData>());
		for (auto p = Entity.components; p < Entity.components + Entity.component_count; ++p)
		{
			Worker_ComponentData Data = { nullptr, p->component_id, Schema_CopyComponentData(p->schema_type), nullptr };
			OpListData->QueriedComponents.Last().Push(Data);
		}
		CurrentEntity.components = OpListData->QueriedComponents.Last().GetData();
		CurrentEntity.component_count = OpListData->QueriedComponents.Last().Num();
		OpListData->QueriedEntities.Last().Push(CurrentEntity);
	}

	return OpListData->QueriedEntities.Last().GetData();
}

OpList EntityComponentOpListBuilder::CreateOpList() &&
{
	return { OpListData->Ops.GetData(), static_cast<uint32>(OpListData->Ops.Num()), MoveTemp(OpListData) };
}
} // namespace SpatialGDK
