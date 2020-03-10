// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Utils/InterestFactory.h"

#include "EngineClasses/Components/ActorInterestComponent.h"
#include "EngineClasses/SpatialNetConnection.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "LoadBalancing/AbstractLBStrategy.h"
#include "SpatialGDKSettings.h"
#include "SpatialConstants.h"
#include "Utils/CheckoutRadiusConstraintUtils.h"

#include "Engine/World.h"
#include "Engine/Classes/GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY(LogInterestFactory);

DECLARE_STATS_GROUP(TEXT("InterestFactory"), STATGROUP_SpatialInterestFactory, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("AddUserDefinedQueries"), STAT_InterestFactoryAddUserDefinedQueries, STATGROUP_SpatialInterestFactory);

namespace SpatialGDK
{
struct FrequencyConstraint
{
	float Frequency;
	SpatialGDK::QueryConstraint Constraint;
};
// Used to cache checkout radius constraints with frequency settings, so queries can be quickly recreated.
static TArray<FrequencyConstraint> CheckoutConstraints;

// The checkout radius constraint is built once for all actors in CreateCheckoutRadiusConstraint as it is equivalent for all actors.
// It is built once per net driver initialization.
static QueryConstraint ClientCheckoutRadiusConstraint;

// Cache the result types of queries.
static ResultType ClientNonAuthInterestResultType;
static ResultType ClientAuthInterestResultType;
static ResultType ServerNonAuthInterestResultType;
static ResultType ServerAuthInterestResultType;

InterestFactory::InterestFactory(AActor* InActor, const FClassInfo& InInfo, const Worker_EntityId InEntityId, USpatialClassInfoManager* InClassInfoManager, USpatialPackageMapClient* InPackageMap)
	: Actor(InActor)
	, Info(InInfo)
	, EntityId(InEntityId)
	, ClassInfoManager(InClassInfoManager)
	, PackageMap(InPackageMap)
{
}

void InterestFactory::CreateAndCacheInterestState(USpatialClassInfoManager* ClassInfoManager)
{
	ClientCheckoutRadiusConstraint = CreateClientCheckoutRadiusConstraint(ClassInfoManager);
	ClientNonAuthInterestResultType = CreateClientNonAuthInterestResultType(ClassInfoManager);
	ClientAuthInterestResultType = CreateClientAuthInterestResultType(ClassInfoManager);
	ServerNonAuthInterestResultType = CreateServerNonAuthInterestResultType(ClassInfoManager);
	ServerAuthInterestResultType = CreateServerAuthInterestResultType(ClassInfoManager);
}

QueryConstraint InterestFactory::CreateClientCheckoutRadiusConstraint(USpatialClassInfoManager* ClassInfoManager)
{
	const USpatialGDKSettings* SpatialGDKSettings = GetDefault<USpatialGDKSettings>();
	QueryConstraint CheckoutRadiusConstraint;
	CheckoutConstraints.Empty();

	if (!SpatialGDKSettings->bEnableNetCullDistanceInterest)
	{
		CheckoutRadiusConstraint = CreateLegacyNetCullDistanceConstraint(ClassInfoManager);
	}
	else
	{
		if (!SpatialGDKSettings->bEnableNetCullDistanceFrequency)
		{
			CheckoutRadiusConstraint = CreateNetCullDistanceConstraint(ClassInfoManager);
		}
		else
		{
			CheckoutRadiusConstraint = CreateNetCullDistanceConstraintWithFrequency(ClassInfoManager);
		}
	}

	return CheckoutRadiusConstraint;
}

QueryConstraint InterestFactory::CreateLegacyNetCullDistanceConstraint(USpatialClassInfoManager* ClassInfoManager)
{
	// Checkout Radius constraints are defined by the NetCullDistanceSquared property on actors.
	//   - Checkout radius is a RelativeCylinder constraint on the player controller.
	//   - NetCullDistanceSquared on AActor is used to define the default checkout radius with no other constraints.
	//   - NetCullDistanceSquared on other actor types is used to define additional constraints if needed.
	//   - If a subtype defines a radius smaller than a parent type, then its requirements are already captured.
	//   - If a subtype defines a radius larger than all parent types, then it needs an additional constraint.
	//   - Other than the default from AActor, all radius constraints also include Component constraints to
	//     capture specific types, including all derived types of that actor.

	QueryConstraint CheckoutRadiusConstraint;

	CheckoutRadiusConstraint.OrConstraint.Add(CheckoutRadiusConstraintUtils::GetDefaultCheckoutRadiusConstraint());

	// Get interest distances for each actor.
	TMap<UClass*, float> ActorComponentSetToRadius = CheckoutRadiusConstraintUtils::GetActorTypeToRadius();

	// For every interest distance that we still want, build a map from radius to list of actor type components that match that radius.
	TMap<float, TArray<UClass*>> DistanceToActorTypeComponents = CheckoutRadiusConstraintUtils::DedupeDistancesAcrossActorTypes(
		ActorComponentSetToRadius);

	// The previously built map removes duplicates of spatial constraints. Now the actual query constraints can be built of the form:
	// OR(AND(cylinder(radius), OR(actor 1 components, actor 2 components, ...)), ...)
	// which is equivalent to having a separate spatial query for each actor type if the radius is the same.
	TArray<QueryConstraint> CheckoutRadiusConstraints = CheckoutRadiusConstraintUtils::BuildNonDefaultActorCheckoutConstraints(
		DistanceToActorTypeComponents, ClassInfoManager);

	// Add all the different actor queries to the overall checkout constraint.
	for (auto& ActorCheckoutConstraint : CheckoutRadiusConstraints)
	{
		CheckoutRadiusConstraint.OrConstraint.Add(ActorCheckoutConstraint);
	}

	return CheckoutRadiusConstraint;
}

QueryConstraint InterestFactory::CreateNetCullDistanceConstraint(USpatialClassInfoManager* ClassInfoManager)
{
	QueryConstraint CheckoutRadiusConstraintRoot;

	const TMap<float, Worker_ComponentId>& NetCullDistancesToComponentIds = ClassInfoManager->GetNetCullDistanceToComponentIds();

	for (const auto& DistanceComponentPair : NetCullDistancesToComponentIds)
	{
		const float MaxCheckoutRadiusMeters = CheckoutRadiusConstraintUtils::NetCullDistanceSquaredToSpatialDistance(DistanceComponentPair.Key);

		QueryConstraint ComponentConstraint;
		ComponentConstraint.ComponentConstraint = DistanceComponentPair.Value;

		QueryConstraint RadiusConstraint;
		RadiusConstraint.RelativeCylinderConstraint = RelativeCylinderConstraint{ MaxCheckoutRadiusMeters };

		QueryConstraint CheckoutRadiusConstraint;
		CheckoutRadiusConstraint.AndConstraint.Add(RadiusConstraint);
		CheckoutRadiusConstraint.AndConstraint.Add(ComponentConstraint);

		CheckoutRadiusConstraintRoot.OrConstraint.Add(CheckoutRadiusConstraint);
	}

	return CheckoutRadiusConstraintRoot;
}

QueryConstraint InterestFactory::CreateNetCullDistanceConstraintWithFrequency(USpatialClassInfoManager* ClassInfoManager)
{
	QueryConstraint CheckoutRadiusConstraintRoot;

	const USpatialGDKSettings* SpatialGDKSettings = GetDefault<USpatialGDKSettings>();
	const TMap<float, Worker_ComponentId>& NetCullDistancesToComponentIds = ClassInfoManager->GetNetCullDistanceToComponentIds();

	for (const auto& DistanceComponentPair : NetCullDistancesToComponentIds)
	{
		const float MaxCheckoutRadiusMeters = CheckoutRadiusConstraintUtils::NetCullDistanceSquaredToSpatialDistance(DistanceComponentPair.Key);

		QueryConstraint ComponentConstraint;
		ComponentConstraint.ComponentConstraint = DistanceComponentPair.Value;

		{
			// Add default interest query which doesn't include a frequency
			float FullFrequencyCheckoutRadius = MaxCheckoutRadiusMeters * SpatialGDKSettings->FullFrequencyNetCullDistanceRatio;

			QueryConstraint RadiusConstraint;
			RadiusConstraint.RelativeCylinderConstraint = RelativeCylinderConstraint{ FullFrequencyCheckoutRadius };

			QueryConstraint CheckoutRadiusConstraint;
			CheckoutRadiusConstraint.AndConstraint.Add(RadiusConstraint);
			CheckoutRadiusConstraint.AndConstraint.Add(ComponentConstraint);

			CheckoutRadiusConstraintRoot.OrConstraint.Add(CheckoutRadiusConstraint);
		}

		// Add interest query for specified distance/frequency pairs
		for (const auto& DistanceFrequencyPair : SpatialGDKSettings->InterestRangeFrequencyPairs)
		{
			float CheckoutRadius = MaxCheckoutRadiusMeters * DistanceFrequencyPair.DistanceRatio;

			QueryConstraint RadiusConstraint;
			RadiusConstraint.RelativeCylinderConstraint = RelativeCylinderConstraint{ CheckoutRadius };

			QueryConstraint CheckoutRadiusConstraint;
			CheckoutRadiusConstraint.AndConstraint.Add(RadiusConstraint);
			CheckoutRadiusConstraint.AndConstraint.Add(ComponentConstraint);

			CheckoutConstraints.Add({ DistanceFrequencyPair.Frequency, CheckoutRadiusConstraint });
		}
	}

	return CheckoutRadiusConstraintRoot;
}

ResultType InterestFactory::CreateClientNonAuthInterestResultType(USpatialClassInfoManager* ClassInfoManager)
{
	ResultType ClientNonAuthResultType;

	// Add the required unreal components
	ClientNonAuthResultType.Append(SpatialConstants::REQUIRED_COMPONENTS_FOR_NON_AUTH_CLIENT_INTEREST);

	// Add all data components- clients don't need to see handover or owner only components on other entities.
	ClientNonAuthResultType.Append(ClassInfoManager->GetComponentIdsForComponentType(ESchemaComponentType::SCHEMA_Data));

	// In direct disagreement with the above comment, we add the owner only components as well.
	// This is because GDK workers currently make assumptions about information being available at the point of possession.
	// TODO(jacques): fix (unr-2865)
	ClientNonAuthResultType.Append(ClassInfoManager->GetComponentIdsForComponentType(ESchemaComponentType::SCHEMA_OwnerOnly));

	return ClientNonAuthResultType;
}

ResultType InterestFactory::CreateClientAuthInterestResultType(USpatialClassInfoManager* ClassInfoManager)
{
	ResultType ClientAuthResultType;

	// Add the required known components
	ClientAuthResultType.Append(SpatialConstants::REQUIRED_COMPONENTS_FOR_AUTH_CLIENT_INTEREST);
	ClientAuthResultType.Append(SpatialConstants::REQUIRED_COMPONENTS_FOR_NON_AUTH_CLIENT_INTEREST);

	// Add all the generated unreal components
	ClientAuthResultType.Append(ClassInfoManager->GetComponentIdsForComponentType(ESchemaComponentType::SCHEMA_Data));
	ClientAuthResultType.Append(ClassInfoManager->GetComponentIdsForComponentType(ESchemaComponentType::SCHEMA_OwnerOnly));

	return ClientAuthResultType;
}

ResultType InterestFactory::CreateServerNonAuthInterestResultType(USpatialClassInfoManager* ClassInfoManager)
{
	ResultType ServerNonAuthResultType;

	// Add the required unreal components
	ServerNonAuthResultType.Append(SpatialConstants::REQUIRED_COMPONENTS_FOR_NON_AUTH_SERVER_INTEREST);

	// Add all data, owner only, and handover components
	ServerNonAuthResultType.Append(ClassInfoManager->GetComponentIdsForComponentType(ESchemaComponentType::SCHEMA_Data));
	ServerNonAuthResultType.Append(ClassInfoManager->GetComponentIdsForComponentType(ESchemaComponentType::SCHEMA_OwnerOnly));
	ServerNonAuthResultType.Append(ClassInfoManager->GetComponentIdsForComponentType(ESchemaComponentType::SCHEMA_Handover));

	return ServerNonAuthResultType;
}

ResultType InterestFactory::CreateServerAuthInterestResultType(USpatialClassInfoManager* ClassInfoManager)
{
	// Just the components that we won't have already checked out through authority
	return SpatialConstants::REQUIRED_COMPONENTS_FOR_AUTH_SERVER_INTEREST;
}

Worker_ComponentData InterestFactory::CreateInterestData() const
{
	return CreateInterest().CreateInterestData();
}

Worker_ComponentUpdate InterestFactory::CreateInterestUpdate() const
{
	return CreateInterest().CreateInterestUpdate();
}

Interest InterestFactory::CreateServerWorkerInterest(const UAbstractLBStrategy* LBStrategy)
{
	const USpatialGDKSettings* SpatialGDKSettings = GetDefault<USpatialGDKSettings>();

	// Build the Interest component as we go by updating the component-> query list mappings.
	Interest ServerInterest;
	ComponentInterest ServerComponentInterest;
	Query ServerQuery;
	QueryConstraint Constraint;

	// Set the result type of the query
	if (SpatialGDKSettings->bEnableResultTypes)
	{
		ServerQuery.ResultComponentId = ServerNonAuthInterestResultType;
	}
	else
	{
		ServerQuery.FullSnapshotResult = true;
	}

	if (SpatialGDKSettings->bEnableOffloading)
	{
		// In offloading scenarios, hijack the server worker entity to ensure each server has interest in all entities
		Constraint.ComponentConstraint = SpatialConstants::POSITION_COMPONENT_ID;
		ServerQuery.Constraint = Constraint;

		// No need to add any further interest as we are already interested in everything
		AddComponentQueryPairToInterestComponent(ServerInterest, SpatialConstants::POSITION_COMPONENT_ID, ServerQuery);
		return ServerInterest;
	}

	// If we aren't offloading, the server gets more granular interest.

	// Ensure server worker receives always relevant entities
	QueryConstraint AlwaysRelevantConstraint = CreateAlwaysRelevantConstraint();

	Constraint = AlwaysRelevantConstraint;

	// If we are using the unreal load balancer, we also add the server worker interest defined by the load balancing strategy.
	if (SpatialGDKSettings->bEnableUnrealLoadBalancer)
	{
		check(LBStrategy != nullptr);
		
		// The load balancer won't be ready when the worker initially connects to SpatialOS. It needs
		// to wait for the virtual worker mappings to be replicated.
		// This function will be called again when that is the case in order to update the interest on the server entity.
		if (LBStrategy->IsReady())
		{
			QueryConstraint LoadBalancerConstraint = LBStrategy->GetWorkerInterestQueryConstraint();

			// Rather than adding the load balancer constraint at the end, reorder the constraints to have the large spatial
			// constraint at the front. This is more likely to be efficient.
			QueryConstraint NewConstraint;
			NewConstraint.OrConstraint.Add(LoadBalancerConstraint);
			NewConstraint.OrConstraint.Add(AlwaysRelevantConstraint);
			Constraint = NewConstraint;
		}
	}

	ServerQuery.Constraint = Constraint;
	AddComponentQueryPairToInterestComponent(ServerInterest, SpatialConstants::POSITION_COMPONENT_ID, ServerQuery);

	// Add another query to get the worker system entities.
	// It allows us to know when a client has disconnected.
	// TODO UNR-3042 : Migrate the VirtualWorkerTranslationManager to use the checked-out worker components instead of making a query.

	ServerQuery = Query();
	SetResultType(ServerQuery, ResultType{ SpatialConstants::WORKER_COMPONENT_ID });
	ServerQuery.Constraint.ComponentConstraint = SpatialConstants::WORKER_COMPONENT_ID;
	AddComponentQueryPairToInterestComponent(ServerInterest, SpatialConstants::POSITION_COMPONENT_ID, ServerQuery);

	return ServerInterest;
}

Interest InterestFactory::CreateInterest() const
{
	const USpatialGDKSettings* Settings = GetDefault<USpatialGDKSettings>();

	// The interest is built progressively by adding the different component query pairs to build the full map.
	Interest ResultInterest;

	if (Actor->IsA(APlayerController::StaticClass()))
	{
		// Put the "main" interest queries on the player controller
		AddPlayerControllerActorInterest(ResultInterest);
	}

	if (Settings->bEnableResultTypes)
	{
		if (Actor->GetNetConnection() != nullptr)
		{
			// Clients need to see owner only and server RPC components on entities they have authority over
			AddClientSelfInterest(ResultInterest);
		}

		// Every actor needs a self query for the server to the client RPC endpoint
		AddServerSelfInterest(ResultInterest);
	}

	return ResultInterest;
}

void InterestFactory::AddPlayerControllerActorInterest(Interest& OutInterest) const
{
	QueryConstraint LevelConstraint = CreateLevelConstraints();

	AddSystemQuery(OutInterest, LevelConstraint);

	AddUserDefinedQueries(OutInterest, Actor, LevelConstraint);

	// If net cull distance frequency queries are enabled, build and add those separately as they have to be built each time.
	// They are added as separate queries for the same reason- different frequencies.
	if (GetDefault<USpatialGDKSettings>()->bEnableNetCullDistanceFrequency)
	{
		AddNetCullDistanceFrequencyQueries(OutInterest, LevelConstraint);
	}
}

void InterestFactory::AddClientSelfInterest(Interest& OutInterest) const
{
	Query NewQuery;
	// Just an entity ID constraint is fine, as clients should not become authoritative over entities outside their loaded levels
	NewQuery.Constraint.EntityIdConstraint = EntityId;

	NewQuery.ResultComponentId = ClientAuthInterestResultType;

	AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::GetClientAuthorityComponent(GetDefault<USpatialGDKSettings>()->UseRPCRingBuffer()), NewQuery);
}

