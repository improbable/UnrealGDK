// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Utils/ComponentFactory.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "UObject/TextProperty.h"

#include "EngineClasses/SpatialActorChannel.h"
#include "EngineClasses/SpatialFastArrayNetSerialize.h"
#include "EngineClasses/SpatialNetBitWriter.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "Net/NetworkProfiler.h"
#include "Schema/Interest.h"
#include "SpatialConstants.h"
#include "Utils/InterestFactory.h"
#include "Utils/RepLayoutUtils.h"
#include "Utils/SpatialLatencyTracer.h"

DEFINE_LOG_CATEGORY(LogComponentFactory);

DECLARE_CYCLE_STAT(TEXT("Factory ProcessPropertyUpdates"), STAT_FactoryProcessPropertyUpdates, STATGROUP_SpatialNet);
DECLARE_CYCLE_STAT(TEXT("Factory ProcessFastArrayUpdate"), STAT_FactoryProcessFastArrayUpdate, STATGROUP_SpatialNet);

namespace
{
	template<typename T>
	TraceKey* GetTraceKeyFromComponentObject(T& Obj)
	{
#if TRACE_LIB_ACTIVE
		return &Obj.Trace;
#else
		return nullptr;
#endif
	}
}
namespace SpatialGDK
{

ComponentFactory::ComponentFactory(bool bInterestDirty, USpatialNetDriver* InNetDriver, USpatialLatencyTracer* InLatencyTracer)
	: NetDriver(InNetDriver)
	, PackageMap(InNetDriver->PackageMap)
	, ClassInfoManager(InNetDriver->ClassInfoManager)
	, bInterestHasChanged(bInterestDirty)
	, LatencyTracer(InLatencyTracer)
{ }

bool ComponentFactory::FillSchemaObject(Schema_Object* ComponentObject, UObject* Object, const FRepChangeState& Changes, ESchemaComponentType PropertyGroup, bool bIsInitialData, TraceKey* OutLatencyTraceId, TArray<Schema_FieldId>* ClearedIds /*= nullptr*/)
{
	SCOPE_CYCLE_COUNTER(STAT_FactoryProcessPropertyUpdates);

	bool bWroteSomething = false;

	// Populate the replicated data component updates from the replicated property changelist.
	if (Changes.RepChanged.Num() > 0)
	{
		FChangelistIterator ChangelistIterator(Changes.RepChanged, 0);
#if ENGINE_MINOR_VERSION <= 22
		FRepHandleIterator HandleIterator(ChangelistIterator, Changes.RepLayout.Cmds, Changes.RepLayout.BaseHandleToCmdIndex, 0, 1, 0, Changes.RepLayout.Cmds.Num() - 1);
#else
		FRepHandleIterator HandleIterator(static_cast<UStruct*>(Changes.RepLayout.GetOwner()), ChangelistIterator, Changes.RepLayout.Cmds, Changes.RepLayout.BaseHandleToCmdIndex, 0, 1, 0, Changes.RepLayout.Cmds.Num() - 1);
#endif
		while (HandleIterator.NextHandle())
		{
			const FRepLayoutCmd& Cmd = Changes.RepLayout.Cmds[HandleIterator.CmdIndex];
			const FRepParentCmd& Parent = Changes.RepLayout.Parents[Cmd.ParentIndex];

#if TRACE_LIB_ACTIVE
			if (LatencyTracer != nullptr && OutLatencyTraceId != nullptr)
			{
				TraceKey PropertyKey = InvalidTraceKey;
				PropertyKey = LatencyTracer->RetrievePendingTrace(Object, Cmd.Property);
				if (PropertyKey == InvalidTraceKey)
				{
					// Check for sending a nested property
					PropertyKey = LatencyTracer->RetrievePendingTrace(Object, Parent.Property);
				}
				if (PropertyKey != InvalidTraceKey)
				{
					// If we have already got a trace for this actor/component, we will end one of them here
					if (*OutLatencyTraceId != InvalidTraceKey)
					{
						UE_LOG(LogComponentFactory, Warning, TEXT("%s property trace being dropped because too many active on this actor (%s)"), *Cmd.Property->GetName(), *Object->GetName());
						LatencyTracer->EndLatencyTrace(*OutLatencyTraceId, TEXT("Multiple actor component traces not supported"));
					}
					*OutLatencyTraceId = PropertyKey;
				}
			}
#endif
			if (GetGroupFromCondition(Parent.Condition) == PropertyGroup)
			{
				const uint8* Data = (uint8*)Object + Cmd.Offset;

				bool bProcessedFastArrayProperty = false;
#if USE_NETWORK_PROFILER
				const uint32 NumBytesStart = Schema_GetWriteBufferLength(ComponentObject);
#endif
				if (Cmd.Type == ERepLayoutCmdType::DynamicArray)
				{
					UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Cmd.Property);

					// Check if this is a FastArraySerializer array and if so, call our custom delta serialization
					if (UScriptStruct* NetDeltaStruct = GetFastArraySerializerProperty(ArrayProperty))
					{
						SCOPE_CYCLE_COUNTER(STAT_FactoryProcessFastArrayUpdate);

						FSpatialNetBitWriter ValueDataWriter(PackageMap);

						if (FSpatialNetDeltaSerializeInfo::DeltaSerializeWrite(NetDriver, ValueDataWriter, Object, Parent.ArrayIndex, Parent.Property, NetDeltaStruct) || bIsInitialData)
						{
							AddBytesToSchema(ComponentObject, HandleIterator.Handle, ValueDataWriter);
						}

						bProcessedFastArrayProperty = true;
					}
				}

				if (!bProcessedFastArrayProperty)
				{
					AddProperty(ComponentObject, HandleIterator.Handle, Cmd.Property, Data, ClearedIds);
				}

				bWroteSomething = true;
#if USE_NETWORK_PROFILER
				/**
				 *  a good proxy for how many bits are being sent for a propery. Reasons for why it might not be fully accurate:
						- the serialized size of a message is just the body contents. Typically something will send the message with the length prefixed, which might be varint encoded, and you pushing the size over some size can cause the encoding of the length be bigger
						- similarly, if you push the message over some size it can cause fragmentation which means you now have to pay for the headers again
						- if there is any compression or anything else going on, the number of bytes actually transferred because of this data can differ
						- lastly somewhat philosophical question of who pays for the overhead of a packet and whether you attribute a part of it to each field or attribute it to the update itself, but I assume you care a bit less about this
				 */
				const uint32 NumBytesEnd = Schema_GetWriteBufferLength(ComponentObject);
				NETWORK_PROFILER(GNetworkProfiler.TrackReplicateProperty(Cmd.Property, (NumBytesEnd - NumBytesStart) * CHAR_BIT, nullptr));
#endif				
			}

			if (Cmd.Type == ERepLayoutCmdType::DynamicArray)
			{
				if (!HandleIterator.JumpOverArray())
				{
					break;
				}
			}
		}
	}

