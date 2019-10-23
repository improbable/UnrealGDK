// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EngineClasses/GridBasedLBStrategy.h"
#include "TestGridBasedLBStrategy.generated.h"

/**
 * 
 */
UCLASS()
class SPATIALGDKTESTS_API UTestGridBasedLBStrategy : public UGridBasedLBStrategy
{
	GENERATED_BODY()

public:

	static UGridBasedLBStrategy* Create(uint32 Rows, uint32 Cols, float WorldWidth, float WorldHeight);

};
