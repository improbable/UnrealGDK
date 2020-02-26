// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Interop/Connection/SpatialWorkerConnection.h"
#if WITH_EDITOR
#include "Interop/Connection/EditorWorkerController.h"
#endif

#include "Async/Async.h"
#include "Improbable/SpatialEngineConstants.h" 
#include "Misc/Paths.h"

#include "SpatialGDKSettings.h"
#include "Utils/ErrorCodeRemapping.h"

DEFINE_LOG_CATEGORY(LogSpatialWorkerConnection);

using namespace SpatialGDK;

struct ConfigureConnection
{
	ConfigureConnection(const FConnectionConfig& InConfig, const bool bConnectAsClient)
		: Config(InConfig)
		, Params()
		, WorkerType(*Config.WorkerType)
		, ProtocolLogPrefix(*FormatProtocolPrefix())
	{
		Params = Worker_DefaultConnectionParameters();

		Params.worker_type = WorkerType.Get();

		Params.enable_protocol_logging_at_startup = Config.EnableProtocolLoggingAtStartup;
		Params.protocol_logging.log_prefix = ProtocolLogPrefix.Get();

		Params.component_vtable_count = 0;
		Params.default_component_vtable = &DefaultVtable;

		Params.network.connection_type = Config.LinkProtocol;
		Params.network.use_external_ip = Config.UseExternalIp;
		Params.network.modular_tcp.multiplex_level = Config.TcpMultiplexLevel;
		if (Config.TcpNoDelay)
		{
			Params.network.modular_tcp.downstream_tcp.flush_delay_millis = 0;
			Params.network.modular_tcp.upstream_tcp.flush_delay_millis = 0;
		}

		// We want the bridge to worker messages to be compressed; not the worker to bridge messages.
		Params.network.modular_kcp.upstream_compression = nullptr;
		Params.network.modular_kcp.downstream_compression = &EnableCompressionParams;

		Params.network.modular_kcp.upstream_kcp.flush_interval_millis = Config.UdpUpstreamIntervalMS;
		Params.network.modular_kcp.downstream_kcp.flush_interval_millis = Config.UdpDownstreamIntervalMS;

#if WITH_EDITOR
		Params.network.modular_tcp.downstream_heartbeat = &HeartbeatParams;
		Params.network.modular_tcp.upstream_heartbeat = &HeartbeatParams;
		Params.network.modular_kcp.downstream_heartbeat = &HeartbeatParams;
		Params.network.modular_kcp.upstream_heartbeat = &HeartbeatParams;
#endif
		
		if (!bConnectAsClient && GetDefault<USpatialGDKSettings>()->bUseSecureServerConnection)
		{
			Params.network.modular_kcp.security_type = WORKER_NETWORK_SECURITY_TYPE_TLS;
			Params.network.modular_tcp.security_type = WORKER_NETWORK_SECURITY_TYPE_TLS;
		}
		else if (bConnectAsClient && GetDefault<USpatialGDKSettings>()->bUseSecureClientConnection)
		{
			Params.network.modular_kcp.security_type = WORKER_NETWORK_SECURITY_TYPE_TLS;
			Params.network.modular_tcp.security_type = WORKER_NETWORK_SECURITY_TYPE_TLS;
		}
		else
		{
			Params.network.modular_kcp.security_type = WORKER_NETWORK_SECURITY_TYPE_INSECURE;
			Params.network.modular_tcp.security_type = WORKER_NETWORK_SECURITY_TYPE_INSECURE;
		}

		Params.enable_dynamic_components = true;
	}

	FString FormatProtocolPrefix() const
	{
		FString FinalProtocolLoggingPrefix = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir());
		if (!Config.ProtocolLoggingPrefix.IsEmpty())
		{
			FinalProtocolLoggingPrefix += Config.ProtocolLoggingPrefix;
		}
		else
		{
			FinalProtocolLoggingPrefix += Config.WorkerId;
		}
		return FinalProtocolLoggingPrefix;
	}

	const FConnectionConfig& Config;
	Worker_ConnectionParameters Params;
	FTCHARToUTF8 WorkerType;
	FTCHARToUTF8 ProtocolLogPrefix;
	Worker_ComponentVtable DefaultVtable{};
	Worker_CompressionParameters EnableCompressionParams{};

#if WITH_EDITOR
	Worker_HeartbeatParameters HeartbeatParams{ WORKER_DEFAULTS_HEARTBEAT_INTERVAL_MILLIS, MAX_int64 };
#endif
};

void USpatialWorkerConnection::FinishDestroy()
{
	DestroyConnection();

	Super::FinishDestroy();
}

