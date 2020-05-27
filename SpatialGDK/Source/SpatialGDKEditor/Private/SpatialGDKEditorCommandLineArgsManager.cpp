// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKEditorCommandLineArgsManager.h"

#include "HAL/PlatformFilemanager.h"
#include "IOSRuntimeSettings.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Serialization/JsonSerializer.h"

#ifdef ENABLE_LAUNCHER_DELEGATE
#include "ILauncher.h"
#include "ILauncherServicesModule.h"
#include "ILauncherWorker.h"
#endif // ENABLE_LAUNCHER_DELEGATE

#include "SpatialCommandUtils.h"
#include "SpatialGDKEditorSettings.h"
#include "SpatialGDKSettings.h"

DEFINE_LOG_CATEGORY(LogSpatialGDKEditorCommandLineArgsManager);

FSpatialGDKEditorCommandLineArgsManager::FSpatialGDKEditorCommandLineArgsManager()
#ifdef ENABLE_LAUNCHER_DELEGATE
	: bAndroidDevice(false)
#endif // ENABLE_LAUNCHER_DELEGATE
{
}

void FSpatialGDKEditorCommandLineArgsManager::Init()
{
#ifdef ENABLE_LAUNCHER_DELEGATE
	ILauncherServicesModule& LauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
	LauncherServicesModule.OnCreateLauncherDelegate.AddRaw(this, &FSpatialGDKEditorCommandLineArgsManager::OnCreateLauncher);
#endif // ENABLE_LAUNCHER_DELEGATE
}

#ifdef ENABLE_LAUNCHER_DELEGATE
void FSpatialGDKEditorCommandLineArgsManager::OnLauncherCanceled(double ExecutionTime)
{
	RemoveCommandLineFromDevice();
}

void FSpatialGDKEditorCommandLineArgsManager::OnLauncherFinished(bool Outcome, double ExecutionTime, int32 ReturnCode)
{
	RemoveCommandLineFromDevice();
}

void FSpatialGDKEditorCommandLineArgsManager::RemoveCommandLineFromDevice()
{
	if (bAndroidDevice)
	{
		RemoveCommandLineFromAndroidDevice();
	}
}

void FSpatialGDKEditorCommandLineArgsManager::OnLaunch(ILauncherWorkerPtr LauncherWorkerPtr, ILauncherProfileRef LauncherProfileRef)
{
	LauncherWorkerPtr->OnCanceled().AddRaw(this, &FSpatialGDKEditorCommandLineArgsManager::OnLauncherCanceled);
	LauncherWorkerPtr->OnCompleted().AddRaw(this, &FSpatialGDKEditorCommandLineArgsManager::OnLauncherFinished);

	bAndroidDevice = false;
	TArray<ILauncherTaskPtr> TaskList;
	LauncherWorkerPtr->GetTasks(TaskList);
	for (const ILauncherTaskPtr& Task : TaskList)
	{
		if (Task->GetDesc().Contains(TEXT("android")))
		{
			bAndroidDevice = true;
		}
	}
	if (bAndroidDevice)
	{
		UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Log, TEXT("Android device launched"));
	}
}

void FSpatialGDKEditorCommandLineArgsManager::OnCreateLauncher(ILauncherRef LauncherRef)
{
	LauncherRef->FLauncherWorkerStartedDelegate.AddRaw(this, &FSpatialGDKEditorCommandLineArgsManager::OnLaunch);
}
#endif // ENABLE_LAUNCHER_DELEGATE

namespace
{

FString GetAdbExePath()
{
	FString AndroidHome = FPlatformMisc::GetEnvironmentVariable(TEXT("ANDROID_HOME"));
	if (AndroidHome.IsEmpty())
	{
		UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Error, TEXT("Environment variable ANDROID_HOME is not set. Please make sure to configure this."));
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Environment variable ANDROID_HOME is not set. Please make sure to configure this.")));
		return TEXT("");
	}

#if PLATFORM_WINDOWS
	const FString AdbExe = FPaths::ConvertRelativePathToFull(FPaths::Combine(AndroidHome, TEXT("platform-tools/adb.exe")));
#else
	const FString AdbExe = FPaths::ConvertRelativePathToFull(FPaths::Combine(AndroidHome, TEXT("platform-tools/adb")));
#endif

