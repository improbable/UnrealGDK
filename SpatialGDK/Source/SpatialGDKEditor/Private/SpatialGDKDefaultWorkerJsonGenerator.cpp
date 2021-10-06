// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKDefaultWorkerJsonGenerator.h"

#include "SpatialGDKServicesConstants.h"
#include "SpatialGDKSettings.h"

#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY(LogSpatialGDKDefaultWorkerJsonGenerator);
#define LOCTEXT_NAMESPACE "SpatialGDKDefaultWorkerJsonGenerator"

bool GenerateWorkerJsonFromUnused(const FString& JsonPath, const FString& UnusedJsonPath, bool& bOutRedeployRequired)
{
	IFileManager& FileManager = IFileManager::Get();
	if (FileManager.Move(*JsonPath, *UnusedJsonPath))
	{
		bOutRedeployRequired = true;
		UE_LOG(LogSpatialGDKDefaultWorkerJsonGenerator, Verbose, TEXT("Found an unused worker json at %s and moved it to %s"),
			   *UnusedJsonPath, *JsonPath);
		return true;
	}
	else
	{
		UE_LOG(LogSpatialGDKDefaultWorkerJsonGenerator, Error, TEXT("Failed to move unused worker json from %s to %s"), *UnusedJsonPath,
			   *JsonPath);
	}

	return false;
}

bool GenerateDefaultWorkerJson(const FString& JsonPath, bool& bOutRedeployRequired)
{
	const FString TemplateWorkerJsonPath =
		FSpatialGDKServicesModule::GetSpatialGDKPluginDirectory(TEXT("SpatialGDK/Extras/templates/WorkerJsonTemplate.json"));

	FString Contents;
	if (FFileHelper::LoadFileToString(Contents, *TemplateWorkerJsonPath))
	{
		if (FFileHelper::SaveStringToFile(Contents, *JsonPath))
		{
			bOutRedeployRequired = true;
			UE_LOG(LogSpatialGDKDefaultWorkerJsonGenerator, Verbose, TEXT("Wrote default worker json to %s"), *JsonPath)

			return true;
		}
		else
		{
			UE_LOG(LogSpatialGDKDefaultWorkerJsonGenerator, Error, TEXT("Failed to write default worker json to %s"), *JsonPath)
		}
	}
	else
	{
		UE_LOG(LogSpatialGDKDefaultWorkerJsonGenerator, Error, TEXT("Failed to read default worker json template at %s"),
			   *TemplateWorkerJsonPath)
	}

	return false;
}

bool GenerateAllDefaultWorkerJsons(bool& bOutRedeployRequired)
{
	const FString WorkerJsonDir = FPaths::Combine(SpatialGDKServicesConstants::SpatialOSDirectory, TEXT("workers/unreal"));
	bool bAllJsonsGeneratedSuccessfully = true;

	if (const USpatialGDKSettings* SpatialGDKSettings = GetDefault<USpatialGDKSettings>())
	{
		// Create an array of worker types with a bool signifying whether the associated worker json should exist or not.
		// Then correct the file system state so it matches our expectations.
		TArray<TPair<FName, bool>> WorkerTypes;
		WorkerTypes.Add(TPair<FName, bool>(SpatialConstants::DefaultServerWorkerType, true));
		const bool bRoutingWorkerEnabled = SpatialGDKSettings->CrossServerRPCImplementation == ECrossServerRPCImplementation::RoutingWorker;
		WorkerTypes.Add(TPair<FName, bool>(SpatialConstants::RoutingWorkerType, bRoutingWorkerEnabled));
		WorkerTypes.Add(TPair<FName, bool>(SpatialConstants::StrategyWorkerType, SpatialGDKSettings->bRunStrategyWorker));

		for (const auto& Pair : WorkerTypes)
		{
			bool bShouldFileExist = Pair.Value;
			FString WorkerType = Pair.Key.ToString();
			FString JsonPath = FPaths::Combine(WorkerJsonDir, FString::Printf(TEXT("spatialos.%s.worker.json"), *WorkerType));
			FString UnusedJsonPath = FPaths::Combine(WorkerJsonDir, FString::Printf(TEXT("spatialos.%s.worker.UNUSED.json"), *WorkerType));
			const bool bFileExists = FPaths::FileExists(JsonPath);
			const bool bUnusedFileExists = FPaths::FileExists(UnusedJsonPath);

			if (!bFileExists && bShouldFileExist)
			{
				UE_LOG(LogSpatialGDKDefaultWorkerJsonGenerator, Verbose, TEXT("Could not find worker json at %s"), *JsonPath);

				bool bCreatedWorkerJson = bUnusedFileExists ? GenerateWorkerJsonFromUnused(JsonPath, UnusedJsonPath, bOutRedeployRequired)
															: GenerateDefaultWorkerJson(JsonPath, bOutRedeployRequired);
				bAllJsonsGeneratedSuccessfully = bAllJsonsGeneratedSuccessfully && bCreatedWorkerJson;
			}
			else if (bFileExists && !bShouldFileExist)
			{
				UE_LOG(LogSpatialGDKDefaultWorkerJsonGenerator, Verbose, TEXT("Found worker json at %s"), *JsonPath);

				if (bUnusedFileExists)
				{
					UE_LOG(LogSpatialGDKDefaultWorkerJsonGenerator, Warning,
						   TEXT("Found leftover unused worker json at %s, overwriting with %s"), *UnusedJsonPath, *JsonPath);
				}

				bool bRemovedWorkerJson = GenerateWorkerJsonFromUnused(UnusedJsonPath, JsonPath, bOutRedeployRequired);
				bAllJsonsGeneratedSuccessfully = bAllJsonsGeneratedSuccessfully && bRemovedWorkerJson;
			}
		}

		return bAllJsonsGeneratedSuccessfully;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