	return bWroteSomething;
}

bool ComponentFactory::FillHandoverSchemaObject(Schema_Object* ComponentObject, UObject* Object, const FClassInfo& Info, const FHandoverChangeState& Changes, bool bIsInitialData, TraceKey* OutLatencyTraceId, TArray<Schema_FieldId>* ClearedIds /* = nullptr */)
{
	bool bWroteSomething = false;

	for (uint16 ChangedHandle : Changes)
	{
		check(ChangedHandle > 0 && ChangedHandle - 1 < Info.HandoverProperties.Num());
		const FHandoverPropertyInfo& PropertyInfo = Info.HandoverProperties[ChangedHandle - 1];

		const uint8* Data = (uint8*)Object + PropertyInfo.Offset;

#if TRACE_LIB_ACTIVE
		if (LatencyTracer != nullptr && OutLatencyTraceId != nullptr)
		{
			// If we have already got a trace for this actor/component, we will end one of them here
			if (*OutLatencyTraceId != InvalidTraceKey)
			{
				UE_LOG(LogComponentFactory, Warning, TEXT("%s handover trace being dropped because too many active on this actor (%s)"), *PropertyInfo.Property->GetName(), *Object->GetName());
				LatencyTracer->EndLatencyTrace(*OutLatencyTraceId, TEXT("Multiple actor component traces not supported"));
			}
			*OutLatencyTraceId = LatencyTracer->RetrievePendingTrace(Object, PropertyInfo.Property);
		}
#endif
		AddProperty(ComponentObject, ChangedHandle, PropertyInfo.Property, Data, ClearedIds);

		bWroteSomething = true;
	}

	return bWroteSomething;
}

