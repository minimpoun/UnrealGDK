// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Commandlets/Commandlet.h"
#include "Commandlets/CookCommandlet.h"

#include "GenerateSchemaCommandlet.generated.h"

UCLASS()
class UGenerateSchemaCommandlet : public UCookCommandlet
{
	GENERATED_BODY()

public:
	UGenerateSchemaCommandlet();

public:
	virtual int32 Main(const FString& Params) override;
};
