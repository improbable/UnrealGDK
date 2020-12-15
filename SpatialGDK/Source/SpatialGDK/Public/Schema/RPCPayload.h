// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Schema/Component.h"
#include "SpatialConstants.h"
#include "Utils/SchemaUtils.h"
#include "Utils/SpatialLatencyTracer.h"

#include <WorkerSDK/improbable/c_schema.h>
#include <WorkerSDK/improbable/c_worker.h>

namespace SpatialGDK
{
struct RPCPayload
{
	RPCPayload() = delete;

	RPCPayload(uint32 InOffset, uint32 InIndex, TOptional<uint64> InId, TArray<uint8>&& Data, TraceKey InTraceKey = InvalidTraceKey)
		: Offset(InOffset)
		, Index(InIndex)
		, Id(InId)
		, PayloadData(MoveTemp(Data))
		, Trace(InTraceKey)
	{
	}

	RPCPayload(Schema_Object* RPCObject)
	{
		Offset = Schema_GetUint32(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_OFFSET_ID);
		Index = Schema_GetUint32(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_RPC_INDEX_ID);
		if (Schema_GetUint64Count(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_RPC_ID) > 0)
		{
			Id = Schema_GetUint64(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_RPC_ID);
		}

		PayloadData = GetBytesFromSchema(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_RPC_PAYLOAD_ID);

#if TRACE_LIB_ACTIVE
		if (USpatialLatencyTracer* Tracer = USpatialLatencyTracer::GetTracer(nullptr))
		{
			Trace = Tracer->ReadTraceFromSchemaObject(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_TRACE_ID);
		}
#endif
	}

	int64 CountDataBits() const { return PayloadData.Num() * 8; }

	void WriteToSchemaObject(Schema_Object* RPCObject) const
	{
		WriteToSchemaObject(RPCObject, Offset, Index, Id, PayloadData.GetData(), PayloadData.Num());

#if TRACE_LIB_ACTIVE
		if (USpatialLatencyTracer* Tracer = USpatialLatencyTracer::GetTracer(nullptr))
		{
			Tracer->WriteTraceToSchemaObject(Trace, RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_TRACE_ID);
		}
#endif
	}

	static void WriteToSchemaObject(Schema_Object* RPCObject, uint32 Offset, uint32 Index, TOptional<uint64> UniqueId, const uint8* Data,
									int32 NumElems)
	{
		Schema_AddUint32(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_OFFSET_ID, Offset);
		Schema_AddUint32(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_RPC_INDEX_ID, Index);
		if (UniqueId.IsSet())
		{
			Schema_AddUint64(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_RPC_ID, UniqueId.GetValue());
		}

		AddBytesToSchema(RPCObject, SpatialConstants::UNREAL_RPC_PAYLOAD_RPC_PAYLOAD_ID, Data, sizeof(uint8) * NumElems);
	}

	uint32 Offset;
	uint32 Index;
	TOptional<uint64> Id;
	TArray<uint8> PayloadData;
	TraceKey Trace = InvalidTraceKey;
};

} // namespace SpatialGDK