void ComponentFactory::AddProperty(Schema_Object* Object, Schema_FieldId FieldId, UProperty* Property, const uint8* Data, TArray<Schema_FieldId>* ClearedIds)
{
	if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProperty->Struct;
		FSpatialNetBitWriter ValueDataWriter(PackageMap);
		bool bHasUnmapped = false;

		if (Struct->StructFlags & STRUCT_NetSerializeNative)
		{
			UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
			check(CppStructOps); // else should not have STRUCT_NetSerializeNative
			bool bSuccess = true;
			if (!CppStructOps->NetSerialize(ValueDataWriter, PackageMap, bSuccess, const_cast<uint8*>(Data)))
			{
				bHasUnmapped = true;
			}

			// Check the success of the serialization and print a warning if it failed. This is how native handles failed serialization.
			if (!bSuccess)
			{
				UE_LOG(LogComponentFactory, Warning, TEXT("AddProperty: NetSerialize %s failed."), *Struct->GetFullName());
				return;
			}
		}
		else
		{
			TSharedPtr<FRepLayout> RepLayout = NetDriver->GetStructRepLayout(Struct);

			RepLayout_SerializePropertiesForStruct(*RepLayout, ValueDataWriter, PackageMap, const_cast<uint8*>(Data), bHasUnmapped);
		}

		AddBytesToSchema(Object, FieldId, ValueDataWriter);
	}
	else if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
	{
		Schema_AddBool(Object, FieldId, (uint8)BoolProperty->GetPropertyValue(Data));
	}
	else if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(Property))
	{
		Schema_AddFloat(Object, FieldId, FloatProperty->GetPropertyValue(Data));
	}
	else if (UDoubleProperty* DoubleProperty = Cast<UDoubleProperty>(Property))
	{
		Schema_AddDouble(Object, FieldId, DoubleProperty->GetPropertyValue(Data));
	}
	else if (UInt8Property* Int8Property = Cast<UInt8Property>(Property))
	{
		Schema_AddInt32(Object, FieldId, (int32)Int8Property->GetPropertyValue(Data));
	}
	else if (UInt16Property* Int16Property = Cast<UInt16Property>(Property))
	{
		Schema_AddInt32(Object, FieldId, (int32)Int16Property->GetPropertyValue(Data));
	}
	else if (UIntProperty* IntProperty = Cast<UIntProperty>(Property))
	{
		Schema_AddInt32(Object, FieldId, IntProperty->GetPropertyValue(Data));
	}
	else if (UInt64Property* Int64Property = Cast<UInt64Property>(Property))
	{
		Schema_AddInt64(Object, FieldId, Int64Property->GetPropertyValue(Data));
	}
	else if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
	{
		Schema_AddUint32(Object, FieldId, (uint32)ByteProperty->GetPropertyValue(Data));
	}
	else if (UUInt16Property* UInt16Property = Cast<UUInt16Property>(Property))
	{
		Schema_AddUint32(Object, FieldId, (uint32)UInt16Property->GetPropertyValue(Data));
	}
	else if (UUInt32Property* UInt32Property = Cast<UUInt32Property>(Property))
	{
		Schema_AddUint32(Object, FieldId, UInt32Property->GetPropertyValue(Data));
	}
	else if (UUInt64Property* UInt64Property = Cast<UUInt64Property>(Property))
	{
		Schema_AddUint64(Object, FieldId, UInt64Property->GetPropertyValue(Data));
	}
	else if (UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property))
	{
		if (Cast<USoftObjectProperty>(Property))
		{
			const FSoftObjectPtr* ObjectPtr = reinterpret_cast<const FSoftObjectPtr*>(Data);

			AddObjectRefToSchema(Object, FieldId, FUnrealObjectRef::FromSoftObjectPath(ObjectPtr->ToSoftObjectPath()));
		}
		else
		{
			UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(Data);

			if (ObjectProperty->PropertyFlags & CPF_AlwaysInterested)
			{
				bInterestHasChanged = true;
			}
			AddObjectRefToSchema(Object, FieldId, FUnrealObjectRef::FromObjectPtr(ObjectValue, PackageMap));
		}
	}
	else if (UNameProperty* NameProperty = Cast<UNameProperty>(Property))
	{
		AddStringToSchema(Object, FieldId, NameProperty->GetPropertyValue(Data).ToString());
	}
	else if (UStrProperty* StrProperty = Cast<UStrProperty>(Property))
	{
		AddStringToSchema(Object, FieldId, StrProperty->GetPropertyValue(Data));
	}
	else if (UTextProperty* TextProperty = Cast<UTextProperty>(Property))
	{
		AddStringToSchema(Object, FieldId, TextProperty->GetPropertyValue(Data).ToString());
	}
	else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, Data);
		for (int i = 0; i < ArrayHelper.Num(); i++)
		{
			AddProperty(Object, FieldId, ArrayProperty->Inner, ArrayHelper.GetRawPtr(i), ClearedIds);
		}

		if (ArrayHelper.Num() == 0 && ClearedIds)
		{
			ClearedIds->Add(FieldId);
		}
	}
	else if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		if (EnumProperty->ElementSize < 4)
		{
			Schema_AddUint32(Object, FieldId, (uint32)EnumProperty->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Data));
		}
		else
		{
			AddProperty(Object, FieldId, EnumProperty->GetUnderlyingProperty(), Data, ClearedIds);
		}
	}
	else if (Property->IsA<UDelegateProperty>() || Property->IsA<UMulticastDelegateProperty>() || Property->IsA<UInterfaceProperty>())
	{
		// These properties can be set to replicate, but won't serialize across the network.
	}
	else if (Property->IsA<UMapProperty>())
	{
		UE_LOG(LogComponentFactory, Error, TEXT("Class %s with name %s in field %d: Replicated TMaps are not supported."), *Property->GetClass()->GetName(), *Property->GetName(), FieldId);
	}
	else if (Property->IsA<USetProperty>())
	{
		UE_LOG(LogComponentFactory, Error, TEXT("Class %s with name %s in field %d: Replicated TSets are not supported."), *Property->GetClass()->GetName(), *Property->GetName(), FieldId);
	}
	else
	{
		UE_LOG(LogComponentFactory, Error, TEXT("Class %s with name %s in field %d: Attempted to add unknown property type."), *Property->GetClass()->GetName(), *Property->GetName(), FieldId);
	}
}

