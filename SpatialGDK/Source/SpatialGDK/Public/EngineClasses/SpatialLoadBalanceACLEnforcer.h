// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include <WorkerSDK/improbable/c_worker.h>

#include "SpatialLoadBalanceACLEnforcer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialLoadBalanceACLEnforcer, Log, All)

class USpatialNetDriver;

UCLASS()
class USpatialLoadBalanceACLEnforcer : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void Init(USpatialNetDriver* InNetDriver);
	void Tick();

	void AuthorityChanged(const Worker_AuthorityChangeOp& AuthOp);
	void QueueAclAssignmentRequest(const Worker_EntityId EntityId);

private:

	USpatialNetDriver* NetDriver;
	TArray<Worker_EntityId> AclWriteAuthAssignmentRequests;

	void ProcessQueuedAclAssignmentRequests();
	void SetAclWriteAuthority(const Worker_EntityId EntityId, const FString& WorkerId);
};
