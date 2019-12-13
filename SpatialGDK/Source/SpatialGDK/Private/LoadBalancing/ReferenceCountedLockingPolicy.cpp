// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "ReferenceCountedLockingPolicy.h"

#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "Interop/SpatialStaticComponentView.h"
#include "Schema/AuthorityIntent.h"

#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogReferenceCountedLockingPolicy);

bool UReferenceCountedLockingPolicy::CanAcquireLock(AActor* Actor) const
{
	const USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(Actor->GetWorld()->GetNetDriver());
	const Worker_EntityId EntityId = NetDriver->PackageMap->GetEntityIdFromObject(Actor);

	if (EntityId == SpatialConstants::INVALID_ENTITY_ID)
	{
		UE_LOG(LogReferenceCountedLockingPolicy, Error, TEXT("CanAcquireLock called for actor without corresponding entity ID. Actor: %s"), *Actor->GetName());
		return false;
	}

	const bool bHasAuthority = NetDriver->StaticComponentView->GetAuthority(EntityId, SpatialConstants::AUTHORITY_INTENT_COMPONENT_ID) == WORKER_AUTHORITY_AUTHORITATIVE;
	if (!bHasAuthority)
	{
		UE_LOG(LogReferenceCountedLockingPolicy, Log, TEXT("Can not lock actor migration. Do not have authority. Actor: %s"), *Actor->GetName());
	}
	const bool bHasAuthorityIntent = NetDriver->VirtualWorkerTranslator->GetLocalVirtualWorkerId() ==
		NetDriver->StaticComponentView->GetComponentData<SpatialGDK::AuthorityIntent>(EntityId)->VirtualWorkerId;
	if (!bHasAuthorityIntent)
	{
		UE_LOG(LogReferenceCountedLockingPolicy, Log, TEXT("Can not lock actor migration. Authority intent does not match this worker. Actor: %s"), *Actor->GetName());
	}
	return bHasAuthorityIntent && bHasAuthority;
}

ActorLockToken UReferenceCountedLockingPolicy::AcquireLock(AActor* Actor, FString DebugString)
{
	if (!CanAcquireLock(Actor))
	{
		UE_LOG(LogReferenceCountedLockingPolicy, Error, TEXT("Called AcquireLock when CanAcquireLock returned false. Actor: %s."), *GetNameSafe(Actor));
		return SpatialConstants::INVALID_ACTOR_LOCK_TOKEN;
	}
	LockingState* ActorLockingState = ActorToLockingState.Find(Actor);
	if (ActorLockingState != nullptr)
	{
		++ActorLockingState->LockCount;
	}
	else
	{
		Actor->OnDestroyed.AddDynamic(this, &UReferenceCountedLockingPolicy::OnLockedActorDeleted);
		ActorToLockingState.Add(Actor, LockingState{ 1, [this, Actor]
		{
			Actor->OnDestroyed.RemoveDynamic(this, &UReferenceCountedLockingPolicy::OnLockedActorDeleted);
		} });
	}

	UE_LOG(LogReferenceCountedLockingPolicy, Log, TEXT("Acquiring migration lock. "
		"Actor: %s. Lock name: %s. Token %d: Locks held: %d."), *GetNameSafe(Actor), *DebugString, NextToken, ActorToLockingState.Find(Actor)->LockCount);
	TokenToNameAndActor.Emplace(NextToken, LockNameAndActor{ MoveTemp(DebugString), Actor });
	return NextToken++;
}

void UReferenceCountedLockingPolicy::ReleaseLock(ActorLockToken Token)
{
	const auto NameAndActor = TokenToNameAndActor.FindAndRemoveChecked(Token);
	const AActor* Actor = NameAndActor.Actor;
	const FString& Name = NameAndActor.LockName;
	UE_LOG(LogReferenceCountedLockingPolicy, Log, TEXT("Releasing actor migration lock. Actor: %s. Token: %d. Lock name: %s"), *Actor->GetName(), Token, *Name);

	check(ActorToLockingState.Contains(Actor));

	{
		// Reduce the reference count and erase the entry if reduced to 0.
		auto CountIt = ActorToLockingState.CreateKeyIterator(Actor);
		LockingState& ActorLockingState = CountIt.Value();
		if (ActorLockingState.LockCount == 1)
		{
			UE_LOG(LogReferenceCountedLockingPolicy, Log, TEXT("Actor migration no longer locked. Actor: %s"), *Actor->GetName());
			ActorLockingState.UnbindActorDeletionDelegateFunc();
			CountIt.RemoveCurrent();
		}
		else
		{
			--ActorLockingState.LockCount;
		}
	}
}

bool UReferenceCountedLockingPolicy::IsLocked(const AActor* Actor) const
{
	if (Actor == nullptr)
	{
		UE_LOG(LogReferenceCountedLockingPolicy, Warning, TEXT("IsLocked called for nullptr"));
		return false;
	}
	return ActorToLockingState.Contains(Actor);
}

void UReferenceCountedLockingPolicy::OnLockedActorDeleted(AActor* DestroyedActor)
{
	// iterate through actor to lock state tokens and delete debug strings
	TArray<ActorLockToken> TokensToRemove;
	for (const auto& KeyValuePair : TokenToNameAndActor)
	{
		if (KeyValuePair.Value.Actor == DestroyedActor)
		{
			TokensToRemove.Add(KeyValuePair.Key);
		}
	}
	for (const auto& Token : TokensToRemove)
	{
		TokenToNameAndActor.Remove(Token);
	}
	ActorToLockingState.Remove(DestroyedActor);
}