TArray<FWorkerComponentData> ComponentFactory::CreateComponentDatas(UObject* Object, const FClassInfo& Info, const FRepChangeState& RepChangeState, const FHandoverChangeState& HandoverChangeState)
{
	TArray<FWorkerComponentData> ComponentDatas;

	if (Info.SchemaComponents[SCHEMA_Data] != SpatialConstants::INVALID_COMPONENT_ID)
	{
		ComponentDatas.Add(CreateComponentData(Info.SchemaComponents[SCHEMA_Data], Object, RepChangeState, SCHEMA_Data));
	}

	if (Info.SchemaComponents[SCHEMA_OwnerOnly] != SpatialConstants::INVALID_COMPONENT_ID)
	{
		ComponentDatas.Add(CreateComponentData(Info.SchemaComponents[SCHEMA_OwnerOnly], Object, RepChangeState, SCHEMA_OwnerOnly));
	}

	if (Info.SchemaComponents[SCHEMA_Handover] != SpatialConstants::INVALID_COMPONENT_ID)
	{
		ComponentDatas.Add(CreateHandoverComponentData(Info.SchemaComponents[SCHEMA_Handover], Object, Info, HandoverChangeState));
	}

	return ComponentDatas;
}

FWorkerComponentData ComponentFactory::CreateComponentData(Worker_ComponentId ComponentId, UObject* Object, const FRepChangeState& Changes, ESchemaComponentType PropertyGroup)
{
	FWorkerComponentData ComponentData = {};
	ComponentData.component_id = ComponentId;
	ComponentData.schema_type = Schema_CreateComponentData();
	Schema_Object* ComponentObject = Schema_GetComponentDataFields(ComponentData.schema_type);

	// We're currently ignoring ClearedId fields, which is problematic if the initial replicated state
	// is different to what the default state is (the client will have the incorrect data). UNR:959
	FillSchemaObject(ComponentObject, Object, Changes, PropertyGroup, true, GetTraceKeyFromComponentObject(ComponentData));

	return ComponentData;
}

FWorkerComponentData ComponentFactory::CreateEmptyComponentData(Worker_ComponentId ComponentId)
{
	FWorkerComponentData ComponentData = {};
	ComponentData.component_id = ComponentId;
	ComponentData.schema_type = Schema_CreateComponentData();

	return ComponentData;
}

FWorkerComponentData ComponentFactory::CreateHandoverComponentData(Worker_ComponentId ComponentId, UObject* Object, const FClassInfo& Info, const FHandoverChangeState& Changes)
{
	FWorkerComponentData ComponentData = CreateEmptyComponentData(ComponentId);
	Schema_Object* ComponentObject = Schema_GetComponentDataFields(ComponentData.schema_type);

	FillHandoverSchemaObject(ComponentObject, Object, Info, Changes, true, GetTraceKeyFromComponentObject(ComponentData));

	return ComponentData;
}