void USpatialWorkerConnection::DestroyConnection()
{
	Stop(); // Stop OpsProcessingThread
	if (OpsProcessingThread != nullptr)
	{
		OpsProcessingThread->WaitForCompletion();
		OpsProcessingThread = nullptr;
	}

	if (WorkerConnection)
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WorkerConnection = WorkerConnection]
		{
			Worker_Connection_Destroy(WorkerConnection);
		});

		WorkerConnection = nullptr;
	}

	if (WorkerLocator)
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WorkerLocator = WorkerLocator]
		{
			Worker_Locator_Destroy(WorkerLocator);
		});

		WorkerLocator = nullptr;
	}

	bIsConnected = false;
	NextRequestId = 0;
	KeepRunning.AtomicSet(true);
}

void USpatialWorkerConnection::Connect(bool bInitAsClient, uint32 PlayInEditorID)
{
	if (bIsConnected)
	{
		check(bInitAsClient == bConnectAsClient);
		AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<USpatialWorkerConnection>(this)]
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnConnectionSuccess();
				}
				else
				{
					UE_LOG(LogSpatialWorkerConnection, Error, TEXT("SpatialWorkerConnection is not valid but was already connected."));
				}
			});
		return;
	}

	bConnectAsClient = bInitAsClient;

	const USpatialGDKSettings* SpatialGDKSettings = GetDefault<USpatialGDKSettings>();
	if (SpatialGDKSettings->bUseDevelopmentAuthenticationFlow && bInitAsClient)
	{
		DevAuthConfig.Deployment = SpatialGDKSettings->DevelopmentDeploymentToConnect;
		DevAuthConfig.WorkerType = SpatialConstants::DefaultClientWorkerType.ToString();
		DevAuthConfig.UseExternalIp = true;
		StartDevelopmentAuth(SpatialGDKSettings->DevelopmentAuthenticationToken);
		return;
	}

	switch (GetConnectionType())
	{
	case ESpatialConnectionType::Receptionist:
		ConnectToReceptionist(PlayInEditorID);
		break;
	case ESpatialConnectionType::Locator:
		ConnectToLocator(&LocatorConfig);
		break;
	case ESpatialConnectionType::DevAuthFlow:
		StartDevelopmentAuth(DevAuthConfig.DevelopmentAuthToken);
		break;
	}
}

void USpatialWorkerConnection::OnLoginTokens(void* UserData, const Worker_Alpha_LoginTokensResponse* LoginTokens)
{
	if (LoginTokens->status.code != WORKER_CONNECTION_STATUS_CODE_SUCCESS)
	{
		UE_LOG(LogSpatialWorkerConnection, Error, TEXT("Failed to get login token, StatusCode: %d, Error: %s"), LoginTokens->status.code, UTF8_TO_TCHAR(LoginTokens->status.detail));
		return;
	}

	if (LoginTokens->login_token_count == 0)
	{
		UE_LOG(LogSpatialWorkerConnection, Warning, TEXT("No deployment found to connect to. Did you add the 'dev_login' tag to the deployment you want to connect to?"));
		return;
	}

	UE_LOG(LogSpatialWorkerConnection, Verbose, TEXT("Successfully received LoginTokens, Count: %d"), LoginTokens->login_token_count);
	USpatialWorkerConnection* Connection = static_cast<USpatialWorkerConnection*>(UserData);
	Connection->ProcessLoginTokensResponse(LoginTokens);
}

void USpatialWorkerConnection::ProcessLoginTokensResponse(const Worker_Alpha_LoginTokensResponse* LoginTokens)
{
	// If LoginTokenResCallback is callable and returns true, return early.
	if (LoginTokenResCallback && LoginTokenResCallback(LoginTokens))
	{
		return;
	}
	
	const FString& DeploymentToConnect = DevAuthConfig.Deployment;
	// If not set, use the first deployment. It can change every query if you have multiple items available, because the order is not guaranteed.
	if (DeploymentToConnect.IsEmpty())
	{
		DevAuthConfig.LoginToken = FString(LoginTokens->login_tokens[0].login_token);
	}
	else
	{
		for (uint32 i = 0; i < LoginTokens->login_token_count; i++)
		{
			FString DeploymentName = FString(LoginTokens->login_tokens[i].deployment_name);
			if (DeploymentToConnect.Compare(DeploymentName) == 0)
			{
				DevAuthConfig.LoginToken = FString(LoginTokens->login_tokens[i].login_token);
				break;
			}
		}
	}
	ConnectToLocator(&  DevAuthConfig);
}

