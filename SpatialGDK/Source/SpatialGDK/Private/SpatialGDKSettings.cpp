// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKSettings.h"
#include "Improbable/SpatialEngineConstants.h"
#include "Misc/MessageDialog.h"
#include "Misc/CommandLine.h"
#include "SpatialConstants.h"

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

DEFINE_LOG_CATEGORY(LogSpatialGDKSettings);

namespace
{
	void CheckCmdLineOverrideBool(const TCHAR* CommandLine, const TCHAR* Parameter, const TCHAR* PrettyName, bool& Value)
	{
		if(FParse::Param(CommandLine, Parameter))
		{
			Value = true;
		}
		else
		{
			TCHAR TempStr[16];
			if (FParse::Value(CommandLine, Parameter, TempStr, 16) && TempStr[0] == '=')
			{
				Value = FCString::ToBool(TempStr + 1); // + 1 to skip =
			}
		}
		UE_LOG(LogSpatialGDKSettings, Log, TEXT("%s is %s."), PrettyName, Value ? TEXT("enabled") : TEXT("disabled"));
	}
}

USpatialGDKSettings::USpatialGDKSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EntityPoolInitialReservationCount(3000)
	, EntityPoolRefreshThreshold(1000)
	, EntityPoolRefreshCount(2000)
	, HeartbeatIntervalSeconds(2.0f)
	, HeartbeatTimeoutSeconds(10.0f)
	, HeartbeatTimeoutWithEditorSeconds(10000.0f)
	, ActorReplicationRateLimit(0)
	, EntityCreationRateLimit(0)
	, bUseIsActorRelevantForConnection(false)
	, OpsUpdateRate(1000.0f)
	, bEnableHandover(true)
	, MaxNetCullDistanceSquared(900000000.0f) // Set to twice the default Actor NetCullDistanceSquared (300m)
	, QueuedIncomingRPCWaitTime(1.0f)
	, PositionUpdateFrequency(1.0f)
	, PositionDistanceThreshold(100.0f) // 1m (100cm)
	, bEnableMetrics(true)
	, bEnableMetricsDisplay(false)
	, MetricsReportRate(2.0f)
	, bUseFrameTimeAsLoad(false)
	, bBatchSpatialPositionUpdates(false)
	, MaxDynamicallyAttachedSubobjectsPerClass(3)
	, bEnableServerQBI(true)
	, bEnableResultTypes(false)
	, bPackRPCs(false)
	, ServicesRegion(EServicesRegion::Default)
	, DefaultWorkerType(FWorkerType(SpatialConstants::DefaultServerWorkerType))
	, bEnableOffloading(false)
	, ServerWorkerTypes({ SpatialConstants::DefaultServerWorkerType })
	, WorkerLogLevel(ESettingsWorkerLogVerbosity::Warning)
	, bEnableUnrealLoadBalancer(false)
	, bRunSpatialWorkerConnectionOnGameThread(false)
	, bUseRPCRingBuffers(false)
	, DefaultRPCRingBufferSize(8)
	, MaxRPCRingBufferSize(32)
	// TODO - UNR 2514 - These defaults are not necessarily optimal - readdress when we have better data
	, bTcpNoDelay(false)
	, UdpServerUpstreamUpdateIntervalMS(1)
	, UdpServerDownstreamUpdateIntervalMS(1)
	, UdpClientUpstreamUpdateIntervalMS(1)
	, UdpClientDownstreamUpdateIntervalMS(1)
	// TODO - end
	, bAsyncLoadNewClassesOnEntityCheckout(false)
	, bEnableNetCullDistanceInterest(false)
	, bEnableNetCullDistanceFrequency(false)
	, FullFrequencyNetCullDistanceRatio(1.0f)
	, bUseDevelopmentAuthenticationFlow(false)
{
	DefaultReceptionistHost = SpatialConstants::LOCAL_HOST;
}

void USpatialGDKSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Check any command line overrides for using QBI, Offloading (after reading the config value):
	const TCHAR* CommandLine = FCommandLine::Get();
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideSpatialOffloading"), TEXT("Offloading"), bEnableOffloading);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideServerInterest"), TEXT("Server interest"), bEnableServerQBI);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideHandover"), TEXT("Handover"), bEnableHandover);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideLoadBalancer"), TEXT("Load balancer"), bEnableUnrealLoadBalancer);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideRPCRingBuffers"), TEXT("RPC ring buffers"), bUseRPCRingBuffers);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideRPCPacking"), TEXT("RPC packing"), bPackRPCs);

	if (bEnableUnrealLoadBalancer)
	{
		if (bEnableServerQBI == false)
		{
			UE_LOG(LogSpatialGDKSettings, Warning, TEXT("Unreal load balancing is enabled, but server interest is disabled."));
		}
		if (bEnableHandover == false)
		{
			UE_LOG(LogSpatialGDKSettings, Warning, TEXT("Unreal load balancing is enabled, but handover is disabled."));
		}
	}

	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideSpatialWorkerConnectionOnGameThread"), TEXT("Spatial worker connection on game thread"), bRunSpatialWorkerConnectionOnGameThread);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideResultTypes"), TEXT("Result types"), bEnableResultTypes);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideNetCullDistanceInterest"), TEXT("Net cull distance interest"), bEnableNetCullDistanceInterest);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideNetCullDistanceInterestFrequency"), TEXT("Net cull distance interest frequency"), bEnableNetCullDistanceFrequency);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideActorRelevantForConnection"), TEXT("Actor relevant for connection"), bUseIsActorRelevantForConnection);
	CheckCmdLineOverrideBool(CommandLine, TEXT("OverrideBatchSpatialPositionUpdates"), TEXT("Batch spatial position updates"), bBatchSpatialPositionUpdates);

#if WITH_EDITOR
	ULevelEditorPlaySettings* PlayInSettings = GetMutableDefault<ULevelEditorPlaySettings>();
	PlayInSettings->bEnableOffloading = bEnableOffloading;
	PlayInSettings->DefaultWorkerType = DefaultWorkerType.WorkerTypeName;
#endif
}

#if WITH_EDITOR
void USpatialGDKSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Use MemberProperty here so we report the correct member name for nested changes
	const FName Name = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (Name == GET_MEMBER_NAME_CHECKED(USpatialGDKSettings, bEnableOffloading))
	{
		GetMutableDefault<ULevelEditorPlaySettings>()->bEnableOffloading = bEnableOffloading;
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(USpatialGDKSettings, DefaultWorkerType))
	{
		GetMutableDefault<ULevelEditorPlaySettings>()->DefaultWorkerType = DefaultWorkerType.WorkerTypeName;
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(USpatialGDKSettings, MaxDynamicallyAttachedSubobjectsPerClass))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(FString::Printf(TEXT("You MUST regenerate schema using the full scan option after changing the number of max dynamic subobjects. "
				"Failing to do will result in unintended behavior or crashes!"))));
	}
}

bool USpatialGDKSettings::CanEditChange(const UProperty* InProperty) const
{
	if (!InProperty)
	{
		return false;
	}

	const FName Name = InProperty->GetFName();

	if (Name == GET_MEMBER_NAME_CHECKED(USpatialGDKSettings, bUseRPCRingBuffers))
	{
		return !bEnableUnrealLoadBalancer;
	}

	if (Name == GET_MEMBER_NAME_CHECKED(USpatialGDKSettings, DefaultRPCRingBufferSize)
	 || Name == GET_MEMBER_NAME_CHECKED(USpatialGDKSettings, RPCRingBufferSizeMap)
	 || Name == GET_MEMBER_NAME_CHECKED(USpatialGDKSettings, MaxRPCRingBufferSize))
	{
		return UseRPCRingBuffer();
	}

	return true;
}

#endif

uint32 USpatialGDKSettings::GetRPCRingBufferSize(ERPCType RPCType) const
{
	if (const uint32* Size = RPCRingBufferSizeMap.Find(RPCType))
	{
		return *Size;
	}

	return DefaultRPCRingBufferSize;
}


bool USpatialGDKSettings::UseRPCRingBuffer() const
{
	// RPC Ring buffer are necessary in order to do RPC handover, something legacy RPC does not handle.
	return bUseRPCRingBuffers || bEnableUnrealLoadBalancer;
}
