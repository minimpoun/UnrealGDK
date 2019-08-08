// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "SpatialGDKEditorSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class SPATIALGDKEDITOR_API USpatialGDKEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	// Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem
	
};
