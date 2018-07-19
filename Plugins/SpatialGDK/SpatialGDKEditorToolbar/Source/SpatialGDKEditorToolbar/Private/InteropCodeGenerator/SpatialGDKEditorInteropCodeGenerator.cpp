// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKEditorInteropCodeGenerator.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "SchemaGenerator.h"
#include "TypeBindingGenerator.h"
#include "TypeStructure.h"
#include "Utils/CodeWriter.h"
#include "Utils/ComponentIdGenerator.h"
#include "Utils/DataTypeUtilities.h"

#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY(LogSpatialGDKInteropCodeGenerator);

namespace
{

int GenerateCompleteSchemaFromClass(const FString& SchemaPath, const FString& ForwardingCodePath, int ComponentId, UClass* Class, const TArray<FString>& TypeBindingHeaders)
{
	FCodeWriter OutputSchema;
	FCodeWriter OutputHeader;
	FCodeWriter OutputSource;

	FString SchemaFilename = FString::Printf(TEXT("Unreal%s"), *UnrealNameToSchemaTypeName(Class->GetName()));
	FString TypeBindingFilename = FString::Printf(TEXT("SpatialTypeBinding_%s"), *Class->GetName());

	// Parent and static array index start at 0 for checksum calculations.
	TSharedPtr<FUnrealType> TypeInfo = CreateUnrealTypeInfo(Class, 0, 0, false);

	// Generate schema.
	int NumComponents = GenerateTypeBindingSchema(OutputSchema, ComponentId, Class, TypeInfo, SchemaPath);
	OutputSchema.WriteToFile(FString::Printf(TEXT("%s%s.schema"), *SchemaPath, *SchemaFilename));

	// Generate forwarding code.
	GenerateTypeBindingHeader(OutputHeader, SchemaFilename, TypeBindingFilename, Class, TypeInfo);
	GenerateTypeBindingSource(OutputSource, SchemaFilename, TypeBindingFilename, Class, TypeInfo, TypeBindingHeaders);
	OutputHeader.WriteToFile(FString::Printf(TEXT("%s%s.h"), *ForwardingCodePath, *TypeBindingFilename));
	OutputSource.WriteToFile(FString::Printf(TEXT("%s%s.cpp"), *ForwardingCodePath, *TypeBindingFilename));

	return NumComponents;
}

bool CheckClassNameListValidity(const ClassHeaderMap& Classes)
{
	// Pull out all the class names from the map. (These might contain underscores like "One_TwoThree" and "OneTwo_Three").
	TArray<FString> ClassNames;
	Classes.GetKeys(ClassNames);

	// Remove all underscores from the class names, check for duplicates.
	for (int i = 0; i < ClassNames.Num() - 1; ++i)
	{
		const FString& ClassA = ClassNames[i];
		const FString SchemaTypeA = UnrealNameToSchemaTypeName(ClassA);

		for (int j = i + 1; j < ClassNames.Num(); ++j)
		{
			const FString& ClassB = ClassNames[j];
			const FString SchemaTypeB = UnrealNameToSchemaTypeName(ClassB);

			if (SchemaTypeA.Equals(SchemaTypeB))
			{
				UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Class name collision after removing underscores: '%s' and '%s' - schema not generated"), *ClassA, *ClassB);
				return false;
			}
		}
	}
	return true;
}
}// ::

const FConfigFile* GetConfigFile(const FString& ConfigFilePath)
{
	const FConfigFile* ConfigFile = GConfig->Find(ConfigFilePath, false);
	if (!ConfigFile)
	{
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Could not open .ini file: \"%s\""), *ConfigFilePath);
		return nullptr;
	}
	return ConfigFile;
}

const FConfigSection* GetConfigSection(const FString& ConfigFilePath, const FString& SectionName)
{
	if (const FConfigFile* ConfigFile = GetConfigFile(ConfigFilePath))
	{
		// Get the key-value pairs in the InteropCodeGenSection.
		if (const FConfigSection* Section = ConfigFile->Find(SectionName))
		{
			return Section;
		}
		else
		{
			UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Could not find section '%s' in '%s'."), *SectionName, *ConfigFilePath);
		}
	}
	return nullptr;
}

