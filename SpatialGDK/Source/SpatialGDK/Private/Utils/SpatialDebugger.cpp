// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Utils/SpatialDebugger.h"

#include "Debug/DebugDrawService.h"
#include "Engine/Engine.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interop/SpatialReceiver.h"
#include "Interop/SpatialStaticComponentView.h"
#include "Kismet/GameplayStatics.h"
#include "Schema/AuthorityIntent.h"

using namespace SpatialGDK;

DEFINE_LOG_CATEGORY(LogSpatialDebugger);

ASpatialDebugger::ASpatialDebugger(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 1.f;

	bAlwaysRelevant = true;
	bNetLoadOnClient = false;
	bReplicates = true;

	NetUpdateFrequency = 1.f;

	NetDriver = Cast<USpatialNetDriver>(GetNetDriver());

	// For GDK design reasons, this is the approach chosen to get a pointer
	// on the net driver to the client ASpatialDebugger.  Various alternatives
	// were considered and this is the best of a bad bunch.
	if (NetDriver != nullptr && GetNetMode() == NM_Client)
	{
		NetDriver->SetSpatialDebugger(this);
	}
}

void ASpatialDebugger::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	check(NetDriver != nullptr);

	if (!NetDriver->IsServer())
	{
		// Since we have no guarantee on the order we'll receive the PC/Pawn/PlayerState
		// over the wire, we check here once per tick (currently 1 Hz tick rate) to setup our local pointers.
		// Note that we can capture the PC in OnEntityAdded() since we know we will only receive one of those.
		if (LocalPawn.IsValid() == false && LocalPlayerController.IsValid())
		{
			LocalPawn = LocalPlayerController->GetPawn();
		}

		if (LocalPlayerState.IsValid() == false && LocalPawn.IsValid())
		{
			LocalPlayerState = LocalPawn->GetPlayerState();
		}

		if (LocalPawn.IsValid())
		{
			SCOPE_CYCLE_COUNTER(STAT_SortingActors);
			const FVector& PlayerLocation = LocalPawn->GetActorLocation();

			EntityActorMapping.ValueSort([PlayerLocation](const TWeakObjectPtr<AActor>& A, const TWeakObjectPtr<AActor>& B) {
				return FVector::Dist(PlayerLocation, A->GetActorLocation()) > FVector::Dist(PlayerLocation, B->GetActorLocation());
			});
		}
	}
}

void ASpatialDebugger::BeginPlay()
{
	Super::BeginPlay();

	check(NetDriver != nullptr);

	if (!NetDriver->IsServer())
	{
		EntityActorMapping.Reserve(ENTITY_ACTOR_MAP_RESERVATION_COUNT);

		LoadIcons();

		TArray<Worker_EntityId_Key> EntityIds;
		NetDriver->StaticComponentView->GetEntityIds(EntityIds);

		// Capture any entities that are already present on this client (ie they came over the wire before the SpatialDebugger did).
		for (const Worker_EntityId_Key EntityId : EntityIds)
		{
			OnEntityAdded(EntityId);
		}

		// Register callbacks to get notified of all future entity arrivals / deletes.
		OnEntityAddedHandle = NetDriver->Receiver->OnEntityAddedDelegate.AddUObject(this, &ASpatialDebugger::OnEntityAdded);
		OnEntityRemovedHandle = NetDriver->Receiver->OnEntityRemovedDelegate.AddUObject(this, &ASpatialDebugger::OnEntityRemoved);

		FontRenderInfo.bClipText = true;
		FontRenderInfo.bEnableShadow = true;

		RenderFont = GEngine->GetSmallFont();

		if (bAutoStart)
		{
			SpatialToggleDebugger();
		}
	}
}

void ASpatialDebugger::Destroyed()
{
	if (NetDriver != nullptr && NetDriver->Receiver != nullptr)
	{
		if (OnEntityAddedHandle.IsValid())
		{
			NetDriver->Receiver->OnEntityAddedDelegate.Remove(OnEntityAddedHandle);
		}

		if (OnEntityRemovedHandle.IsValid())
		{
			NetDriver->Receiver->OnEntityRemovedDelegate.Remove(OnEntityRemovedHandle);
		}
	}

	if (DrawDebugDelegateHandle.IsValid())
	{
		UDebugDrawService::Unregister(DrawDebugDelegateHandle);
	}

	Super::Destroyed();
}

