// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialTestPropertyReplicationMultiworker.h"
#include "Kismet/GameplayStatics.h"
#include "ReplicatedTestActorBasic.h"

/**
 * "This tests that the data debug mode detects property changes on non-auth servers for basic types.
 * This test contains 2 Servers and 2 Client workers.
 *
 * The flow is as follows:
 * - Setup:
 *  - The authorative server spawns one ReplicatedTestActor
 * - Test:
 *  - All workers check that they can see exactly 1 ReplicatedTestActor.
 *  - The authorative server changes the replicated properties.
 *  - All workers check that the replicated properties have changed.
 *  - The non-auth server changes the replicated properties which generates expected errors.
 *  - Auth server check that the replicated properties are unchanged.
 * - Clean-up:
 *  - ReplicatedTestActor is destroyed using the RegisterAutoDestroyActor helper function.
 */

ASpatialTestPropertyReplicationMultiworker::ASpatialTestPropertyReplicationMultiworker()
	: Super()
{
	Author = "Victoria Bloom";
	Description = TEXT("This tests that the data debug mode detects property changes on non-auth servers for basic types.");
}

void ASpatialTestPropertyReplicationMultiworker::PrepareTest()
{
	Super::PrepareTest();

	if (HasAuthority())
	{
		// Expected errors generated by changing replicated properties on a non-auth server
		AddExpectedLogError(TEXT("ReplicatedTestActorBasic, property changed without authority was ReplicatedIntProperty!"), 1);
		AddExpectedLogError(TEXT("ReplicatedTestActorBasic, property changed without authority was ReplicatedFloatProperty!"), 1);
		AddExpectedLogError(TEXT("ReplicatedTestActorBasic, property changed without authority was bReplicatedBoolProperty!"), 1);
		AddExpectedLogError(TEXT("ReplicatedTestActorBasic, property changed without authority was ReplicatedStringProperty!"), 1);
	}

	AddStep(
		TEXT("The auth server spawns one ReplicatedTestActorBasic"), FWorkerDefinition::Server(1), nullptr,
		[this]() {
			TestActor = GetWorld()->SpawnActor<AReplicatedTestActorBasic>(FVector(0.0f, 0.0f, 50.0f), FRotator::ZeroRotator,
																		  FActorSpawnParameters());
			RegisterAutoDestroyActor(TestActor);

			FinishStep();
		},
		nullptr, 5.0f);

	AddStep(
		TEXT("All workers check that they can see exactly 1 ReplicatedTestActorBasic"), FWorkerDefinition::AllWorkers, nullptr, nullptr,
		[this](float DeltaTime) {
			TArray<AActor*> FoundReplicatedTestActors;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AReplicatedTestActorBasic::StaticClass(), FoundReplicatedTestActors);

			RequireEqual_Int(FoundReplicatedTestActors.Num(), 1,
							 TEXT("The number of AReplicatedTestActorBasic found in the world should equal 1."));

			if (FoundReplicatedTestActors.Num() == 1)
			{
				TestActor = Cast<AReplicatedTestActorBasic>(FoundReplicatedTestActors[0]);
				RequireTrue(IsValid(TestActor), TEXT("The TestActor must be Valid (usable : non-null and not pending kill)."));
				FinishStep();
			}
		},
		5.0f);

	AddStep(
		TEXT("The auth server changes the properties"), FWorkerDefinition::Server(1),
		[this]() -> bool {
			return IsValid(TestActor);
		},
		[this]() {
			// Replicated properties
			TestActor->ReplicatedIntProperty = 99;
			TestActor->ReplicatedFloatProperty = 0.99;
			TestActor->bReplicatedBoolProperty = true;
			TestActor->ReplicatedStringProperty = TEXT("hello");

			// Non-replicated properties
			TestActor->IntProperty = 99;
			TestActor->FloatProperty = 0.99;
			TestActor->bBoolProperty = true;
			TestActor->StringProperty = TEXT("hello");

			FinishStep();
		});

	AddStep(
		TEXT("All workers check that the replicated properties have changed"), FWorkerDefinition::AllWorkers,
		[this]() -> bool {
			return IsValid(TestActor);
		},
		nullptr,
		[this](float DeltaTime) {
			RequireEqual_Int(TestActor->ReplicatedIntProperty, 99, TEXT("The ReplicatedIntProperty should equal 99."));
			RequireEqual_Float(TestActor->ReplicatedFloatProperty, 0.99, TEXT("The ReplicatedFloatProperty should equal 0.99."));
			RequireEqual_Bool(TestActor->bReplicatedBoolProperty, true, TEXT("The bReplicatedBoolProperty should be true."));
			RequireEqual_String(TestActor->ReplicatedStringProperty, TEXT("hello"),
								TEXT("The ReplicatedStringProperty should be 'hello'."));

			FinishStep();
		},
		5.0f);

	// This step generates an expected error for each replicated property
	AddStep(
		TEXT("The non-auth server changes the properties"), FWorkerDefinition::Server(2),
		[this]() -> bool {
			return IsValid(TestActor);
		},
		[this]() {
			// Replicated properties
			TestActor->ReplicatedIntProperty = 55;
			TestActor->ReplicatedFloatProperty = 0.55;
			TestActor->bReplicatedBoolProperty = false;
			TestActor->ReplicatedStringProperty = TEXT("world");

			// Non-replicated properties
			TestActor->IntProperty = 55;
			TestActor->FloatProperty = 0.55;
			TestActor->bBoolProperty = false;
			TestActor->StringProperty = TEXT("world");

			FinishStep();
		});

	AddStep(
		TEXT("Auth server check that the properties still have their original values"), FWorkerDefinition::Server(1),
		[this]() -> bool {
			return IsValid(TestActor);
		},
		nullptr,
		[this](float DeltaTime) {
			RequireEqual_Int(TestActor->ReplicatedIntProperty, 99, TEXT("The ReplicatedIntProperty should equal 99."));
			RequireEqual_Float(TestActor->ReplicatedFloatProperty, 0.99, TEXT("The ReplicatedFloatProperty should equal 0.99."));
			RequireEqual_Bool(TestActor->bReplicatedBoolProperty, true, TEXT("The bReplicatedBoolProperty should be true."));
			RequireEqual_String(TestActor->ReplicatedStringProperty, TEXT("hello"),
								TEXT("The ReplicatedStringProperty should be 'hello'."));

			RequireEqual_Int(TestActor->IntProperty, 99, TEXT("The ReplicatedIntProperty should equal 99."));
			RequireEqual_Float(TestActor->FloatProperty, 0.99, TEXT("The ReplicatedFloatProperty should equal 0.99."));
			RequireEqual_Bool(TestActor->bBoolProperty, true, TEXT("The bReplicatedBoolProperty should be true."));
			RequireEqual_String(TestActor->StringProperty, TEXT("hello"), TEXT("The ReplicatedStringProperty should be 'hello'."));

			FinishStep();
		},
		5.0f);
}
