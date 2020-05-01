// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "LocalDeploymentManager.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonWriter.h"
#include "Templates/SharedPointer.h"
#include "TickableEditorObject.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

class FMenuBuilder;
class FSpatialGDKEditor;
class FToolBarBuilder;
class FUICommandList;
class SSpatialGDKSimulatedPlayerDeployment;
class SWindow;
class USoundBase;

struct FWorkerTypeLaunchSection;

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialGDKEditorToolbar, Log, All);

class FSpatialGDKEditorToolbarModule : public IModuleInterface, public FTickableEditorObject
{
public:
	FSpatialGDKEditorToolbarModule();

	void StartupModule() override;
	void ShutdownModule() override;
	void PreUnloadCallback() override;

	/** FTickableEditorObject interface */
	void Tick(float DeltaTime) override;
	bool IsTickable() const override
	{
		return true;
	}

	TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSpatialGDKEditorToolbarModule, STATGROUP_Tickables);
	}

	void OnShowSuccessNotification(const FString& NotificationText);
	void OnShowFailedNotification(const FString& NotificationText);
	void OnShowTaskStartNotification(const FString& NotificationText);

	FReply OnLaunchDeployment();
	void OnBuildSuccess();
	bool CanLaunchDeployment() const;

	/** Delegate to determine the 'Launch Deployment' button enabled state */
	bool IsDeploymentConfigurationValid() const;
	bool CanBuildAndUpload() const;

	bool IsSimulatedPlayersEnabled() const;
	/** Delegate called when the user either clicks the simulated players checkbox */
	void OnCheckedSimulatedPlayers();

	bool IsBuildClientWorkerEnabled() const;
	void OnCheckedBuildClientWorker();

private:
	void MapActions(TSharedPtr<FUICommandList> PluginCommands);
	void SetupToolbar(TSharedPtr<FUICommandList> PluginCommands);
	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	void VerifyAndStartDeployment();

	void StartNoAutomaticConnectionButtonClicked();
	void StartLocalSpatialDeploymentButtonClicked();
	void StartCloudSpatialDeploymentButtonClicked();
	void StopSpatialDeploymentButtonClicked();

	void StartSpatialServiceButtonClicked();
	void StopSpatialServiceButtonClicked();

	bool StartNoAutomaticConnectionIsVisible() const;
	bool StartNoAutomaticConnectionCanExecute() const;

	bool StartLocalSpatialDeploymentIsVisible() const;
	bool StartLocalSpatialDeploymentCanExecute() const;

	bool StartCloudSpatialDeploymentIsVisible() const;
	bool StartCloudSpatialDeploymentCanExecute() const;

	bool StopSpatialDeploymentIsVisible() const;
	bool StopSpatialDeploymentCanExecute() const;

	bool StartSpatialServiceIsVisible() const;
	bool StartSpatialServiceCanExecute() const;

	bool StopSpatialServiceIsVisible() const;
	bool StopSpatialServiceCanExecute() const;

	void OnToggleSpatialNetworking();
	bool OnIsSpatialNetworkingEnabled() const;

	void GDKEditorSettingsClicked() const;
	bool IsNoAutomaticConnectionSelected() const;
	bool IsLocalDeploymentSelected() const;
	bool IsCloudDeploymentSelected() const;
	bool IsSpatialOSNetFlowConfigurable() const;
	void NoAutomaticConnectionClicked();
	void LocalDeploymentClicked();
	void CloudDeploymentClicked();
	bool IsLocalDeploymentIPEditable() const;
	bool IsCloudDeploymentNameEditable() const;

	void LaunchInspectorWebpageButtonClicked();
	void CreateSnapshotButtonClicked();
	void SchemaGenerateButtonClicked();
	void SchemaGenerateFullButtonClicked();
	void DeleteSchemaDatabaseButtonClicked();
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	void ShowSimulatedPlayerDeploymentDialog();
	void OpenLaunchConfigurationEditor();
	void LaunchOrShowDeployment();

	void AddDeploymentTagIfMissing(const FString& Tag);

private:
	bool CanExecuteSchemaGenerator() const;
	bool CanExecuteSnapshotGenerator() const;

	TSharedRef<SWidget> CreateGenerateSchemaMenuContent();
	TSharedRef<SWidget> CreateLaunchDeploymentMenuContent();
	TSharedRef<SWidget> CreateStartDropDownMenuContent();

	void ShowTaskStartNotification(const FString& NotificationText);

	void ShowSuccessNotification(const FString& NotificationText);

	void ShowFailedNotification(const FString& NotificationText);

	bool FillWorkerLaunchConfigFromWorldSettings(UWorld& World, FWorkerTypeLaunchSection& OutLaunchConfig, FIntPoint& OutWorldDimension);

	void GenerateSchema(bool bFullScan);

	bool IsSnapshotGenerated() const;
	bool IsSchemaGenerated() const;

	FString GetOptionalExposedRuntimeIP() const;

	void RefreshAutoStartLocalDeployment();

	static void ShowCompileLog();

	TSharedPtr<FUICommandList> PluginCommands;
	FDelegateHandle OnPropertyChangedDelegateHandle;
	bool bStopSpatialOnExit;

	bool bSchemaBuildError;

	TWeakPtr<SNotificationItem> TaskNotificationPtr;

	// Sounds used for execution of tasks.
	USoundBase* ExecutionStartSound;
	USoundBase* ExecutionSuccessSound;
	USoundBase* ExecutionFailSound;

	TFuture<bool> SchemaGeneratorResult;
	TSharedPtr<FSpatialGDKEditor> SpatialGDKEditorInstance;

	TSharedPtr<SWindow> SimulatedPlayerDeploymentWindowPtr;
	TSharedPtr<SSpatialGDKSimulatedPlayerDeployment> SimulatedPlayerDeploymentConfigPtr;
	
	FLocalDeploymentManager* LocalDeploymentManager;

	TFuture<bool> AttemptSpatialAuthResult;
};
