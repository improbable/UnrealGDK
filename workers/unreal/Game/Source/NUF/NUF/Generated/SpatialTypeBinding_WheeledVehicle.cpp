// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
// Note that this file has been generated automatically

#include "SpatialTypeBinding_WheeledVehicle.h"
#include "Engine.h"
#include "SpatialOS.h"
#include "EntityBuilder.h"

#include "../SpatialConstants.h"
#include "../SpatialUnrealObjectRef.h"
#include "../SpatialActorChannel.h"
#include "../SpatialPackageMapClient.h"
#include "../SpatialNetDriver.h"
#include "../SpatialInterop.h"

#include "WheeledVehicleMovementComponent.h"

const FRepHandlePropertyMap& USpatialTypeBinding_WheeledVehicle::GetHandlePropertyMap()
{
	static FRepHandlePropertyMap HandleToPropertyMap;
	if (HandleToPropertyMap.Num() == 0)
	{
		UClass* Class = FindObject<UClass>(ANY_PACKAGE, TEXT("WheeledVehicle"));
		HandleToPropertyMap.Add(1, FRepHandleData{nullptr, Class->FindPropertyByName("bHidden"), COND_None});
		HandleToPropertyMap.Add(2, FRepHandleData{nullptr, Class->FindPropertyByName("bReplicateMovement"), COND_None});
		HandleToPropertyMap.Add(3, FRepHandleData{nullptr, Class->FindPropertyByName("bTearOff"), COND_None});
		HandleToPropertyMap.Add(4, FRepHandleData{nullptr, Class->FindPropertyByName("RemoteRole"), COND_None});
		HandleToPropertyMap.Add(5, FRepHandleData{nullptr, Class->FindPropertyByName("Owner"), COND_None});
		HandleToPropertyMap.Add(6, FRepHandleData{nullptr, Class->FindPropertyByName("ReplicatedMovement"), COND_SimulatedOrPhysics});
		HandleToPropertyMap.Add(7, FRepHandleData{Class->FindPropertyByName("AttachmentReplication"), nullptr, COND_Custom});
		HandleToPropertyMap[7].Property = Cast<UStructProperty>(HandleToPropertyMap[7].Parent)->Struct->FindPropertyByName("AttachParent");
		HandleToPropertyMap.Add(8, FRepHandleData{Class->FindPropertyByName("AttachmentReplication"), nullptr, COND_Custom});
		HandleToPropertyMap[8].Property = Cast<UStructProperty>(HandleToPropertyMap[8].Parent)->Struct->FindPropertyByName("LocationOffset");
		HandleToPropertyMap.Add(9, FRepHandleData{Class->FindPropertyByName("AttachmentReplication"), nullptr, COND_Custom});
		HandleToPropertyMap[9].Property = Cast<UStructProperty>(HandleToPropertyMap[9].Parent)->Struct->FindPropertyByName("RelativeScale3D");
		HandleToPropertyMap.Add(10, FRepHandleData{Class->FindPropertyByName("AttachmentReplication"), nullptr, COND_Custom});
		HandleToPropertyMap[10].Property = Cast<UStructProperty>(HandleToPropertyMap[10].Parent)->Struct->FindPropertyByName("RotationOffset");
		HandleToPropertyMap.Add(11, FRepHandleData{Class->FindPropertyByName("AttachmentReplication"), nullptr, COND_Custom});
		HandleToPropertyMap[11].Property = Cast<UStructProperty>(HandleToPropertyMap[11].Parent)->Struct->FindPropertyByName("AttachSocket");
		HandleToPropertyMap.Add(12, FRepHandleData{Class->FindPropertyByName("AttachmentReplication"), nullptr, COND_Custom});
		HandleToPropertyMap[12].Property = Cast<UStructProperty>(HandleToPropertyMap[12].Parent)->Struct->FindPropertyByName("AttachComponent");
		HandleToPropertyMap.Add(13, FRepHandleData{nullptr, Class->FindPropertyByName("Role"), COND_None});
		HandleToPropertyMap.Add(14, FRepHandleData{nullptr, Class->FindPropertyByName("bCanBeDamaged"), COND_None});
		HandleToPropertyMap.Add(15, FRepHandleData{nullptr, Class->FindPropertyByName("Instigator"), COND_None});
		HandleToPropertyMap.Add(16, FRepHandleData{nullptr, Class->FindPropertyByName("PlayerState"), COND_None});
		HandleToPropertyMap.Add(17, FRepHandleData{nullptr, Class->FindPropertyByName("RemoteViewPitch"), COND_SkipOwner});
		HandleToPropertyMap.Add(18, FRepHandleData{nullptr, Class->FindPropertyByName("Controller"), COND_None});
	}
	return HandleToPropertyMap;
}

void USpatialTypeBinding_WheeledVehicle::Init(USpatialInterop* InInterop, USpatialPackageMapClient* InPackageMap)
{
	Super::Init(InInterop, InPackageMap);

	RPCToSenderMap.Emplace("ServerUpdateState", &USpatialTypeBinding_WheeledVehicle::ServerUpdateState_SendCommand);
}

