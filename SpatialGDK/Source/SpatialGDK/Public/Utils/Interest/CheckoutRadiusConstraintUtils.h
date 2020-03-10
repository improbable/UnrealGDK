// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Interop/SpatialClassInfoManager.h"
#include "Schema/Interest.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCheckoutRadiusConstraintUtils, Log, All);

namespace SpatialGDK
{

class SPATIALGDK_API CheckoutRadiusConstraintUtils
{
public:

	static FrequencyConstraints CreateLegacyNetCullDistanceConstraint(USpatialClassInfoManager* InClassInfoManager);
	static FrequencyConstraints CreateNetCullDistanceConstraint(USpatialClassInfoManager* InClassInfoManager);
	static FrequencyConstraints CreateNetCullDistanceConstraintWithFrequency(USpatialClassInfoManager* InClassInfoManager);

	static QueryConstraint GetDefaultCheckoutRadiusConstraint();
	static TMap<UClass*, float> GetActorTypeToRadius();
	static TMap<float, TArray<UClass*>> DedupeDistancesAcrossActorTypes(const TMap<UClass*, float> ComponentSetToRadius);
	static TArray<QueryConstraint> BuildNonDefaultActorCheckoutConstraints(const TMap<float, TArray<UClass*>> DistanceToActorTypes, USpatialClassInfoManager* ClassInfoManager);
	static float NetCullDistanceSquaredToSpatialDistance(float NetCullDistanceSquared);

private:

	static void AddToFrequencyConstraintMap(const float Frequency, const QueryConstraint& Constraint, FrequencyToConstraintsMap& OutFrequencyToConstraints);
	static void AddTypeHierarchyToConstraint(const UClass& BaseType, QueryConstraint& OutConstraint, USpatialClassInfoManager* ClassInfoManager);
};

} // namespace SpatialGDK
