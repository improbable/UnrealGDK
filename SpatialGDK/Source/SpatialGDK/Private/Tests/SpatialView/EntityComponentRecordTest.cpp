// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Tests/TestDefinitions.h"

#include "SpatialView/EntityComponentRecord.h"

#include "EntityComponentTestUtils.h"

#define ENTITYCOMPONENTRECORD_TEST(TestName) \
	GDK_TEST(Core, EntityComponentRecord, TestName)

using namespace SpatialGDK;

namespace
{
}  // anonymous namespace

ENTITYCOMPONENTRECORD_TEST(CanAddComponent)
{
	const Worker_EntityId kTestEntityId = 1337;
	const Worker_ComponentId kTestComponentId = 1338;
	const double kTestValue = 7331;

	auto TestData = CreateTestComponentData(kTestComponentId, kTestValue);

	const TArray<EntityComponentId> ExpectedComponentsRemoved = {};
	TArray<EntityComponentData> ExpectedComponentsAdded;
	ExpectedComponentsAdded.Push(EntityComponentData{ kTestEntityId, TestData.DeepCopy() });

	EntityComponentRecord Storage;
	Storage.AddComponent(kTestEntityId, MoveTemp(TestData));

	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsAdded(), ExpectedComponentsAdded));
	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsRemoved(), ExpectedComponentsRemoved));
	return true;
}

ENTITYCOMPONENTRECORD_TEST(CanRemoveComponent)
{
	const EntityComponentId kEntityComponentId = { 1337, 1338 };

	EntityComponentRecord Storage;
	Storage.RemoveComponent(kEntityComponentId.EntityId, kEntityComponentId.ComponentId);

	const TArray<EntityComponentData> ExpectedComponentsAdded = {};
	const TArray<EntityComponentId> ExpectedComponentsRemoved = { kEntityComponentId };

	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsAdded(), ExpectedComponentsAdded));
	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsRemoved(), ExpectedComponentsRemoved));

	return true;
}

ENTITYCOMPONENTRECORD_TEST(CanAddThenRemoveComponent)
{
	const Worker_EntityId kTestEntityId = 1337;
	const Worker_ComponentId kTestComponentId = 1338;
	const double kTestValue = 7331;

	auto TestData = CreateTestComponentData(kTestComponentId, kTestValue);

	const TArray<EntityComponentData> ExpectedComponentsAdded = {};
	const TArray<EntityComponentId> ExpectedComponentsRemoved = {};

	EntityComponentRecord Storage;
	Storage.AddComponent(kTestEntityId, MoveTemp(TestData));
	Storage.RemoveComponent(kTestEntityId, kTestComponentId);

	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsAdded(), ExpectedComponentsAdded));
	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsRemoved(), ExpectedComponentsRemoved));
	return true;
}

// This should not produce the component added - just cancel removing it
ENTITYCOMPONENTRECORD_TEST(CanRemoveThenAddComponent)
{
	const Worker_EntityId kTestEntityId = 1337;
	const Worker_ComponentId kTestComponentId = 1338;
	const double kTestValue = 7331;

	auto TestData = CreateTestComponentData(kTestComponentId, kTestValue);

	const TArray<EntityComponentData> ExpectedComponentsAdded = {};
	const TArray<EntityComponentId> ExpectedComponentsRemoved = {};

	EntityComponentRecord Storage;
	Storage.RemoveComponent(kTestEntityId, kTestComponentId);
	Storage.AddComponent(kTestEntityId, MoveTemp(TestData));

	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsAdded(), ExpectedComponentsAdded));
	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsRemoved(), ExpectedComponentsRemoved));
	return true;
}

