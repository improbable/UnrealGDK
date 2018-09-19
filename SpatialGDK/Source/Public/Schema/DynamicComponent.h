// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Platform.h"

#include "Schema/Component.h"
#include "Utils/SchemaUtils.h"

#include <improbable/c_schema.h>
#include <improbable/c_worker.h>

namespace improbable
{

// Represents any Unreal rep component
struct DynamicComponent : Component
{
	DynamicComponent()
	{
		bIsDynamic = true;
	}

	DynamicComponent(const Worker_ComponentData& InData)
		: Data(Worker_AcquireComponentData(&InData))
	{
		bIsDynamic = true;
	}

	~DynamicComponent()
	{
		Worker_ReleaseComponentData(Data);
	}

	Worker_ComponentData* Data;
};

}
