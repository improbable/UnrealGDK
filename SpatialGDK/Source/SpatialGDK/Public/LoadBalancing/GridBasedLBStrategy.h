// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "LoadBalancing/AbstractLBStrategy.h"
#include "GridBasedLBStrategy.generated.h"

/**
 * A load balancing strategy that divides the world into a grid.
 * Divides the load between Rows * Cols number of workers, each handling a
 * square of the world (WorldWidth / Cols)cm by (WorldHeight / Rows)cm
 *
 * Given a Point, for each Cell:
 * Point is inside Cell iff Min(Cell) <= Point < Max(Cell)
 *
 * Intended Usage: Create a data-only blueprint subclass and change
 * the Cols, Rows, WorldWidth, WorldHeight.
 */
UCLASS(Blueprintable)
class SPATIALGDK_API UGridBasedLBStrategy : public UAbstractLBStrategy
{
	GENERATED_BODY()

public:
	UGridBasedLBStrategy();

/* UAbstractLBStrategy Interface */
	virtual void Init(const class USpatialNetDriver* InNetDriver) override;

	virtual TArray<uint32> GetVirtualWorkerIds() const;

	virtual bool ShouldRelinquishAuthority(const AActor& Actor) const override;
	virtual uint32 WhoShouldHaveAuthority(const AActor& Actor) const override;
/* End UAbstractLBStrategy Interface */

protected:
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	uint32 Rows;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	uint32 Cols;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	float WorldWidth;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	float WorldHeight;

private:

	TArray<uint32> VirtualWorkerIds;

	TArray<FBox2D> WorkerCells;

	static bool IsInside(const FBox2D& Box, const FVector2D& Location);
};
