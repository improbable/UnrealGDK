// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "EngineClasses/SpatialGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "GeneralProjectSettings.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Settings/LevelEditorPlaySettings.h"
#endif

#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPendingNetGame.h"
#include "Interop/Connection/SpatialWorkerConnection.h"
#include "Utils/SpatialDebugger.h"
#include "Utils/SpatialMetrics.h"
#include "Utils/SpatialMetricsDisplay.h"
#include "Utils/SpatialLatencyTracer.h"

DEFINE_LOG_CATEGORY(LogSpatialGameInstance);

bool USpatialGameInstance::HasSpatialNetDriver() const
{
	bool bHasSpatialNetDriver = false;

	if (WorldContext != nullptr)
	{
		UWorld* World = GetWorld();
		UNetDriver * NetDriver = GEngine->FindNamedNetDriver(World, NAME_PendingNetDriver);
		bool bShouldDestroyNetDriver = false;

		if (NetDriver == nullptr)
		{
			// If Spatial networking is enabled, override the GameNetDriver with the SpatialNetDriver
			if (GetDefault<UGeneralProjectSettings>()->UsesSpatialNetworking())
			{
				if (FNetDriverDefinition* DriverDefinition = GEngine->NetDriverDefinitions.FindByPredicate([](const FNetDriverDefinition& CurDef)
				{
					return CurDef.DefName == NAME_GameNetDriver;
				}))
				{
					DriverDefinition->DriverClassName = DriverDefinition->DriverClassNameFallback = TEXT("/Script/SpatialGDK.SpatialNetDriver");
				}
			}

			bShouldDestroyNetDriver = GEngine->CreateNamedNetDriver(World, NAME_PendingNetDriver, NAME_GameNetDriver);
			NetDriver = GEngine->FindNamedNetDriver(World, NAME_PendingNetDriver);
		}

		if (NetDriver != nullptr)
		{
			bHasSpatialNetDriver = NetDriver->IsA<USpatialNetDriver>();

			if (bShouldDestroyNetDriver)
			{
				GEngine->DestroyNamedNetDriver(World, NAME_PendingNetDriver);
			}
		}
	}

	if (GetDefault<UGeneralProjectSettings>()->UsesSpatialNetworking() && !bHasSpatialNetDriver)
	{
		UE_LOG(LogSpatialGameInstance, Error, TEXT("Could not find SpatialNetDriver even though Spatial networking is switched on! "
										  "Please make sure you set up the net driver definitions as specified in the porting "
										  "guide and that you don't override the main net driver."));
	}

	return bHasSpatialNetDriver;
}

void USpatialGameInstance::CreateNewSpatialWorkerConnection()
{
	SpatialConnection = NewObject<USpatialWorkerConnection>(this);

#if TRACE_LIB_ACTIVE
	SpatialConnection->OnEnqueueMessage.AddUObject(SpatialLatencyTracer, &USpatialLatencyTracer::OnEnqueueMessage);
	SpatialConnection->OnDequeueMessage.AddUObject(SpatialLatencyTracer, &USpatialLatencyTracer::OnDequeueMessage);
#endif
}

void USpatialGameInstance::DestroySpatialWorkerConnection()
{
	if (SpatialConnection != nullptr)
	{
		SpatialConnection->DestroyConnection();
		SpatialConnection = nullptr;
	}
}

#if WITH_EDITOR
FGameInstancePIEResult USpatialGameInstance::StartPlayInEditorGameInstance(ULocalPlayer* LocalPlayer, const FGameInstancePIEParameters& Params)
{
	if (HasSpatialNetDriver())
	{
		// If we are using spatial networking then prepare a spatial connection.
		CreateNewSpatialWorkerConnection();
	}

	return Super::StartPlayInEditorGameInstance(LocalPlayer, Params);
}
#endif

void USpatialGameInstance::StartGameInstance()
{
	if (HasSpatialNetDriver())
	{
		// If we are using spatial networking then prepare a spatial connection.
		CreateNewSpatialWorkerConnection();

		// Native Unreal creates a NetDriver and attempts to automatically connect if a Host is specified as the first commandline argument.
		// Since the SpatialOS Launcher does not specify this, we need to check for a locator loginToken to allow automatic connection to provide parity with native.
		// If a developer wants to use the Launcher and NOT automatically connect they will have to set the `PreventAutoConnectWithLocator` flag to true.
		if (!bPreventAutoConnectWithLocator)
		{
			// Initialize a locator configuration which will parse command line arguments.
			FLocatorConfig LocatorConfig;
			if (LocatorConfig.TryLoadCommandLineArgs())
			{
				// Modify the commandline args to have a Host IP to force a NetDriver to be used.
				const TCHAR* CommandLineArgs = FCommandLine::Get();

				FString NewCommandLineArgs = LocatorConfig.LocatorHost + TEXT(" ");
				NewCommandLineArgs.Append(FString(CommandLineArgs));

				FCommandLine::Set(*NewCommandLineArgs);
			}
		}
	}

	Super::StartGameInstance();
}

bool USpatialGameInstance::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	if (Super::ProcessConsoleExec(Cmd, Ar, Executor))
	{
		return true;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(World->GetNetDriver()))
		{
			if (NetDriver->SpatialMetrics && NetDriver->SpatialMetrics->ProcessConsoleExec(Cmd, Ar, Executor))
			{
				return true;
			}

			if (NetDriver->SpatialMetricsDisplay && NetDriver->SpatialMetricsDisplay->ProcessConsoleExec(Cmd, Ar, Executor))
			{
				return true;
			}

			if (NetDriver->SpatialDebugger && NetDriver->SpatialDebugger->ProcessConsoleExec(Cmd, Ar, Executor))
			{
				return true;
			}
		}
	}

	return false;
}

void USpatialGameInstance::Init()
{
	Super::Init();

	SpatialLatencyTracer = NewObject<USpatialLatencyTracer>(this);
}

void USpatialGameInstance::HandleOnConnected()
{
	UE_LOG(LogSpatialGameInstance, Log, TEXT("Successfully connected to SpatialOS"));
	SpatialWorkerId = SpatialConnection->GetWorkerId();
#if TRACE_LIB_ACTIVE
	SpatialLatencyTracer->SetWorkerId(SpatialWorkerId);
#endif
	OnConnected.Broadcast();
}

void USpatialGameInstance::HandleOnConnectionFailed(const FString& Reason)
{
	UE_LOG(LogSpatialGameInstance, Error, TEXT("Could not connect to SpatialOS. Reason: %s"), *Reason);
#if TRACE_LIB_ACTIVE
	SpatialLatencyTracer->ResetWorkerId();
#endif
	OnConnectionFailed.Broadcast(Reason);
}