void InterestFactory::AddServerSelfInterest(Interest& OutInterest) const
{
	// Add a query for components all servers need to read client data
	Query ClientQuery;
	ClientQuery.Constraint.EntityIdConstraint = EntityId;
	ClientQuery.ResultComponentId = ServerAuthInterestResultType;
	AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::POSITION_COMPONENT_ID, ClientQuery);

	// Add a query for the load balancing worker (whoever is delegated the ACL) to read the authority intent
	Query LoadBalanceQuery;
	LoadBalanceQuery.Constraint.EntityIdConstraint = EntityId;
	LoadBalanceQuery.ResultComponentId = ResultType{ SpatialConstants::AUTHORITY_INTENT_COMPONENT_ID };
	AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::ENTITY_ACL_COMPONENT_ID, LoadBalanceQuery);
}

void InterestFactory::AddSystemQuery(Interest& OutInterest, const QueryConstraint& LevelConstraint) const
{
	const USpatialGDKSettings* Settings = GetDefault<USpatialGDKSettings>();

	QueryConstraint CheckoutRadiusConstraint = CreateCheckoutRadiusConstraints();
	QueryConstraint AlwaysInterestedConstraint = CreateAlwaysInterestedConstraint();
	QueryConstraint AlwaysRelevantConstraint = CreateAlwaysRelevantConstraint();

	QueryConstraint SystemDefinedConstraints;

	if (CheckoutRadiusConstraint.IsValid())
	{
		SystemDefinedConstraints.OrConstraint.Add(CheckoutRadiusConstraint);
	}

	if (AlwaysInterestedConstraint.IsValid())
	{
		SystemDefinedConstraints.OrConstraint.Add(AlwaysInterestedConstraint);
	}

	if (AlwaysRelevantConstraint.IsValid())
	{
		SystemDefinedConstraints.OrConstraint.Add(AlwaysRelevantConstraint);
	}

	// Add the level constraint here as all client queries need to make sure they don't check out anything outside their loaded levels.
	QueryConstraint SystemAndLevelConstraint;
	SystemAndLevelConstraint.AndConstraint.Add(SystemDefinedConstraints);
	SystemAndLevelConstraint.AndConstraint.Add(LevelConstraint);

	Query ClientSystemQuery;
	ClientSystemQuery.Constraint = SystemAndLevelConstraint;

	SetResultType(ClientSystemQuery, ClientNonAuthInterestResultType);

	AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::GetClientAuthorityComponent(Settings->UseRPCRingBuffer()), ClientSystemQuery);

	// Add the spatial and always interested constraint to the server as well to make sure the server sees the same as the client.
	// The always relevant constraint is added as part of the server worker query, so leave that out here.
	// Servers also don't need to be level constrained.
	if (Settings->bEnableClientQueriesOnServer)
	{
		Query ServerSystemQuery;
		QueryConstraint ServerSystemConstraint;
		ServerSystemConstraint.OrConstraint.Add(CheckoutRadiusConstraint);
		ServerSystemConstraint.OrConstraint.Add(AlwaysInterestedConstraint);
		ServerSystemQuery.Constraint = ServerSystemConstraint;

		SetResultType(ServerSystemQuery, ServerNonAuthInterestResultType);

		AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::POSITION_COMPONENT_ID, ServerSystemQuery);
	}
}

