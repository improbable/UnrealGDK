// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialTestInitialOnlyForSpawnActor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SpatialFunctionalTestFlowController.h"
#include "SpatialGDKFunctionalTests/SpatialGDK/TestActors/TestPossessionPawn.h"
#include "SpatialGDKSettings.h"
#include "TestClasses/SpatialTestInitialOnlySpawnActor.h"

/**
 * Basic initial only test case.
 * Spawn an actor in front of player, change initial only & replicate value at server side, check client side value.
 *
 * step 1: server 1 create a cube with replicate property rep1=1 and initial only property initial1=1.
 * step 2: client 1 checkout actor and print these properties, should got this: rep1=1, initial1=1.
 * step 3: server 1 change rep1=2, initial1=2.
 * step 4: client 1 should got this: rep1=2, initial1=1.
 * step 5: clean up.
 */

ASpatialTestInitialOnlyForSpawnActor::ASpatialTestInitialOnlyForSpawnActor()
	: Super()
{
	Author = "Jeff Xu";
	Description = TEXT("Spawn an actor in front of player, change initial only & replicate value at server side, check client side value.");
}

void ASpatialTestInitialOnlyForSpawnActor::PrepareTest()
{
	Super::PrepareTest();

	AddStep(TEXT("Init test environment"), FWorkerDefinition::Server(1), nullptr, [this]() {
		// Spawn cube
		SpawnActor<ASpatialTestInitialOnlySpawnActor>(FVector(-50.0f, 0.0f, 75.0f), FRotator::ZeroRotator);

		AssertTrue(GetDefault<USpatialGDKSettings>()->bEnableInitialOnlyReplicationCondition, TEXT("Initial Only Enabled"));

		// Spawn the TestPossessionPawn actor for Client 1 to possess.
		ASpatialFunctionalTestFlowController* FlowController = GetFlowController(ESpatialFunctionalTestWorkerType::Client, 1);
		ATestPossessionPawn* TestCharacter = SpawnActor<ATestPossessionPawn>(FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator);
		APlayerController* PlayerController = Cast<APlayerController>(FlowController->GetOwner());

		// Set a reference to the previous Pawn so that it can be processed back in the last step of the test
		OriginalPawn = TPair<AController*, APawn*>(PlayerController, PlayerController->GetPawn());

		PlayerController->Possess(TestCharacter);

		FinishStep();
	});

	AddStep(
		TEXT("Check default value."), FWorkerDefinition::Client(1),
		[this]() -> bool {
			bool IsReady = false;
			TArray<AActor*> SpawnActors;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpatialTestInitialOnlySpawnActor::StaticClass(), SpawnActors);
			for (AActor* Actor : SpawnActors)
			{
				ASpatialTestInitialOnlySpawnActor* SpawnActor = Cast<ASpatialTestInitialOnlySpawnActor>(Actor);
				if (SpawnActor != nullptr)
				{
					IsReady = true;
				}
			}
			return IsReady;
		},
		[this]() {
			TArray<AActor*> SpawnActors;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpatialTestInitialOnlySpawnActor::StaticClass(), SpawnActors);
			for (AActor* Actor : SpawnActors)
			{
				ASpatialTestInitialOnlySpawnActor* SpawnActor = Cast<ASpatialTestInitialOnlySpawnActor>(Actor);
				if (SpawnActor != nullptr)
				{
					AssertEqual_Int(SpawnActor->Int_Initial, 1, TEXT("Check Actor.Int_Initial value."));
					AssertEqual_Int(SpawnActor->Int_Replicate, 1, TEXT("Check Actor.Int_Replicate value."));
				}
			}

			FinishStep();
		});

	AddStep(TEXT("Server change value."), FWorkerDefinition::Server(1), nullptr, [this]() {
		TArray<AActor*> SpawnActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpatialTestInitialOnlySpawnActor::StaticClass(), SpawnActors);
		AssertEqual_Int(SpawnActors.Num(), 1, TEXT("There should be exactly one InitialOnly actor in the world."));
		for (AActor* Actor : SpawnActors)
		{
			ASpatialTestInitialOnlySpawnActor* SpawnActor = Cast<ASpatialTestInitialOnlySpawnActor>(Actor);
			if (AssertIsValid(SpawnActor, TEXT("SpawnActor should be valid.")))
			{
				SpawnActor->Int_Initial = 2;
				SpawnActor->Int_Replicate = 2;
			}
		}

		FinishStep();
	});

	AddStep(
		TEXT("Check changed value."), FWorkerDefinition::Client(1), nullptr, nullptr,
		[this](float DeltaTime) {
			TArray<AActor*> SpawnActors;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpatialTestInitialOnlySpawnActor::StaticClass(), SpawnActors);
			AssertEqual_Int(SpawnActors.Num(), 1, TEXT("There should be exactly one InitialOnly actor in the world."));
			for (AActor* Actor : SpawnActors)
			{
				ASpatialTestInitialOnlySpawnActor* SpawnActor = Cast<ASpatialTestInitialOnlySpawnActor>(Actor);
				if (AssertIsValid(SpawnActor, TEXT("SpawnActor should be valid.")))
				{
					RequireEqual_Int(SpawnActor->Int_Initial, 1, TEXT("Check Actor.Int_Initial value."));
					RequireEqual_Int(SpawnActor->Int_Replicate, 2, TEXT("Check Actor.Int_Replicate value."));
				}
			}

			FinishStep();
		},
		10.0f);

	AddStep(TEXT("Cleanup"), FWorkerDefinition::Server(1), nullptr, [this]() {
		// Possess the original pawn, so that other tests start from the expected, default set-up
		OriginalPawn.Key->Possess(OriginalPawn.Value);

		FinishStep();
	});
}
