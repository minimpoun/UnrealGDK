// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "EngineClasses/SpatialLoadBalanceACLEnforcer.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "Interop/Connection/SpatialWorkerConnection.h"
#include "Interop/SpatialStaticComponentView.h"

DEFINE_LOG_CATEGORY(LogSpatialLoadBalanceACLEnforcer);

using namespace SpatialGDK;

USpatialLoadBalanceACLEnforcer::USpatialLoadBalanceACLEnforcer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NetDriver(nullptr)
{
}

void USpatialLoadBalanceACLEnforcer::Init(USpatialNetDriver* InNetDriver)
{
	NetDriver = InNetDriver;
}

void USpatialLoadBalanceACLEnforcer::Tick()
{
	ProcessQueuedAclAssignmentRequests();
}

void USpatialLoadBalanceACLEnforcer::AuthorityChanged(const Worker_AuthorityChangeOp& AuthOp)
{
	if(AuthOp.component_id == SpatialConstants::ENTITY_ACL_COMPONENT_ID &&
	   AuthOp.authority == WORKER_AUTHORITY_AUTHORITATIVE)
	{
		// We have gained authority over the ACL component for some entity
		// If we have authority, the responsibility here is to set the EntityACL write auth to match the worker requested via the virtual worker component
		QueueAclAssignmentRequest(AuthOp.entity_id);
	}
}

void USpatialLoadBalanceACLEnforcer::QueueAclAssignmentRequest(const Worker_EntityId EntityId)
{
	if (!AclWriteAuthAssignmentRequests.Contains(EntityId))
	{
		UE_LOG(LogSpatialLoadBalanceACLEnforcer, Log, TEXT("(%s) Queueing ACL assignment request for %lld"), *NetDriver->Connection->GetWorkerId(), EntityId);
		AclWriteAuthAssignmentRequests.Add(EntityId);
	}
}

void USpatialLoadBalanceACLEnforcer::ProcessQueuedAclAssignmentRequests()
{
	// TODO: add a mechanism to detect when these requests stay in the queue longer than expected
	const int32 Size = AclWriteAuthAssignmentRequests.Num();
	for (int i = Size - 1; i >= 0; i--)
	{
		Worker_EntityId EntityId = AclWriteAuthAssignmentRequests[i];
		const AuthorityIntent* MyAuthorityIntentComponent = NetDriver->StaticComponentView->GetComponentData<SpatialGDK::AuthorityIntent>(EntityId);

		// TODO - if some entities won't have the component we should detect that before queueing the request.
		// Need to be certain it is invalid to get here before receiving the AuthIntentComponent for an entity, then we can check() on it.
		if (!MyAuthorityIntentComponent)
		{
			//UE_LOG(LogSpatialVirtualWorkerTranslator, Warning, TEXT("Detected entity without AuthIntent component"));
			continue;
		}

		const FString& VirtualWorkerId = MyAuthorityIntentComponent->VirtualWorkerId;

		if (VirtualWorkerId.IsEmpty())
		{
			continue;
		}

		const USpatialVirtualWorkerTranslator* VirtualWorkerTranslator = NetDriver->VirtualWorkerTranslator;
		int32 VirtualWorkerIndex;
		VirtualWorkerTranslator->GetVirtualWorkers().Find(VirtualWorkerId, VirtualWorkerIndex);
		check(VirtualWorkerIndex != -1);

		const FString& OwningWorkerId = VirtualWorkerTranslator->GetVirtualWorkerAssignments()[VirtualWorkerIndex];
		if (OwningWorkerId.IsEmpty())
		{
			// A virtual worker -> physical worker mapping may not be established yet.
			// We'll retry on the next Tick().
			continue;
		}

		SetAclWriteAuthority(EntityId, OwningWorkerId);
		AclWriteAuthAssignmentRequests.RemoveAt(i);
	}
}

void USpatialLoadBalanceACLEnforcer::SetAclWriteAuthority(const Worker_EntityId EntityId, const FString& WorkerId)
{
	check(NetDriver);
	if (!NetDriver->StaticComponentView->HasAuthority(EntityId, SpatialConstants::ENTITY_ACL_COMPONENT_ID))
	{
		UE_LOG(LogSpatialLoadBalanceACLEnforcer, Warning, TEXT("(%s) Failing to set Acl WriteAuth for entity %lld to workerid: %s because this worker doesn't have authority over the EntityACL component."), *NetDriver->Connection->GetWorkerId(), EntityId, *WorkerId);
		return;
	}

	EntityAcl* EntityACL = NetDriver->StaticComponentView->GetComponentData<EntityAcl>(EntityId);
	check(EntityACL);

	const FString& WriteWorkerId = FString::Printf(TEXT("workerId:%s"), *WorkerId);

	WorkerAttributeSet OwningWorkerAttribute = { WriteWorkerId };

	TArray<Worker_ComponentId> ComponentIds;
	EntityACL->ComponentWriteAcl.GetKeys(ComponentIds);

	for (int i = 0; i < ComponentIds.Num(); ++i)
	{
		if (ComponentIds[i] == SpatialConstants::ENTITY_ACL_COMPONENT_ID ||
			ComponentIds[i] == SpatialConstants::CLIENT_RPC_ENDPOINT_COMPONENT_ID)
		{
			continue;
		}

		WorkerRequirementSet* RequirementSet = EntityACL->ComponentWriteAcl.Find(ComponentIds[i]);
		check(RequirementSet->Num() == 1);
		RequirementSet->Empty();
		RequirementSet->Add(OwningWorkerAttribute);
	}

	UE_LOG(LogSpatialLoadBalanceACLEnforcer, Log, TEXT("(%s) Setting Acl WriteAuth for entity %lld to workerid: %s"), *NetDriver->Connection->GetWorkerId(), EntityId, *WorkerId);

	Worker_ComponentUpdate Update = EntityACL->CreateEntityAclUpdate();
	NetDriver->Connection->SendComponentUpdate(EntityId, &Update);
}
