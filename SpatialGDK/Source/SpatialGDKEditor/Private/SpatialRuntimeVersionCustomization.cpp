// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialRuntimeVersionCustomization.h"

#include "SpatialGDKEditorSettings.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FSpatialRuntimeVersionCustomization::MakeInstance()
{
	return MakeShareable(new FSpatialRuntimeVersionCustomization);
}

void FSpatialRuntimeVersionCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
}

void FSpatialRuntimeVersionCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const FName& PinnedGDKPropertyName = GET_MEMBER_NAME_CHECKED(FRuntimeVariantVersion, bUseGDKPinnedRuntimeVersion);
	const FName& PinnedGDKPropertyVersionName = GET_MEMBER_NAME_CHECKED(FRuntimeVariantVersion, PinnedVersion);

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	// Get the correct pinned version name
	FString PinnedVersionString;
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> ChildProperty = StructPropertyHandle->GetChildHandle(ChildIdx);
		if (ChildProperty->GetProperty()->GetFName() == PinnedGDKPropertyVersionName)
		{
			ChildProperty->GetValueAsFormattedString(PinnedVersionString);
		}
	}

	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> ChildProperty = StructPropertyHandle->GetChildHandle(ChildIdx);

		// Layout other properties as usual.
		if (ChildProperty->GetProperty()->GetFName() != PinnedGDKPropertyName)
		{
			StructBuilder.AddProperty(ChildProperty.ToSharedRef());
			continue;
		}

		IDetailPropertyRow& CustomRow = StructBuilder.AddProperty(ChildProperty.ToSharedRef());

		FString PinnedVersionDisplay = FString::Printf(TEXT("GDK Pinned Version : %s"), *PinnedVersionString);

		CustomRow.CustomWidget()
			.NameContent()
			[
				ChildProperty->CreatePropertyNameWidget()
			]
		.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				ChildProperty->CreatePropertyValueWidget()
			]
		+ SHorizontalBox::Slot()
			.Padding(5)
			.HAlign(HAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(PinnedVersionDisplay))
			]
			];
	}
}