void ASpatialDebugger::LoadIcons()
{
	check(NetDriver != nullptr && !NetDriver->IsServer());

	UTexture2D* DefaultTexture = DefaultTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));

	const float IconWidth = 16.0f;
	const float IconHeight = 16.0f;

	Icons[ICON_AUTH] = UCanvas::MakeIcon(AuthTexture != nullptr ? AuthTexture : DefaultTexture, 0.0f, 0.0f, IconWidth, IconHeight);
	Icons[ICON_AUTH_INTENT] = UCanvas::MakeIcon(AuthIntentTexture != nullptr ? AuthIntentTexture : DefaultTexture, 0.0f, 0.0f, IconWidth, IconHeight);
	Icons[ICON_UNLOCKED] = UCanvas::MakeIcon(UnlockedTexture != nullptr ? UnlockedTexture : DefaultTexture, 0.0f, 0.0f, IconWidth, IconHeight);
	Icons[ICON_LOCKED] = UCanvas::MakeIcon(LockedTexture != nullptr ? LockedTexture : DefaultTexture, 0.0f, 0.0f, IconWidth, IconHeight);
	Icons[ICON_BOX] = UCanvas::MakeIcon(BoxTexture != nullptr ? BoxTexture : DefaultTexture, 0.0f, 0.0f, IconWidth, IconHeight);
}

void ASpatialDebugger::OnEntityAdded(const Worker_EntityId EntityId)
{
	check(NetDriver != nullptr && !NetDriver->IsServer());

	TWeakObjectPtr<AActor>* ExistingActor = EntityActorMapping.Find(EntityId);

	if (ExistingActor != nullptr)
	{
		return;
	}

	if (AActor* Actor = Cast<AActor>(NetDriver->PackageMap->GetObjectFromEntityId(EntityId).Get()))
	{
		EntityActorMapping.Add(EntityId, Actor);

		// Each client will only receive a PlayerController once.
		if (Actor->IsA<APlayerController>())
		{
			LocalPlayerController = Cast<APlayerController>(Actor);
		}
	}
}

void ASpatialDebugger::OnEntityRemoved(const Worker_EntityId EntityId)
{
	check(NetDriver != nullptr && !NetDriver->IsServer());

	EntityActorMapping.Remove(EntityId);
}

void ASpatialDebugger::DrawTag(UCanvas* Canvas, const FVector2D& ScreenLocation, const Worker_EntityId EntityId, const FString& ActorName)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawTag);

	// TODO: Smarter positioning of elements so they're centered no matter how many are enabled https://improbableio.atlassian.net/browse/UNR-2360.
	int32 HorizontalOffset = -32.0f;

	if (bShowLock)
	{
		SCOPE_CYCLE_COUNTER(STAT_DrawIcons);
		const bool bIsLocked = GetLockStatus(EntityId);
		const EIcon LockIcon = bIsLocked ? ICON_LOCKED : ICON_UNLOCKED;

		Canvas->SetDrawColor(FColor::White);
		Canvas->DrawIcon(Icons[LockIcon], ScreenLocation.X + HorizontalOffset, ScreenLocation.Y, 1.0f);
		HorizontalOffset += 16.0f;
	}

	if (bShowAuth)
	{
		SCOPE_CYCLE_COUNTER(STAT_DrawIcons);
		const FColor& ServerWorkerColor = GetServerWorkerColor(EntityId);
		Canvas->SetDrawColor(FColor::White);
		Canvas->DrawIcon(Icons[ICON_AUTH], ScreenLocation.X + HorizontalOffset, ScreenLocation.Y, 1.0f);
		HorizontalOffset += 16.0f;
		Canvas->SetDrawColor(ServerWorkerColor);
		Canvas->DrawIcon(Icons[ICON_BOX], ScreenLocation.X + HorizontalOffset, ScreenLocation.Y, 1.0f);
		HorizontalOffset += 16.0f;
	}

	if (bShowAuthIntent)
	{
		SCOPE_CYCLE_COUNTER(STAT_DrawIcons);
		const FColor& VirtualWorkerColor = GetVirtualWorkerColor(EntityId);
		Canvas->SetDrawColor(FColor::White);
		Canvas->DrawIcon(Icons[ICON_AUTH_INTENT], ScreenLocation.X + HorizontalOffset, ScreenLocation.Y, 1.0f);
		HorizontalOffset += 16.0f;
		Canvas->SetDrawColor(VirtualWorkerColor);
		Canvas->DrawIcon(Icons[ICON_BOX], ScreenLocation.X + HorizontalOffset, ScreenLocation.Y, 1.0f);
		HorizontalOffset += 16.0f;
	}

	FString Label;
	if (bShowEntityId)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuildText);
		Label += FString::Printf(TEXT("%lld "), EntityId);
	}

	if (bShowActorName)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuildText);
		Label += FString::Printf(TEXT("(%s)"), *ActorName);
	}

	if (bShowEntityId || bShowActorName)
	{
		SCOPE_CYCLE_COUNTER(STAT_DrawText);
		Canvas->SetDrawColor(FColor::Green);
		Canvas->DrawText(RenderFont, Label, ScreenLocation.X + HorizontalOffset, ScreenLocation.Y, 1.0f, 1.0f, FontRenderInfo);
	}
}

