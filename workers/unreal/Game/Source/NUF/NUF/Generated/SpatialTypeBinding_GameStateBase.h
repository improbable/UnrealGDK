// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
// Note that this file has been generated automatically
#pragma once

#include <improbable/worker.h>
#include <improbable/view.h>
#include <improbable/unreal/core_types.h>
#include <improbable/unreal/unreal_metadata.h>
#include <improbable/unreal/generated/UnrealGameStateBase.h>
#include "ScopedViewCallbacks.h"

#include "../SpatialHandlePropertyMap.h"
#include "../SpatialTypeBinding.h"

#include "SpatialTypeBinding_GameStateBase.generated.h"

UCLASS()
class USpatialTypeBinding_GameStateBase : public USpatialTypeBinding
{
	GENERATED_BODY()

public:
	static const FRepHandlePropertyMap& GetHandlePropertyMap();

	UClass* GetBoundClass() const override;

	void Init(USpatialInterop* InInterop, USpatialPackageMapClient* InPackageMap) override;
	void BindToView() override;
	void UnbindFromView() override;
	worker::ComponentId GetReplicatedGroupComponentId(EReplicatedPropertyGroup Group) const override;

	worker::Entity CreateActorEntity(const FString& ClientWorkerId, const FVector& Position, const FString& Metadata, const FPropertyChangeState& InitialChanges, USpatialActorChannel* Channel) const override;
	void SendComponentUpdates(const FPropertyChangeState& Changes, USpatialActorChannel* Channel, const worker::EntityId& EntityId) const override;
	void SendRPCCommand(UObject* TargetObject, const UFunction* const Function, FFrame* const Frame) override;

	void ReceiveAddComponent(USpatialActorChannel* Channel, UAddComponentOpWrapperBase* AddComponentOp) const override;
	void ApplyQueuedStateToChannel(USpatialActorChannel* ActorChannel) override;

private:
	improbable::unreal::callbacks::FScopedViewCallbacks ViewCallbacks;

	// Pending updates.
	TMap<worker::EntityId, improbable::unreal::UnrealGameStateBaseSingleClientReplicatedData::Data> PendingSingleClientData;
	TMap<worker::EntityId, improbable::unreal::UnrealGameStateBaseMultiClientReplicatedData::Data> PendingMultiClientData;

	// RPC to sender map.
	using FRPCSender = void (USpatialTypeBinding_GameStateBase::*)(worker::Connection* const, struct FFrame* const, UObject*);
	TMap<FName, FRPCSender> RPCToSenderMap;

	// Component update helper functions.
	void BuildSpatialComponentUpdate(
		const FPropertyChangeState& Changes,
		USpatialActorChannel* Channel,
		improbable::unreal::UnrealGameStateBaseSingleClientReplicatedData::Update& SingleClientUpdate,
		bool& bSingleClientUpdateChanged,
		improbable::unreal::UnrealGameStateBaseMultiClientReplicatedData::Update& MultiClientUpdate,
		bool& bMultiClientUpdateChanged) const;
	void ServerSendUpdate_SingleClient(
		const uint8* RESTRICT Data,
		int32 Handle,
		UProperty* Property,
		USpatialActorChannel* Channel,
		improbable::unreal::UnrealGameStateBaseSingleClientReplicatedData::Update& OutUpdate) const;
	void ServerSendUpdate_MultiClient(
		const uint8* RESTRICT Data,
		int32 Handle,
		UProperty* Property,
		USpatialActorChannel* Channel,
		improbable::unreal::UnrealGameStateBaseMultiClientReplicatedData::Update& OutUpdate) const;
	void ClientReceiveUpdate_SingleClient(
		USpatialActorChannel* ActorChannel,
		const improbable::unreal::UnrealGameStateBaseSingleClientReplicatedData::Update& Update) const;
	void ClientReceiveUpdate_MultiClient(
		USpatialActorChannel* ActorChannel,
		const improbable::unreal::UnrealGameStateBaseMultiClientReplicatedData::Update& Update) const;

	// RPC command sender functions.

	// RPC command request handler functions.

	// RPC command response handler functions.
};
