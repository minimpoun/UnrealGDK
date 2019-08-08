// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "SpatialGDKEditorSubsystem.h"
#include "Editor.h"

void USpatialGDKEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogTemp, Display, TEXT("-------------------- GDK Editor Subsystem --------------------"));

	if (GEditor)
	{
		UE_LOG(LogTemp, Display, TEXT("Subsystem GEditor IsPresent"));


		GEditor->OnObjectsReplaced().AddLambda([](TMap<UObject*, UObject*> ReplacementMap) {
			for (auto& Entry : ReplacementMap)
			{
				if (const UObject* NewObject = Entry.Value)
				{
					if (NewObject == NewObject->GetClass()->GetDefaultObject())
					{
						UE_LOG(LogTemp, Display, TEXT("Replaced CDO %s."), *GetPathNameSafe(NewObject))
					}
				}
			}
		});
	}
}

void USpatialGDKEditorSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Display, TEXT("-------------------- GDK Editor Subsystem --------------------"))
}
