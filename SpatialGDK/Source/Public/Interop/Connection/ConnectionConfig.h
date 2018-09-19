// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Containers/UnrealString.h"

#include <improbable/c_worker.h>

struct FConnectionConfig
{
	FConnectionConfig()
		: UseExternalIp(false)
		, EnableProtocolLoggingAtStartup(false)
		, LinkProtocol(WORKER_NETWORK_CONNECTION_TYPE_RAKNET)
	{
		const TCHAR* CommandLine = FCommandLine::Get();

		FParse::Value(CommandLine, TEXT("workerType"), WorkerType);
		FParse::Value(CommandLine, TEXT("workerId"), WorkerId);
		FParse::Bool(CommandLine, TEXT("useExternalIpForBridge"), UseExternalIp);

		FString LinkProtocolString;
		FParse::Value(CommandLine, TEXT("linkProtocol"), LinkProtocolString);
		LinkProtocol = LinkProtocolString == TEXT("Tcp") ? WORKER_NETWORK_CONNECTION_TYPE_TCP : WORKER_NETWORK_CONNECTION_TYPE_RAKNET;
	}

	FString WorkerId;
	FString WorkerType;
	bool UseExternalIp;
	bool EnableProtocolLoggingAtStartup;
	Worker_NetworkConnectionType LinkProtocol;
	Worker_ConnectionParameters ConnectionParams;
};

struct FReceptionistConfig : public FConnectionConfig
{
	FReceptionistConfig()
		: ReceptionistHost(TEXT("127.0.0.1"))
		, ReceptionistPort(7777)
	{
		const TCHAR* CommandLine = FCommandLine::Get();

		FParse::Value(CommandLine, TEXT("receptionistHost"), ReceptionistHost);
		FParse::Value(CommandLine, TEXT("receptionistPort"), ReceptionistPort);
	}

	FString ReceptionistHost;
	uint16 ReceptionistPort;
};

struct FLocatorConfig : public FConnectionConfig
{
	FLocatorConfig()
		: LocatorHost(TEXT("locator.improbable.io"))
	{
		const TCHAR* CommandLine = FCommandLine::Get();

		FParse::Value(CommandLine, TEXT("projectName"), ProjectName);
		FParse::Value(CommandLine, TEXT("loginToken"), LoginToken);
		FParse::Value(CommandLine, TEXT("locatorHost"), LocatorHost);
	}

	FString ProjectName;
	FString LocatorHost;
	FString LoginToken;
};
