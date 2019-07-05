// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Schema/RPCPayload.h"
#include "Schema/UnrealObjectRef.h"
#include "SpatialConstants.h"

#include "UObject/WeakObjectPtr.h"
#include "CoreMinimal.h"

struct FUnrealObjectRef;
struct FPendingRPCParams;
using FPendingRPCParamsPtr = TUniquePtr<FPendingRPCParams>;
DECLARE_DELEGATE_RetVal_OneParam(bool, FProcessRPCDelegate, const FPendingRPCParams&)

struct FPendingRPCParams
{
	FPendingRPCParams(const FUnrealObjectRef& InTargetObjectRef, SpatialGDK::RPCPayload&& InPayload, int InReliableRPCIndex = 0);

	int ReliableRPCIndex;
	FUnrealObjectRef ObjectRef;
	SpatialGDK::RPCPayload Payload;
};

class FRPCContainer
{
public:
	void QueueRPC(FPendingRPCParamsPtr Params, ESchemaComponentType Type);
	void ProcessRPCs(const FProcessRPCDelegate& FunctionToApply);
	bool ObjectHasRPCsQueuedOfType(const FUnrealObjectRef& TargetObjectRef, ESchemaComponentType Type) const;

private:
	using FArrayOfParams = TArray<FPendingRPCParamsPtr>;
	using FRPCMap = TMap<FUnrealObjectRef, FArrayOfParams>;
	using RPCContainerType = TMap<ESchemaComponentType, FRPCMap>;

	void ProcessRPCs(const FProcessRPCDelegate& FunctionToApply, FArrayOfParams& RPCList);
	bool ApplyFunction(const FProcessRPCDelegate& FunctionToApply, const FPendingRPCParams& Params) const;

	RPCContainerType QueuedRPCs;
};
