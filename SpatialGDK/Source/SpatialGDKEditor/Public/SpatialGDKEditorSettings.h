// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Misc/Paths.h"

#include "SpatialGDKEditorSettings.generated.h"

USTRUCT()
struct FWorldLaunchSection
{
	GENERATED_BODY()

	FWorldLaunchSection()
		: Dimensions(2000, 2000)
		, ChunkEdgeLenghtMeters(50)
		, StreamingQueryInterval(4)
		, SnapshotWritePeriodSeconds(0)
	{
		LegacyFlags.Add(TEXT("bridge_qos_max_timeout"), TEXT("0"));
		LegacyFlags.Add(TEXT("bridge_soft_handover_enabled"), TEXT("false"));
		LegacyFlags.Add(TEXT("enable_chunk_interest"), TEXT("false"));
	}

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Dimensions"))
	FIntPoint Dimensions;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Chunk edge length in meters"))
	int32 ChunkEdgeLenghtMeters;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Streaming query interval"))
	int32 StreamingQueryInterval;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Snapshot write period in seconds"))
	int32 SnapshotWritePeriodSeconds;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Legacy flags"))
	TMap<FString, FString> LegacyFlags;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Legacy java parameters"))
	TMap<FString, FString> LegacyJavaParams;
};

USTRUCT()
struct FWorkerPermissionsSection
{
	GENERATED_BODY()

	FWorkerPermissionsSection()
		: bAllPermissions(true)
		, bAllowEntityCreation(true)
		, bAllowEntityDeletion(true)
		, bAllowEntityQuery(true)
		, Components()
	{

	}

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "All"))
	bool bAllPermissions;

	UPROPERTY(EditAnywhere, config, meta = (EditCondition = "!bAllPermissions", ConfigRestartRequired = false, DisplayName = "Allow entity creation"))
	bool bAllowEntityCreation;

	UPROPERTY(EditAnywhere, config, meta = (EditCondition = "!bAllPermissions", ConfigRestartRequired = false, DisplayName = "Allow entity deletion"))
	bool bAllowEntityDeletion;

	UPROPERTY(EditAnywhere, config, meta = (EditCondition = "!bAllPermissions", ConfigRestartRequired = false, DisplayName = "Allow entity query"))
	bool bAllowEntityQuery;

	UPROPERTY(EditAnywhere, config, meta = (EditCondition = "!bAllPermissions", ConfigRestartRequired = false, DisplayName = "Component queries"))
	TArray<FString> Components;
};

USTRUCT()
struct FLoginRateLimitSection
{
	GENERATED_BODY()

	FLoginRateLimitSection()
		: Duration()
		, RequestsPerDuration(0)
	{

	}

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Duration"))
	FString Duration;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Requests per duration", ClampMin = "1", UIMin = "1"))
	int32 RequestsPerDuration;
};

USTRUCT()
struct FWorkerTypeLaunchSection
{
	GENERATED_BODY()

	FWorkerTypeLaunchSection()
		: WorkerTypeName()
		, WorkerPermissions()
		, MaxConnectionCapacityLimit(0)
		, bLoginRateLimitEnabled(false)
		, LoginRateLimit()
		, Columns(1)
		, Rows(1)
		, ManualWorkerConnectionOnly(true)
	{

	}

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Worker type name"))
	FString WorkerTypeName;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Worker permissions"))
	FWorkerPermissionsSection WorkerPermissions;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Max connection capacity limit (0 = Unlimited capacity)", ClampMin = "0", UIMin = "0"))
	int32 MaxConnectionCapacityLimit;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Login rate limit enabled"))
	bool bLoginRateLimitEnabled;

	UPROPERTY(EditAnywhere, config, meta = (EditCondition = "bLoginRateLimitEnabled", ConfigRestartRequired = false, DisplayName = "Login rate limit"))
	FLoginRateLimitSection LoginRateLimit;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Rectangle grid column count", ClampMin = "1", UIMin = "1"))
	int32 Columns;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Rectangle grid row count", ClampMin = "1", UIMin = "1"))
	int32 Rows;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Flags"))
	TMap<FString, FString> Flags;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Manual Worker Connection Only"))
	bool ManualWorkerConnectionOnly;
};

USTRUCT()
struct FSpatialLaunchConfigDescription
{
	GENERATED_BODY()

	FSpatialLaunchConfigDescription()
		: Template(TEXT("small"))
		, World()
	{
		FWorkerTypeLaunchSection UnrealWorkerDefaultSetting;
		UnrealWorkerDefaultSetting.WorkerTypeName = TEXT("UnrealWorker");
		UnrealWorkerDefaultSetting.Rows = 1;
		UnrealWorkerDefaultSetting.Columns = 1;
		UnrealWorkerDefaultSetting.ManualWorkerConnectionOnly = true;

		Workers.Add(UnrealWorkerDefaultSetting);
	}

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Template"))
	FString Template;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "World"))
	FWorldLaunchSection World;

	UPROPERTY(EditAnywhere, config, meta = (ConfigRestartRequired = false, DisplayName = "Workers"))
	TArray<FWorkerTypeLaunchSection> Workers;
};

