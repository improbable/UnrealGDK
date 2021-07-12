// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Schema/UnrealMetadata.h"
#include "SpatialCommonTypes.h"
#include "Utils/SpatialStatics.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEntityFactory, Log, All);

class AActor;
class UAbstractLBStrategy;
class USpatialActorChannel;
class USpatialNetDriver;
class USpatialPackageMap;
class USpatialClassInfoManager;
class USpatialPackageMapClient;

namespace SpatialGDK
{
class InterestFactory;
class SpatialRPCService;
struct QueryConstraint;

class SPATIALGDK_API EntityFactory
{
public:
	EntityFactory(USpatialNetDriver* InNetDriver, USpatialPackageMapClient* InPackageMap, USpatialClassInfoManager* InClassInfoManager,
				  SpatialRPCService* InRPCService);

	// The philosophy behind having this function is to have a minimal set of SpatialOS components associated with an Unreal actor.
	// This should primarily be enough to reason about the actor's identity and possibly inform some level of load-balancing.
	static TArray<FWorkerComponentData> CreateMinimalEntityComponents(AActor* Actor);
	void WriteUnrealComponents(TArray<FWorkerComponentData>& ComponentDatas, USpatialActorChannel* Channel, uint32& OutBytesWritten);
	void WriteLBComponents(TArray<FWorkerComponentData>& ComponentDatas, AActor* Actor);
	void WriteRPCComponents(TArray<FWorkerComponentData>& ComponentDatas, USpatialActorChannel& Channel);
	TArray<FWorkerComponentData> CreateEntityComponents(USpatialActorChannel* Channel, uint32& OutBytesWritten);
	TArray<FWorkerComponentData> CreateSkeletonEntityComponents(AActor* Actor);
	TArray<FWorkerComponentData> CreateTombstoneEntityComponents(AActor* Actor) const;
	void CreatePopulateSkeletonComponents(USpatialActorChannel& ActorChannel, TArray<FWorkerComponentData>& OutComponentCreates,
										  TArray<FWorkerComponentUpdate>& OutComponentUpdates, uint32& OutBytesWritten);
	static UnrealMetadata CreateMetadata(const AActor& InActor);

	static TArray<FWorkerComponentData> CreatePartitionEntityComponents(const FString& PartitionName, const Worker_EntityId EntityId,
																		const InterestFactory* InterestFactory,
																		const SpatialGDK::QueryConstraint& LoadBalancingConstraint,
																		VirtualWorkerId VirtualWorker, bool bDebugContexValid);

	static inline bool IsClientAuthoritativeComponent(Worker_ComponentId ComponentId)
	{
		return ComponentId == SpatialConstants::CLIENT_ENDPOINT_COMPONENT_ID
			   || ComponentId == SpatialConstants::PLAYER_CONTROLLER_COMPONENT_ID;
	}

private:
	void CheckStablyNamedActorPath(const TArray<FWorkerComponentData>& ComponentDatas, const AActor* Actor, Worker_EntityId EntityId) const;

	USpatialNetDriver* NetDriver;
	USpatialPackageMapClient* PackageMap;
	USpatialClassInfoManager* ClassInfoManager;
	SpatialRPCService* RPCService;
};
} // namespace SpatialGDK
