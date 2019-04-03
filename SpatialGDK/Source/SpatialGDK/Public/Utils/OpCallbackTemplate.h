// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Interop/SpatialStaticComponentView.h"
#include "SpatialConstants.h"

#include <WorkerSDK/improbable/c_worker.h>

#include "OpCallbackTemplate.generated.h"

UCLASS(abstract)
class SPATIALGDK_API UOpCallbackTemplate : public UObject
{
	GENERATED_BODY()

public:
	UOpCallbackTemplate() = default;
	~UOpCallbackTemplate() = default;

	/**
	  * Called by SpatialDispatcher on initialization providing UWorld and USpatialStaticComponentView pointers to help the user write more meaningful callbacks.
	  * @param InWorld the UWorld top level hierarchy object.
	  * @param InStaticComponentView the static component view for the worker.
	  */
	void InternalInit(UWorld* InWorld, USpatialStaticComponentView* InStaticComponentView)
	{
		World = InWorld;
		StaticComponentView = InStaticComponentView;
		Init();
	}

	/**
	  * Called by InternalInit on initialization to allow the user to write their own initialization logic.
	  */
	virtual void Init() {}

	/**
	  * Component ID the callbacks should apply to.
	  * Must be between SpatialConstants::MIN_EXTERNAL_SCHEMA_ID and SpatialConstants::MAX_EXTERNAL_SCHEMA_ID.
	  */
	virtual Worker_ComponentId GetComponentId() PURE_VIRTUAL(UOpCallbackTemplate::GetComponentId, return SpatialConstants::INVALID_COMPONENT_ID;);

	/**
	  * Add callbacks for the ops being processed by the SpatialDispatcher for component_id
	  * @param op the Worker_OP C API object reference.
	  */
	virtual void OnAddComponent(const Worker_AddComponentOp& op) {}
	virtual void OnRemoveComponent(const Worker_RemoveComponentOp& op) {}
	virtual void OnComponentUpdate(const Worker_ComponentUpdateOp& op) {}
	virtual void OnAuthorityChange(const Worker_AuthorityChangeOp& op) {}
	virtual void OnCommandRequest(const Worker_CommandRequestOp& op) {}
	virtual void OnCommandResponse(const Worker_CommandResponseOp& op) {}

protected:
	UWorld* World;
	USpatialStaticComponentView* StaticComponentView;
};
