// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "FileCache.h"
#include "Improbable/SpatialGDKSettingsBridge.h"
#include "Misc/MonitoredProcess.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "TimerManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialDeploymentManager, Log, All);

class FJsonObject;

class FLocalDeploymentManager
{
public:
	FLocalDeploymentManager();

	void SPATIALGDKSERVICES_API PreInit(bool bChinaEnabled);

	void SPATIALGDKSERVICES_API Init(FString RuntimeIPToExpose);

	bool CheckIfPortIsBound(int32 Port);
	bool KillProcessBlockingPort(int32 Port);
	bool LocalDeploymentPreRunChecks();

	using LocalDeploymentCallback = TFunction<void(bool)>;

	void SPATIALGDKSERVICES_API TryStartLocalDeployment(FString LaunchConfig, FString RuntimeVersion, FString LaunchArgs,
														FString SnapshotName, FString RuntimeIPToExpose,
														const LocalDeploymentCallback& CallBack);
	bool SPATIALGDKSERVICES_API TryStopLocalDeployment();

	bool SPATIALGDKSERVICES_API IsLocalDeploymentRunning() const;

	bool SPATIALGDKSERVICES_API IsDeploymentStarting() const;
	bool SPATIALGDKSERVICES_API IsDeploymentStopping() const;

	bool SPATIALGDKSERVICES_API IsRedeployRequired() const;
	void SPATIALGDKSERVICES_API SetRedeployRequired();

	// Helper function to inform a client or server whether it should wait for a local deployment to become active.
	bool SPATIALGDKSERVICES_API ShouldWaitForDeployment() const;

	void SPATIALGDKSERVICES_API SetAutoDeploy(bool bAutoDeploy);

	void SPATIALGDKSERVICES_API TakeSnapshot(UWorld* World, bool bUseStandard, FSpatialSnapshotTakenFunc OnSnapshotTaken);

	void WorkerBuildConfigAsync();

	FSimpleMulticastDelegate OnSpatialShutdown;
	FSimpleMulticastDelegate OnDeploymentStart;

	FDelegateHandle WorkerConfigDirectoryChangedDelegateHandle;
	IDirectoryWatcher::FDirectoryChanged WorkerConfigDirectoryChangedDelegate;

private:
	void StartUpWorkerConfigDirectoryWatcher();
	void OnWorkerConfigDirectoryChanged(const TArray<FFileChangeData>& FileChanges);

	void KillExistingRuntime();

	TFuture<bool> AttemptSpatialAuthResult;

	static const int32 ExitCodeSuccess = 0;
	static const int32 ExitCodeNotRunning = 4;

	TOptional<FMonitoredProcess> RuntimeProcess = {};

	static const int32 RequiredRuntimePort = 5301;
	static const int32 WorkerPort = 8018;
	static const int32 HTTPPort = 5006;
	static const int32 GRPCPort = 7777;

	const float RuntimeTimeout = 5.0;

	bool bLocalDeploymentManagerEnabled = true;

	bool bLocalDeploymentRunning;

	bool bStartingDeployment;
	bool bStoppingDeployment;

	FString ExposedRuntimeIP;

	bool bRedeployRequired = false;
	bool bAutoDeploy = false;
	bool bIsInChina = false;
};
