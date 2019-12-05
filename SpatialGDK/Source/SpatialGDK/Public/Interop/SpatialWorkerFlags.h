// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include <WorkerSDK/improbable/c_worker.h>
#include "SpatialWorkerFlags.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnWorkerFlagsUpdatedBP, FString, FlagName, FString, FlagValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorkerFlagsUpdated, FString, FlagName, FString, FlagValue);

UCLASS()
class SPATIALGDK_API USpatialWorkerFlags : public UObject
{
	GENERATED_BODY()

public:
	/** Gets value of a worker flag. Must be connected to SpatialOS to properly work.
	 * @param Name - Name of worker flag
	 * @param OutValue - Value of worker flag
	 * @return - If worker flag was found.
	 */
	bool GetWorkerFlag(const FString& Name, FString& OutValue);

	FOnWorkerFlagsUpdated& GetOnWorkerFlagsUpdated();
	
	UFUNCTION(BlueprintCallable, Category = "SpatialOS")
	void BindToOnWorkerFlagsUpdated(const FOnWorkerFlagsUpdatedBP& InDelegate);

	UFUNCTION(BlueprintCallable, Category = "SpatialOS")
	void UnbindFromOnWorkerFlagsUpdated(const FOnWorkerFlagsUpdatedBP& InDelegate);

	void ApplyWorkerFlagUpdate(const Worker_FlagUpdateOp& Op);

private:

	FOnWorkerFlagsUpdated OnWorkerFlagsUpdated;

	TMap<FString, FString> WorkerFlags;
};
