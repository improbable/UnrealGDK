// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include <improbable/c_worker.h>

namespace improbable
{

struct Component
{
	virtual ~Component() {}
	virtual void ApplyComponentUpdate(const Worker_ComponentUpdate& Update) {}

	bool bIsDynamic = false;
};

class ComponentStorageBase
{
public:
	virtual ~ComponentStorageBase(){};
	virtual TUniquePtr<ComponentStorageBase> Copy() const = 0;
};

template <typename T>
class ComponentStorage : public ComponentStorageBase
{
public:
	explicit ComponentStorage(const typename T& data) : data{data} {}
	explicit ComponentStorage(typename T&& data) : data{std::move(data)} {}
	~ComponentStorage() override {}

	TUniquePtr<ComponentStorageBase> Copy() const override
	{
		return TUniquePtr<ComponentStorageBase>{new ComponentStorage{data}};
	}

	typename T& Get()
	{
		return data;
	}

private:
	typename T data;
};

}
