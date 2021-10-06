// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "SpatialFunctionalTest.h"
#include "SpatialTestRepNotifySubobject.generated.h"

UCLASS()
class SPATIALGDKFUNCTIONALTESTS_API USpatialTestRepNotifySubobject : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialTestRepNotifySubobject();

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 ExpectedParentInt1Property = 0;

	bool bParentPropertyWasExpectedProperty;

	bool bParentRepNotifyWasCalledBeforeChild;

	UPROPERTY(ReplicatedUsing = OnRep_OnChangedRepNotifyInt)
	int32 OnChangedRepNotifyInt;

	UFUNCTION()
	void OnRep_OnChangedRepNotifyInt(int32 OldOnChangedRepNotifyInt);
};
