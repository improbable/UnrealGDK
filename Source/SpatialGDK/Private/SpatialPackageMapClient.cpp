// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialPackageMapClient.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "EntityRegistry.h"
#include "SpatialActorChannel.h"
#include "SpatialConstants.h"
#include "SpatialInterop.h"
#include "SpatialNetDriver.h"
#include "SpatialTypeBinding.h"

DEFINE_LOG_CATEGORY(LogSpatialOSPackageMap);

void GetSubobjects(UObject* Object, TArray<UObject*>& InSubobjects)
{
	InSubobjects.Empty();
	ForEachObjectWithOuter(Object, [&InSubobjects](UObject* Object)
	{
		// Objects can only be allocated NetGUIDs if this is true.
		if (Object->IsSupportedForNetworking() && !Object->IsPendingKill() && !Object->IsEditorOnly())
		{
			InSubobjects.Add(Object);
		}
	});

	InSubobjects.StableSort([](UObject& A, UObject& B)
	{
		return A.GetName() < B.GetName();
	});
}

FNetworkGUID USpatialPackageMapClient::ResolveEntityActor(AActor* Actor, FEntityId EntityId, const SubobjectToOffsetMap& SubobjectToOffset)
{
	FSpatialNetGUIDCache* SpatialGuidCache = static_cast<FSpatialNetGUIDCache*>(GuidCache.Get());
	FNetworkGUID NetGUID = SpatialGuidCache->GetNetGUIDFromEntityId(EntityId.ToSpatialEntityId());

	// check we haven't already assigned a NetGUID to this object
	if (!NetGUID.IsValid())
	{
		NetGUID = SpatialGuidCache->AssignNewEntityActorNetGUID(Actor, SubobjectToOffset);
	}
	return NetGUID;
}

void USpatialPackageMapClient::RemoveEntityActor(const FEntityId& EntityId)
{
	FSpatialNetGUIDCache* SpatialGuidCache = static_cast<FSpatialNetGUIDCache*>(GuidCache.Get());
	if (SpatialGuidCache->GetNetGUIDFromEntityId(EntityId.ToSpatialEntityId()).IsValid())
	{
		SpatialGuidCache->RemoveEntityNetGUID(EntityId.ToSpatialEntityId());
	}
}

bool USpatialPackageMapClient::SerializeNewActor(FArchive & Ar, UActorChannel * Channel, AActor *& Actor)
{
	bool bResult = Super::SerializeNewActor(Ar, Channel, Actor);
	//will remove the override if remains unused.
	return bResult;
}

improbable::unreal::UnrealObjectRef USpatialPackageMapClient::GetUnrealObjectRefFromNetGUID(const FNetworkGUID & NetGUID) const
{
	FSpatialNetGUIDCache* SpatialGuidCache = static_cast<FSpatialNetGUIDCache*>(GuidCache.Get());
	return SpatialGuidCache->GetUnrealObjectRefFromNetGUID(NetGUID);
}

FNetworkGUID USpatialPackageMapClient::GetNetGUIDFromUnrealObjectRef(const improbable::unreal::UnrealObjectRef & ObjectRef) const
{
	FSpatialNetGUIDCache* SpatialGuidCache = static_cast<FSpatialNetGUIDCache*>(GuidCache.Get());
	return SpatialGuidCache->GetNetGUIDFromUnrealObjectRef(ObjectRef);
}

FNetworkGUID USpatialPackageMapClient::GetNetGUIDFromEntityId(const worker::EntityId & EntityId) const
{
	FSpatialNetGUIDCache* SpatialGuidCache = static_cast<FSpatialNetGUIDCache*>(GuidCache.Get());
	improbable::unreal::UnrealObjectRef ObjectRef{EntityId, 0};
	return GetNetGUIDFromUnrealObjectRef(ObjectRef);
}

void USpatialPackageMapClient::RegisterStaticObjects(const improbable::unreal::UnrealLevelData& LevelData)
{
	FSpatialNetGUIDCache* SpatialGuidCache = static_cast<FSpatialNetGUIDCache*>(GuidCache.Get());
	SpatialGuidCache->RegisterStaticObjects(LevelData);
}

uint32 USpatialPackageMapClient::GetHashFromStaticClass(const UClass* StaticClass) const
{
	FSpatialNetGUIDCache* SpatialGuidCache = static_cast<FSpatialNetGUIDCache*>(GuidCache.Get());
	return SpatialGuidCache->GetHashFromStaticClass(StaticClass);
}