void InterestFactory::AddUserDefinedQueries(Interest& OutInterest, const AActor* InActor, const QueryConstraint& LevelConstraint) const
{
	SCOPE_CYCLE_COUNTER(STAT_InterestFactoryAddUserDefinedQueries);
	const USpatialGDKSettings* Settings = GetDefault<USpatialGDKSettings>();

	FrequencyToConstraintsMap FrequencyConstraintsMap = GetUserDefinedFrequencyToConstraintsMap(InActor);

	for (const auto& FrequencyToConstraints : FrequencyConstraintsMap)
	{
		Query UserQuery;
		QueryConstraint UserConstraint;

		UserQuery.Frequency = FrequencyToConstraints.Key;

		// If there is only one constraint, don't make the constraint an OR.
		if (FrequencyToConstraints.Value.Num() == 1)
		{
			UserConstraint = FrequencyToConstraints.Value[0];
		}
		else
		{
			UserConstraint.OrConstraint.Append(FrequencyToConstraints.Value);
		}

		if (!UserConstraint.IsValid())
		{
			continue;
		}

		// All constraints have to be limited to the checked out levels, so create an AND constraint with the level.
		UserQuery.Constraint.AndConstraint.Add(UserConstraint);
		UserQuery.Constraint.AndConstraint.Add(LevelConstraint);

		// We enforce result type even for user defined queries. Here we are assuming what a user wants from their defined
		// queries are for their players to check out more actors than they normally would, so use the client non auth result type,
		// which includes all components required for a client to see non-authoritative actors.
		SetResultType(UserQuery, ClientNonAuthInterestResultType);
		
		AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::GetClientAuthorityComponent(Settings->UseRPCRingBuffer()), UserQuery);

		// Add the user interest to the server as well if load balancing is enabled and the client queries on server flag is flipped
		// Need to check if load balancing is enabled otherwise there is not chance the client could see and entity the server can't,
		// which is what the client queries on server flag is to avoid.
		if (Settings->bEnableClientQueriesOnServer)
		{
			Query ServerUserQuery;
			ServerUserQuery.Constraint = UserConstraint;
			ServerUserQuery.Frequency = FrequencyToConstraints.Key;

			SetResultType(ServerUserQuery, ServerNonAuthInterestResultType);

			AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::POSITION_COMPONENT_ID, ServerUserQuery);
		}
	}
}

