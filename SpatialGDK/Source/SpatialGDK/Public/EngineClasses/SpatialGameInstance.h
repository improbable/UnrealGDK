// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

#include "SpatialGameInstance.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialGameInstance, Log, All);

class USpatialWorkerConnection;

UCLASS(config = Engine)
class SPATIALGDK_API USpatialGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FGameInstancePIEResult StartPlayInEditorGameInstance(ULocalPlayer* LocalPlayer, const FGameInstancePIEParameters& Params) override;
#endif
	virtual void StartGameInstance() override;
	virtual void Shutdown() override;

	// bResponsibleForSnapshotLoading exists to have persistent knowledge if this worker has authority over the GSM during ServerTravel.
	bool bResponsibleForSnapshotLoading = false;

	// The SpatialWorkerConnection must always be owned by the SpatialGameInstance and so must be created here to prevent TrimMemory from deleting it during Browse.
	void CreateNewSpatialWorkerConnection();

	USpatialWorkerConnection* GetSpatialWorkerConnection();

protected:
	// Checks whether the current net driver is a USpatialNetDriver.
	// Can be used to decide whether to use Unreal networking or SpatialOS networking.
	bool HasSpatialNetDriver() const;

private:

	// SpatialConnection is stored here for persistence between map travels.
	UPROPERTY()
	USpatialWorkerConnection* SpatialConnection;

	// If this flag is set to true standalone clients will not attempt to connect to a deployment automatically if a 'loginToken' exists in arguments.
	UPROPERTY(Config)
	bool bPreventAutoConnectWithLocator;
};