void USpatialTypeBinding_WheeledVehicle::BindToView()
{
	TSharedPtr<worker::View> View = Interop->GetSpatialOS()->GetView().Pin();
	if (Interop->GetNetDriver()->GetNetMode() == NM_Client)
	{
		SingleClientAddCallback = View->OnAddComponent<improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData>([this](
			const worker::AddComponentOp<improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData>& Op)
		{
			auto Update = improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Update::FromInitialData(Op.Data);
			USpatialActorChannel* ActorChannel = Interop->GetActorChannelByEntityId(Op.EntityId);
			if (ActorChannel)
			{
				ClientReceiveUpdate_SingleClient(ActorChannel, Update);
			}
			else
			{
				PendingSingleClientData.Add(Op.EntityId, Op.Data);
			}
		});
		SingleClientUpdateCallback = View->OnComponentUpdate<improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData>([this](
			const worker::ComponentUpdateOp<improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData>& Op)
		{
			USpatialActorChannel* ActorChannel = Interop->GetActorChannelByEntityId(Op.EntityId);
			if (ActorChannel)
			{
				ClientReceiveUpdate_SingleClient(ActorChannel, Op.Update);
			}
			else
			{
				Op.Update.ApplyTo(PendingSingleClientData.FindOrAdd(Op.EntityId));
			}
		});
		MultiClientAddCallback = View->OnAddComponent<improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData>([this](
			const worker::AddComponentOp<improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData>& Op)
		{
			auto Update = improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Update::FromInitialData(Op.Data);
			USpatialActorChannel* ActorChannel = Interop->GetActorChannelByEntityId(Op.EntityId);
			if (ActorChannel)
			{
				ClientReceiveUpdate_MultiClient(ActorChannel, Update);
			}
			else
			{
				PendingMultiClientData.Add(Op.EntityId, Op.Data);
			}
		});
		MultiClientUpdateCallback = View->OnComponentUpdate<improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData>([this](
			const worker::ComponentUpdateOp<improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData>& Op)
		{
			USpatialActorChannel* ActorChannel = Interop->GetActorChannelByEntityId(Op.EntityId);
			if (ActorChannel)
			{
				ClientReceiveUpdate_MultiClient(ActorChannel, Op.Update);
			}
			else
			{
				Op.Update.ApplyTo(PendingMultiClientData.FindOrAdd(Op.EntityId));
			}
		});
	}

	using ServerRPCCommandTypes = improbable::unreal::UnrealWheeledVehicleServerRPCs::Commands;
	RPCReceiverCallbacks.AddUnique(View->OnCommandRequest<ServerRPCCommandTypes::Serverupdatestate>(std::bind(&USpatialTypeBinding_WheeledVehicle::ServerUpdateState_OnCommandRequest, this, std::placeholders::_1)));
	RPCReceiverCallbacks.AddUnique(View->OnCommandResponse<ServerRPCCommandTypes::Serverupdatestate>(std::bind(&USpatialTypeBinding_WheeledVehicle::ServerUpdateState_OnCommandResponse, this, std::placeholders::_1)));
}

void USpatialTypeBinding_WheeledVehicle::UnbindFromView()
{
	TSharedPtr<worker::View> View = Interop->GetSpatialOS()->GetView().Pin();
	if (Interop->GetNetDriver()->GetNetMode() == NM_Client)
	{
		View->Remove(SingleClientAddCallback);
		View->Remove(SingleClientUpdateCallback);
		View->Remove(MultiClientAddCallback);
		View->Remove(MultiClientUpdateCallback);
	}
	for (auto& Callback : RPCReceiverCallbacks)
	{
		View->Remove(Callback);
	}
}

worker::ComponentId USpatialTypeBinding_WheeledVehicle::GetReplicatedGroupComponentId(EReplicatedPropertyGroup Group) const
{
	switch (Group)
	{
	case GROUP_SingleClient:
		return improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::ComponentId;
	case GROUP_MultiClient:
		return improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::ComponentId;
	default:
		checkNoEntry();
		return 0;
	}
}

worker::Entity USpatialTypeBinding_WheeledVehicle::CreateActorEntity(const FString& ClientWorkerId, const FVector& Position, const FString& Metadata, const FPropertyChangeState& InitialChanges, USpatialActorChannel* Channel) const
{
	// Setup initial data.
	improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Data SingleClientData;
	improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Update SingleClientUpdate;
	bool bSingleClientUpdateChanged = false;
	improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Data MultiClientData;
	improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Update MultiClientUpdate;
	bool bMultiClientUpdateChanged = false;
	BuildSpatialComponentUpdate(InitialChanges, Channel, SingleClientUpdate, bSingleClientUpdateChanged, MultiClientUpdate, bMultiClientUpdateChanged);
	SingleClientUpdate.ApplyTo(SingleClientData);
	MultiClientUpdate.ApplyTo(MultiClientData);

	// Create entity.
	std::string ClientWorkerIdString = TCHAR_TO_UTF8(*ClientWorkerId);

	improbable::WorkerAttributeSet WorkerAttribute{{worker::List<std::string>{"UnrealWorker"}}};
	improbable::WorkerAttributeSet ClientAttribute{{worker::List<std::string>{"UnrealClient"}}};
	improbable::WorkerAttributeSet OwningClientAttribute{{"workerId:" + ClientWorkerIdString}};

	improbable::WorkerRequirementSet WorkersOnly{{WorkerAttribute}};
	improbable::WorkerRequirementSet ClientsOnly{{ClientAttribute}};
	improbable::WorkerRequirementSet OwningClientOnly{{OwningClientAttribute}};
	improbable::WorkerRequirementSet AnyUnrealWorkerOrClient{{WorkerAttribute, ClientAttribute}};
	improbable::WorkerRequirementSet AnyUnrealWorkerOrOwningClient{{WorkerAttribute, OwningClientAttribute}};

	const improbable::Coordinates SpatialPosition = SpatialConstants::LocationToSpatialOSCoordinates(Position);
	worker::Option<std::string> StaticPath;
	if (Channel->Actor->IsFullNameStableForNetworking())
	{
		StaticPath = {std::string{TCHAR_TO_UTF8(*Channel->Actor->GetPathName(Channel->Actor->GetWorld()))}};
	}
	return improbable::unreal::FEntityBuilder::Begin()
		.AddPositionComponent(improbable::Position::Data{SpatialPosition}, WorkersOnly)
		.AddMetadataComponent(improbable::Metadata::Data{TCHAR_TO_UTF8(*Metadata)})
		.SetPersistence(true)
		.SetReadAcl(AnyUnrealWorkerOrClient)
		.AddComponent<improbable::unreal::UnrealMetadata>(improbable::unreal::UnrealMetadata::Data{StaticPath}, WorkersOnly)
		.AddComponent<improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData>(SingleClientData, WorkersOnly)
		.AddComponent<improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData>(MultiClientData, WorkersOnly)
		.AddComponent<improbable::unreal::UnrealWheeledVehicleCompleteData>(improbable::unreal::UnrealWheeledVehicleCompleteData::Data{}, WorkersOnly)
		.AddComponent<improbable::unreal::UnrealWheeledVehicleClientRPCs>(improbable::unreal::UnrealWheeledVehicleClientRPCs::Data{}, OwningClientOnly)
		.AddComponent<improbable::unreal::UnrealWheeledVehicleServerRPCs>(improbable::unreal::UnrealWheeledVehicleServerRPCs::Data{}, WorkersOnly)
		.Build();
}