UCLASS(config = SpatialGDKEditorSettings, defaultconfig)
class SPATIALGDKEDITOR_API USpatialGDKEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	USpatialGDKEditorSettings(const FObjectInitializer& ObjectInitializer);

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;

private:
	/** Path to the directory containing the SpatialOS-related files. */
	UPROPERTY(EditAnywhere, config, Category = "General", meta = (ConfigRestartRequired = false, DisplayName = "SpatialOS directory"))
	FDirectoryPath SpatialOSDirectory;

public:
	/** If checked, all dynamically spawned entities will be deleted when server workers disconnect. */
	UPROPERTY(EditAnywhere, config, Category = "Play in editor settings", meta = (ConfigRestartRequired = false, DisplayName = "Delete dynamically spawned entities"))
	bool bDeleteDynamicEntities;

	/** If checked, a launch configuration will be generated by default when launching spatial through the toolbar. */
	UPROPERTY(EditAnywhere, config, Category = "Launch", meta = (ConfigRestartRequired = false, DisplayName = "Generate default launch config"))
	bool bGenerateDefaultLaunchConfig;

private:
	/** Launch configuration file used for `spatial local launch`. */
	UPROPERTY(EditAnywhere, config, Category = "Launch", meta = (EditCondition = "!bGenerateDefaultLaunchConfig", ConfigRestartRequired = false, DisplayName = "Launch configuration"))
	FFilePath SpatialOSLaunchConfig;

public:
	/** Stop `spatial local launch` when shutting down editor. */
	UPROPERTY(EditAnywhere, config, Category = "Launch", meta = (ConfigRestartRequired = false, DisplayName = "Stop on exit"))
	bool bStopSpatialOnExit;

private:
	/** Path to your SpatialOS snapshot. */
	UPROPERTY(EditAnywhere, config, Category = "Snapshots", meta = (ConfigRestartRequired = false, DisplayName = "Snapshot path"))
	FDirectoryPath SpatialOSSnapshotPath;

	/** Name of your SpatialOS snapshot file. */
	UPROPERTY(EditAnywhere, config, Category = "Snapshots", meta = (ConfigRestartRequired = false, DisplayName = "Snapshot file name"))
	FString SpatialOSSnapshotFile;

	/** If checked, the GDK creates a launch configuration file by default when you launch a local deployment through the toolbar. */
	UPROPERTY(EditAnywhere, config, Category = "Schema", meta = (ConfigRestartRequired = false, DisplayName = "Output path for the generated schemas"))
	FDirectoryPath GeneratedSchemaOutputFolder;

	/** Command line flags passed in to `spatial local launch`.*/
	UPROPERTY(EditAnywhere, config, Category = "Launch", meta = (ConfigRestartRequired = false, DisplayName = "Command line flags for local launch"))
	TArray<FString> SpatialOSCommandLineLaunchFlags;

public:
	/** Launch configuration description. */
	UPROPERTY(EditAnywhere, config, Category = "Launch", meta = (EditCondition = "bGenerateDefaultLaunchConfig", ConfigRestartRequired = false, DisplayName = "Launch configuration description"))
	FSpatialLaunchConfigDescription LaunchConfigDesc;

	/** If checked, placeholder entities will be added to the snapshot on generation */
	UPROPERTY(EditAnywhere, config, Category = "Snapshots", meta = (ConfigRestartRequired = false, DisplayName = "Generate placeholder entities in snapshot"))
	bool bGeneratePlaceholderEntitiesInSnapshot;

	FORCEINLINE FString GetSpatialOSDirectory() const
	{
		return SpatialOSDirectory.Path.IsEmpty()
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("/../spatial/")))
			: SpatialOSDirectory.Path;
	}

	FORCEINLINE FString GetSpatialOSLaunchConfig() const
	{
		return SpatialOSLaunchConfig.FilePath.IsEmpty()
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("/../spatial/default_launch.json")))
			: SpatialOSLaunchConfig.FilePath;
	}

	FORCEINLINE FString GetSpatialOSSnapshotFile() const
	{
		return SpatialOSSnapshotFile.IsEmpty()
			? FString(TEXT("default.snapshot"))
			: SpatialOSSnapshotFile;
	}

	FORCEINLINE FString GetSpatialOSSnapshotFolderPath() const
	{
		return SpatialOSSnapshotPath.Path.IsEmpty()
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(GetSpatialOSDirectory(), TEXT("../spatial/snapshots/")))
			: SpatialOSSnapshotPath.Path;
	}

	FORCEINLINE FString GetGeneratedSchemaOutputFolder() const
	{
		return GeneratedSchemaOutputFolder.Path.IsEmpty()
			? FPaths::ConvertRelativePathToFull(FPaths::Combine(GetSpatialOSDirectory(), FString(TEXT("schema/unreal/generated/"))))
			: GeneratedSchemaOutputFolder.Path;
	}

	FORCEINLINE FString GetSpatialOSCommandLineLaunchFlags() const
	{
		FString CommandLineLaunchFlags = TEXT("");

		for (FString Flag : SpatialOSCommandLineLaunchFlags)
		{
			Flag = Flag.StartsWith(TEXT("--")) ? Flag : TEXT("--") + Flag;
			CommandLineLaunchFlags += Flag + TEXT(" ");
		}

		return CommandLineLaunchFlags;
	}
};