void ASpatialDebugger::DrawDebug(UCanvas* Canvas, APlayerController* /* Controller */) // Controller is invalid.
{
	SCOPE_CYCLE_COUNTER(STAT_DrawDebug);

	check(NetDriver != nullptr && !NetDriver->IsServer());

#if WITH_EDITOR
	// Prevent one client's data rendering in another client's view in PIE when using UDebugDrawService.  Lifted from EQSRenderingComponent.
	if (Canvas && Canvas->SceneView && Canvas->SceneView->Family && Canvas->SceneView->Family->Scene && Canvas->SceneView->Family->Scene->GetWorld() != GetWorld())
	{
		return;
	}
#endif

	DrawDebugLocalPlayer(Canvas);

	FVector PlayerLocation = FVector::ZeroVector;

	if (LocalPawn.IsValid())
	{
		PlayerLocation = LocalPawn->GetActorLocation();
	}

	for (TPair<Worker_EntityId_Key, TWeakObjectPtr<AActor>>& EntityActorPair : EntityActorMapping)
	{
		const TWeakObjectPtr<AActor> Actor = EntityActorPair.Value;
		const Worker_EntityId EntityId = EntityActorPair.Key;

		if (Actor != nullptr)
		{
			FVector ActorLocation = Actor->GetActorLocation();

			if (ActorLocation.IsZero())
			{
				continue;
			}

			if (FVector::Dist(PlayerLocation, ActorLocation) > MaxRange)
			{
				continue;
			}

			FVector2D ScreenLocation = FVector2D::ZeroVector;
			if (LocalPlayerController.IsValid())
			{
				SCOPE_CYCLE_COUNTER(STAT_Projection);
				UGameplayStatics::ProjectWorldToScreen(LocalPlayerController.Get(), ActorLocation + WorldSpaceActorTagOffset, ScreenLocation, false);
			}

			if (ScreenLocation.IsZero())
			{
				continue;
			}

			DrawTag(Canvas, ScreenLocation, EntityId, Actor->GetName());
		}
	}
}

void ASpatialDebugger::DrawDebugLocalPlayer(UCanvas* Canvas)
{
	if (LocalPawn == nullptr ||	LocalPlayerController == nullptr ||	LocalPlayerState == nullptr)
	{
		return;
	}

	const TArray<TWeakObjectPtr<AActor>> LocalPlayerActors =
	{
		LocalPawn,
		LocalPlayerController,
		LocalPlayerState
	};

	FVector2D ScreenLocation(PlayerPanelStartX, PlayerPanelStartY);

	for (int32 i = 0; i < LocalPlayerActors.Num(); ++i)
	{
		if (LocalPlayerActors[i].IsValid())
		{
			const Worker_EntityId EntityId = NetDriver->PackageMap->GetEntityIdFromObject(LocalPlayerActors[i].Get());
			DrawTag(Canvas, ScreenLocation, EntityId, LocalPlayerActors[i]->GetName());
			ScreenLocation.Y -= PLAYER_TAG_VERTICAL_OFFSET;
		}
	}
}

const FColor& ASpatialDebugger::GetVirtualWorkerColor(const Worker_EntityId EntityId) const
{
	check(NetDriver != nullptr && !NetDriver->IsServer());

	const AuthorityIntent* AuthorityIntentComponent = NetDriver->StaticComponentView->GetComponentData<AuthorityIntent>(EntityId);
	const int32 VirtualWorkerId = (AuthorityIntentComponent != nullptr) ? AuthorityIntentComponent->VirtualWorkerId : SpatialConstants::INVALID_VIRTUAL_WORKER_ID;

	if (VirtualWorkerId != SpatialConstants::INVALID_VIRTUAL_WORKER_ID &&
		VirtualWorkerId < ServerTintColors.Num())
	{
		return ServerTintColors[VirtualWorkerId];
	}
	else
	{
		return InvalidServerTintColor;
	}
}

// TODO: Implement once this functionality is available https://improbableio.atlassian.net/browse/UNR-2362.
const FColor& ASpatialDebugger::GetServerWorkerColor(const Worker_EntityId EntityId) const
{
	check(NetDriver != nullptr && !NetDriver->IsServer());
	return InvalidServerTintColor;
}

// TODO: Implement once this functionality is available https://improbableio.atlassian.net/browse/UNR-2361.
bool ASpatialDebugger::GetLockStatus(const Worker_EntityId Entityid)
{
	check(NetDriver != nullptr && !NetDriver->IsServer());
	return false;
}

void ASpatialDebugger::SpatialToggleDebugger()
{
	check(NetDriver != nullptr && !NetDriver->IsServer());

	if (DrawDebugDelegateHandle.IsValid())
	{
		UDebugDrawService::Unregister(DrawDebugDelegateHandle);
		DrawDebugDelegateHandle.Reset();
	}
	else
	{
		DrawDebugDelegateHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &ASpatialDebugger::DrawDebug));
	}
}