// Correct for common user mistakes in InteropCodeGen.ClassesToGenerate in
// Config/DefaultEditorSpatialGDK.ini Currently supporting:
//		Forgetting _C in blueprints
//
// This will modify ClassName in place to correct for above-mentioned cases
bool ValidateClassName(FString& ClassName)
{
	UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);

	if (Class)
	{
		return true;
	}

	UE_LOG(LogSpatialGDKInteropCodeGenerator, Verbose, TEXT("Could not find unreal class for interop code generation: '%s', trying to find %s_C..."), *ClassName, *ClassName);

	// Add _C if class doesn't exist in case the user forgot to do so for blueprints.
	ClassName.Append(TEXT("_C"));
	Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);

	if (Class)
	{
		return true;
	}

	UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Could not find unreal class for interop code generation: '%s', skipping."), *ClassName);

	return false;
}

bool GenerateClassHeaderMap(const FConfigSection* UserInteropCodeGenSection, ClassHeaderMap& OutClasses)
{
	TArray<FName> AllCodeGenKeys;
	UserInteropCodeGenSection->GetKeys(AllCodeGenKeys);

	// Iterate over the keys (class names) and extract header includes.
	for (FName ClassKey : AllCodeGenKeys)
	{
		TArray<FString> HeaderValueArray;
		UserInteropCodeGenSection->MultiFind(ClassKey, HeaderValueArray);
		FString ClassName = ClassKey.ToString();

		// Check for class name typos.
		if (!ValidateClassName(ClassName))
		{
			return false;
		}

		// Now, ClassName is a class that must exist.
		// Note this doesn't modify UserInteropCodeGenSection, which still contains old class
		// names without _C.
		OutClasses.Add(ClassName, HeaderValueArray);

		// Just for some user facing logging.
		FString Headers = FString::Join(HeaderValueArray, TEXT(" "));
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Log, TEXT("Found class to generate interop code for: '%s', with includes %s"), *ClassName, *Headers);
	}
	return true;
}

FString GetOutputPath(const FString& ConfigFilePath)
{
	FString OutputPath = FString::Printf(TEXT("%s/Generated/"), FApp::GetProjectName());
	const FString SettingsSectionName = "InteropCodeGen.Settings";
	if (const FConfigSection* SettingsSection = GetConfigSection(ConfigFilePath, SettingsSectionName))
	{
		if (const FConfigValue* OutputModuleSetting = SettingsSection->Find("OutputPath"))
		{
			OutputPath = OutputModuleSetting->GetValue();
		}
	}

	// Ensure that the specified path ends with a path separator.
	OutputPath.AppendChar('/');

	return OutputPath;
}

void GenerateInteropFromClasses(const ClassHeaderMap& Classes, const FString& CombinedSchemaPath, const FString& CombinedForwardingCodePath)
{
	// Component IDs 100000 to 100009 reserved for other SpatialGDK components.
	int ComponentId = 100010;
	for (auto& ClassHeaderList : Classes)
	{
		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassHeaderList.Key);

		const TArray<FString>& TypeBindingHeaders = ClassHeaderList.Value;
		ComponentId += GenerateCompleteSchemaFromClass(CombinedSchemaPath, CombinedForwardingCodePath, ComponentId, Class, TypeBindingHeaders);
	}
}

bool RunProcess(const FString& Command, const FString& Arguments)
{
	int32 ReturnCode = 0;
	FString StandardOutput;

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(*Command, *Arguments, false, true, true, nullptr, 0, nullptr, WritePipe, ReadPipe);

	if (ProcHandle.IsValid())
	{
		FPlatformProcess::WaitForProc(ProcHandle);
		StandardOutput = FPlatformProcess::ReadPipe(ReadPipe);
		FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
	}

	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
	FPlatformProcess::CloseProc(ProcHandle);

	if (ReturnCode != 0)
	{
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("%s"), *StandardOutput);
		return false;
	}

	UE_LOG(LogSpatialGDKInteropCodeGenerator, Display, TEXT("%s"), *StandardOutput);
	return true;
}