void USpatialTypeBinding_WheeledVehicle::SendComponentUpdates(const FPropertyChangeState& Changes, USpatialActorChannel* Channel, const worker::EntityId& EntityId) const
{
	// Build SpatialOS updates.
	improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Update SingleClientUpdate;
	bool bSingleClientUpdateChanged = false;
	improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Update MultiClientUpdate;
	bool bMultiClientUpdateChanged = false;
	BuildSpatialComponentUpdate(Changes, Channel, SingleClientUpdate, bSingleClientUpdateChanged, MultiClientUpdate, bMultiClientUpdateChanged);

	// Send SpatialOS updates if anything changed.
	TSharedPtr<worker::Connection> Connection = Interop->GetSpatialOS()->GetConnection().Pin();
	if (bSingleClientUpdateChanged)
	{
		Connection->SendComponentUpdate<improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData>(EntityId, SingleClientUpdate);
	}
	if (bMultiClientUpdateChanged)
	{
		Connection->SendComponentUpdate<improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData>(EntityId, MultiClientUpdate);
	}
}

void USpatialTypeBinding_WheeledVehicle::SendRPCCommand(UObject* TargetObject, const UFunction* const Function, FFrame* const Frame)
{
	TSharedPtr<worker::Connection> Connection = Interop->GetSpatialOS()->GetConnection().Pin();
	auto SenderFuncIterator = RPCToSenderMap.Find(Function->GetFName());
	checkf(*SenderFuncIterator, TEXT("Sender for %s has not been registered with RPCToSenderMap."), *Function->GetFName().ToString());
	(this->*(*SenderFuncIterator))(Connection.Get(), Frame, TargetObject);
}

void USpatialTypeBinding_WheeledVehicle::ApplyQueuedStateToChannel(USpatialActorChannel* ActorChannel)
{
	improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Data* SingleClientData = PendingSingleClientData.Find(ActorChannel->GetEntityId());
	if (SingleClientData)
	{
		auto Update = improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Update::FromInitialData(*SingleClientData);
		PendingSingleClientData.Remove(ActorChannel->GetEntityId());
		ClientReceiveUpdate_SingleClient(ActorChannel, Update);
	}
	improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Data* MultiClientData = PendingMultiClientData.Find(ActorChannel->GetEntityId());
	if (MultiClientData)
	{
		auto Update = improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Update::FromInitialData(*MultiClientData);
		PendingMultiClientData.Remove(ActorChannel->GetEntityId());
		ClientReceiveUpdate_MultiClient(ActorChannel, Update);
	}
}

void USpatialTypeBinding_WheeledVehicle::BuildSpatialComponentUpdate(
	const FPropertyChangeState& Changes,
	USpatialActorChannel* Channel,
	improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Update& SingleClientUpdate,
	bool& bSingleClientUpdateChanged,
	improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Update& MultiClientUpdate,
	bool& bMultiClientUpdateChanged) const
{
	// Build up SpatialOS component updates.
	auto& PropertyMap = GetHandlePropertyMap();
	FChangelistIterator ChangelistIterator(Changes.Changed, 0);
	FRepHandleIterator HandleIterator(ChangelistIterator, Changes.Cmds, Changes.BaseHandleToCmdIndex, 0, 1, 0, Changes.Cmds.Num() - 1);
	while (HandleIterator.NextHandle())
	{
		const FRepLayoutCmd& Cmd = Changes.Cmds[HandleIterator.CmdIndex];
		const uint8* Data = Changes.SourceData + HandleIterator.ArrayOffset + Cmd.Offset;
		auto& PropertyMapData = PropertyMap[HandleIterator.Handle];
		UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Sending property update. actor %s (%lld), property %s (handle %d)"),
			*Interop->GetSpatialOS()->GetWorkerId(),
			*Channel->Actor->GetName(),
			Channel->GetEntityId(),
			*Cmd.Property->GetName(),
			HandleIterator.Handle);
		switch (GetGroupFromCondition(PropertyMapData.Condition))
		{
		case GROUP_SingleClient:
			ServerSendUpdate_SingleClient(Data, HandleIterator.Handle, Cmd.Property, Channel, SingleClientUpdate);
			bSingleClientUpdateChanged = true;
			break;
		case GROUP_MultiClient:
			ServerSendUpdate_MultiClient(Data, HandleIterator.Handle, Cmd.Property, Channel, MultiClientUpdate);
			bMultiClientUpdateChanged = true;
			break;
		}
	}
}

void USpatialTypeBinding_WheeledVehicle::ServerSendUpdate_SingleClient(
	const uint8* RESTRICT Data,
	int32 Handle,
	UProperty* Property,
	USpatialActorChannel* Channel,
	improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Update& OutUpdate) const
{
}