FrequencyToConstraintsMap InterestFactory::GetUserDefinedFrequencyToConstraintsMap(const AActor* InActor) const
{
	// This function builds a frequency to constraint map rather than queries. It does this for two reasons:
	// - We need to set the result type later
	// - The map implicitly removes duplicates queries that have the same constraint. Result types are set for each query and these are large,
	//   so worth simplifying as much as possible.
	FrequencyToConstraintsMap FrequencyToConstraints;

	if (const APlayerController* PlayerController = Cast<APlayerController>(InActor))
	{
		// If this is for a player controller, loop through the pawns of the controller as well, because we only add interest to
		// the player controller entity but interest can be specified on the pawn of the controller as well.
		GetActorUserDefinedQueryConstraints(InActor, FrequencyToConstraints, true);
		GetActorUserDefinedQueryConstraints(PlayerController->GetPawn(), FrequencyToConstraints, true);
	}
	else
	{
		GetActorUserDefinedQueryConstraints(InActor, FrequencyToConstraints, false);
	}

	return FrequencyToConstraints;
}

void InterestFactory::GetActorUserDefinedQueryConstraints(const AActor* InActor, FrequencyToConstraintsMap& OutFrequencyToConstraints, bool bRecurseChildren) const
{
	check(ClassInfoManager);

	if (InActor == nullptr)
	{
		return;
	}

	// The defined actor interest component populates the frequency to constraints map with the user defined queries.
	TArray<UActorInterestComponent*> ActorInterestComponents;
	InActor->GetComponents<UActorInterestComponent>(ActorInterestComponents);
	if (ActorInterestComponents.Num() == 1)
	{
		ActorInterestComponents[0]->PopulateFrequencyToConstraintsMap(*ClassInfoManager, OutFrequencyToConstraints);
	}
	else if (ActorInterestComponents.Num() > 1)
	{
		UE_LOG(LogInterestFactory, Error, TEXT("%s has more than one ActorInterestComponent"), *InActor->GetPathName());
		checkNoEntry()
	}

	if (bRecurseChildren)
	{
		for (const auto& Child : InActor->Children)
		{
			GetActorUserDefinedQueryConstraints(Child, OutFrequencyToConstraints, true);
		}
	}
}