UClass* USpatialPackageMapClient::GetStaticClassFromHash(uint32 Hash) const
{
	FSpatialNetGUIDCache* SpatialGuidCache = static_cast<FSpatialNetGUIDCache*>(GuidCache.Get());
	return SpatialGuidCache->GetStaticClassFromHash(Hash);
}

FSpatialNetGUIDCache::FSpatialNetGUIDCache(USpatialNetDriver* InDriver)
	: FNetGUIDCache(InDriver)
{
	CreateStaticClassMapping();
}

FNetworkGUID FSpatialNetGUIDCache::AssignNewEntityActorNetGUID(AActor* Actor, const ::worker::Map< std::string, std::uint32_t >& SubobjectToOffset)
{
	FEntityId EntityId = Cast<USpatialNetDriver>(Driver)->GetEntityRegistry()->GetEntityIdFromActor(Actor);
	check(EntityId.ToSpatialEntityId() > 0);

	// Get interop.
	USpatialInterop* Interop = Cast<USpatialNetDriver>(Driver)->GetSpatialInterop();
	check(Interop);

	// Set up the NetGUID and ObjectRef for this actor.
	FNetworkGUID NetGUID = GetOrAssignNetGUID_SpatialGDK(Actor);
	improbable::unreal::UnrealObjectRef ObjectRef{EntityId.ToSpatialEntityId(), 0};
	RegisterObjectRef(NetGUID, ObjectRef);
	UE_LOG(LogSpatialOSPackageMap, Log, TEXT("Registered new object ref for actor: %s. NetGUID: %s, entity ID: %lld"),
		*Actor->GetName(), *NetGUID.ToString(), EntityId.ToSpatialEntityId());
	Interop->ResolvePendingOperations(Actor, ObjectRef);

	// Allocate NetGUIDs for each subobject, sorting alphabetically to ensure stable references.
	TArray<UObject*> ActorSubobjects;
	GetSubobjects(Actor, ActorSubobjects);

	for (UObject* Subobject : ActorSubobjects)
	{
		auto OffsetIterator = SubobjectToOffset.find(std::string(TCHAR_TO_UTF8(*(Subobject->GetName()))));
		if (OffsetIterator != SubobjectToOffset.end()) {
			std::uint32_t Offset = OffsetIterator->second;

			FNetworkGUID SubobjectNetGUID = GetOrAssignNetGUID_SpatialGDK(Subobject);
			improbable::unreal::UnrealObjectRef SubobjectRef{EntityId.ToSpatialEntityId(), Offset};
			RegisterObjectRef(SubobjectNetGUID, SubobjectRef);
			UE_LOG(LogSpatialOSPackageMap, Log, TEXT("Registered new object ref for subobject %s inside actor %s. NetGUID: %s, object ref: %s"),
				*Subobject->GetName(), *Actor->GetName(), *SubobjectNetGUID.ToString(), *ObjectRefToString(SubobjectRef));
			Interop->ResolvePendingOperations(Subobject, SubobjectRef);
		}
	}

	return NetGUID;
}

void FSpatialNetGUIDCache::RemoveEntityNetGUID(worker::EntityId EntityId)
{
	FNetworkGUID EntityNetGUID = GetNetGUIDFromEntityId(EntityId);
	FHashableUnrealObjectRef* ActorRef = NetGUIDToUnrealObjectRef.Find(EntityNetGUID);
	NetGUIDToUnrealObjectRef.Remove(EntityNetGUID);
	UnrealObjectRefToNetGUID.Remove(*ActorRef);
}

FNetworkGUID FSpatialNetGUIDCache::GetNetGUIDFromUnrealObjectRef(const improbable::unreal::UnrealObjectRef& ObjectRef) const
{
	const FNetworkGUID* NetGUID = UnrealObjectRefToNetGUID.Find(ObjectRef);
	return (NetGUID == nullptr ? FNetworkGUID(0) : *NetGUID);
}

improbable::unreal::UnrealObjectRef FSpatialNetGUIDCache::GetUnrealObjectRefFromNetGUID(const FNetworkGUID& NetGUID) const
{
	const FHashableUnrealObjectRef* ObjRef = NetGUIDToUnrealObjectRef.Find(NetGUID);
	return ObjRef ? (improbable::unreal::UnrealObjectRef)*ObjRef : SpatialConstants::UNRESOLVED_OBJECT_REF;
}

FNetworkGUID FSpatialNetGUIDCache::GetNetGUIDFromEntityId(worker::EntityId EntityId) const
{
	improbable::unreal::UnrealObjectRef ObjRef{EntityId, 0};
	const FNetworkGUID* NetGUID = UnrealObjectRefToNetGUID.Find(ObjRef);
	return (NetGUID == nullptr ? FNetworkGUID(0) : *NetGUID);
}