void USpatialTypeBinding_WheeledVehicle::ServerSendUpdate_MultiClient(
	const uint8* RESTRICT Data,
	int32 Handle,
	UProperty* Property,
	USpatialActorChannel* Channel,
	improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Update& OutUpdate) const
{
	switch (Handle)
	{
		case 1: // field_bhidden
		{
			bool Value = static_cast<UBoolProperty*>(Property)->GetPropertyValue(Data);

			OutUpdate.set_field_bhidden(Value);
			break;
		}
		case 2: // field_breplicatemovement
		{
			bool Value = static_cast<UBoolProperty*>(Property)->GetPropertyValue(Data);

			OutUpdate.set_field_breplicatemovement(Value);
			break;
		}
		case 3: // field_btearoff
		{
			bool Value = static_cast<UBoolProperty*>(Property)->GetPropertyValue(Data);

			OutUpdate.set_field_btearoff(Value);
			break;
		}
		case 4: // field_remoterole
		{
			TEnumAsByte<ENetRole> Value = *(reinterpret_cast<TEnumAsByte<ENetRole> const*>(Data));

			OutUpdate.set_field_remoterole(uint32_t(Value));
			break;
		}
		case 5: // field_owner
		{
			AActor* Value = *(reinterpret_cast<AActor* const*>(Data));

			if (Value != nullptr)
			{
				FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromObject(Value);
				improbable::unreal::UnrealObjectRef ObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(NetGUID);
				if (ObjectRef == SpatialConstants::UNRESOLVED_OBJECT_REF)
				{
					Interop->QueueOutgoingObjectUpdate_Internal(Value, Channel, 5);
				}
				else
				{
					OutUpdate.set_field_owner(ObjectRef);
				}
			}
			else
			{
				OutUpdate.set_field_owner(SpatialConstants::NULL_OBJECT_REF);
			}
			break;
		}
		case 6: // field_replicatedmovement
		{
			FRepMovement Value = *(reinterpret_cast<FRepMovement const*>(Data));

			{
				TArray<uint8> ValueData;
				FMemoryWriter ValueDataWriter(ValueData);
				bool Success;
				Value.NetSerialize(ValueDataWriter, nullptr, Success);
				OutUpdate.set_field_replicatedmovement(std::string((char*)ValueData.GetData(), ValueData.Num()));
			}
			break;
		}
		case 7: // field_attachmentreplication_attachparent
		{
			AActor* Value = *(reinterpret_cast<AActor* const*>(Data));

			if (Value != nullptr)
			{
				FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromObject(Value);
				improbable::unreal::UnrealObjectRef ObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(NetGUID);
				if (ObjectRef == SpatialConstants::UNRESOLVED_OBJECT_REF)
				{
					Interop->QueueOutgoingObjectUpdate_Internal(Value, Channel, 7);
				}
				else
				{
					OutUpdate.set_field_attachmentreplication_attachparent(ObjectRef);
				}
			}
			else
			{
				OutUpdate.set_field_attachmentreplication_attachparent(SpatialConstants::NULL_OBJECT_REF);
			}
			break;
		}
		case 8: // field_attachmentreplication_locationoffset
		{
			FVector_NetQuantize100 Value = *(reinterpret_cast<FVector_NetQuantize100 const*>(Data));

			OutUpdate.set_field_attachmentreplication_locationoffset(improbable::Vector3f(Value.X, Value.Y, Value.Z));
			break;
		}
		case 9: // field_attachmentreplication_relativescale3d
		{
			FVector_NetQuantize100 Value = *(reinterpret_cast<FVector_NetQuantize100 const*>(Data));

			OutUpdate.set_field_attachmentreplication_relativescale3d(improbable::Vector3f(Value.X, Value.Y, Value.Z));
			break;
		}
		case 10: // field_attachmentreplication_rotationoffset
		{
			FRotator Value = *(reinterpret_cast<FRotator const*>(Data));

			OutUpdate.set_field_attachmentreplication_rotationoffset(improbable::unreal::UnrealFRotator(Value.Yaw, Value.Pitch, Value.Roll));
			break;
		}
		case 11: // field_attachmentreplication_attachsocket
		{
			FName Value = *(reinterpret_cast<FName const*>(Data));

			OutUpdate.set_field_attachmentreplication_attachsocket(TCHAR_TO_UTF8(*Value.ToString()));
			break;
		}
		case 12: // field_attachmentreplication_attachcomponent
		{
			USceneComponent* Value = *(reinterpret_cast<USceneComponent* const*>(Data));

			if (Value != nullptr)
			{
				FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromObject(Value);
				improbable::unreal::UnrealObjectRef ObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(NetGUID);
				if (ObjectRef == SpatialConstants::UNRESOLVED_OBJECT_REF)
				{
					Interop->QueueOutgoingObjectUpdate_Internal(Value, Channel, 12);
				}
				else
				{
					OutUpdate.set_field_attachmentreplication_attachcomponent(ObjectRef);
				}
			}
			else
			{
				OutUpdate.set_field_attachmentreplication_attachcomponent(SpatialConstants::NULL_OBJECT_REF);
			}
			break;
		}
		case 13: // field_role
		{
			TEnumAsByte<ENetRole> Value = *(reinterpret_cast<TEnumAsByte<ENetRole> const*>(Data));

			OutUpdate.set_field_role(uint32_t(Value));
			break;
		}
		case 14: // field_bcanbedamaged
		{
			bool Value = static_cast<UBoolProperty*>(Property)->GetPropertyValue(Data);

			OutUpdate.set_field_bcanbedamaged(Value);
			break;
		}
		case 15: // field_instigator
		{
			APawn* Value = *(reinterpret_cast<APawn* const*>(Data));

			if (Value != nullptr)
			{
				FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromObject(Value);
				improbable::unreal::UnrealObjectRef ObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(NetGUID);
				if (ObjectRef == SpatialConstants::UNRESOLVED_OBJECT_REF)
				{
					Interop->QueueOutgoingObjectUpdate_Internal(Value, Channel, 15);
				}
				else
				{
					OutUpdate.set_field_instigator(ObjectRef);
				}
			}
			else
			{
				OutUpdate.set_field_instigator(SpatialConstants::NULL_OBJECT_REF);
			}
			break;
		}
		case 16: // field_playerstate
		{
			APlayerState* Value = *(reinterpret_cast<APlayerState* const*>(Data));

			if (Value != nullptr)
			{
				FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromObject(Value);
				improbable::unreal::UnrealObjectRef ObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(NetGUID);
				if (ObjectRef == SpatialConstants::UNRESOLVED_OBJECT_REF)
				{
					Interop->QueueOutgoingObjectUpdate_Internal(Value, Channel, 16);
				}
				else
				{
					OutUpdate.set_field_playerstate(ObjectRef);
				}
			}
			else
			{
				OutUpdate.set_field_playerstate(SpatialConstants::NULL_OBJECT_REF);
			}
			break;
		}
		case 17: // field_remoteviewpitch
		{
			uint8 Value = *(reinterpret_cast<uint8 const*>(Data));

			OutUpdate.set_field_remoteviewpitch(uint32_t(Value));
			break;
		}
		case 18: // field_controller
		{
			AController* Value = *(reinterpret_cast<AController* const*>(Data));

			if (Value != nullptr)
			{
				FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromObject(Value);
				improbable::unreal::UnrealObjectRef ObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(NetGUID);
				if (ObjectRef == SpatialConstants::UNRESOLVED_OBJECT_REF)
				{
					Interop->QueueOutgoingObjectUpdate_Internal(Value, Channel, 18);
				}
				else
				{
					OutUpdate.set_field_controller(ObjectRef);
				}
			}
			else
			{
				OutUpdate.set_field_controller(SpatialConstants::NULL_OBJECT_REF);
			}
			break;
		}
	default:
		checkf(false, TEXT("Unknown replication handle %d encountered when creating a SpatialOS update."));
		break;
	}
}

