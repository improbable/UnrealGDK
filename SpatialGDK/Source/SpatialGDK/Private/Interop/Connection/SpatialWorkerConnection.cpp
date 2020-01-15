// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Interop/Connection/SpatialWorkerConnection.h"
#if WITH_EDITOR
#include "Interop/Connection/EditorWorkerController.h"
#endif

#include "Async/Async.h"
#include "Misc/Paths.h"

#include "SpatialGDKSettings.h"
#include "Utils/ErrorCodeRemapping.h"

DEFINE_LOG_CATEGORY(LogSpatialWorkerConnection);

using namespace SpatialGDK;

struct ConfigureConnection
{
	ConfigureConnection(const FConnectionConfig& InConfig)
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

USpatialWorkerConnection::~USpatialWorkerConnection()
{
	// TODO(Alex): could be unsafe, since not sure if OpsProcessingThread has been executed
	DestroyConnection();

	//SerializedOpLists.DumpSavedOpLists();
}

void USpatialWorkerConnection::DestroyConnection()
{
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

	// TODO(Alex): is it safe to change NextRequestId after bIsConnected?
	NextRequestId = 0;
}

Worker_Connection* USpatialWorkerConnection::Connect(uint32 PlayInEditorID, bool bConnectAsClient)
{
	Worker_ConnectionFuture* ConnectionFuture = nullptr;

	switch (GetConnectionType())
	{
	case ESpatialConnectionType::Receptionist:
		ConnectionFuture = ConnectToReceptionist(PlayInEditorID, bConnectAsClient);
		break;
	case ESpatialConnectionType::Locator:
		ConnectionFuture = ConnectToLocator(bConnectAsClient);
		break;
	}

	Worker_Connection* NewCAPIWorkerConnection = Worker_ConnectionFuture_Get(ConnectionFuture, nullptr);
	Worker_ConnectionFuture_Destroy(ConnectionFuture);

	if (Worker_Connection_IsConnected(NewCAPIWorkerConnection))
	{
		return NewCAPIWorkerConnection;
	}
	else
	{
		return nullptr;
	}
}

void USpatialWorkerConnection::SetConnection(Worker_Connection* InConnection)
{
	WorkerConnection = InConnection;
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
	const FString& DeploymentToConnect = GetDefault<USpatialGDKSettings>()->DevelopmentDeploymentToConnect;
	// If not set, use the first deployment. It can change every query if you have multiple items available, because the order is not guaranteed.
	if (DeploymentToConnect.IsEmpty())
	{
		Connection->LocatorConfig.LoginToken = FString(LoginTokens->login_tokens[0].login_token);
	}
	else
	{
		for (uint32 i = 0; i < LoginTokens->login_token_count; i++)
		{
			FString DeploymentName = FString(LoginTokens->login_tokens[i].deployment_name);
			if (DeploymentToConnect.Compare(DeploymentName) == 0)
			{
				Connection->LocatorConfig.LoginToken = FString(LoginTokens->login_tokens[i].login_token);
				break;
			}
		}
	}
	Connection->ConnectToLocator(Connection->bConnectToLocatorAsClient);
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
	Connection->LocatorConfig.PlayerIdentityToken = UTF8_TO_TCHAR(PIToken->player_identity_token);
	Worker_Alpha_LoginTokensRequest LTParams{};
	LTParams.player_identity_token = PIToken->player_identity_token;
	FTCHARToUTF8 WorkerType(*Connection->LocatorConfig.WorkerType);
	LTParams.worker_type = WorkerType.Get();
	LTParams.use_insecure_connection = false;

	if (Worker_Alpha_LoginTokensResponseFuture* LTFuture = Worker_Alpha_CreateDevelopmentLoginTokensAsync(TCHAR_TO_UTF8(*Connection->LocatorConfig.LocatorHost), SpatialConstants::LOCATOR_PORT, &LTParams))
	{
		Worker_Alpha_LoginTokensResponseFuture_Get(LTFuture, nullptr, Connection, &USpatialWorkerConnection::OnLoginTokens);
	}
}

void USpatialWorkerConnection::StartDevelopmentAuth(FString DevAuthToken, bool bInConnectToLocatorAsClient)
{
	bConnectToLocatorAsClient = bInConnectToLocatorAsClient;

	Worker_Alpha_PlayerIdentityTokenRequest PITParams{};
	FTCHARToUTF8 DAToken(*DevAuthToken);
	FTCHARToUTF8 PlayerId(*SpatialConstants::DEVELOPMENT_AUTH_PLAYER_ID);
	PITParams.development_authentication_token = DAToken.Get();
	PITParams.player_id = PlayerId.Get();
	PITParams.display_name = "";
	PITParams.metadata = "";
	PITParams.use_insecure_connection = false;

	if (Worker_Alpha_PlayerIdentityTokenResponseFuture* PITFuture = Worker_Alpha_CreateDevelopmentPlayerIdentityTokenAsync(TCHAR_TO_UTF8(*LocatorConfig.LocatorHost), SpatialConstants::LOCATOR_PORT, &PITParams))
	{
		Worker_Alpha_PlayerIdentityTokenResponseFuture_Get(PITFuture, nullptr, this, &USpatialWorkerConnection::OnPlayerIdentityToken);
	}
}

Worker_ConnectionFuture* USpatialWorkerConnection::ConnectToReceptionist(uint32 PlayInEditorID, bool bConnectAsClient)
{
#if WITH_EDITOR
	SpatialGDKServices::InitWorkers(bConnectAsClient, PlayInEditorID, ReceptionistConfig.WorkerId);
#endif

	ReceptionistConfig.PreConnectInit(bConnectAsClient);

	ConfigureConnection ConnectionConfig(ReceptionistConfig);

	Worker_ConnectionFuture* ConnectionFuture = Worker_ConnectAsync(
		TCHAR_TO_UTF8(*ReceptionistConfig.GetReceptionistHost()), ReceptionistConfig.ReceptionistPort,
		TCHAR_TO_UTF8(*ReceptionistConfig.WorkerId), &ConnectionConfig.Params);

	return ConnectionFuture;
}

Worker_ConnectionFuture* USpatialWorkerConnection::ConnectToLocator(bool bConnectAsClient)
{
	LocatorConfig.PreConnectInit(bConnectAsClient);

	ConfigureConnection ConnectionConfig(LocatorConfig);

	FTCHARToUTF8 PlayerIdentityTokenCStr(*LocatorConfig.PlayerIdentityToken);
	FTCHARToUTF8 LoginTokenCStr(*LocatorConfig.LoginToken);

	Worker_LocatorParameters LocatorParams = {};
	FString ProjectName;
	FParse::Value(FCommandLine::Get(), TEXT("projectName"), ProjectName);
	LocatorParams.project_name = TCHAR_TO_UTF8(*ProjectName);
	LocatorParams.credentials_type = Worker_LocatorCredentialsTypes::WORKER_LOCATOR_PLAYER_IDENTITY_CREDENTIALS;
	LocatorParams.player_identity.player_identity_token = PlayerIdentityTokenCStr.Get();
	LocatorParams.player_identity.login_token = LoginTokenCStr.Get();

	// Connect to the locator on the default port(0 will choose the default)
	WorkerLocator = Worker_Locator_Create(TCHAR_TO_UTF8(*LocatorConfig.LocatorHost), SpatialConstants::LOCATOR_PORT, &LocatorParams);

	Worker_ConnectionFuture* ConnectionFuture = Worker_Locator_ConnectAsync(WorkerLocator, &ConnectionConfig.Params);
	return ConnectionFuture;
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

void USpatialWorkerConnection::GetErrorCodeAndMessage(uint8_t& OutConnectionStatusCode, FString& OutErrorMessage) const
{
	if (WorkerConnection != nullptr)
	{
		OutConnectionStatusCode = Worker_Connection_GetConnectionStatusCode(WorkerConnection);
		OutErrorMessage = UTF8_TO_TCHAR(Worker_Connection_GetConnectionStatusDetailString(WorkerConnection));
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

		// Serialize Op List for testing;
		SerializedOpLists.SerializedOpList(OutOpList);
	}

	return OpLists;
}

Worker_RequestId USpatialWorkerConnection::SendReserveEntityIdsRequest(uint32_t NumOfEntities)
{
	QueueOutgoingMessage<FReserveEntityIdsRequest>(NumOfEntities);
	return NextRequestId++;
}

Worker_RequestId USpatialWorkerConnection::SendCreateEntityRequest(TArray<Worker_ComponentData>&& Components, const Worker_EntityId* EntityId)
{
	QueueOutgoingMessage<FCreateEntityRequest>(MoveTemp(Components), EntityId);
	return NextRequestId++;
}

Worker_RequestId USpatialWorkerConnection::SendDeleteEntityRequest(Worker_EntityId EntityId)
{
	QueueOutgoingMessage<FDeleteEntityRequest>(EntityId);
	return NextRequestId++;
}

void USpatialWorkerConnection::SendAddComponent(Worker_EntityId EntityId, Worker_ComponentData* ComponentData)
{
	QueueOutgoingMessage<FAddComponent>(EntityId, *ComponentData);
}

void USpatialWorkerConnection::SendRemoveComponent(Worker_EntityId EntityId, Worker_ComponentId ComponentId)
{
	QueueOutgoingMessage<FRemoveComponent>(EntityId, ComponentId);
}

void USpatialWorkerConnection::SendComponentUpdate(Worker_EntityId EntityId, const Worker_ComponentUpdate* ComponentUpdate, const TraceKey Key)
{
	QueueOutgoingMessage<FComponentUpdate>(EntityId, *ComponentUpdate, Key);
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

void USpatialWorkerConnection::QueueLatestOpList()
{
	Worker_OpList* OpList = Worker_Connection_GetOpList(WorkerConnection, 0);
	if (OpList->op_count > 0)
	{
		OpListQueue.Enqueue(OpList);
	}
	else
	{
		// TODO(Alex): memory leak
		//Worker_OpList_Destroy(OpList);
	}
}

void USpatialWorkerConnection::ProcessOutgoingMessages()
{
	while (!OutgoingMessagesQueue.IsEmpty())
	{
		TUniquePtr<FOutgoingMessage> OutgoingMessage;
		OutgoingMessagesQueue.Dequeue(OutgoingMessage);

		// TODO(Alex): fix these
		//OnDequeueMessage.Broadcast(OutgoingMessage.Get());

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

			Worker_Connection_SendCreateEntityRequest(WorkerConnection,
				Message->Components.Num(),
				Message->Components.GetData(),
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
	// TODO(Alex): fix it
	//OnEnqueueMessage.Broadcast(Message.Get());
	OutgoingMessagesQueue.Enqueue(MoveTemp(Message));
}
