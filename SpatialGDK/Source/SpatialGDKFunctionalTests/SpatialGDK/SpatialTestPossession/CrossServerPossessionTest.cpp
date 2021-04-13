// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "CrossServerPossessionTest.h"

#include "Containers/Array.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "GameFramework/PlayerController.h"
#include "SpatialFunctionalTestFlowController.h"
#include "SpatialGDKFunctionalTests/SpatialGDK/TestActors/TestMovementCharacter.h"
#include "SpatialGDKFunctionalTests/SpatialGDK/TestActors/TestPossessionPawn.h"
#include "TestPossessionPlayerController.h"

/**
 * This test tests 1 Controller remote possess over 1 Pawn.
 *
 * This test expects a load balancing grid and ACrossServerPossessionGameMode
 * Recommend to use 2*2 load balancing grid because the position is written in the code
 * The client workers begin with a player controller and their default pawns, which they initially possess.
 * The flow is as follows:
 *  - Setup:
 *    - Specify `GameMode Override` as ACrossServerPossessionGameMode
 *    - Specify `Multi Worker Settings Class` as Zoning 2x2(e.g. BP_Possession_Settings_Zoning2_2 of UnrealGDKTestGyms)
 *	  - Set `Num Required Clients` as 1
 *  - Test:
 *	  - Create a Pawn in first quadrant
 *	  - Create Controller in other quadrant, the position is determined by ACrossServerPossessionGameMode
 *	  - Wait for Pawn in right worker.
 *	  -	The Controller possess the Pawn in server-side
 *	- Result Check:
 *    - ATestPossessionPlayerController::OnPossess should be called == 1 times
 */

ACrossServerPossessionTest::ACrossServerPossessionTest()
{
	Author = "Ken.Yu";
	Description = TEXT("Test Cross-Server Possession");
}

void ACrossServerPossessionTest::PrepareTest()
{
	Super::PrepareTest();

	AddStep(
		TEXT("Cross-Server Possession"), FWorkerDefinition::AllServers, nullptr, nullptr,
		[this](float) {
			ATestPossessionPawn* Pawn = GetPawn();
			// AssertIsValid(Pawn, TEXT("Test requires a Pawn"));
			for (ASpatialFunctionalTestFlowController* FlowController : GetFlowControllers())
			{
				if (FlowController->WorkerDefinition.Type == ESpatialFunctionalTestWorkerType::Client)
				{
					ATestPossessionPlayerController* PlayerController = Cast<ATestPossessionPlayerController>(FlowController->GetOwner());
					if (PlayerController && PlayerController->HasAuthority())
					{
						AssertTrue(PlayerController->HasAuthority(), TEXT("PlayerController should HasAuthority"), PlayerController);
						AssertFalse(Pawn->HasAuthority(), TEXT("Pawn shouldn't HasAuthority"), Pawn);
						// RequireFalse(Pawn->HasAuthority(), TEXT("Pawn shouldn't HasAuthority"));
						if (!(Pawn->HasAuthority()))
						{
							AddToOriginalPawns(PlayerController, PlayerController->GetPawn());
							PlayerController->UnPossess();
							PlayerController->RemotePossessOnServer(Pawn);
						}
					}
				}
			}
			FinishStep();
		},
		10000.0);

	AddStep(
		TEXT("Check test result"), FWorkerDefinition::Server(1),
		[this]() -> bool {
			return ATestPossessionPlayerController::OnPossessCalled == 1;
		},
		nullptr,
		[this](float DeltaTime) {
			for (ASpatialFunctionalTestFlowController* FlowController : GetFlowControllers())
			{
				if (FlowController->WorkerDefinition.Type == ESpatialFunctionalTestWorkerType::Client)
				{
					ATestPossessionPlayerController* PlayerController = Cast<ATestPossessionPlayerController>(FlowController->GetOwner());
					if (PlayerController && PlayerController->HasAuthority())
					{
						AssertTrue(PlayerController->HasMigrated(), TEXT("PlayerController should have migrated"), PlayerController);
					}
				}
			}
			FinishStep();
		});

	AddStep(TEXT("Clean up the test"), FWorkerDefinition::AllServers, nullptr, nullptr, [this](float) {
		for (const auto& OriginalPawnPair : OriginalPawns)
		{
			if (OriginalPawnPair.Controller.Get() != nullptr && OriginalPawnPair.Controller.Get()->HasAuthority())
			{
				OriginalPawnPair.Controller.Get()->UnPossess();
				OriginalPawnPair.Controller.Get()->RemotePossessOnServer(OriginalPawnPair.Pawn.Get());
			}
		}
		FinishStep();
	});

	AddStep(
		TEXT("Wait for all controllers to migrate"), FWorkerDefinition::AllServers, nullptr, nullptr,
		[this](float) {
			for (const auto& OriginalPawnPair : OriginalPawns)
			{
				if (OriginalPawnPair.Controller.Get() != nullptr && OriginalPawnPair.Controller.Get()->HasAuthority())
				{
					RequireTrue(OriginalPawnPair.Pawn.Get()->HasAuthority(),
								TEXT("We should have authority over both original pawn and player controller on their initial server"));
					RequireTrue(OriginalPawnPair.Controller.Get()->GetPawn() == OriginalPawnPair.Pawn.Get(),
								TEXT("The player controller should have possession over its original pawn"));
				}
			}
			FinishStep();
		});
}