void InterestFactory::AddNetCullDistanceFrequencyQueries(Interest& OutInterest, const QueryConstraint& LevelConstraint) const
{
	const USpatialGDKSettings* Settings = GetDefault<USpatialGDKSettings>();

	// The CheckouConstraints list contains items with a constraint and a frequency.
	// They are then converted to queries by adding a result type to them, and the constraints are conjoined with the level constraint.
	for (const auto& CheckoutRadiusConstraintFrequencyPair : CheckoutConstraints)
	{
		Query NewQuery;

		NewQuery.Constraint.AndConstraint.Add(CheckoutRadiusConstraintFrequencyPair.Constraint);

		if (LevelConstraint.IsValid())
		{
			NewQuery.Constraint.AndConstraint.Add(LevelConstraint);
		}

		NewQuery.Frequency = CheckoutRadiusConstraintFrequencyPair.Frequency;

		SetResultType(NewQuery, ClientNonAuthInterestResultType);

		AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::GetClientAuthorityComponent(Settings->UseRPCRingBuffer()), NewQuery);

		// Add the queries to the server as well to ensure that all entities checked out on the client will be present on the server.
		if (Settings->bEnableClientQueriesOnServer)
		{
			Query ServerQuery;
			ServerQuery.Constraint = CheckoutRadiusConstraintFrequencyPair.Constraint;
			ServerQuery.Frequency = CheckoutRadiusConstraintFrequencyPair.Frequency;

			SetResultType(ServerQuery, ServerNonAuthInterestResultType);

			AddComponentQueryPairToInterestComponent(OutInterest, SpatialConstants::POSITION_COMPONENT_ID, ServerQuery);
		}
	}
}

