// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "TestMaps/GeneratedTestMap.h"
#include "SpatialInitialDormancyMap.generated.h"

/**
 * This map is for tests that require a DORM_Initial actor to be present in the map
 * Bare in mind that the state of the actor is modified in each test that is run.
 * Additional dormancy tests may require a "fresh" dormant actor in which case another map is required.
 */
UCLASS()
class SPATIALGDKFUNCTIONALTESTS_API USpatialInitialDormancyMap : public UGeneratedTestMap
{
	GENERATED_BODY()

public:
	USpatialInitialDormancyMap();

protected:
	virtual void CreateCustomContentForMap() override;
};
