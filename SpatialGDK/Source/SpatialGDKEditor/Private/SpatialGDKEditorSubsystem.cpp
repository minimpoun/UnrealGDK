// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "SpatialGDKEditorSubsystem.h"
#include "SpatialGDKEditorSchemaGenerator.h"
#include "Editor.h"
#include "SchemaDatabase.h"

void USpatialGDKEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogTemp, Display, TEXT("-------------------- GDK Editor Subsystem --------------------"));
	if (GEditor && !IsRunningCommandlet())
	{
		SchemaDatabase = TryLoadExistingSchemaDatabase();
		UE_LOG(LogTemp, Display, TEXT("Attaching Callback to Blueprint Recompile"));
		GEditor->OnObjectsReplaced().AddLambda([this](TMap<UObject*, UObject*> ReplacementMap) {
			if (GEditor->GetEditorWorldContext().World() == nullptr)
			{
				UE_LOG(LogTemp, Display, TEXT("World not ready, don't generate schema yet."));
				return;
			}

			TSet<UClass*> ModifiedClasses;
			for (auto& Entry : ReplacementMap)
			{
				if (const UObject* NewObject = Entry.Value)
				{

					if (NewObject->HasAllFlags(RF_ArchetypeObject) && !NewObject->HasAnyFlags(RF_NeedPostLoad | RF_NeedPostLoadSubobjects))
					{
						/*UE_LOG(LogTemp, Display, TEXT("[%s] ArchetypeObject"), *GetPathNameSafe(NewObject));
						UE_LOG(LogTemp, Display, TEXT("[%s] ObjectFlags: %#010x"), *GetPathNameSafe(NewObject), NewObject->GetFlags());
						UE_LOG(LogTemp, Display, TEXT("[%s] ClassPath: %s"), *GetPathNameSafe(NewObject), *NewObject->GetClass()->GetPathName());
						UE_LOG(LogTemp, Display, TEXT("[%s] ClassFlags: %#010x"), *GetPathNameSafe(NewObject), NewObject->GetClass()->GetClassFlags());*/
						ModifiedClasses.Add(NewObject->GetClass());
					}
				}
			}
			if (ModifiedClasses.Num() > 0)
			{
				UE_LOG(LogTemp, Display, TEXT("Generating Schema for %d Classes"), ModifiedClasses.Num());

				for (auto& Class : ModifiedClasses)
				{
					//UE_LOG(LogTemp, Display, TEXT("Class: [%s]"), *GetPathNameSafe(Class));
				}
				SpatialGDKGenerateSchema(ModifiedClasses, false);
			}
		});
	}
}

void USpatialGDKEditorSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Display, TEXT("-------------------- GDK Editor Subsystem Deinit --------------------"));
}