void InterestFactory::AddComponentQueryPairToInterestComponent(Interest& OutInterest, const Worker_ComponentId ComponentId, const Query& QueryToAdd)
{
	if (!OutInterest.ComponentInterestMap.Contains(ComponentId))
	{
		ComponentInterest NewComponentInterest;
		OutInterest.ComponentInterestMap.Add(ComponentId, NewComponentInterest);
	}
	OutInterest.ComponentInterestMap[ComponentId].Queries.Add(QueryToAdd);
}

QueryConstraint InterestFactory::CreateCheckoutRadiusConstraints() const
{
	// If the actor has a component to specify interest and that indicates that we shouldn't generate
	// constraints based on NetCullDistanceSquared, abort. There is a check elsewhere to ensure that
	// there is at most one ActorInterestQueryComponent.
	TArray<UActorInterestComponent*> ActorInterestComponents;
	Actor->GetComponents<UActorInterestComponent>(ActorInterestComponents);
	if (ActorInterestComponents.Num() == 1)
	{
		const UActorInterestComponent* ActorInterest = ActorInterestComponents[0];
		check(ActorInterest);
		if (!ActorInterest->bUseNetCullDistanceSquaredForCheckoutRadius)
		{
			return QueryConstraint{};
		}
	}

	// Otherwise, return the previously computed checkout radius constraint.
	return ClientCheckoutRadiusConstraint;
}

