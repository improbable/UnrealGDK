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

class FLocalDeploymentManager final
{
public:
	FLocalDeploymentManager();
	~FLocalDeploymentManager();

	void SPATIALGDKSERVICES_API PreInit(bool bChinaEnabled);

	void SPATIALGDKSERVICES_API Init();

	bool CheckIfPortIsBound(uint16 Port) const;
	bool KillProcessBlockingPort(uint16 Port);
	bool LocalDeploymentPreRunChecks(const uint16 RuntimeGRPCPort);

	using LocalDeploymentCallback = TFunction<void(bool)>;

	void SPATIALGDKSERVICES_API TryStartLocalDeployment(const FString& LaunchConfig, const FString& RuntimeVersion,
														const FString& LaunchArgs, const FString& SnapshotName,
														const FString& RuntimeIPToExpose, const uint16 RuntimeGRPCPort,
														const bool bEnableSessionLogRecording, const LocalDeploymentCallback& CallBack);

	bool SPATIALGDKSERVICES_API TryStopLocalDeployment();
	bool SPATIALGDKSERVICES_API TryStopLocalDeploymentGracefully();

	bool SPATIALGDKSERVICES_API IsLocalDeploymentRunning() const;

	bool SPATIALGDKSERVICES_API IsDeploymentStarting() const;
	bool SPATIALGDKSERVICES_API IsDeploymentStopping() const;

	bool SPATIALGDKSERVICES_API IsRedeployRequired() const;
	void SPATIALGDKSERVICES_API SetRedeployRequired();

	// Helper function to inform a client or server whether it should wait for a local deployment to become active.
	bool SPATIALGDKSERVICES_API ShouldWaitForDeployment() const;

	void SPATIALGDKSERVICES_API SetAutoDeploy(bool bAutoDeploy);

	void SPATIALGDKSERVICES_API TakeSnapshot(UWorld* World, FSpatialSnapshotTakenFunc OnSnapshotTaken);

	void WorkerBuildConfigAsync();

	FSimpleMulticastDelegate OnDeploymentStart;

	FDelegateHandle WorkerConfigDirectoryChangedDelegateHandle;
	IDirectoryWatcher::FDirectoryChanged WorkerConfigDirectoryChangedDelegate;

private:
	void StartUpWorkerConfigDirectoryWatcher();
	void OnWorkerConfigDirectoryChanged(const TArray<FFileChangeData>& FileChanges);

	bool SetupRuntimeFileLogger(const FString& SpatialLogsSubDirectoryName);

	bool WaitForRuntimeProcessToShutDown();
	bool StartLocalDeploymentShutDown();
	bool GracefulShutdownAndWaitForTermination();
	bool ForceShutdownAndWaitForTermination();
	void FinishLocalDeploymentShutDown();

	enum class ERuntimeStartResponse
	{
		AlreadyRunning,
		PreRunChecksFailed,
		Timeout,
		Success
	};

	ERuntimeStartResponse StartLocalDeployment(const FString& LaunchConfig, const FString& RuntimeVersion, const FString& LaunchArgs,
											   const FString& SnapshotName, const FString& RuntimeIPToExpose, const uint16 RuntimeGRPCPort,
											   const bool bEnableSessionLogRecording, const LocalDeploymentCallback& CallBack);

	FCriticalSection StoppingDeployment;

	TFuture<bool> AttemptSpatialAuthResult;

	TOptional<FMonitoredProcess> RuntimeProcess = {};
	TUniquePtr<IFileHandle> RuntimeLogFileHandle;
	FDateTime RuntimeStartTime;

	static const uint16 RequiredRuntimePort = 5301;
	static const uint16 WorkerPort = 8018;
	static const uint16 HTTPPort = 5006;

	static constexpr double RuntimeTimeout = 10.0;
	static constexpr int32 RuntimeStartRetries = 3;

	bool bLocalDeploymentManagerEnabled = true;

	bool bLocalDeploymentRunning;

	bool bStartingDeployment;
	bool bStoppingDeployment;
	bool bTestRunnning;

	FString RuntimePath;

	FString ExposedRuntimeIP;

	FString CurrentSnapshotPath;

	bool bRedeployRequired = false;
	bool bAutoDeploy = false;
	bool bIsInChina = false;
};