void USpatialTypeBinding_WheeledVehicle::ClientReceiveUpdate_SingleClient(
	USpatialActorChannel* ActorChannel,
	const improbable::unreal::UnrealWheeledVehicleSingleClientReplicatedData::Update& Update) const
{
	FBunchPayloadWriter OutputWriter(PackageMap);

	auto& HandleToPropertyMap = GetHandlePropertyMap();
	const bool bAutonomousProxy = ActorChannel->IsClientAutonomousProxy(improbable::unreal::UnrealWheeledVehicleClientRPCs::ComponentId);
	ConditionMapFilter ConditionMap(ActorChannel, bAutonomousProxy);
	Interop->ReceiveSpatialUpdate(ActorChannel, OutputWriter.GetNetBitWriter());
}

void USpatialTypeBinding_WheeledVehicle::ClientReceiveUpdate_MultiClient(
	USpatialActorChannel* ActorChannel,
	const improbable::unreal::UnrealWheeledVehicleMultiClientReplicatedData::Update& Update) const
{
	FBunchPayloadWriter OutputWriter(PackageMap);

	auto& HandleToPropertyMap = GetHandlePropertyMap();
	const bool bAutonomousProxy = ActorChannel->IsClientAutonomousProxy(improbable::unreal::UnrealWheeledVehicleClientRPCs::ComponentId);
	ConditionMapFilter ConditionMap(ActorChannel, bAutonomousProxy);
	if (!Update.field_bhidden().empty())
	{
		// field_bhidden
		uint32 Handle = 1;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			uint8 Value;

			Value = (*Update.field_bhidden().data());
			// Because Unreal will look for a specific bit in the serialization function below, we simply set all bits.
			// We will soon move away from using bunches when receiving Spatial updates.
			if (Value)
			{
				Value = 0xFF;
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_breplicatemovement().empty())
	{
		// field_breplicatemovement
		uint32 Handle = 2;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			uint8 Value;

			Value = (*Update.field_breplicatemovement().data());
			// Because Unreal will look for a specific bit in the serialization function below, we simply set all bits.
			// We will soon move away from using bunches when receiving Spatial updates.
			if (Value)
			{
				Value = 0xFF;
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_btearoff().empty())
	{
		// field_btearoff
		uint32 Handle = 3;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			uint8 Value;

			Value = (*Update.field_btearoff().data());
			// Because Unreal will look for a specific bit in the serialization function below, we simply set all bits.
			// We will soon move away from using bunches when receiving Spatial updates.
			if (Value)
			{
				Value = 0xFF;
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_remoterole().empty())
	{
		// field_remoterole
		uint32 Handle = 4;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			TEnumAsByte<ENetRole> Value;

			Value = TEnumAsByte<ENetRole>(uint8((*Update.field_remoterole().data())));

			// Downgrade role from AutonomousProxy to SimulatedProxy if we aren't authoritative over
			// the server RPCs component.
			if (Value == ROLE_AutonomousProxy && !bAutonomousProxy)
			{
				Value = ROLE_SimulatedProxy;
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_owner().empty())
	{
		// field_owner
		uint32 Handle = 5;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			bool bWriteObjectProperty = true;
			AActor* Value;

			{
				improbable::unreal::UnrealObjectRef ObjectRef = (*Update.field_owner().data());
				check(ObjectRef != SpatialConstants::UNRESOLVED_OBJECT_REF);
				if (ObjectRef == SpatialConstants::NULL_OBJECT_REF)
				{
					Value = nullptr;
				}
				else
				{
					FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromUnrealObjectRef(ObjectRef);
					if (NetGUID.IsValid())
					{
						Value = static_cast<AActor*>(PackageMap->GetObjectFromNetGUID(NetGUID, true));
					}
					else
					{
						UE_LOG(LogSpatialOSInterop, Log, TEXT("%s: Received unresolved object property. Value: %s. actor %s (%llu), property %s (handle %d)"),
							*Interop->GetSpatialOS()->GetWorkerId(),
							*ObjectRefToString(ObjectRef),
							*ActorChannel->Actor->GetName(),
							ActorChannel->GetEntityId(),
							*Data.Property->GetName(),
							Handle);
						bWriteObjectProperty = false;
						Interop->QueueIncomingObjectUpdate_Internal(ObjectRef, ActorChannel, Cast<UObjectPropertyBase>(Data.Property), Handle);
					}
				}
			}

			if (bWriteObjectProperty)
			{
				OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
				UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
					*Interop->GetSpatialOS()->GetWorkerId(),
					*ActorChannel->Actor->GetName(),
					ActorChannel->GetEntityId(),
					*Data.Property->GetName(),
					Handle);
			}
		}
	}
	if (!Update.field_replicatedmovement().empty())
	{
		// field_replicatedmovement
		uint32 Handle = 6;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			FRepMovement Value;

			{
				auto& ValueDataStr = (*Update.field_replicatedmovement().data());
				TArray<uint8> ValueData;
				ValueData.Append((uint8*)ValueDataStr.data(), ValueDataStr.size());
				FMemoryReader ValueDataReader(ValueData);
				bool bSuccess;
				Value.NetSerialize(ValueDataReader, nullptr, bSuccess);
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_attachmentreplication_attachparent().empty())
	{
		// field_attachmentreplication_attachparent
		uint32 Handle = 7;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			bool bWriteObjectProperty = true;
			AActor* Value;

			{
				improbable::unreal::UnrealObjectRef ObjectRef = (*Update.field_attachmentreplication_attachparent().data());
				check(ObjectRef != SpatialConstants::UNRESOLVED_OBJECT_REF);
				if (ObjectRef == SpatialConstants::NULL_OBJECT_REF)
				{
					Value = nullptr;
				}
				else
				{
					FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromUnrealObjectRef(ObjectRef);
					if (NetGUID.IsValid())
					{
						Value = static_cast<AActor*>(PackageMap->GetObjectFromNetGUID(NetGUID, true));
					}
					else
					{
						UE_LOG(LogSpatialOSInterop, Log, TEXT("%s: Received unresolved object property. Value: %s. actor %s (%llu), property %s (handle %d)"),
							*Interop->GetSpatialOS()->GetWorkerId(),
							*ObjectRefToString(ObjectRef),
							*ActorChannel->Actor->GetName(),
							ActorChannel->GetEntityId(),
							*Data.Property->GetName(),
							Handle);
						bWriteObjectProperty = false;
						Interop->QueueIncomingObjectUpdate_Internal(ObjectRef, ActorChannel, Cast<UObjectPropertyBase>(Data.Property), Handle);
					}
				}
			}

			if (bWriteObjectProperty)
			{
				OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
				UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
					*Interop->GetSpatialOS()->GetWorkerId(),
					*ActorChannel->Actor->GetName(),
					ActorChannel->GetEntityId(),
					*Data.Property->GetName(),
					Handle);
			}
		}
	}
	if (!Update.field_attachmentreplication_locationoffset().empty())
	{
		// field_attachmentreplication_locationoffset
		uint32 Handle = 8;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			FVector_NetQuantize100 Value;

			{
				auto& Vector = (*Update.field_attachmentreplication_locationoffset().data());
				Value.X = Vector.x();
				Value.Y = Vector.y();
				Value.Z = Vector.z();
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_attachmentreplication_relativescale3d().empty())
	{
		// field_attachmentreplication_relativescale3d
		uint32 Handle = 9;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			FVector_NetQuantize100 Value;

			{
				auto& Vector = (*Update.field_attachmentreplication_relativescale3d().data());
				Value.X = Vector.x();
				Value.Y = Vector.y();
				Value.Z = Vector.z();
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_attachmentreplication_rotationoffset().empty())
	{
		// field_attachmentreplication_rotationoffset
		uint32 Handle = 10;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			FRotator Value;

			{
				auto& Rotator = (*Update.field_attachmentreplication_rotationoffset().data());
				Value.Yaw = Rotator.yaw();
				Value.Pitch = Rotator.pitch();
				Value.Roll = Rotator.roll();
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_attachmentreplication_attachsocket().empty())
	{
		// field_attachmentreplication_attachsocket
		uint32 Handle = 11;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			FName Value;

			Value = FName(((*Update.field_attachmentreplication_attachsocket().data())).data());

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_attachmentreplication_attachcomponent().empty())
	{
		// field_attachmentreplication_attachcomponent
		uint32 Handle = 12;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			bool bWriteObjectProperty = true;
			USceneComponent* Value;

			{
				improbable::unreal::UnrealObjectRef ObjectRef = (*Update.field_attachmentreplication_attachcomponent().data());
				check(ObjectRef != SpatialConstants::UNRESOLVED_OBJECT_REF);
				if (ObjectRef == SpatialConstants::NULL_OBJECT_REF)
				{
					Value = nullptr;
				}
				else
				{
					FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromUnrealObjectRef(ObjectRef);
					if (NetGUID.IsValid())
					{
						Value = static_cast<USceneComponent*>(PackageMap->GetObjectFromNetGUID(NetGUID, true));
					}
					else
					{
						UE_LOG(LogSpatialOSInterop, Log, TEXT("%s: Received unresolved object property. Value: %s. actor %s (%llu), property %s (handle %d)"),
							*Interop->GetSpatialOS()->GetWorkerId(),
							*ObjectRefToString(ObjectRef),
							*ActorChannel->Actor->GetName(),
							ActorChannel->GetEntityId(),
							*Data.Property->GetName(),
							Handle);
						bWriteObjectProperty = false;
						Interop->QueueIncomingObjectUpdate_Internal(ObjectRef, ActorChannel, Cast<UObjectPropertyBase>(Data.Property), Handle);
					}
				}
			}

			if (bWriteObjectProperty)
			{
				OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
				UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
					*Interop->GetSpatialOS()->GetWorkerId(),
					*ActorChannel->Actor->GetName(),
					ActorChannel->GetEntityId(),
					*Data.Property->GetName(),
					Handle);
			}
		}
	}
	if (!Update.field_role().empty())
	{
		// field_role
		uint32 Handle = 13;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			TEnumAsByte<ENetRole> Value;

			Value = TEnumAsByte<ENetRole>(uint8((*Update.field_role().data())));

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_bcanbedamaged().empty())
	{
		// field_bcanbedamaged
		uint32 Handle = 14;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			uint8 Value;

			Value = (*Update.field_bcanbedamaged().data());
			// Because Unreal will look for a specific bit in the serialization function below, we simply set all bits.
			// We will soon move away from using bunches when receiving Spatial updates.
			if (Value)
			{
				Value = 0xFF;
			}

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_instigator().empty())
	{
		// field_instigator
		uint32 Handle = 15;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			bool bWriteObjectProperty = true;
			APawn* Value;

			{
				improbable::unreal::UnrealObjectRef ObjectRef = (*Update.field_instigator().data());
				check(ObjectRef != SpatialConstants::UNRESOLVED_OBJECT_REF);
				if (ObjectRef == SpatialConstants::NULL_OBJECT_REF)
				{
					Value = nullptr;
				}
				else
				{
					FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromUnrealObjectRef(ObjectRef);
					if (NetGUID.IsValid())
					{
						Value = static_cast<APawn*>(PackageMap->GetObjectFromNetGUID(NetGUID, true));
					}
					else
					{
						UE_LOG(LogSpatialOSInterop, Log, TEXT("%s: Received unresolved object property. Value: %s. actor %s (%llu), property %s (handle %d)"),
							*Interop->GetSpatialOS()->GetWorkerId(),
							*ObjectRefToString(ObjectRef),
							*ActorChannel->Actor->GetName(),
							ActorChannel->GetEntityId(),
							*Data.Property->GetName(),
							Handle);
						bWriteObjectProperty = false;
						Interop->QueueIncomingObjectUpdate_Internal(ObjectRef, ActorChannel, Cast<UObjectPropertyBase>(Data.Property), Handle);
					}
				}
			}

			if (bWriteObjectProperty)
			{
				OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
				UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
					*Interop->GetSpatialOS()->GetWorkerId(),
					*ActorChannel->Actor->GetName(),
					ActorChannel->GetEntityId(),
					*Data.Property->GetName(),
					Handle);
			}
		}
	}
	if (!Update.field_playerstate().empty())
	{
		// field_playerstate
		uint32 Handle = 16;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			bool bWriteObjectProperty = true;
			APlayerState* Value;

			{
				improbable::unreal::UnrealObjectRef ObjectRef = (*Update.field_playerstate().data());
				check(ObjectRef != SpatialConstants::UNRESOLVED_OBJECT_REF);
				if (ObjectRef == SpatialConstants::NULL_OBJECT_REF)
				{
					Value = nullptr;
				}
				else
				{
					FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromUnrealObjectRef(ObjectRef);
					if (NetGUID.IsValid())
					{
						Value = static_cast<APlayerState*>(PackageMap->GetObjectFromNetGUID(NetGUID, true));
					}
					else
					{
						UE_LOG(LogSpatialOSInterop, Log, TEXT("%s: Received unresolved object property. Value: %s. actor %s (%llu), property %s (handle %d)"),
							*Interop->GetSpatialOS()->GetWorkerId(),
							*ObjectRefToString(ObjectRef),
							*ActorChannel->Actor->GetName(),
							ActorChannel->GetEntityId(),
							*Data.Property->GetName(),
							Handle);
						bWriteObjectProperty = false;
						Interop->QueueIncomingObjectUpdate_Internal(ObjectRef, ActorChannel, Cast<UObjectPropertyBase>(Data.Property), Handle);
					}
				}
			}

			if (bWriteObjectProperty)
			{
				OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
				UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
					*Interop->GetSpatialOS()->GetWorkerId(),
					*ActorChannel->Actor->GetName(),
					ActorChannel->GetEntityId(),
					*Data.Property->GetName(),
					Handle);
			}
		}
	}
	if (!Update.field_remoteviewpitch().empty())
	{
		// field_remoteviewpitch
		uint32 Handle = 17;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			uint8 Value;

			Value = uint8(uint8((*Update.field_remoteviewpitch().data())));

			OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
			UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ActorChannel->Actor->GetName(),
				ActorChannel->GetEntityId(),
				*Data.Property->GetName(),
				Handle);
		}
	}
	if (!Update.field_controller().empty())
	{
		// field_controller
		uint32 Handle = 18;
		const FRepHandleData& Data = HandleToPropertyMap[Handle];
		if (ConditionMap.IsRelevant(Data.Condition))
		{
			bool bWriteObjectProperty = true;
			AController* Value;

			{
				improbable::unreal::UnrealObjectRef ObjectRef = (*Update.field_controller().data());
				check(ObjectRef != SpatialConstants::UNRESOLVED_OBJECT_REF);
				if (ObjectRef == SpatialConstants::NULL_OBJECT_REF)
				{
					Value = nullptr;
				}
				else
				{
					FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromUnrealObjectRef(ObjectRef);
					if (NetGUID.IsValid())
					{
						Value = static_cast<AController*>(PackageMap->GetObjectFromNetGUID(NetGUID, true));
					}
					else
					{
						UE_LOG(LogSpatialOSInterop, Log, TEXT("%s: Received unresolved object property. Value: %s. actor %s (%llu), property %s (handle %d)"),
							*Interop->GetSpatialOS()->GetWorkerId(),
							*ObjectRefToString(ObjectRef),
							*ActorChannel->Actor->GetName(),
							ActorChannel->GetEntityId(),
							*Data.Property->GetName(),
							Handle);
						bWriteObjectProperty = false;
						Interop->QueueIncomingObjectUpdate_Internal(ObjectRef, ActorChannel, Cast<UObjectPropertyBase>(Data.Property), Handle);
					}
				}
			}

			if (bWriteObjectProperty)
			{
				OutputWriter.SerializeProperty(Handle, Data.Property, &Value);
				UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received property update. actor %s (%llu), property %s (handle %d)"),
					*Interop->GetSpatialOS()->GetWorkerId(),
					*ActorChannel->Actor->GetName(),
					ActorChannel->GetEntityId(),
					*Data.Property->GetName(),
					Handle);
			}
		}
	}
	Interop->ReceiveSpatialUpdate(ActorChannel, OutputWriter.GetNetBitWriter());
}

