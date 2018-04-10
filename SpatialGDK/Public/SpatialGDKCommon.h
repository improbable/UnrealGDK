// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
// ===========
// DO NOT EDIT - this file is automatically regenerated.
// =========== 


// TODO: This needs to be codegenned

#pragma once

#include "SpatialOSWorkerTypes.h"
#include "improbable/unreal/level_data.h"
#include "improbable/unreal/player.h"
#include "improbable/unreal/spawner.h"
#include "improbable/unreal/unreal_metadata.h"
#include "improbable/unreal/generated/UnrealCharacter.h"
#include "improbable/unreal/generated/UnrealPlayerController.h"
#include "improbable/unreal/generated/UnrealPlayerState.h"
#include "improbable/unreal/generated/UnrealWheeledVehicle.h"
#include "improbable/standard_library.h"

namespace improbable
{
namespace unreal
{
	using Components = worker::Components< improbable::unreal::UnrealLevel,
		improbable::unreal::UnrealLevelPlaceholder,
		improbable::unreal::PlayerControlClient,
		improbable::unreal::PlayerSpawner,
		improbable::unreal::UnrealMetadata,
		improbable::unreal::UnrealCharacterSingleClientRepData,
		improbable::unreal::UnrealCharacterMultiClientRepData,
		improbable::unreal::UnrealCharacterMigratableData,
		improbable::unreal::UnrealCharacterClientRPCs,
		improbable::unreal::UnrealCharacterServerRPCs,
		improbable::unreal::UnrealPlayerControllerSingleClientRepData,
		improbable::unreal::UnrealPlayerControllerMultiClientRepData,
		improbable::unreal::UnrealPlayerControllerMigratableData,
		improbable::unreal::UnrealPlayerControllerClientRPCs,
		improbable::unreal::UnrealPlayerControllerServerRPCs,
		improbable::unreal::UnrealPlayerStateSingleClientRepData,
		improbable::unreal::UnrealPlayerStateMultiClientRepData,
		improbable::unreal::UnrealPlayerStateMigratableData,
		improbable::unreal::UnrealPlayerStateClientRPCs,
		improbable::unreal::UnrealPlayerStateServerRPCs,
		improbable::unreal::UnrealWheeledVehicleSingleClientRepData,
		improbable::unreal::UnrealWheeledVehicleMultiClientRepData,
		improbable::unreal::UnrealWheeledVehicleMigratableData,
		improbable::unreal::UnrealWheeledVehicleClientRPCs,
		improbable::unreal::UnrealWheeledVehicleServerRPCs,
		improbable::EntityAcl,
		improbable::Metadata,
		improbable::Position,
		improbable::Persistence >;

} // ::unreal
} // ::improbable

