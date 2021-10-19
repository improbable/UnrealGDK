// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "LocalDeploymentManagerUtilities.h"

#include "LocalDeploymentManager.h"
#include "SpatialGDKDefaultLaunchConfigGenerator.h"
#include "SpatialGDKDefaultWorkerJsonGenerator.h"
#include "SpatialGDKEditorSettings.h"
#include "SpatialGDKServicesConstants.h"

#include "CoreMinimal.h"

namespace SpatialGDK
{
FLocalDeploymentManager* GetLocalDeploymentManager()
{
	FSpatialGDKServicesModule& GDKServices = FModuleManager::GetModuleChecked<FSpatialGDKServicesModule>("SpatialGDKServices");
	FLocalDeploymentManager* LocalDeploymentManager = GDKServices.GetLocalDeploymentManager();
	return LocalDeploymentManager;
}
} // namespace SpatialGDK

namespace
{
const double MAX_WAIT_TIME_FOR_LOCAL_DEPLOYMENT_OPERATION = 30.0;

const FName AutomationWorkerType = TEXT("AutomationWorker");
const FString AutomationLaunchConfig = FString(TEXT("Improbable/")) + *AutomationWorkerType.ToString() + FString(TEXT(".json"));

bool GenerateWorkerAssemblies()
{
	FString BuildConfigArgs = TEXT("worker build build-config");
	FString WorkerBuildConfigResult;
	int32 ExitCode;
	FSpatialGDKServicesModule::ExecuteAndReadOutput(SpatialGDKServicesConstants::SpatialExe, BuildConfigArgs,
													SpatialGDKServicesConstants::SpatialOSDirectory, WorkerBuildConfigResult, ExitCode);

	return ExitCode == SpatialGDKServicesConstants::ExitCodeSuccess;
}

FString GetWorkerJsonPath()
{
	const FString WorkerJsonDir = FPaths::Combine(SpatialGDKServicesConstants::SpatialOSDirectory, TEXT("workers/unreal"));

	FString Filename = FString(TEXT("spatialos.")) + *AutomationWorkerType.ToString() + FString(TEXT(".worker.json"));
	FString JsonPath = FPaths::Combine(WorkerJsonDir, Filename);
	return JsonPath;
}

bool GenerateWorkerJson()
{
	FString JsonPath = GetWorkerJsonPath();
	if (!FPaths::FileExists(JsonPath))
	{
		bool bOutRedeployRequired = false;
		return GenerateDefaultWorkerJson(JsonPath, bOutRedeployRequired);
	}

	return true;
}

bool DeleteWorkerJson()
{
	FString JsonPath = GetWorkerJsonPath();
	if (FPaths::FileExists(JsonPath))
	{
		IFileManager& FileManager = IFileManager::Get();
		return FileManager.Delete(*JsonPath);
	}

	return true;
}
} // namespace

bool FStartDeployment::Update()
{
	if (const USpatialGDKEditorSettings* SpatialGDKSettings = GetDefault<USpatialGDKEditorSettings>())
	{
		FLocalDeploymentManager* LocalDeploymentManager = SpatialGDK::GetLocalDeploymentManager();
		const FString LaunchConfig =
			FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir()), AutomationLaunchConfig);
		const FString LaunchFlags = SpatialGDKSettings->GetSpatialOSCommandLineLaunchFlags();
		const FString SnapshotName = SpatialGDKSettings->GetSpatialOSSnapshotToLoadPath();
		const FString RuntimeVersion = SpatialGDKSettings->GetSelectedRuntimeVariantVersion().GetVersionForLocal();

		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [LocalDeploymentManager, LaunchConfig, LaunchFlags, SnapshotName,
																 RuntimeVersion] {
			if (!GenerateWorkerJson())
			{
				return;
			}

			if (!GenerateWorkerAssemblies())
			{
				return;
			}

			FSpatialLaunchConfigDescription LaunchConfigDescription;

			FWorkerTypeLaunchSection AutomationWorkerConfig;
			AutomationWorkerConfig.WorkerTypeName = TEXT("AutomationWorker");
			LaunchConfigDescription.AdditionalWorkerConfigs.Add(AutomationWorkerConfig);

			if (!GenerateLaunchConfig(LaunchConfig, &LaunchConfigDescription, /*bGenerateCloudConfig*/ false))
			{
				return;
			}

			if (LocalDeploymentManager->IsLocalDeploymentRunning() || LocalDeploymentManager->IsDeploymentStarting()
				|| LocalDeploymentManager->IsDeploymentStopping())
			{
				return;
			}

			LocalDeploymentManager->TryStartLocalDeployment(LaunchConfig, RuntimeVersion, LaunchFlags, SnapshotName, TEXT(""), nullptr);
		});
	}

	return true;
}

