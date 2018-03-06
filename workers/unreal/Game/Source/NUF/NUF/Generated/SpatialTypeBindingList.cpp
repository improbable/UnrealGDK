// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
// Note that this file has been generated automatically

#include "SpatialTypeBindingList.h"

#include "SpatialTypeBinding_WheeledVehicle.h"
#include "SpatialTypeBinding_PlayerController.h"
#include "SpatialTypeBinding_PlayerState.h"
#include "SpatialTypeBinding_GameStateBase.h"
#include "SpatialTypeBinding_Character.h"

TArray<UClass*> GetGeneratedTypeBindings()
{
	return {
		USpatialTypeBinding_WheeledVehicle::StaticClass(),
		USpatialTypeBinding_PlayerController::StaticClass(),
		USpatialTypeBinding_PlayerState::StaticClass(),
		USpatialTypeBinding_GameStateBase::StaticClass(),
		USpatialTypeBinding_Character::StaticClass()
	};
}