bool SpatialGDKGenerateInteropCode()
{
	// SpatialGDK config file definitions.
	const FString FileName = "DefaultEditorSpatialGDK.ini";
	const FString ConfigFilePath = FPaths::SourceConfigDir().Append(FileName);
	// Load the SpatialGDK config file
	GConfig->LoadFile(ConfigFilePath);

	const FString UserClassesSectionName = "InteropCodeGen.ClassesToGenerate";
	const FConfigSection* UserInteropCodeGenSection = GetConfigSection(ConfigFilePath, UserClassesSectionName);

	if (UserInteropCodeGenSection == nullptr)
	{
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Unable to find section 'InteropCodeGen.ClassesToGenerate'."));
		return false;
	}

	if (!GenerateClassHeaderMap(UserInteropCodeGenSection, InteropGeneratedClasses))  // Checks that all classes are found and generate the class mapping.
	{
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Not all classes found; check your DefaultEditorSpatialGDK.ini file."));
		return false;
	}

	if (!CheckClassNameListValidity(InteropGeneratedClasses))
	{
		return false;
	}

	const FString CombinedSchemaIntermediatePath = FPaths::Combine(*FPaths::GetPath(FPaths::GetProjectFilePath()), TEXT("Intermediate/Improbable/"), *FGuid::NewGuid().ToString(), TEXT("/"));
	FString AbsoluteCombinedSchemaIntermediatePath = FPaths::ConvertRelativePathToFull(CombinedSchemaIntermediatePath);
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*AbsoluteCombinedSchemaIntermediatePath);

	const FString CombinedSchemaPath = FPaths::Combine(*FPaths::GetPath(FPaths::GetProjectFilePath()), TEXT("../spatial/schema/improbable/unreal/generated/"));
	FString AbsoluteCombinedSchemaPath = FPaths::ConvertRelativePathToFull(CombinedSchemaPath);

	const FString CombinedIntermediatePath = FPaths::Combine(*FPaths::GetPath(FPaths::GetProjectFilePath()), TEXT("Intermediate/Improbable/"), *FGuid::NewGuid().ToString(), TEXT("/"));
	FString AbsoluteCombinedIntermediatePath = FPaths::ConvertRelativePathToFull(CombinedIntermediatePath);
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*AbsoluteCombinedIntermediatePath);

	const FString CombinedForwardingCodePath = FPaths::Combine(*FPaths::GetPath(FPaths::GameSourceDir()), *GetOutputPath(ConfigFilePath));
	FString AbsoluteCombinedForwardingCodePath = FPaths::ConvertRelativePathToFull(CombinedForwardingCodePath);

	UE_LOG(LogSpatialGDKInteropCodeGenerator, Display, TEXT("Schema path %s - Forwarding code path %s"), *AbsoluteCombinedSchemaPath, *AbsoluteCombinedForwardingCodePath);

	// Check schema path is valid.
	if (!FPaths::CollapseRelativeDirectories(AbsoluteCombinedSchemaPath) ||
		!FPaths::CollapseRelativeDirectories(AbsoluteCombinedForwardingCodePath))
	{
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Path was invalid - schema not generated"));
		return false;
	}

	GenerateInteropFromClasses(InteropGeneratedClasses, AbsoluteCombinedSchemaIntermediatePath, AbsoluteCombinedIntermediatePath);

	const FString DiffCopyPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::GetPath(FPaths::GetProjectFilePath()), TEXT("Scripts/DiffCopy.bat")));

	// Copy Interop files.
	FString DiffCopyArguments = FString::Printf(TEXT("\"%s\" \"%s\" --verbose --remove-input"), *AbsoluteCombinedIntermediatePath, *AbsoluteCombinedForwardingCodePath);
	if (!RunProcess(DiffCopyPath, DiffCopyArguments))
	{
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Could not copy Interop files"));
		return false;
	}

	// Copy schema files
	DiffCopyArguments = FString::Printf(TEXT("\"%s\" \"%s\" --verbose --remove-input"), *AbsoluteCombinedSchemaIntermediatePath, *AbsoluteCombinedSchemaPath);
	if (!RunProcess(DiffCopyPath, DiffCopyArguments))
	{
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Could not copy Schema files."));
		return false;
	}

	// Run Codegen
	const FString CodegenPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::GetPath(FPaths::GetProjectFilePath()), TEXT("Scripts/Codegen.bat")));
	if (!RunProcess(CodegenPath, TEXT("")))
	{
		UE_LOG(LogSpatialGDKInteropCodeGenerator, Error, TEXT("Codegen failed."));
		return false;
	}

	return true;
}