bool FStopDeployment::Update()
{
	FLocalDeploymentManager* LocalDeploymentManager = SpatialGDK::GetLocalDeploymentManager();

	if (LocalDeploymentManager->IsDeploymentStarting())
	{
		// Wait for deployment to finish starting before stopping it
		return false;
	}

	if (!LocalDeploymentManager->IsLocalDeploymentRunning() && !LocalDeploymentManager->IsDeploymentStopping())
	{
		DeleteWorkerJson();
		return true;
	}

	if (!LocalDeploymentManager->IsDeploymentStopping())
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [LocalDeploymentManager] {
			DeleteWorkerJson();
			LocalDeploymentManager->TryStopLocalDeployment();
		});
	}

	return true;
}

bool FWaitForDeployment::Update()
{
	// Workaround code to initialize StartTime outside of latent command execution
	// Allows for this function to be executed immediately
	if (StartTime == 0.0)
	{
		StartTime = FPlatformTime::Seconds();
	}

	FLocalDeploymentManager* const LocalDeploymentManager = SpatialGDK::GetLocalDeploymentManager();

	if (LocalDeploymentManager->IsDeploymentStarting())
	{
		// Wait for deployment to finish starting before stopping it
		return false;
	}

	const double NewTime = FPlatformTime::Seconds();

	if (NewTime - StartTime >= MAX_WAIT_TIME_FOR_LOCAL_DEPLOYMENT_OPERATION)
	{
		// The given time for the deployment to start/stop has expired - test its current state.
		if (ExpectedDeploymentState == EDeploymentState::IsRunning)
		{
			Test->TestTrue(TEXT("Deployment is running"),
						   LocalDeploymentManager->IsLocalDeploymentRunning() && !LocalDeploymentManager->IsDeploymentStopping());
		}
		else
		{
			Test->TestFalse(TEXT("Deployment is not running"),
							LocalDeploymentManager->IsLocalDeploymentRunning() || LocalDeploymentManager->IsDeploymentStopping());
		}
		return true;
	}

	if (LocalDeploymentManager->IsDeploymentStopping())
	{
		return false;
	}
	else
	{
		return (ExpectedDeploymentState == EDeploymentState::IsRunning) ? LocalDeploymentManager->IsLocalDeploymentRunning()
																		: !LocalDeploymentManager->IsLocalDeploymentRunning();
	}
}

bool FCheckDeploymentState::Update()
{
	FLocalDeploymentManager* LocalDeploymentManager = SpatialGDK::GetLocalDeploymentManager();

	if (ExpectedDeploymentState == EDeploymentState::IsRunning)
	{
		Test->TestTrue(TEXT("Deployment is running"),
					   LocalDeploymentManager->IsLocalDeploymentRunning() && !LocalDeploymentManager->IsDeploymentStopping());
	}
	else
	{
		Test->TestFalse(TEXT("Deployment is not running"),
						LocalDeploymentManager->IsLocalDeploymentRunning() || LocalDeploymentManager->IsDeploymentStopping());
	}

	return true;
}

// Wrapper function for automation latent commands to execute them commands synchronously
void ExecuteLatentCommandSynchronously(IAutomationLatentCommand* Command)
{
	while (true)
	{
		bool bResult = Command->Update();
		if (bResult)
		{
			break;
		}
		else
		{
			continue;
		}
	}
}