QueryConstraint InterestFactory::CreateAlwaysInterestedConstraint() const
{
	QueryConstraint AlwaysInterestedConstraint;

	for (const FInterestPropertyInfo& PropertyInfo : Info.InterestProperties)
	{
		uint8* Data = (uint8*)Actor + PropertyInfo.Offset;
		if (UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(PropertyInfo.Property))
		{
			AddObjectToConstraint(ObjectProperty, Data, AlwaysInterestedConstraint);
		}
		else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(PropertyInfo.Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, Data);
			for (int i = 0; i < ArrayHelper.Num(); i++)
			{
				AddObjectToConstraint(Cast<UObjectPropertyBase>(ArrayProperty->Inner), ArrayHelper.GetRawPtr(i), AlwaysInterestedConstraint);
			}
		}
		else
		{
			checkNoEntry();
		}
	}

	return AlwaysInterestedConstraint;
}

QueryConstraint InterestFactory::CreateAlwaysRelevantConstraint()
{
	QueryConstraint AlwaysRelevantConstraint;

	Worker_ComponentId ComponentIds[] = {
		SpatialConstants::SINGLETON_COMPONENT_ID,
		SpatialConstants::SINGLETON_MANAGER_COMPONENT_ID,
		SpatialConstants::STARTUP_ACTOR_MANAGER_COMPONENT_ID,
		SpatialConstants::VIRTUAL_WORKER_TRANSLATION_COMPONENT_ID,
		SpatialConstants::ALWAYS_RELEVANT_COMPONENT_ID
	};

	for (Worker_ComponentId ComponentId : ComponentIds)
	{
		QueryConstraint Constraint;
		Constraint.ComponentConstraint = ComponentId;
		AlwaysRelevantConstraint.OrConstraint.Add(Constraint);
	}

	return AlwaysRelevantConstraint;
}

