// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreNet.h"

#include "Schema/UnrealObjectRef.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialNetBitReader, All, All);

class USpatialPackageMapClient;

class SPATIALGDK_API FSpatialNetBitReader : public FNetBitReader
{
public:
	FSpatialNetBitReader(USpatialPackageMapClient* InPackageMap, uint8* Source, int64 CountBits, TSet<FUnrealObjectRef>& InDynamicRefs, TSet<FUnrealObjectRef>& InUnresolvedRefs);

	~FSpatialNetBitReader();

	using FArchive::operator<<; // For visibility of the overloads we don't override

	static UObject* ReadObject(FArchive& Archive, USpatialPackageMapClient* PackageMap, bool& bUnresolved);

	FArchive& operator<<(FWeakObjectPtr& Value);

protected:
	static void DeserializeObjectRef(FArchive& Archive, FUnrealObjectRef& ObjectRef);

	TSet<FUnrealObjectRef>& DynamicRefs;
	TSet<FUnrealObjectRef>& UnresolvedRefs;
};
