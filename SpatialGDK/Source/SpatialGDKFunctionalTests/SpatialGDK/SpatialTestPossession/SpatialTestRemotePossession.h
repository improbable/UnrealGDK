// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "SpatialFunctionalTest.h"
#include "SpatialTestRemotePossession.generated.h"

class ATestPossessionPawn;

UCLASS()
class SPATIALGDKFUNCTIONALTESTS_API ASpatialTestRemotePossession : public ASpatialFunctionalTest
{
	GENERATED_BODY()
public:
	ASpatialTestRemotePossession();

	virtual void PrepareTest() override;

	ATestPossessionPawn* GetPawn();

	bool IsReadyForPossess();

	void AddWaitStep(const FWorkerDefinition& Worker);

protected:
	FVector LocationOfPawn;
	float WaitTime;
	const static float MaxWaitTime;
};
