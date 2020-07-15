// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialFunctionalTestStep.h"
#include "SpatialFunctionalTestFlowController.generated.h"

namespace
{
	constexpr int INVALID_FLOW_CONTROLLER_ID = 0;
}

class ASpatialFunctionalTest;

UCLASS()
class SPATIALGDKFUNCTIONALTESTS_API ASpatialFunctionalTestFlowController : public AActor
{
	GENERATED_BODY()

public:

	ASpatialFunctionalTestFlowController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

	virtual void OnAuthorityGained() override;

	virtual void Tick(float DeltaSeconds) override;

	// Convenience function to know if this FlowController is locally owned
	bool IsLocalController() const;
	
	// # Testing APIs

	// Locally triggers StepIndex Test Step to start
	UFUNCTION(CrossServer, Reliable)
	void CrossServerStartStep(int StepIndex);

	// Tells Test owner that the current Step is finished locally
	void NotifyStepFinished();

	// Tell the Test owner that we want to end the Test 
	void NotifyFinishTest(EFunctionalTestResult TestResult, const FString& Message);
	
	UPROPERTY(Replicated)
	ASpatialFunctionalTest* OwningTest;

	UPROPERTY(Replicated)
	ESpatialFunctionalTestFlowControllerType ControllerType;

	UPROPERTY(Replicated)
	int ControllerInstanceId; //client defined by login order; server maps to virtual worker

	// Prettier way to display type+id combo since it can be quite useful
	const FString GetDisplayName();

	// When Test is finished, this gets triggered. It's mostly important for when a Test was failed during runtime
	void OnTestFinished();

	// Returns if the data regarding the FlowControllers has been replicated to their owners
	bool IsReadyToRunTest() { return ControllerInstanceId != INVALID_FLOW_CONTROLLER_ID && bIsReadyToRunTest; }

	// Each server worker will assign local client ids, this function will be used by
	// the Test owner server worker to guarantee they are all unique
	UFUNCTION(CrossServer, Reliable)
	void CrossServerSetControllerInstanceId(int NewControllerInstanceId);

private:
	// Current Step being executed
	SpatialFunctionalTestStep CurrentStep;

	UPROPERTY(ReplicatedUsing = OnReadyToRegisterWithTest)
	uint8 bReadyToRegisterWithTest : 1;

	UPROPERTY(Replicated)
	bool bIsReadyToRunTest;

	UFUNCTION()
	void OnReadyToRegisterWithTest();

	UFUNCTION(Server, Reliable)
	void ServerSetReadyToRunTest();

	UFUNCTION(Client, Reliable)
	void ClientStartStep(int StepIndex);

	void StartStepInternal(const int StepIndex);
	
	void StopStepInternal();

	UFUNCTION(Server, Reliable)
	void ServerNotifyStepFinished();


	UFUNCTION(CrossServer, Reliable)
	void CrossServerNotifyStepFinished();

	UFUNCTION(Server, Reliable)
	void ServerNotifyFinishTest(EFunctionalTestResult TestResult, const FString& Message);
	
	void ServerNotifyFinishTestInternal(EFunctionalTestResult TestResult, const FString& Message);
};