void FSpatialNetGUIDCache::RegisterStaticObjects(const improbable::unreal::UnrealLevelData& LevelData)
{
	// Get interop.
	USpatialInterop* Interop = Cast<USpatialNetDriver>(Driver)->GetSpatialInterop();
	check(Interop);

	// Build list of static objects in the world.
	UWorld* World = Driver->GetWorld();
	TMap<FString, AActor*> PersistentActorsInWorld;
	for (TActorIterator<AActor> Itr(World); Itr; ++Itr)
	{
		AActor* Actor = *Itr;
		FString PathName = Actor->GetPathName(World);
		PersistentActorsInWorld.Add(PathName, Actor);
		UE_LOG(LogSpatialOSPackageMap, Log, TEXT("%s: Static object in world. Name: %s. Path: %s. Replicated: %d"), *Interop->GetSpatialOS()->GetWorkerId(), *Actor->GetName(), *PathName, (int)Actor->GetIsReplicated());
	}

	// Match the above list with the static actor data.
	auto& LevelDataActors = LevelData.static_actor_map();
	for (auto& Pair : LevelDataActors)
	{
		uint32_t StaticObjectId = Pair.first << 7; // Reserve 127 slots for static objects.
		const char* LevelDataActorPath = Pair.second.c_str();
		AActor* Actor = PersistentActorsInWorld.FindRef(UTF8_TO_TCHAR(LevelDataActorPath));

		// Skip objects which don't exist.
		if (!Actor)
		{
			UE_LOG(LogSpatialOSPackageMap, Warning, TEXT("Expected object not found in level: %s"), UTF8_TO_TCHAR(LevelDataActorPath));
			continue;
		}

		// Skip objects which we've already registered.
		if (NetGUIDLookup.FindRef(Actor).IsValid())
		{
			continue;
		}

		// Set up the NetGUID and ObjectRef for this actor.
		FNetworkGUID NetGUID = GetOrAssignNetGUID_SpatialGDK(Actor);
		improbable::unreal::UnrealObjectRef ObjectRef{0, StaticObjectId};
		RegisterObjectRef(NetGUID, ObjectRef);
		UE_LOG(LogSpatialOSPackageMap, Log, TEXT("Registered new static object ref for actor: %s. NetGUID: %s, object ref: %s"),
			*Actor->GetName(), *NetGUID.ToString(), *ObjectRefToString(ObjectRef));
		Interop->ResolvePendingOperations(Actor, GetUnrealObjectRefFromNetGUID(NetGUID));

		// Deal with sub-objects of static objects.
		TArray<UObject*> StaticSubobjects;
		GetSubobjects(Actor, StaticSubobjects);
		uint32 SubobjectOffset = 1;
		for (UObject* Subobject : StaticSubobjects)
		{
			checkf((SubobjectOffset >> 7) == 0, TEXT("Static object has exceeded 127 subobjects."));
			FNetworkGUID SubobjectNetGUID = GetOrAssignNetGUID_SpatialGDK(Subobject);
			improbable::unreal::UnrealObjectRef SubobjectRef{0, StaticObjectId + SubobjectOffset++};
			RegisterObjectRef(SubobjectNetGUID, SubobjectRef);
			UE_LOG(LogSpatialOSPackageMap, Log, TEXT("Registered new static object ref for subobject %s inside actor %s. NetGUID: %s, object ref: %s"),
				*Subobject->GetName(), *Actor->GetName(), *SubobjectNetGUID.ToString(), *ObjectRefToString(SubobjectRef));
			Interop->ResolvePendingOperations(Subobject, GetUnrealObjectRefFromNetGUID(SubobjectNetGUID));
		}
	}
}

uint32 FSpatialNetGUIDCache::GetHashFromStaticClass(const UClass* StaticClass) const
{
	return GetTypeHash(*StaticClass->GetPathName());
}

UClass* FSpatialNetGUIDCache::GetStaticClassFromHash(uint32 Hash) const
{
	// This should never fail in production code, but might in development if the client and server are running versions
	// with inconsistent static class lists.
	bool bContainsHash = StaticClassHashMap.Contains(Hash);
	checkf(bContainsHash, TEXT("Failed to find static class for hash: %d"), Hash);
	return bContainsHash ? StaticClassHashMap[Hash] : nullptr;
}