	return AdbExe;
}

} // anonymous namespace

FReply FSpatialGDKEditorCommandLineArgsManager::PushCommandLineToIOSDevice()
{
	const UIOSRuntimeSettings* IOSRuntimeSettings = GetDefault<UIOSRuntimeSettings>();
	FString OutCommandLineArgsFile;

	if (!TryConstructMobileCommandLineArgumentsFile(OutCommandLineArgsFile))
	{
		return FReply::Unhandled();
	}

	FString Executable = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/DotNET/IOS/deploymentserver.exe")));
	FString DeploymentServerArguments = FString::Printf(TEXT("copyfile -bundle \"%s\" -file \"%s\" -file \"/Documents/ue4commandline.txt\""), *(IOSRuntimeSettings->BundleIdentifier.Replace(TEXT("[PROJECT_NAME]"), FApp::GetProjectName())), *OutCommandLineArgsFile);

#if PLATFORM_MAC
	DeploymentServerArguments = FString::Printf(TEXT("%s %s"), *Executable, *DeploymentServerArguments);
	Executable = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Mono/Mac/bin/mono")));
#endif

	if (!TryPushCommandLineArgsToDevice(Executable, DeploymentServerArguments, OutCommandLineArgsFile))
	{
		return FReply::Unhandled();
	}

	return FReply::Handled();
}

FReply FSpatialGDKEditorCommandLineArgsManager::PushCommandLineToAndroidDevice()
{
	const FString AdbExe = GetAdbExePath();
	if (AdbExe.IsEmpty())
	{
		return FReply::Unhandled();
	}

	FString OutCommandLineArgsFile;

	if (!TryConstructMobileCommandLineArgumentsFile(OutCommandLineArgsFile))
	{
		return FReply::Unhandled();
	}

	const FString AndroidCommandLineFile = FString::Printf(TEXT("/mnt/sdcard/UE4Game/%s/UE4CommandLine.txt"), *FString(FApp::GetProjectName()));
	const FString AdbArguments = FString::Printf(TEXT("push \"%s\" \"%s\""), *OutCommandLineArgsFile, *AndroidCommandLineFile);

	if (!TryPushCommandLineArgsToDevice(AdbExe, AdbArguments, OutCommandLineArgsFile))
	{
		return FReply::Unhandled();
	}

	return FReply::Handled();
}

FReply FSpatialGDKEditorCommandLineArgsManager::RemoveCommandLineFromAndroidDevice()
{
	const FString AdbExe = GetAdbExePath();
	if (AdbExe.IsEmpty())
	{
		return FReply::Unhandled();
	}

	FString ExeOutput;
	FString StdErr;
	int32 ExitCode;

	FString ExeArguments = FString::Printf(TEXT("shell rm -f /mnt/sdcard/UE4Game/%s/UE4CommandLine.txt"), FApp::GetProjectName());

	FPlatformProcess::ExecProcess(*AdbExe, *ExeArguments, &ExitCode, &ExeOutput, &StdErr);
	if (ExitCode != 0)
	{
		UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Error, TEXT("Failed to remove settings from the mobile client. %s %s"), *ExeOutput, *StdErr);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to remove settings from the mobile client. See the Output log for more information.")));
		return FReply::Unhandled();
	}
	UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Log, TEXT("Remove ue4commandline.txt from the Android device. %s %s"), *ExeOutput, *StdErr);

	return FReply::Handled();
}