QueryConstraint InterestFactory::CreateLevelConstraints() const
{
	QueryConstraint LevelConstraint;

	QueryConstraint DefaultConstraint;
	DefaultConstraint.ComponentConstraint = SpatialConstants::NOT_STREAMED_COMPONENT_ID;
	LevelConstraint.OrConstraint.Add(DefaultConstraint);

	UNetConnection* Connection = Actor->GetNetConnection();
	check(Connection);
	APlayerController* PlayerController = Connection->GetPlayerController(nullptr);
	check(PlayerController);

	const TSet<FName>& LoadedLevels = PlayerController->NetConnection->ClientVisibleLevelNames;

	// Create component constraints for every loaded sublevel
	for (const auto& LevelPath : LoadedLevels)
	{
		const Worker_ComponentId ComponentId = ClassInfoManager->GetComponentIdFromLevelPath(LevelPath.ToString());
		if (ComponentId != SpatialConstants::INVALID_COMPONENT_ID)
		{
			QueryConstraint SpecificLevelConstraint;
			SpecificLevelConstraint.ComponentConstraint = ComponentId;
			LevelConstraint.OrConstraint.Add(SpecificLevelConstraint);
		}
		else
		{
			UE_LOG(LogInterestFactory, Error, TEXT("Error creating query constraints for Actor %s. "
				"Could not find Streaming Level Component for Level %s. Have you generated schema?"), *Actor->GetName(), *LevelPath.ToString());
		}
	}

	return LevelConstraint;
}

void InterestFactory::AddObjectToConstraint(UObjectPropertyBase* Property, uint8* Data, QueryConstraint& OutConstraint) const
{
	UObject* ObjectOfInterest = Property->GetObjectPropertyValue(Data);

	if (ObjectOfInterest == nullptr)
	{
		return;
	}

	FUnrealObjectRef UnrealObjectRef = PackageMap->GetUnrealObjectRefFromObject(ObjectOfInterest);

	if (!UnrealObjectRef.IsValid())
	{
		return;
	}

	QueryConstraint EntityIdConstraint;
	EntityIdConstraint.EntityIdConstraint = UnrealObjectRef.Entity;
	OutConstraint.OrConstraint.Add(EntityIdConstraint);
}

void InterestFactory::SetResultType(Query& OutQuery, const ResultType& InResultType)
{
	if (GetDefault<USpatialGDKSettings>()->bEnableResultTypes)
	{
		OutQuery.ResultComponentId = InResultType;
	}
	else
	{
		OutQuery.FullSnapshotResult = true;
	}
}

} // namespace SpatialGDK
