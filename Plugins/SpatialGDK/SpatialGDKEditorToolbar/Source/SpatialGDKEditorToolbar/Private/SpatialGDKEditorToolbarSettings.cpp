// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
#include "SpatialGDKEditorToolbarSettings.h"

USpatialGDKEditorToolbarSettings::USpatialGDKEditorToolbarSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	SpatialOSLaunchConfig(TEXT("default_launch.json")),
	bStopSpatialOnExit(false),
	SpatialOSSnapshotFile(FString(TEXT("default.snapshot")))
{
	ProjectRootFolder.Path = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPaths::GetProjectFilePath()) + FString(TEXT("/../spatial/")));
	SpatialOSSnapshotPath.Path = FPaths::Combine(*ProjectRootFolder.Path, TEXT("snapshots/"));
}

FString USpatialGDKEditorToolbarSettings::ToString()
{
	TArray<FStringFormatArg> Args;
	Args.Add(ProjectRootFolder.Path);
	Args.Add(SpatialOSLaunchConfig);
	Args.Add(bStopSpatialOnExit);
	Args.Add(SpatialOSSnapshotPath.Path);
	Args.Add(SpatialOSSnapshotFile);

	return FString::Format(TEXT("ProjectRootFolder={0}, SpatialOSLaunchArgument={1}, "
		"bStopSpatialOnExit={2}, SpatialOSSnapshotPath={3}, "
		"SpatialOSSnapshotFile={4}"),
						   Args);
}