void USpatialWorkerConnection::RequestDeploymentLoginTokens()
{
	Worker_Alpha_LoginTokensRequest LTParams{};
	FTCHARToUTF8 PlayerIdentityToken(*DevAuthConfig.PlayerIdentityToken);
	LTParams.player_identity_token = PlayerIdentityToken.Get();
	FTCHARToUTF8 WorkerType(*DevAuthConfig.WorkerType);
	LTParams.worker_type = WorkerType.Get();
	LTParams.use_insecure_connection = false;
	
	if (Worker_Alpha_LoginTokensResponseFuture* LTFuture = Worker_Alpha_CreateDevelopmentLoginTokensAsync(TCHAR_TO_UTF8(*DevAuthConfig.LocatorHost), SpatialConstants::LOCATOR_PORT, &LTParams))
	{
		Worker_Alpha_LoginTokensResponseFuture_Get(LTFuture, nullptr, this, &USpatialWorkerConnection::OnLoginTokens);
	}
}

void USpatialWorkerConnection::OnPlayerIdentityToken(void* UserData, const Worker_Alpha_PlayerIdentityTokenResponse* PIToken)
{
	if (PIToken->status.code != WORKER_CONNECTION_STATUS_CODE_SUCCESS)
	{
		UE_LOG(LogSpatialWorkerConnection, Error, TEXT("Failed to get PlayerIdentityToken, StatusCode: %d, Error: %s"), PIToken->status.code, UTF8_TO_TCHAR(PIToken->status.detail));
		return;
	}

	UE_LOG(LogSpatialWorkerConnection, Log, TEXT("Successfully received PIToken: %s"), UTF8_TO_TCHAR(PIToken->player_identity_token));
	USpatialWorkerConnection* Connection = static_cast<USpatialWorkerConnection*>(UserData);
	Connection->DevAuthConfig.PlayerIdentityToken = UTF8_TO_TCHAR(PIToken->player_identity_token);
	
	Connection->RequestDeploymentLoginTokens();
}

void USpatialWorkerConnection::StartDevelopmentAuth(const FString& DevAuthToken)
{
	FTCHARToUTF8 DAToken(*DevAuthToken);
	FTCHARToUTF8 PlayerId(*DevAuthConfig.PlayerId);
	FTCHARToUTF8 DisplayName(*DevAuthConfig.DisplayName);
	FTCHARToUTF8 MetaData(*DevAuthConfig.MetaData);

	Worker_Alpha_PlayerIdentityTokenRequest PITParams{};
	PITParams.development_authentication_token = DAToken.Get();
	PITParams.player_id = PlayerId.Get();
	PITParams.display_name = DisplayName.Get();
	PITParams.metadata = MetaData.Get();
	PITParams.use_insecure_connection = false;

	if (Worker_Alpha_PlayerIdentityTokenResponseFuture* PITFuture = Worker_Alpha_CreateDevelopmentPlayerIdentityTokenAsync(TCHAR_TO_UTF8(*DevAuthConfig.LocatorHost), SpatialConstants::LOCATOR_PORT, &PITParams))
	{
		Worker_Alpha_PlayerIdentityTokenResponseFuture_Get(PITFuture, nullptr, this, &USpatialWorkerConnection::OnPlayerIdentityToken);
	}
}

void USpatialWorkerConnection::ConnectToReceptionist(uint32 PlayInEditorID)
{
#if WITH_EDITOR
	SpatialGDKServices::InitWorkers(bConnectAsClient, PlayInEditorID, ReceptionistConfig.WorkerId);
#endif

	ReceptionistConfig.PreConnectInit(bConnectAsClient);

	ConfigureConnection ConnectionConfig(ReceptionistConfig, bConnectAsClient);

	Worker_ConnectionFuture* ConnectionFuture = Worker_ConnectAsync(
		TCHAR_TO_UTF8(*ReceptionistConfig.GetReceptionistHost()), ReceptionistConfig.ReceptionistPort,
		TCHAR_TO_UTF8(*ReceptionistConfig.WorkerId), &ConnectionConfig.Params);

	FinishConnecting(ConnectionFuture);
}

