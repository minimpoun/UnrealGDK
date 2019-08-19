// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "GenerateSchemaCommandlet.h"
#include "SpatialGDKEditorCommandletPrivate.h"
#include "SpatialGDKEditor.h"
#include "SpatialGDKEditorSchemaGenerator.h"

UGenerateSchemaCommandlet::UGenerateSchemaCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGenerateSchemaCommandlet::Main(const FString& Args)
{
	UE_LOG(LogSpatialGDKEditorCommandlet, Display, TEXT("Schema Generation Commandlet Started"));

	UE_LOG(LogTemp, Display, TEXT("Running Initial Schema Generation"));
	TryLoadExistingSchemaDatabase();
	SpatialGDKGenerateSchema({}, false);

	UE_LOG(LogTemp, Display, TEXT("Subsystem GEditor IsPresent"));
	GEditor->OnObjectsReplaced().AddLambda([](TMap<UObject*, UObject*> ReplacementMap) {
		TSet<UClass*> ModifiedClasses;
		for (auto& Entry : ReplacementMap)
		{
			if (const UObject* NewObject = Entry.Value)
			{

				if (NewObject->HasAllFlags(RF_ArchetypeObject) && !NewObject->HasAnyFlags(RF_NeedPostLoad | RF_NeedPostLoadSubobjects))
				{
					UE_LOG(LogTemp, Display, TEXT("[%s] Architype object"), *GetPathNameSafe(NewObject));
					UE_LOG(LogTemp, Display, TEXT("[%s] ObjectFlags: %#010x"), *GetPathNameSafe(NewObject), NewObject->GetFlags());
					UE_LOG(LogTemp, Display, TEXT("[%s] ClassPath: %s"), *GetPathNameSafe(NewObject), *NewObject->GetClass()->GetPathName());
					UE_LOG(LogTemp, Display, TEXT("[%s] ClassFlags: %#010x"), *GetPathNameSafe(NewObject), NewObject->GetClass()->GetClassFlags());
					ModifiedClasses.Add(NewObject->GetClass());
				}
			}
		}
		if (ModifiedClasses.Num() > 0)
		{
			UE_LOG(LogTemp, Display, TEXT("Generating Schema for %d Classes"), ModifiedClasses.Num());

			for (auto& Class : ModifiedClasses)
			{
				UE_LOG(LogTemp, Display, TEXT("Class: [%s]"), *GetPathNameSafe(Class));
			}

			SpatialGDKGenerateSchema(ModifiedClasses, false);
		}
	});

	UE_LOG(LogTemp, Display, TEXT("Triggering Cook Commandlet"));
	Super::Main(Args);
	UE_LOG(LogTemp, Display, TEXT("Cook Command Complete, Saving Schemadatabse"));
	SaveSchemaDatabase();
	RunSchemaCompiler();

	return 0;
}
