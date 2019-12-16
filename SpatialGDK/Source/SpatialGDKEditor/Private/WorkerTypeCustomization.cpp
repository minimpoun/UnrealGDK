// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "WorkerTypeCustomization.h"

#include "SpatialGDKSettings.h"
#include "Utils/SpatialActorGroupManager.h"

#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Widgets/SToolTip.h"

TSharedRef<IPropertyTypeCustomization> FWorkerTypeCustomization::MakeInstance()
{
	return MakeShared<FWorkerTypeCustomization>();
}

void FWorkerTypeCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> WorkerTypeNameProperty = StructPropertyHandle->GetChildHandle("WorkerTypeName");

	if (WorkerTypeNameProperty->IsValidHandle())
	{
		HeaderRow.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			]
		.ValueContent()
			[
				PropertyCustomizationHelpers::MakePropertyComboBox(WorkerTypeNameProperty,
				FOnGetPropertyComboBoxStrings::CreateStatic(&FWorkerTypeCustomization::OnGetStrings, WorkerTypeNameProperty),
				FOnGetPropertyComboBoxValue::CreateStatic(&FWorkerTypeCustomization::OnGetValue, WorkerTypeNameProperty),
				FOnPropertyComboBoxValueSelected::CreateStatic(&FWorkerTypeCustomization::OnValueSelected, WorkerTypeNameProperty))
			];
	}
}

void FWorkerTypeCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FWorkerTypeCustomization::OnGetStrings(TArray<TSharedPtr<FString>>& OutComboBoxStrings, TArray<TSharedPtr<class SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems, TSharedPtr<IPropertyHandle> WorkerTypeNameHandle)
{
	if (!WorkerTypeNameHandle->IsValidHandle())
	{
		return;
	}

	TArray<UObject*> Objects;
	WorkerTypeNameHandle->GetOuterObjects(Objects);

	TSet<FName> RestrictedWorkerTypes;

	if (USetProperty* SetProp = Cast<USetProperty>(WorkerTypeNameHandle->GetParentHandle()->GetParentHandle()->GetProperty()))
	{
		FScriptSetHelper SetHelper = FScriptSetHelper(SetProp, WorkerTypeNameHandle->GetParentHandle()->GetParentHandle()->GetValueBaseAddress((uint8*)(Objects[0])));
		for (int i = 0; i < SetHelper.GetMaxIndex(); i++)
		{
			if (!SetHelper.IsValidIndex(i))
			{
				continue;
			}
			FWorkerType* FoundType = (FWorkerType*) SetHelper.GetElementPtr(i);
			if (FoundType->WorkerTypeName != NAME_None)
			{
				RestrictedWorkerTypes.Add(FoundType->WorkerTypeName);
			}
		}
	}

	// If this is inside FActorGroupInfo::ExcludedWorkers, we should exclude the OwningWorkerType too.
	if (UStructProperty* StructProp = Cast<UStructProperty>(WorkerTypeNameHandle->GetParentHandle()->GetParentHandle()->GetParentHandle()->GetProperty()))
	{
		TSharedPtr<IPropertyHandle> OwningWorkerTypeNameHandle = WorkerTypeNameHandle->GetParentHandle()->GetParentHandle()->GetParentHandle()->GetChildHandle("OwningWorkerType")->GetChildHandle("WorkerTypeName");
		if (OwningWorkerTypeNameHandle->IsValidHandle())
		{
			FName OwningWorkerTypeName;
			OwningWorkerTypeNameHandle->GetValue(OwningWorkerTypeName);
			if (OwningWorkerTypeName != NAME_None)
			{
				RestrictedWorkerTypes.Add(OwningWorkerTypeName);
			}
		}
	}

	if (const USpatialGDKSettings* Settings = GetDefault<USpatialGDKSettings>())
	{
		for (const FName& WorkerType : Settings->ServerWorkerTypes)
		{
			OutComboBoxStrings.Add(MakeShared<FString>(WorkerType.ToString()));
			OutToolTips.Add(SNew(SToolTip).Text(FText::FromName(WorkerType)));
			OutRestrictedItems.Add(RestrictedWorkerTypes.Contains(WorkerType));
		}
	}
}

FString FWorkerTypeCustomization::OnGetValue(TSharedPtr<IPropertyHandle> WorkerTypeNameHandle)
{
	if (!WorkerTypeNameHandle->IsValidHandle())
	{
		return FString();
	}

	FString WorkerTypeValue;

	if (const USpatialGDKSettings* Settings = GetDefault<USpatialGDKSettings>())
	{
		WorkerTypeNameHandle->GetValue(WorkerTypeValue);
		const FName WorkerTypeName = FName(*WorkerTypeValue);

		return Settings->ServerWorkerTypes.Contains(WorkerTypeName) ? WorkerTypeValue : TEXT("INVALID");
	}

	return WorkerTypeValue;
}

void FWorkerTypeCustomization::OnValueSelected(const FString& SelectedValue, TSharedPtr<IPropertyHandle> WorkerTypeNameHandle)
{
	if (WorkerTypeNameHandle->IsValidHandle())
	{
		FName CurrentValue;
		WorkerTypeNameHandle->GetValue(CurrentValue);
		FName NewValue = FName(*SelectedValue);

		if (CurrentValue == NewValue) {		
			return;
		}
		
		WorkerTypeNameHandle->SetValue(NewValue);
	}
}