void USpatialTypeBinding_WheeledVehicle::ServerUpdateState_SendCommand(worker::Connection* const Connection, struct FFrame* const RPCFrame, UObject* TargetObject)
{
	FFrame& Stack = *RPCFrame;
	P_GET_PROPERTY(UFloatProperty, InSteeringInput);
	P_GET_PROPERTY(UFloatProperty, InThrottleInput);
	P_GET_PROPERTY(UFloatProperty, InBrakeInput);
	P_GET_PROPERTY(UFloatProperty, InHandbrakeInput);
	P_GET_PROPERTY(UIntProperty, CurrentGear);

	auto Sender = [this, Connection, TargetObject, InSteeringInput, InThrottleInput, InBrakeInput, InHandbrakeInput, CurrentGear]() mutable -> FRPCCommandRequestResult
	{
		// Resolve TargetObject.
		improbable::unreal::UnrealObjectRef TargetObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(PackageMap->GetNetGUIDFromObject(TargetObject));
		if (TargetObjectRef == SpatialConstants::UNRESOLVED_OBJECT_REF)
		{
			UE_LOG(LogSpatialOSInterop, Log, TEXT("%s: RPC ServerUpdateState queued. Target object is unresolved."), *Interop->GetSpatialOS()->GetWorkerId());
			return {TargetObject};
		}

		// Build request.
		improbable::unreal::UnrealServerUpdateStateRequest Request;
		Request.set_field_insteeringinput(InSteeringInput);
		Request.set_field_inthrottleinput(InThrottleInput);
		Request.set_field_inbrakeinput(InBrakeInput);
		Request.set_field_inhandbrakeinput(InHandbrakeInput);
		Request.set_field_currentgear(CurrentGear);

		// Send command request.
		Request.set_target_subobject_offset(TargetObjectRef.offset());
		UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Sending RPC: ServerUpdateState, target: %s %s"),
			*Interop->GetSpatialOS()->GetWorkerId(),
			*TargetObject->GetName(),
			*ObjectRefToString(TargetObjectRef));
		auto RequestId = Connection->SendCommandRequest<improbable::unreal::UnrealWheeledVehicleServerRPCs::Commands::Serverupdatestate>(TargetObjectRef.entity(), Request, 0);
		return {RequestId.Id};
	};
	Interop->SendCommandRequest_Internal(Sender, /*bReliable*/ true);
}