ENTITYCOMPONENTRECORD_TEST(CanApplyUpdateToComponentAdded)
{
	const Worker_EntityId kTestEntityId = 1337;
	const Worker_ComponentId kTestComponentId = 1338;
	const double kTestValue = 7331;
	const double kTestUpdateValue = 7332;

	ComponentData TestData = CreateTestComponentData(kTestComponentId, kTestValue);
	ComponentUpdate TestUpdate = CreateTestComponentUpdate(kTestComponentId, kTestUpdateValue);
	ComponentData ExpectedData = CreateTestComponentData(kTestComponentId, kTestUpdateValue);

	TArray<EntityComponentData> ExpectedComponentsAdded;
	ExpectedComponentsAdded.Push(EntityComponentData{ kTestEntityId, MoveTemp(ExpectedData) });
	const TArray<EntityComponentId> ExpectedComponentsRemoved = {};

	EntityComponentRecord Storage;
	Storage.AddComponent(kTestEntityId, MoveTemp(TestData));
	Storage.AddUpdate(kTestEntityId, MoveTemp(TestUpdate));

	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsAdded(), ExpectedComponentsAdded));
	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsRemoved(), ExpectedComponentsRemoved));
	return true;
}

ENTITYCOMPONENTRECORD_TEST(CanNotApplyUpdateIfNoComponentAdded)
{
	const Worker_EntityId kTestEntityId = 1337;
	const Worker_ComponentId kTestComponentId = 1338;
	const double kTestUpdateValue = 7332;

	ComponentUpdate TestUpdate = CreateTestComponentUpdate(kTestComponentId, kTestUpdateValue);

	const TArray<EntityComponentData> ExpectedComponentsAdded = {};
	const TArray<EntityComponentId> ExpectedComponentsRemoved = {};

	EntityComponentRecord Storage;
	Storage.AddUpdate(kTestEntityId, MoveTemp(TestUpdate));

	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsAdded(), ExpectedComponentsAdded));
	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsRemoved(), ExpectedComponentsRemoved));
	return true;
}

ENTITYCOMPONENTRECORD_TEST(CanApplyCompleteUpdateToComponentAdded)
{
	const Worker_EntityId kTestEntityId = 1337;
	const Worker_ComponentId kTestComponentId = 1338;
	const double kTestValue = 7331;
	const double kTestUpdateValue = 7332;

	ComponentData TestData = CreateTestComponentData(kTestComponentId, kTestValue);
	ComponentData TestUpdate = CreateTestComponentData(kTestComponentId, kTestUpdateValue);
	ComponentData ExpectedData = CreateTestComponentData(kTestComponentId, kTestUpdateValue);

	TArray<EntityComponentData> ExpectedComponentsAdded;
	ExpectedComponentsAdded.Push(EntityComponentData{ kTestEntityId, MoveTemp(ExpectedData) });
	const TArray<EntityComponentId> ExpectedComponentsRemoved = {};

	EntityComponentRecord Storage;
	Storage.AddComponent(kTestEntityId, MoveTemp(TestData));
	Storage.AddComponentAsUpdate(kTestEntityId, MoveTemp(TestUpdate));

	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsAdded(), ExpectedComponentsAdded));
	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsRemoved(), ExpectedComponentsRemoved));
	return true;
}

ENTITYCOMPONENTRECORD_TEST(CanNotApplyCompleteUpdateIfNoComponentAdded)
{
	const Worker_EntityId kTestEntityId = 1337;
	const Worker_ComponentId kTestComponentId = 1338;
	const double kTestUpdateValue = 7332;

	ComponentData TestUpdate = CreateTestComponentData(kTestComponentId, kTestUpdateValue);

	const TArray<EntityComponentData> ExpectedComponentsAdded = {};
	const TArray<EntityComponentId> ExpectedComponentsRemoved = {};

	EntityComponentRecord Storage;
	Storage.AddComponentAsUpdate(kTestEntityId, MoveTemp(TestUpdate));

	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsAdded(), ExpectedComponentsAdded));
	TestTrue(TEXT(""), AreEquivalent(Storage.GetComponentsRemoved(), ExpectedComponentsRemoved));
	return true;
}