TArray<FWorkerComponentUpdate> ComponentFactory::CreateComponentUpdates(UObject* Object, const FClassInfo& Info, Worker_EntityId EntityId, const FRepChangeState* RepChangeState, const FHandoverChangeState* HandoverChangeState)
{
	TArray<FWorkerComponentUpdate> ComponentUpdates;

	if (RepChangeState)
	{
		if (Info.SchemaComponents[SCHEMA_Data] != SpatialConstants::INVALID_COMPONENT_ID)
		{
			bool bWroteSomething = false;
			FWorkerComponentUpdate MultiClientUpdate = CreateComponentUpdate(Info.SchemaComponents[SCHEMA_Data], Object, *RepChangeState, SCHEMA_Data, bWroteSomething);
			if (bWroteSomething)
			{
				ComponentUpdates.Add(MultiClientUpdate);
			}
		}

		if (Info.SchemaComponents[SCHEMA_OwnerOnly] != SpatialConstants::INVALID_COMPONENT_ID)
		{
			bool bWroteSomething = false;
			FWorkerComponentUpdate SingleClientUpdate = CreateComponentUpdate(Info.SchemaComponents[SCHEMA_OwnerOnly], Object, *RepChangeState, SCHEMA_OwnerOnly, bWroteSomething);
			if (bWroteSomething)
			{
				ComponentUpdates.Add(SingleClientUpdate);
			}
		}
	}

	if (HandoverChangeState)
	{
		if (Info.SchemaComponents[SCHEMA_Handover] != SpatialConstants::INVALID_COMPONENT_ID)
		{
			bool bWroteSomething = false;
			FWorkerComponentUpdate HandoverUpdate = CreateHandoverComponentUpdate(Info.SchemaComponents[SCHEMA_Handover], Object, Info, *HandoverChangeState, bWroteSomething);
			if (bWroteSomething)
			{
				ComponentUpdates.Add(HandoverUpdate);
			}
		}
	}

	// Only support Interest for Actors for now.
	if (Object->IsA<AActor>() && bInterestHasChanged)
	{
		InterestFactory InterestUpdateFactory(Cast<AActor>(Object), Info, EntityId, NetDriver->ClassInfoManager, NetDriver->PackageMap);
		ComponentUpdates.Add(InterestUpdateFactory.CreateInterestUpdate());
	}

	return ComponentUpdates;
}

FWorkerComponentUpdate ComponentFactory::CreateComponentUpdate(Worker_ComponentId ComponentId, UObject* Object, const FRepChangeState& Changes, ESchemaComponentType PropertyGroup, bool& bWroteSomething)
{
	FWorkerComponentUpdate ComponentUpdate = {};

	ComponentUpdate.component_id = ComponentId;
	ComponentUpdate.schema_type = Schema_CreateComponentUpdate();
	Schema_Object* ComponentObject = Schema_GetComponentUpdateFields(ComponentUpdate.schema_type);

	TArray<Schema_FieldId> ClearedIds;

	bWroteSomething = FillSchemaObject(ComponentObject, Object, Changes, PropertyGroup, false, GetTraceKeyFromComponentObject(ComponentUpdate), &ClearedIds);

	for (Schema_FieldId Id : ClearedIds)
	{
		Schema_AddComponentUpdateClearedField(ComponentUpdate.schema_type, Id);
	}

	if (!bWroteSomething)
	{
		Schema_DestroyComponentUpdate(ComponentUpdate.schema_type);
	}

	return ComponentUpdate;
}

FWorkerComponentUpdate ComponentFactory::CreateHandoverComponentUpdate(Worker_ComponentId ComponentId, UObject* Object, const FClassInfo& Info, const FHandoverChangeState& Changes, bool& bWroteSomething)
{
	FWorkerComponentUpdate ComponentUpdate = {};

	ComponentUpdate.component_id = ComponentId;
	ComponentUpdate.schema_type = Schema_CreateComponentUpdate();
	Schema_Object* ComponentObject = Schema_GetComponentUpdateFields(ComponentUpdate.schema_type);

	TArray<Schema_FieldId> ClearedIds;

	bWroteSomething = FillHandoverSchemaObject(ComponentObject, Object, Info, Changes, false, GetTraceKeyFromComponentObject(ComponentUpdate), &ClearedIds);

	for (Schema_FieldId Id : ClearedIds)
	{
		Schema_AddComponentUpdateClearedField(ComponentUpdate.schema_type, Id);
	}

	if (!bWroteSomething)
	{
		Schema_DestroyComponentUpdate(ComponentUpdate.schema_type);
	}

	return ComponentUpdate;
}

} // namespace SpatialGDK