void USpatialWorkerConnection::ConnectToLocator(FLocatorConfig* InLocatorConfig)
{
	if (InLocatorConfig == nullptr)
	{
		UE_LOG(LogSpatialWorkerConnection, Error, TEXT("Trying to connect to locator with invalid locator config"));
		return;
	}

	InLocatorConfig->PreConnectInit(bConnectAsClient);

	ConfigureConnection ConnectionConfig(*InLocatorConfig, bConnectAsClient);

	FTCHARToUTF8 PlayerIdentityTokenCStr(*InLocatorConfig->PlayerIdentityToken);
	FTCHARToUTF8 LoginTokenCStr(*InLocatorConfig->LoginToken);

	Worker_LocatorParameters LocatorParams = {};
	FString ProjectName;
	FParse::Value(FCommandLine::Get(), TEXT("projectName"), ProjectName);
	LocatorParams.project_name = TCHAR_TO_UTF8(*ProjectName);
	LocatorParams.credentials_type = Worker_LocatorCredentialsTypes::WORKER_LOCATOR_PLAYER_IDENTITY_CREDENTIALS;
	LocatorParams.player_identity.player_identity_token = PlayerIdentityTokenCStr.Get();
	LocatorParams.player_identity.login_token = LoginTokenCStr.Get();

	// Connect to the locator on the default port(0 will choose the default)
	WorkerLocator = Worker_Locator_Create(TCHAR_TO_UTF8(*InLocatorConfig->LocatorHost), SpatialConstants::LOCATOR_PORT, &LocatorParams);

	Worker_ConnectionFuture* ConnectionFuture = Worker_Locator_ConnectAsync(WorkerLocator, &ConnectionConfig.Params);

	FinishConnecting(ConnectionFuture);
}

void USpatialWorkerConnection::FinishConnecting(Worker_ConnectionFuture* ConnectionFuture)
{
	TWeakObjectPtr<USpatialWorkerConnection> WeakSpatialWorkerConnection(this);

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [ConnectionFuture, WeakSpatialWorkerConnection]
	{
		Worker_Connection* NewCAPIWorkerConnection = Worker_ConnectionFuture_Get(ConnectionFuture, nullptr);
		Worker_ConnectionFuture_Destroy(ConnectionFuture);

		AsyncTask(ENamedThreads::GameThread, [WeakSpatialWorkerConnection, NewCAPIWorkerConnection]
		{
			USpatialWorkerConnection* SpatialWorkerConnection = WeakSpatialWorkerConnection.Get();

			if (SpatialWorkerConnection == nullptr)
			{
				return;
			}

			SpatialWorkerConnection->WorkerConnection = NewCAPIWorkerConnection;

			if (Worker_Connection_IsConnected(NewCAPIWorkerConnection))
			{
				SpatialWorkerConnection->CacheWorkerAttributes();
				SpatialWorkerConnection->OnConnectionSuccess();
			}
			else
			{
				// TODO: Try to reconnect - UNR-576
				SpatialWorkerConnection->OnConnectionFailure();
			}
		});
	});
}

ESpatialConnectionType USpatialWorkerConnection::GetConnectionType() const
{
	return ConnectionType;
}

void USpatialWorkerConnection::SetConnectionType(ESpatialConnectionType InConnectionType)
{
	// The locator config may not have been initialized
	check(!(InConnectionType == ESpatialConnectionType::Locator && LocatorConfig.LocatorHost.IsEmpty()))

	ConnectionType = InConnectionType;
}

bool USpatialWorkerConnection::TrySetupConnectionConfigFromCommandLine(const FString& SpatialWorkerType)
{
	bool bSuccessfullyLoaded = LocatorConfig.TryLoadCommandLineArgs();
	if (bSuccessfullyLoaded)
	{
		SetConnectionType(ESpatialConnectionType::Locator);
		LocatorConfig.WorkerType = SpatialWorkerType;
	}
	else
	{
		bSuccessfullyLoaded = DevAuthConfig.TryLoadCommandLineArgs();
		if (bSuccessfullyLoaded)
		{
			SetConnectionType(ESpatialConnectionType::DevAuthFlow);
			DevAuthConfig.WorkerType = SpatialWorkerType;
		}
		else
		{
			bSuccessfullyLoaded = ReceptionistConfig.TryLoadCommandLineArgs();
			SetConnectionType(ESpatialConnectionType::Receptionist);
			ReceptionistConfig.WorkerType = SpatialWorkerType;
		}
	}

	return bSuccessfullyLoaded;
}