bool FSpatialGDKEditorCommandLineArgsManager::TryConstructMobileCommandLineArgumentsFile(FString& CommandLineArgsFile)
{
	const USpatialGDKEditorSettings* SpatialGDKSettings = GetDefault<USpatialGDKEditorSettings>();
	const FString ProjectName = FApp::GetProjectName();

	// The project path is based on this: https://github.com/improbableio/UnrealEngine/blob/4.22-SpatialOSUnrealGDK-release/Engine/Source/Programs/AutomationTool/AutomationUtils/DeploymentContext.cs#L408
	const FString MobileProjectPath = FString::Printf(TEXT("../../../%s/%s.uproject"), *ProjectName, *ProjectName);
	FString TravelUrl;
	FString SpatialOSOptions = FString::Printf(TEXT("-workerType %s"), *(SpatialGDKSettings->MobileWorkerType));
	if (SpatialGDKSettings->bMobileConnectToLocalDeployment)
	{
		if (SpatialGDKSettings->MobileRuntimeIP.IsEmpty())
		{
			UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Error, TEXT("The Runtime IP is currently not set. Please make sure to specify a Runtime IP."));
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("The Runtime IP is currently not set. Please make sure to specify a Runtime IP."))));
			return false;
		}

		TravelUrl = SpatialGDKSettings->MobileRuntimeIP;
	}
	else
	{
		TravelUrl = TEXT("connect.to.spatialos");

		if (SpatialGDKSettings->DevelopmentAuthenticationToken.IsEmpty())
		{
			FReply GeneratedTokenReply = GenerateDevAuthToken();
			if (!GeneratedTokenReply.IsEventHandled())
			{
				return false;
			}
		}

		SpatialOSOptions += FString::Printf(TEXT(" +devauthToken %s"), *(SpatialGDKSettings->DevelopmentAuthenticationToken));
		if (!SpatialGDKSettings->DevelopmentDeploymentToConnect.IsEmpty())
		{
			SpatialOSOptions += FString::Printf(TEXT(" +deployment %s"), *(SpatialGDKSettings->DevelopmentDeploymentToConnect));
		}
	}

	const FString SpatialOSCommandLineArgs = FString::Printf(TEXT("%s %s %s %s"), *MobileProjectPath, *TravelUrl, *SpatialOSOptions, *(SpatialGDKSettings->MobileExtraCommandLineArgs));
	CommandLineArgsFile = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::ProjectLogDir(), TEXT("ue4commandline.txt")));

	if (!FFileHelper::SaveStringToFile(SpatialOSCommandLineArgs, *CommandLineArgsFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Error, TEXT("Failed to write command line args to file: %s"), *CommandLineArgsFile);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Failed to write command line args to file: %s"), *CommandLineArgsFile)));
		return false;
	}

	return true;
}

FReply FSpatialGDKEditorCommandLineArgsManager::GenerateDevAuthToken()
{
	FString DevAuthToken;
	FString ErrorMessage;
	if (!SpatialCommandUtils::GenerateDevAuthToken(GetMutableDefault<USpatialGDKSettings>()->IsRunningInChina(), DevAuthToken, ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorMessage));
		return FReply::Unhandled();
	}
	if (USpatialGDKEditorSettings* SpatialGDKEditorSettings = GetMutableDefault<USpatialGDKEditorSettings>())
	{
		SpatialGDKEditorSettings->SetDevelopmentAuthenticationToken(DevAuthToken);
	}
	return FReply::Handled();
}

bool FSpatialGDKEditorCommandLineArgsManager::TryPushCommandLineArgsToDevice(const FString& Executable, const FString& ExeArguments, const FString& CommandLineArgsFile)
{
	FString ExeOutput;
	FString StdErr;
	int32 ExitCode;

	FPlatformProcess::ExecProcess(*Executable, *ExeArguments, &ExitCode, &ExeOutput, &StdErr);
	if (ExitCode != 0)
	{
		UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Error, TEXT("Failed to update the mobile client. %s %s"), *ExeOutput, *StdErr);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to update the mobile client. See the Output log for more information.")));
		return false;
	}

	UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Log, TEXT("Successfully stored command line args on device: %s"), *ExeOutput);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DeleteFile(*CommandLineArgsFile))
	{
		UE_LOG(LogSpatialGDKEditorCommandLineArgsManager, Error, TEXT("Failed to delete file %s"), *CommandLineArgsFile);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Failed to delete file %s"), *CommandLineArgsFile)));
		return false;
	}

	return true;
}
