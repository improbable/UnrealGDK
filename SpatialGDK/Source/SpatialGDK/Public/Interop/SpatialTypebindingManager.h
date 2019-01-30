// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Utils/SchemaDatabase.h"

#include <WorkerSDK/improbable/c_worker.h>

#include "SpatialTypebindingManager.generated.h"

FORCEINLINE void ForAllSchemaComponentTypes(TFunction<void(ESchemaComponentType)> Callback)
{
	for (int32 Type = SCHEMA_Begin; Type < SCHEMA_Count; Type++)
	{
		Callback(ESchemaComponentType(Type));
	}
}

FORCEINLINE ESchemaComponentType GetGroupFromCondition(ELifetimeCondition Condition)
{
	switch (Condition)
	{
	case COND_AutonomousOnly:
	case COND_OwnerOnly:
		return SCHEMA_OwnerOnly;
	default:
		return SCHEMA_Data;
	}
}

struct FRPCInfo
{
	ESchemaComponentType Type;
	uint32 Index;
};

struct FHandoverPropertyInfo
{
	uint16 Handle;
	int32 Offset;
	int32 ArrayIdx;
	UProperty* Property;
};

struct FInterestPropertyInfo
{
	UProperty* Property;
	int32 Offset;
};

USTRUCT()
struct FClassInfo
{
	GENERATED_BODY()

	TWeakObjectPtr<UClass> Class;

	TMap<ESchemaComponentType, TArray<UFunction*>> RPCs;
	TMap<UFunction*, FRPCInfo> RPCInfoMap;

	TArray<FHandoverPropertyInfo> HandoverProperties;
	TArray<FInterestPropertyInfo> InterestProperties;

	Worker_ComponentId SchemaComponents[ESchemaComponentType::SCHEMA_Count] = {};

	FName SubobjectName;

	TMap<uint32, TSharedRef<FClassInfo>> SubobjectInfo;
};

class USpatialNetDriver;

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialTypebindingManager, Log, All)

UCLASS()
class SPATIALGDK_API USpatialTypebindingManager : public UObject
{
	GENERATED_BODY()

public:
	void Init(USpatialNetDriver* NetDriver);

	// Returns true if the class path corresponds to an Actor or Subobject class path in SchemaDatabase
	bool IsSupportedClass(UClass* Class) const;

	FClassInfo& FindClassInfoByClass(UClass* Class);
	FClassInfo* FindClassInfoByActorClassAndOffset(UClass* Class, uint32 Offset);

	// Object should be an Actor or a Subobject with a valid UnrealObjectRef
	FClassInfo* FindClassInfoByObject(UObject* Object);

	FClassInfo* FindClassInfoByComponentId(Worker_ComponentId ComponentId);
	UClass* FindClassByComponentId(Worker_ComponentId ComponentId);
	bool FindOffsetByComponentId(Worker_ComponentId ComponentId, uint32& OutOffset);
	ESchemaComponentType FindCategoryByComponentId(Worker_ComponentId ComponentId);

private:
	void AddTypebindingsForClass(UClass* Class);
	UClass* LoadClassForComponent(Worker_ComponentId ComponentId);

private:
	UPROPERTY()
	USpatialNetDriver* NetDriver;

	UPROPERTY()
	USchemaDatabase* SchemaDatabase;

	TMap<TWeakObjectPtr<UClass>, TSharedRef<FClassInfo>> ClassInfoMap;

	TMap<Worker_ComponentId, TSharedRef<FClassInfo>> ComponentToClassInfoMap;
	TMap<Worker_ComponentId, uint32> ComponentToOffsetMap;
	TMap<Worker_ComponentId, ESchemaComponentType> ComponentToCategoryMap;
};