void USpatialWorkerConnection::SetupConnectionConfigFromURL(const FURL& URL, const FString& SpatialWorkerType)
{
	if (URL.Host == SpatialConstants::LOCATOR_HOST && URL.HasOption(TEXT("locator")))
	{
		SetConnectionType(ESpatialConnectionType::Locator);
		// TODO: UNR-2811 We might add a feature whereby we get the locator host from the URL option.
		FParse::Value(FCommandLine::Get(), TEXT("locatorHost"), LocatorConfig.LocatorHost);
		LocatorConfig.PlayerIdentityToken = URL.GetOption(*SpatialConstants::URL_PLAYER_IDENTITY_OPTION, TEXT(""));
		LocatorConfig.LoginToken = URL.GetOption(*SpatialConstants::URL_LOGIN_OPTION, TEXT(""));
		LocatorConfig.WorkerType = SpatialWorkerType;
	}
	else if (URL.Host == SpatialConstants::LOCATOR_HOST && URL.HasOption(TEXT("devauth")))
	{
		SetConnectionType(ESpatialConnectionType::DevAuthFlow);
		// TODO: UNR-2811 Also set the locator host of DevAuthConfig from URL.
		FParse::Value(FCommandLine::Get(), TEXT("locatorHost"), DevAuthConfig.LocatorHost);
		DevAuthConfig.DevelopmentAuthToken = URL.GetOption(*SpatialConstants::URL_DEV_AUTH_TOKEN_OPTION, TEXT(""));
		DevAuthConfig.Deployment = URL.GetOption(*SpatialConstants::URL_TARGET_DEPLOYMENT_OPTION, TEXT(""));
		DevAuthConfig.PlayerId = URL.GetOption(*SpatialConstants::URL_PLAYER_ID_OPTION, *SpatialConstants::DEVELOPMENT_AUTH_PLAYER_ID);
		DevAuthConfig.DisplayName = URL.GetOption(*SpatialConstants::URL_DISPLAY_NAME_OPTION, TEXT(""));
		DevAuthConfig.MetaData = URL.GetOption(*SpatialConstants::URL_METADATA_OPTION, TEXT(""));
		DevAuthConfig.WorkerType = SpatialWorkerType;
	}
	else
	{
		SetConnectionType(ESpatialConnectionType::Receptionist);
		ReceptionistConfig.SetReceptionistHost(URL.Host);
		ReceptionistConfig.WorkerType = SpatialWorkerType;

		const TCHAR* UseExternalIpForBridge = TEXT("useExternalIpForBridge");
		if (URL.HasOption(UseExternalIpForBridge))
		{
			FString UseExternalIpOption = URL.GetOption(UseExternalIpForBridge, TEXT(""));
			ReceptionistConfig.UseExternalIp = !UseExternalIpOption.Equals(TEXT("false"), ESearchCase::IgnoreCase);
		}
	}
}

TArray<Worker_OpList*> USpatialWorkerConnection::GetOpList()
{
	TArray<Worker_OpList*> OpLists;
	while (!OpListQueue.IsEmpty())
	{
		Worker_OpList* OutOpList;
		OpListQueue.Dequeue(OutOpList);
		OpLists.Add(OutOpList);
	}

	return OpLists;
}

Worker_RequestId USpatialWorkerConnection::SendReserveEntityIdsRequest(uint32_t NumOfEntities)
{
	QueueOutgoingMessage<FReserveEntityIdsRequest>(NumOfEntities);
	return NextRequestId++;
}

Worker_RequestId USpatialWorkerConnection::SendCreateEntityRequest(TArray<FWorkerComponentData>&& Components, const Worker_EntityId* EntityId)
{
	QueueOutgoingMessage<FCreateEntityRequest>(MoveTemp(Components), EntityId);
	return NextRequestId++;
}

Worker_RequestId USpatialWorkerConnection::SendDeleteEntityRequest(Worker_EntityId EntityId)
{
	QueueOutgoingMessage<FDeleteEntityRequest>(EntityId);
	return NextRequestId++;
}

void USpatialWorkerConnection::SendAddComponent(Worker_EntityId EntityId, FWorkerComponentData* ComponentData)
{
	QueueOutgoingMessage<FAddComponent>(EntityId, *ComponentData);
}

void USpatialWorkerConnection::SendRemoveComponent(Worker_EntityId EntityId, Worker_ComponentId ComponentId)
{
	QueueOutgoingMessage<FRemoveComponent>(EntityId, ComponentId);
}

void USpatialWorkerConnection::SendComponentUpdate(Worker_EntityId EntityId, const FWorkerComponentUpdate* ComponentUpdate)
{
	QueueOutgoingMessage<FComponentUpdate>(EntityId, *ComponentUpdate);
}

Worker_RequestId USpatialWorkerConnection::SendCommandRequest(Worker_EntityId EntityId, const Worker_CommandRequest* Request, uint32_t CommandId)
{
	QueueOutgoingMessage<FCommandRequest>(EntityId, *Request, CommandId);
	return NextRequestId++;
}

