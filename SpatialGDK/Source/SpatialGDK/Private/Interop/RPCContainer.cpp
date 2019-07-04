// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "RPCContainer.h"

#include "Schema/UnrealObjectRef.h"

using namespace SpatialGDK;

FPendingRPCParams::FPendingRPCParams(const FUnrealObjectRef& InTargetObjectRef, SpatialGDK::RPCPayload&& InPayload, int InReliableRPCIndex /* = 0 */)
	//: Function(InFunction)
	: ReliableRPCIndex(InReliableRPCIndex)
	, ObjectRef(InTargetObjectRef)
	, Payload(MoveTemp(InPayload))
{
}

void FRPCContainer::QueueRPC(const FUnrealObjectRef& TargetObjectRef, FPendingRPCParamsPtr Params, ESchemaComponentType Type)
{
	FArrayOfParams& ArrayOfParams = QueuedRPCs.FindOrAdd(Type).FindOrAdd(TargetObjectRef);
	ArrayOfParams.Push(Params);
}

void FRPCContainer::ProcessRPCs(const FProcessRPCDelegate& FunctionToApply, FArrayOfParams& RPCList)
{
	int NumProcessedParams = 0;
	for (auto& Params : RPCList)
	{
		// TODO(Alex): Delete deprecated RPCS somehow
		//if (!Params->TargetObject.IsValid())
		//{
		//	// The target object was destroyed before we could send the RPC.
		//	RPCList.Empty();
		//	break;
		//}
		if (ApplyFunction(FunctionToApply, Params))
		{
			NumProcessedParams++;
		}
		else
		{
			break;
		}
	}
	RPCList.RemoveAt(0, NumProcessedParams);
}

void FRPCContainer::ProcessRPCs(const FProcessRPCDelegate& FunctionToApply)
{
	for (auto& RPCs : QueuedRPCs)
	{
		FRPCMap& MapOfQueues = RPCs.Value;
		for(auto It = MapOfQueues.CreateIterator(); It; ++It)
		{
			FArrayOfParams& RPCList = It.Value();
			ProcessRPCs(FunctionToApply, RPCList);
			if (RPCList.Num() == 0)
			{
				It.RemoveCurrent();
			}
		}
	}
}

bool FRPCContainer::ObjectHasRPCsQueuedOfType(const FUnrealObjectRef& TargetObjectRef, ESchemaComponentType Type) const
{
	const FRPCMap* MapOfQueues = QueuedRPCs.Find(Type);
	if(MapOfQueues)
	{
		const FArrayOfParams* RPCList = MapOfQueues->Find(TargetObjectRef);
		if(RPCList)
		{
			return (RPCList->Num() > 0);
		}
	}

	return false;
}

bool FRPCContainer::ApplyFunction(const FProcessRPCDelegate& FunctionToApply, FPendingRPCParamsPtr Params)
{
	return FunctionToApply.Execute(Params);
}
