// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "GeneratableTestMap.h"
#include "GenerateTestMapsCommandlet.h"
#include "GeneratedTestMap.generated.h"

enum class EMapCategory
{
	CI_PREMERGE,
	CI_PREMERGE_SPATIAL_ONLY,
	CI_NIGHTLY,
	CI_NIGHTLY_SPATIAL_ONLY,
	NO_CI
};

/**
 * The base class for automatically generating test maps.
 * Descend from this and your class should automatically be picked up by the GenerateTestMapsCommandlet for generation.
 */
UCLASS()
class SPATIALGDKFUNCTIONALTESTS_API UGeneratedTestMap : public UObject, public IGeneratableTestMap
{
	GENERATED_BODY()

public:
	UGeneratedTestMap();

	// This constructor with arguments is the one that the descended classes should be using within their no-argument constructor.
	UGeneratedTestMap(EMapCategory MapCategory, FString MapName);

	// Should be used directly after the no argument constructor.
	void Init(EMapCategory InMapCategory, FString InMapName);

	void GenerateMap() override;
	bool SaveMap() override;
	bool GenerateCustomConfig() override;
	virtual bool ShouldGenerateMap() override
	{
		return this->GetClass() != UGeneratedTestMap::StaticClass();
	} // To control whether to generate a map from this class
	FString GetMapName() override { return MapName; }
	static FString GetGeneratedMapFolder();

	// You may be asking: why do we have these two simple functions here (one private AddActorToLevel and one public)?
	// The answer is linkers & compilers. I wanted an easy templated function that will automatically cast the result of adding an actor to
	// a level to the appropriate class, since we basically know it should be our desired class and if it isn't, we can check and crash.
	// However, I have to introduce a layer of indirection, using the non-templated AddActorToLevel, so that the usage of GEditor is
	// localized to the cpp file, so that the include does not leak into the header file. Thus modules relying on SpatialGDKFunctionalTests
	// can define TestMaps without needing UnrealEd (most of the time).
	template <class T>
	T& AddActorToLevel(ULevel* Level, const FTransform& Transform)
	{
		return *CastChecked<T>(AddActorToLevel(Level, T::StaticClass(), Transform));
	}

	// Derived test maps can call this to set the string that will be printed into the .ini file to be used with this map to override
	// settings specifically for this test map
	void SetCustomConfig(FString String) { CustomConfigString = String; }

	// Use this to override the default number of clients when running the test map.
	void SetNumberOfClients(int32 InNumberOfClients) { NumberOfClients = InNumberOfClients; }

	virtual UWorld* GetWorld() const override;

	AActor* AddActorToLevel(ULevel* Level, UClass* Class, const FTransform& Transform);

	void SetMapCategory(const EMapCategory InMapCategory);

protected:
	// This is what test maps descending from this class should normally override to add their own content to the map
	virtual void CreateCustomContentForMap() {}

	UPROPERTY()
	UWorld* World;
	UPROPERTY()
	UStaticMesh* PlaneStaticMesh;
	UPROPERTY()
	UMaterial* BasicShapeMaterial;

private:
	void GenerateBaseMap();
	FString GetPathToSaveTheMap();

	bool bIsValidForGeneration;
	EMapCategory MapCategory;
	FString MapName;
	FString CustomConfigString;
	TOptional<int32> NumberOfClients;

	bool bIsGeneratingMap;
};
