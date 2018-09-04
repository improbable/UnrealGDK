// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "EngineClasses/SpatialNetBitWriter.h"
#include "Schema/UnrealObjectRef.h"

#include <improbable/c_schema.h>
#include <improbable/c_worker.h>

inline void Schema_AddString(Schema_Object* Object, Schema_FieldId Id, const FString& Value)
{
	FTCHARToUTF8 CStrConvertion(*Value);
	std::uint32_t StringLength = CStrConvertion.Length();
	std::uint8_t* StringBuffer = Schema_AllocateBuffer(Object, sizeof(char) * StringLength);
	FMemory::Memcpy(StringBuffer, CStrConvertion.Get(), sizeof(char) * StringLength);
	Schema_AddBytes(Object, Id, StringBuffer, sizeof(char) * StringLength);
}

inline FString Schema_IndexString(const Schema_Object* Object, Schema_FieldId Id, std::uint32_t Index)
{
	int32 StringLength = (int32)Schema_IndexBytesLength(Object, Id, Index);
	return FString(StringLength, UTF8_TO_TCHAR(Schema_IndexBytes(Object, Id, Index)));
}

inline FString Schema_GetString(const Schema_Object* Object, Schema_FieldId Id)
{
	return Schema_IndexString(Object, Id, 0);
}

inline void Schema_AddPayload(Schema_Object* Object, Schema_FieldId Id, FSpatialNetBitWriter& Writer)
{
	std::uint32_t PayloadSize = Writer.GetNumBytes();
	std::uint8_t* PayloadBuffer = Schema_AllocateBuffer(Object, sizeof(char) * PayloadSize);
	FMemory::Memcpy(PayloadBuffer, Writer.GetData(), sizeof(char) * PayloadSize);
	Schema_AddBytes(Object, Id, PayloadBuffer, sizeof(char) * PayloadSize);
}

inline TArray<uint8> Schema_IndexPayload(const Schema_Object* Object, Schema_FieldId Id, std::uint32_t Index)
{
	int32 PayloadSize = (int32)Schema_IndexBytesLength(Object, Id, Index);
	return TArray<uint8>((const uint8*)Schema_IndexBytes(Object, Id, Index), PayloadSize);
}

inline TArray<uint8> Schema_GetPayload(const Schema_Object* Object, Schema_FieldId Id)
{
	return Schema_IndexPayload(Object, Id, 0);
}

using WorkerAttributeSet = TArray<FString>;
using WorkerRequirementSet = TArray<WorkerAttributeSet>;

inline void Schema_AddWorkerRequirementSet(Schema_Object* Object, Schema_FieldId Id, const WorkerRequirementSet& Value)
{
	Schema_Object* RequirementSetObject = Schema_AddObject(Object, Id);
	for (const WorkerAttributeSet& AttributeSet : Value)
	{
		Schema_Object* AttributeSetObject = Schema_AddObject(RequirementSetObject, 1);

		for (const FString& Attribute : AttributeSet)
		{
			Schema_AddString(AttributeSetObject, 1, Attribute);
		}
	}
}

inline WorkerRequirementSet Schema_GetWorkerRequirementSet(Schema_Object* Object, Schema_FieldId Id)
{
	Schema_Object* RequirementSetObject = Schema_GetObject(Object, Id);

	int32 AttributeSetCount = (int32)Schema_GetObjectCount(RequirementSetObject, 1);
	WorkerRequirementSet RequirementSet;
	RequirementSet.Reserve(AttributeSetCount);

	for (int32 i = 0; i < AttributeSetCount; i++)
	{
		Schema_Object* AttributeSetObject = Schema_IndexObject(RequirementSetObject, 1, i);

		int32 AttributeCount = (int32)Schema_GetBytesCount(AttributeSetObject, 1);
		WorkerAttributeSet AttributeSet;
		AttributeSet.Reserve(AttributeCount);

		for (int32 j = 0; j < AttributeCount; j++)
		{
			AttributeSet.Add(Schema_IndexString(AttributeSetObject, 1, j));
		}

		RequirementSet.Add(AttributeSet);
	}

	return RequirementSet;
}

inline void Schema_AddObjectRef(Schema_Object* Object, Schema_FieldId Id, const UnrealObjectRef& ObjectRef)
{
	Schema_Object* ObjectRefObject = Schema_AddObject(Object, Id);

	Schema_AddEntityId(ObjectRefObject, 1, ObjectRef.Entity);
	Schema_AddUint32(ObjectRefObject, 2, ObjectRef.Offset);
	if (ObjectRef.Path)
	{
		Schema_AddString(ObjectRefObject, 3, *ObjectRef.Path);
	}
	if (ObjectRef.Outer)
	{
		Schema_AddObjectRef(ObjectRefObject, 4, *ObjectRef.Outer);
	}
}

UnrealObjectRef Schema_GetObjectRef(Schema_Object* Object, Schema_FieldId Id);

inline UnrealObjectRef Schema_IndexObjectRef(Schema_Object* Object, Schema_FieldId Id, std::uint32_t Index)
{
	UnrealObjectRef ObjectRef;

	Schema_Object* ObjectRefObject = Schema_IndexObject(Object, Id, Index);

	ObjectRef.Entity = Schema_GetEntityId(ObjectRefObject, 1);
	ObjectRef.Offset = Schema_GetUint32(ObjectRefObject, 2);
	if (Schema_GetBytesCount(ObjectRefObject, 3) > 0)
	{
		ObjectRef.Path = Schema_GetString(ObjectRefObject, 3);
	}
	if (Schema_GetObjectCount(ObjectRefObject, 4) > 0)
	{
		ObjectRef.Outer = UnrealObjectRef(Schema_GetObjectRef(ObjectRefObject, 4));
	}

	return ObjectRef;
}

inline UnrealObjectRef Schema_GetObjectRef(Schema_Object* Object, Schema_FieldId Id)
{
	return Schema_IndexObjectRef(Object, Id, 0);
}

using StringToEntityMap = TMap<FString, Worker_EntityId>;

inline StringToEntityMap Schema_GetStringToEntityMap(Schema_Object* Object, Schema_FieldId Id)
{
	StringToEntityMap Map;

	int32 MapCount = (int32)Schema_GetObjectCount(Object, Id);
	for (int32 i = 0; i < MapCount; i++)
	{
		Schema_Object* PairObject = Schema_IndexObject(Object, Id, i);

		FString String = Schema_GetString(PairObject, SCHEMA_MAP_KEY_FIELD_ID);
		Worker_EntityId Entity = Schema_GetEntityId(PairObject, SCHEMA_MAP_VALUE_FIELD_ID);

		Map.Add(String, Entity);
	}

	return Map;
}

inline void Schema_AddStringToEntityMap(Schema_Object* Object, Schema_FieldId Id, StringToEntityMap& Map)
{
	if (Map.Num() == 0)
	{
		return;
	}

	for (auto& Pair : Map)
	{
		Schema_Object* PairObject = Schema_AddObject(Object, 1);
		Schema_AddString(PairObject, SCHEMA_MAP_KEY_FIELD_ID, Pair.Key);
		Schema_AddEntityId(PairObject, SCHEMA_MAP_VALUE_FIELD_ID, Pair.Value);
	}
}
