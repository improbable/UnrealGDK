// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Misc/MonitoredProcess.h"
#include "Widgets/Notifications/SNotificationList.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialGDKEditorPackageAssembly, Log, All);

class SPATIALGDKEDITOR_API FSpatialGDKPackageAssembly : public TSharedFromThis<FSpatialGDKPackageAssembly>
{
public:
	FSpatialGDKPackageAssembly();

	bool CanBuild() const;

	void BuildAllAndUpload(const FString& AssemblyName, const FString& Configuration, const FString& AdditionalArgs, bool bForce);

	FSimpleDelegate OnSuccess;

private:
	enum class EPackageAssemblyStep
	{
		NONE = 0,
		BUILD_SERVER,
		BUILD_CLIENT,
		BUILD_SIMULATED_PLAYERS,
		UPLOAD_ASSEMBLY,
	};

	TQueue<EPackageAssemblyStep> Steps;

	TSharedPtr<FMonitoredProcess> PackageAssemblyTask;
	TWeakPtr<SNotificationItem> TaskNotificationPtr;

	struct AssemblyDetails
	{
		AssemblyDetails(const FString& Name, const FString& Config, bool bForce);
		void Upload(FSpatialGDKPackageAssembly& PackageAssembly);
		FString AssemblyName;
		FString Configuration;
		bool bForce;
	};

	TUniquePtr<AssemblyDetails> AssemblyDetailsPtr;

	void BuildAssembly(const FString& ProjectName, const FString& Platform, const FString& Configuration, const FString& AdditionalArgs);
	void UploadAssembly(const FString& AssemblyName, bool bForce);

	bool NextStep();

	void ShowTaskStartedNotification(const FString& NotificationText);
	void ShowTaskEndedNotification(const FString& NotificationText, SNotificationItem::ECompletionState CompletionState);
	void HandleCancelButtonClicked();
	void OnTaskCompleted(int32 TaskResult);
	void OnTaskOutput(FString Message);
	void OnTaskCanceled();
};