void USpatialTypeBinding_WheeledVehicle::ServerUpdateState_OnCommandRequest(const worker::CommandRequestOp<improbable::unreal::UnrealWheeledVehicleServerRPCs::Commands::Serverupdatestate>& Op)
{
	auto Receiver = [this, Op]() mutable -> FRPCCommandResponseResult
	{
		improbable::unreal::UnrealObjectRef TargetObjectRef{Op.EntityId, Op.Request.target_subobject_offset()};
		FNetworkGUID TargetNetGUID = PackageMap->GetNetGUIDFromUnrealObjectRef(TargetObjectRef);
		if (!TargetNetGUID.IsValid())
		{
			UE_LOG(LogSpatialOSInterop, Log, TEXT("%s: ServerUpdateState_OnCommandRequest: Target object %s is not resolved on this worker."),
				*Interop->GetSpatialOS()->GetWorkerId(),
				*ObjectRefToString(TargetObjectRef));
			return {TargetObjectRef};
		}
		UObject* TargetObjectUntyped = PackageMap->GetObjectFromNetGUID(TargetNetGUID, false);
		UWheeledVehicleMovementComponent* TargetObject = Cast<UWheeledVehicleMovementComponent>(TargetObjectUntyped);
		checkf(TargetObjectUntyped, TEXT("%s: ServerUpdateState_OnCommandRequest: Object Ref %s (NetGUID %s) does not correspond to a UObject."),
			*Interop->GetSpatialOS()->GetWorkerId(),
			*ObjectRefToString(TargetObjectRef),
			*TargetNetGUID.ToString());
		checkf(TargetObject, TEXT("%s: ServerUpdateState_OnCommandRequest: Object Ref %s (NetGUID %s) is the wrong type. Name: %s"),
			*Interop->GetSpatialOS()->GetWorkerId(),
			*ObjectRefToString(TargetObjectRef),
			*TargetNetGUID.ToString(),
			*TargetObjectUntyped->GetName());

		// Extract InSteeringInput
		float InSteeringInput;
		InSteeringInput = Op.Request.field_insteeringinput();

		// Extract InThrottleInput
		float InThrottleInput;
		InThrottleInput = Op.Request.field_inthrottleinput();

		// Extract InBrakeInput
		float InBrakeInput;
		InBrakeInput = Op.Request.field_inbrakeinput();

		// Extract InHandbrakeInput
		float InHandbrakeInput;
		InHandbrakeInput = Op.Request.field_inhandbrakeinput();

		// Extract CurrentGear
		int32 CurrentGear;
		CurrentGear = Op.Request.field_currentgear();

		// Call implementation.
		UE_LOG(LogSpatialOSInterop, Verbose, TEXT("%s: Received RPC: ServerUpdateState, target: %s %s"),
			*Interop->GetSpatialOS()->GetWorkerId(),
			*TargetObject->GetName(),
			*ObjectRefToString(TargetObjectRef));
		TargetObject->ServerUpdateState_Implementation(InSteeringInput, InThrottleInput, InBrakeInput, InHandbrakeInput, CurrentGear);

		// Send command response.
		TSharedPtr<worker::Connection> Connection = Interop->GetSpatialOS()->GetConnection().Pin();
		Connection->SendCommandResponse<improbable::unreal::UnrealWheeledVehicleServerRPCs::Commands::Serverupdatestate>(Op.RequestId, {});
		return {};
	};
	Interop->SendCommandResponse_Internal(Receiver);
}

void USpatialTypeBinding_WheeledVehicle::ServerUpdateState_OnCommandResponse(const worker::CommandResponseOp<improbable::unreal::UnrealWheeledVehicleServerRPCs::Commands::Serverupdatestate>& Op)
{
	Interop->HandleCommandResponse_Internal(TEXT("ServerUpdateState"), Op.RequestId.Id, Op.EntityId, Op.StatusCode, FString(UTF8_TO_TCHAR(Op.Message.c_str())));
}