void USpatialWorkerConnection::SendCommandResponse(Worker_RequestId RequestId, const Worker_CommandResponse* Response)
{
	QueueOutgoingMessage<FCommandResponse>(RequestId, *Response);
}

void USpatialWorkerConnection::SendCommandFailure(Worker_RequestId RequestId, const FString& Message)
{
	QueueOutgoingMessage<FCommandFailure>(RequestId, Message);
}

void USpatialWorkerConnection::SendLogMessage(const uint8_t Level, const FName& LoggerName, const TCHAR* Message)
{
	QueueOutgoingMessage<FLogMessage>(Level, LoggerName, Message);
}

void USpatialWorkerConnection::SendComponentInterest(Worker_EntityId EntityId, TArray<Worker_InterestOverride>&& ComponentInterest)
{
	QueueOutgoingMessage<FComponentInterest>(EntityId, MoveTemp(ComponentInterest));
}

Worker_RequestId USpatialWorkerConnection::SendEntityQueryRequest(const Worker_EntityQuery* EntityQuery)
{
	QueueOutgoingMessage<FEntityQueryRequest>(*EntityQuery);
	return NextRequestId++;
}

void USpatialWorkerConnection::SendMetrics(const SpatialMetrics& Metrics)
{
	QueueOutgoingMessage<FMetrics>(Metrics);
}

PhysicalWorkerName USpatialWorkerConnection::GetWorkerId() const
{
	return PhysicalWorkerName(UTF8_TO_TCHAR(Worker_Connection_GetWorkerId(WorkerConnection)));
}

const TArray<FString>& USpatialWorkerConnection::GetWorkerAttributes() const
{
	return CachedWorkerAttributes;
}

void USpatialWorkerConnection::CacheWorkerAttributes()
{
	const Worker_WorkerAttributes* Attributes = Worker_Connection_GetWorkerAttributes(WorkerConnection);

	CachedWorkerAttributes.Empty();

	if (Attributes->attributes == nullptr)
	{
		return;
	}

	for (uint32 Index = 0; Index < Attributes->attribute_count; ++Index)
	{
		CachedWorkerAttributes.Add(UTF8_TO_TCHAR(Attributes->attributes[Index]));
	}
}

void USpatialWorkerConnection::OnConnectionSuccess()
{
	bIsConnected = true;

	const USpatialGDKSettings* SpatialGDKSettings = GetDefault<USpatialGDKSettings>();
	if (!SpatialGDKSettings->bRunSpatialWorkerConnectionOnGameThread)
	{
		if (OpsProcessingThread == nullptr)
		{
			InitializeOpsProcessingThread();
		}
	}

	OnConnectedCallback.ExecuteIfBound();
}

void USpatialWorkerConnection::OnConnectionFailure()
{
	bIsConnected = false;

	if (WorkerConnection != nullptr)
	{
		uint8_t ConnectionStatusCode = Worker_Connection_GetConnectionStatusCode(WorkerConnection);
		const FString ErrorMessage(UTF8_TO_TCHAR(Worker_Connection_GetConnectionStatusDetailString(WorkerConnection)));
		OnFailedToConnectCallback.ExecuteIfBound(ConnectionStatusCode, ErrorMessage);
	}
}

bool USpatialWorkerConnection::Init()
{
	OpsUpdateInterval = 1.0f / GetDefault<USpatialGDKSettings>()->OpsUpdateRate;

	return true;
}

uint32 USpatialWorkerConnection::Run()
{
	const USpatialGDKSettings* SpatialGDKSettings = GetDefault<USpatialGDKSettings>();
	check(!SpatialGDKSettings->bRunSpatialWorkerConnectionOnGameThread);

	while (KeepRunning)
	{
		FPlatformProcess::Sleep(OpsUpdateInterval);
		QueueLatestOpList();
		ProcessOutgoingMessages();
	}

	return 0;
}

void USpatialWorkerConnection::Stop()
{
	KeepRunning.AtomicSet(false);
}

void USpatialWorkerConnection::InitializeOpsProcessingThread()
{
	check(IsInGameThread());

	OpsProcessingThread = FRunnableThread::Create(this, TEXT("SpatialWorkerConnectionWorker"), 0);
	check(OpsProcessingThread);
}

void USpatialWorkerConnection::QueueLatestOpList()
{
	Worker_OpList* OpList = Worker_Connection_GetOpList(WorkerConnection, 0);
	if (OpList->op_count > 0)
	{
		OpListQueue.Enqueue(OpList);
	}
	else
	{
		Worker_OpList_Destroy(OpList);
	}
}

