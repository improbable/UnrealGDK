// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "EngineClasses/SpatialNetConnection.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

namespace SpatialGDK
{

inline FString GetOwnerWorkerAttribute(AActor* Actor)
{
	if (const USpatialNetConnection* NetConnection = Cast<USpatialNetConnection>(Actor->GetNetConnection()))
	{
		return NetConnection->WorkerAttribute;
	}

	return FString();
}

inline FVector GetActorSpatialPosition(const AActor* InActor)
{
	FVector Location = FVector::ZeroVector;

	// If the Actor is a Controller, use its Pawn's position,
	// Otherwise if the Actor is a PlayerController, use its last spectator sync location, otherwise its focal point
	// Otherwise if the Actor has an Owner, use its position.
	// Otherwise if the Actor has a well defined location then use that
	// Otherwise use the origin
	const AController* Controller = Cast<AController>(InActor);
	if (Controller != nullptr && Controller->GetPawn() != nullptr)
	{
		USceneComponent* PawnRootComponent = Controller->GetPawn()->GetRootComponent();
		Location = PawnRootComponent ? PawnRootComponent->GetComponentLocation() : FVector::ZeroVector;
	}
	else if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (PlayerController->IsInState(NAME_Spectating))
		{
			Location = PlayerController->LastSpectatorSyncLocation;
		}
		else
		{
			Location = PlayerController->GetFocalLocation();
		}
	}
	else if (InActor->GetOwner() != nullptr && InActor->GetOwner()->GetIsReplicated())
	{
		return GetActorSpatialPosition(InActor->GetOwner());
	}
	else if (USceneComponent* RootComponent = InActor->GetRootComponent())
	{
		Location = RootComponent->GetComponentLocation();
	}

	// Rebase location onto zero origin so actor is positioned correctly in SpatialOS.
	return FRepMovement::RebaseOntoZeroOrigin(Location, InActor);
}

} // namespace SpatialGDK
