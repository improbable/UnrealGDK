// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "RegisterAutoDestroyActorsTest.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "GameFramework/Character.h"
#include "LoadBalancing/AbstractLBStrategy.h"
#include "SpatialFunctionalTestFlowController.h"

ARegisterAutoDestroyActorsTestPart1::ARegisterAutoDestroyActorsTestPart1()
{
	Author = "Nuno";
	Description = TEXT("Part1: Verify that server spawned a character and that is is visible to the clients");
}

void ARegisterAutoDestroyActorsTestPart1::PrepareTest()
{
	Super::PrepareTest();
	{ // Step 1 - Spawn Actor On Auth
		AddStep(TEXT("SERVER_1_Spawn"), FWorkerDefinition::Server(1), nullptr, [this]() {
			UWorld* World = GetWorld();
			int32 NumVirtualWorkers = GetNumberOfServerWorkers();

			// spawn 1 per server worker
			// since the information about positioning of the virtual workers is currently hidden, will assume they are all around zero
			// and spawn them in that radius
			FVector SpawnPosition = FVector(200.0f, -200.0f, 0.0f);
			FRotator SpawnPositionRotator = FRotator(0.0f, 360.0f / NumVirtualWorkers, 0.0f);
			for (int32 i = 0; i != NumVirtualWorkers; ++i)
			{
				ACharacter* Character = World->SpawnActor<ACharacter>(SpawnPosition, FRotator::ZeroRotator);
				AssertTrue(IsValid(Character),
						   FString::Printf(TEXT("Spawned ACharacter %s in worker %s"), *GetNameSafe(Character),
										   *GetFlowController(ESpatialFunctionalTestWorkerType::Server, i + 1)->GetDisplayName()));
				SpawnPosition = SpawnPositionRotator.RotateVector(SpawnPosition);
			}

			FinishStep();
		});
	}

	{ // Step 2 - Check If Clients have it
		AddStep(
			TEXT("CLIENT_ALL_CheckActorsSpawned"), FWorkerDefinition::AllClients, nullptr, nullptr,
			[this](float DeltaTime) {
				int32 NumCharactersFound = 0;
				int32 NumCharactersExpected = GetNumberOfServerWorkers();
				UWorld* World = GetWorld();
				for (ACharacter* Character : TActorRange<ACharacter>(World))
				{
					++NumCharactersFound;
				}

				RequireEqual_Int(NumCharactersFound, NumCharactersExpected,
								 TEXT("Clients should observe the correct number of characters in their world."));
				FinishStep();
			},
			5.0f);
	}

	{ // Step 3 - Destroy by all servers that have authority
		AddStep(
			TEXT("SERVER_ALL_RegisterAutoDestroyActors"), FWorkerDefinition::AllServers, nullptr, nullptr,
			[this](float DeltaTime) {
				int32 NumCharactersFound = 0;
				int32 NumCharactersExpected = 1; // 1, because we filter by authority
				ACharacter* FoundAuthoritativeCharacter = nullptr;
				UWorld* World = GetWorld();
				for (ACharacter* Character : TActorRange<ACharacter>(World))
				{
					if (Character->HasAuthority())
					{
						++NumCharactersFound;
						FoundAuthoritativeCharacter = Character;
					}
				}
				if (RequireEqual_Int(NumCharactersFound, NumCharactersExpected,
									 TEXT("Servers should observe the correct number of characters in their world.")))
				{
					AssertTrue(IsValid(FoundAuthoritativeCharacter), FString::Printf(TEXT("Registering ACharacter for destruction: %s"),
																					 *GetNameSafe(FoundAuthoritativeCharacter)));
					RegisterAutoDestroyActor(FoundAuthoritativeCharacter);
				}

				FinishStep();
			},
			5.0f);
	}
}

ARegisterAutoDestroyActorsTestPart2::ARegisterAutoDestroyActorsTestPart2()
{
	Author = "Nuno";
	Description = TEXT("Part2: Verify that the actors have been destroyed across all workers");
}

void ARegisterAutoDestroyActorsTestPart2::PrepareTest()
{
	Super::PrepareTest();
	{
		// Check nobody has characters
		FSpatialFunctionalTestStepDefinition StepDefinition(/*bIsNativeDefinition*/ true);
		StepDefinition.StepName = TEXT("Check No Worker Has Characters");
		StepDefinition.TimeLimit = 0.0f;
		StepDefinition.NativeStartEvent.BindLambda([this]() {
			int32 NumCharactersFound = 0;
			int32 NumCharactersExpected = 0;
			UWorld* World = GetWorld();
			for (ACharacter* Character : TActorRange<ACharacter>(World))
			{
				++NumCharactersFound;
			}
			AssertEqual_Int(NumCharactersFound, NumCharactersExpected,
							FString::Printf(TEXT("Cleanup of ACharacter successful, no ACharacter found by %s"),
											*GetLocalFlowController()->GetDisplayName()));
			FinishStep();
		});

		AddStepFromDefinition(StepDefinition, FWorkerDefinition::AllWorkers);
	}
}