FNetworkGUID FSpatialNetGUIDCache::GetOrAssignNetGUID_SpatialGDK(const UObject* Object)
{
	FNetworkGUID NetGUID = GetOrAssignNetGUID(Object);
	UE_LOG(LogSpatialOSPackageMap, Log, TEXT("%s: GetOrAssignNetGUID for object %s returned %s. IsDynamicObject: %d"),
		*Cast<USpatialNetDriver>(Driver)->GetSpatialOS()->GetWorkerId(),
		*Object->GetName(),
		*NetGUID.ToString(),
		(int)IsDynamicObject(Object));

	// One major difference between how Unreal does NetGUIDs vs us is, we don't attempt to make them consistent across workers and client.
	// The function above might have returned without assigning new GUID, because we are the client.
	// Let's directly call the client function in that case.
	if (NetGUID == FNetworkGUID::GetDefault() && !IsNetGUIDAuthority())
	{
		// Here we have to borrow from FNetGuidCache::AssignNewNetGUID_Server to avoid a source change.
#define COMPOSE_NET_GUID(Index, IsStatic)	(((Index) << 1) | (IsStatic) )
#define ALLOC_NEW_NET_GUID(IsStatic)		(COMPOSE_NET_GUID(++UniqueNetIDs[IsStatic], IsStatic))

		// Generate new NetGUID and assign it
		const int32 IsStatic = IsDynamicObject(Object) ? 0 : 1;

		NetGUID = FNetworkGUID(ALLOC_NEW_NET_GUID(IsStatic));

		FNetGuidCacheObject CacheObject;
		CacheObject.Object = MakeWeakObjectPtr(const_cast<UObject*>(Object));
		CacheObject.PathName = Object->GetFName();
		RegisterNetGUID_Internal(NetGUID, CacheObject);

		UE_LOG(LogSpatialOSPackageMap, Log, TEXT("%s: NetGUID for object %s was not found in the cache. Generated new NetGUID %s."),
			*Cast<USpatialNetDriver>(Driver)->GetSpatialOS()->GetWorkerId(),
			*Object->GetName(),
			*NetGUID.ToString());
	}

	check(NetGUID.IsValid());
	check(!NetGUID.IsDefault());
	return NetGUID;
}

void FSpatialNetGUIDCache::RegisterObjectRef(FNetworkGUID NetGUID, const improbable::unreal::UnrealObjectRef& ObjectRef)
{
	checkSlow(!NetGUIDToUnrealObjectRef.Contains(NetGUID) || (NetGUIDToUnrealObjectRef.Contains(NetGUID) && NetGUIDToUnrealObjectRef.FindChecked(NetGUID) == ObjectRef));
	checkSlow(!UnrealObjectRefToNetGUID.Contains(ObjectRef) || (UnrealObjectRefToNetGUID.Contains(ObjectRef) && UnrealObjectRefToNetGUID.FindChecked(ObjectRef) == NetGUID));
	NetGUIDToUnrealObjectRef.Emplace(NetGUID, ObjectRef);
	UnrealObjectRefToNetGUID.Emplace(ObjectRef, NetGUID);
}

FNetworkGUID FSpatialNetGUIDCache::AssignStaticActorNetGUID(const UObject* Object, const FNetworkGUID& StaticNetGUID)
{
	check(!NetGUIDLookup.FindRef(Object).IsValid());

	FNetGuidCacheObject CacheObject;
	CacheObject.Object = MakeWeakObjectPtr(const_cast<UObject*>(Object));
	CacheObject.PathName = Object->GetFName();
	RegisterNetGUID_Internal(StaticNetGUID, CacheObject);

	// Register object ref.
	improbable::unreal::UnrealObjectRef ObjectRef{0, StaticNetGUID.Value};
	RegisterObjectRef(StaticNetGUID, ObjectRef);

	UE_LOG(LogSpatialOSPackageMap, Log, TEXT("%s: Registered static object %s. NetGUID: %s, Object Ref: %s."),
		*Cast<USpatialNetDriver>(Driver)->GetSpatialOS()->GetWorkerId(),
		*Object->GetName(),
		*StaticNetGUID.ToString(),
		*ObjectRefToString(ObjectRef));

	return StaticNetGUID;
}

void FSpatialNetGUIDCache::CreateStaticClassMapping()
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->HasAnyClassFlags(CLASS_Abstract))
		{
			uint32 PathHash = GetHashFromStaticClass(*It);
			checkf(StaticClassHashMap.Contains(PathHash) == false, TEXT("Hash clash between %s and %s"), *It->GetPathName(), *StaticClassHashMap[PathHash]->GetPathName());
			StaticClassHashMap.Add(PathHash, *It);
		}
	}

	UE_LOG(LogSpatialOSPackageMap, Log, TEXT("Registered %d static classes to the class hashmap"), StaticClassHashMap.Num());
}
