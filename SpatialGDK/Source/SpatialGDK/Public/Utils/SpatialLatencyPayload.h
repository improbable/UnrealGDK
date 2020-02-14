// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Hash/CityHash.h"

#include "SpatialLatencyPayload.generated.h"

USTRUCT(BlueprintType)
struct SPATIALGDK_API FSpatialLatencyPayload 
{
	GENERATED_BODY()

	FSpatialLatencyPayload() {}

	FSpatialLatencyPayload(TArray<uint8>&& TraceBytes, TArray<uint8>&& SpanBytes)
		: TraceId(MoveTemp(TraceBytes))
		, SpanId(MoveTemp(SpanBytes))
	{}

	UPROPERTY()
	TArray<uint8> TraceId;

	UPROPERTY()
	TArray<uint8> SpanId;

	// Required for TMap hash
	bool operator== (const FSpatialLatencyPayload& Other) const
	{
		return TraceId == Other.TraceId && SpanId == Other.SpanId;
	}
	friend uint32 GetTypeHash(const FSpatialLatencyPayload& Obj)
	{
		return CityHash32((const char*)Obj.TraceId.GetData(), Obj.TraceId.Num()) ^ CityHash32((const char*)Obj.SpanId.GetData(), Obj.SpanId.Num());
	}
};