void USpatialWorkerConnection::ProcessOutgoingMessages()
{
	while (!OutgoingMessagesQueue.IsEmpty())
	{
		TUniquePtr<FOutgoingMessage> OutgoingMessage;
		OutgoingMessagesQueue.Dequeue(OutgoingMessage);

		OnDequeueMessage.Broadcast(OutgoingMessage.Get());

		static const Worker_UpdateParameters DisableLoopback{ /*loopback*/ WORKER_COMPONENT_UPDATE_LOOPBACK_NONE };

		switch (OutgoingMessage->Type)
		{
		case EOutgoingMessageType::ReserveEntityIdsRequest:
		{
			FReserveEntityIdsRequest* Message = static_cast<FReserveEntityIdsRequest*>(OutgoingMessage.Get());

			Worker_Connection_SendReserveEntityIdsRequest(WorkerConnection,
				Message->NumOfEntities,
				nullptr);
			break;
		}
		case EOutgoingMessageType::CreateEntityRequest:
		{
			FCreateEntityRequest* Message = static_cast<FCreateEntityRequest*>(OutgoingMessage.Get());

#if TRACE_LIB_ACTIVE
			// We have to unpack these as Worker_ComponentData is not the same as FWorkerComponentData
			TArray<Worker_ComponentData> UnpackedComponentData;
			UnpackedComponentData.SetNum(Message->Components.Num());
			for (int i = 0, Num = Message->Components.Num(); i < Num; i++)
			{
				UnpackedComponentData[i] = Message->Components[i];
			}
			Worker_ComponentData* ComponentData = UnpackedComponentData.GetData();
			uint32 ComponentCount = UnpackedComponentData.Num();
#else
			Worker_ComponentData* ComponentData = Message->Components.GetData();
			uint32 ComponentCount = Message->Components.Num();
#endif
			Worker_Connection_SendCreateEntityRequest(WorkerConnection,
				ComponentCount,
				ComponentData,
				Message->EntityId.IsSet() ? &(Message->EntityId.GetValue()) : nullptr,
				nullptr);
			break;
		}
		case EOutgoingMessageType::DeleteEntityRequest:
		{
			FDeleteEntityRequest* Message = static_cast<FDeleteEntityRequest*>(OutgoingMessage.Get());

			Worker_Connection_SendDeleteEntityRequest(WorkerConnection,
				Message->EntityId,
				nullptr);
			break;
		}
		case EOutgoingMessageType::AddComponent:
		{
			FAddComponent* Message = static_cast<FAddComponent*>(OutgoingMessage.Get());

			Worker_Connection_SendAddComponent(WorkerConnection,
				Message->EntityId,
				&Message->Data,
				&DisableLoopback);
			break;
		}
		case EOutgoingMessageType::RemoveComponent:
		{
			FRemoveComponent* Message = static_cast<FRemoveComponent*>(OutgoingMessage.Get());

			Worker_Connection_SendRemoveComponent(WorkerConnection,
				Message->EntityId,
				Message->ComponentId,
				&DisableLoopback);
			break;
		}
		case EOutgoingMessageType::ComponentUpdate:
		{
			FComponentUpdate* Message = static_cast<FComponentUpdate*>(OutgoingMessage.Get());

			Worker_Connection_SendComponentUpdate(WorkerConnection,
				Message->EntityId,
				&Message->Update,
				&DisableLoopback);

			break;
		}
		case EOutgoingMessageType::CommandRequest:
		{
			FCommandRequest* Message = static_cast<FCommandRequest*>(OutgoingMessage.Get());

			static const Worker_CommandParameters DefaultCommandParams{};
			Worker_Connection_SendCommandRequest(WorkerConnection,
				Message->EntityId,
				&Message->Request,
				nullptr,
				&DefaultCommandParams);
			break;
		}
		case EOutgoingMessageType::CommandResponse:
		{
			FCommandResponse* Message = static_cast<FCommandResponse*>(OutgoingMessage.Get());

			Worker_Connection_SendCommandResponse(WorkerConnection,
				Message->RequestId,
				&Message->Response);
			break;
		}
		case EOutgoingMessageType::CommandFailure:
		{
			FCommandFailure* Message = static_cast<FCommandFailure*>(OutgoingMessage.Get());

			Worker_Connection_SendCommandFailure(WorkerConnection,
				Message->RequestId,
				TCHAR_TO_UTF8(*Message->Message));
			break;
		}
		case EOutgoingMessageType::LogMessage:
		{
			FLogMessage* Message = static_cast<FLogMessage*>(OutgoingMessage.Get());

			FTCHARToUTF8 LoggerName(*Message->LoggerName.ToString());
			FTCHARToUTF8 LogString(*Message->Message);

			Worker_LogMessage LogMessage{};
			LogMessage.level = Message->Level;
			LogMessage.logger_name = LoggerName.Get();
			LogMessage.message = LogString.Get();
			Worker_Connection_SendLogMessage(WorkerConnection, &LogMessage);
			break;
		}
		case EOutgoingMessageType::ComponentInterest:
		{
			FComponentInterest* Message = static_cast<FComponentInterest*>(OutgoingMessage.Get());

			Worker_Connection_SendComponentInterest(WorkerConnection,
				Message->EntityId,
				Message->Interests.GetData(),
				Message->Interests.Num());
			break;
		}
		case EOutgoingMessageType::EntityQueryRequest:
		{
			FEntityQueryRequest* Message = static_cast<FEntityQueryRequest*>(OutgoingMessage.Get());

			Worker_Connection_SendEntityQueryRequest(WorkerConnection,
				&Message->EntityQuery,
				nullptr);
			break;
		}
		case EOutgoingMessageType::Metrics:
		{
			FMetrics* Message = static_cast<FMetrics*>(OutgoingMessage.Get());

			// Do the conversion here so we can store everything on the stack.
			Worker_Metrics WorkerMetrics;

			WorkerMetrics.load = Message->Metrics.Load.IsSet() ? &Message->Metrics.Load.GetValue() : nullptr;

			TArray<Worker_GaugeMetric> WorkerGaugeMetrics;
			WorkerGaugeMetrics.SetNum(Message->Metrics.GaugeMetrics.Num());
			for (int i = 0; i < Message->Metrics.GaugeMetrics.Num(); i++)
			{
				WorkerGaugeMetrics[i].key = Message->Metrics.GaugeMetrics[i].Key.c_str();
				WorkerGaugeMetrics[i].value = Message->Metrics.GaugeMetrics[i].Value;
			}

			WorkerMetrics.gauge_metric_count = static_cast<uint32_t>(WorkerGaugeMetrics.Num());
			WorkerMetrics.gauge_metrics = WorkerGaugeMetrics.GetData();

			TArray<Worker_HistogramMetric> WorkerHistogramMetrics;
			TArray<TArray<Worker_HistogramMetricBucket>> WorkerHistogramMetricBuckets;
			WorkerHistogramMetrics.SetNum(Message->Metrics.HistogramMetrics.Num());
			for (int i = 0; i < Message->Metrics.HistogramMetrics.Num(); i++)
			{
				WorkerHistogramMetrics[i].key = Message->Metrics.HistogramMetrics[i].Key.c_str();
				WorkerHistogramMetrics[i].sum = Message->Metrics.HistogramMetrics[i].Sum;

				WorkerHistogramMetricBuckets[i].SetNum(Message->Metrics.HistogramMetrics[i].Buckets.Num());
				for (int j = 0; j < Message->Metrics.HistogramMetrics[i].Buckets.Num(); j++)
				{
					WorkerHistogramMetricBuckets[i][j].upper_bound = Message->Metrics.HistogramMetrics[i].Buckets[j].UpperBound;
					WorkerHistogramMetricBuckets[i][j].samples = Message->Metrics.HistogramMetrics[i].Buckets[j].Samples;
				}

				WorkerHistogramMetrics[i].bucket_count = static_cast<uint32_t>(WorkerHistogramMetricBuckets[i].Num());
				WorkerHistogramMetrics[i].buckets = WorkerHistogramMetricBuckets[i].GetData();
			}

			WorkerMetrics.histogram_metric_count = static_cast<uint32_t>(WorkerHistogramMetrics.Num());
			WorkerMetrics.histogram_metrics = WorkerHistogramMetrics.GetData();

			Worker_Connection_SendMetrics(WorkerConnection, &WorkerMetrics);
			break;
		}
		default:
		{
			checkNoEntry();
			break;
		}
		}
	}
}

template <typename T, typename... ArgsType>
void USpatialWorkerConnection::QueueOutgoingMessage(ArgsType&&... Args)
{
	// TODO UNR-1271: As later optimization, we can change the queue to hold a union
	// of all outgoing message types, rather than having a pointer.
	auto Message = MakeUnique<T>(Forward<ArgsType>(Args)...);
	OnEnqueueMessage.Broadcast(Message.Get());
	OutgoingMessagesQueue.Enqueue(MoveTemp(Message));
}
