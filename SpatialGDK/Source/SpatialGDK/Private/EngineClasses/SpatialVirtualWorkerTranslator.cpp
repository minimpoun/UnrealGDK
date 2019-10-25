// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "EngineClasses/SpatialVirtualWorkerTranslator.h"
#include "EngineClasses/SpatialGameInstance.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "Interop/Connection/SpatialWorkerConnection.h"
#include "Interop/SpatialStaticComponentView.h"
#include "Interop/SpatialReceiver.h"
#include "Net/UnrealNetwork.h"
#include "Schema/StandardLibrary.h"
#include "SpatialConstants.h"

DEFINE_LOG_CATEGORY(LogSpatialVirtualWorkerTranslator);

using namespace SpatialGDK;

USpatialVirtualWorkerTranslator::USpatialVirtualWorkerTranslator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NetDriver(nullptr)
	, bWorkerEntityQueryInFlight(false)
{
	TranslatorEntityId = SpatialConstants::INITIAL_VIRTUAL_WORKER_TRANSLATOR_ENTITY_ID;
}

void USpatialVirtualWorkerTranslator::Init(USpatialNetDriver* InNetDriver)
{
	NetDriver = InNetDriver;

	// These collections contain static data that is accessible on all server workers via accessor methods
	// This data should likely live somewhere else, but for the purposes of the prototype it's here
	// Zones
	// VirtualWorkers
	// TODO - replace with real data from the editor
	Zones.Add(TEXT("Zone_A"));
	Zones.Add(TEXT("Zone_B"));
	Zones.Add(TEXT("Zone_C"));
	Zones.Add(TEXT("Zone_D"));

	VirtualWorkers.Add("VW_A");
	VirtualWorkers.Add("VW_B");
	VirtualWorkers.Add("VW_C");
	VirtualWorkers.Add("VW_D");
}

void USpatialVirtualWorkerTranslator::AuthorityChanged(const Worker_AuthorityChangeOp& AuthOp)
{
	if (AuthOp.component_id == SpatialConstants::VIRTUAL_WORKER_MANAGER_COMPONENT_ID)
	{
		const bool bAuthoritative = AuthOp.authority == WORKER_AUTHORITY_AUTHORITATIVE;
		UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) Authority over the VirtualWorkerTranslator has changed. This worker %s authority."), *NetDriver->Connection->GetWorkerId(), bAuthoritative ? TEXT("now has") : TEXT("does not have"));

		// Only the VirtualWorkerTranslator has this component, so we can use it to get our real EntityId
		TranslatorEntityId = AuthOp.entity_id;

		if (bAuthoritative)
		{
			for (const FString& VirtualWorker : VirtualWorkers)
			{
				UnassignedVirtualWorkers.Enqueue(VirtualWorker);
			}

			VirtualWorkerAssignment.AddDefaulted(VirtualWorkers.Num());

			QueryForWorkerEntities();
		}
	}
}

void USpatialVirtualWorkerTranslator::AssignWorker(const FString& WorkerId)
{
	check(!UnassignedVirtualWorkers.IsEmpty());
	check(NetDriver->StaticComponentView->HasAuthority(TranslatorEntityId, SpatialConstants::VIRTUAL_WORKER_MANAGER_COMPONENT_ID));

	FString VirtualWorkerId;
	int32 VirtualWorkerIndex;

	UnassignedVirtualWorkers.Dequeue(VirtualWorkerId);
	VirtualWorkers.Find(VirtualWorkerId, VirtualWorkerIndex);
	VirtualWorkerAssignment[VirtualWorkerIndex] = WorkerId;

	UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) Assigned VirtualWorker %s to simulate on Worker %s"), *NetDriver->Connection->GetWorkerId(), *VirtualWorkerId, *VirtualWorkerAssignment[VirtualWorkerIndex]);
}

// TODO: call this from appropriate location
void USpatialVirtualWorkerTranslator::UnassignWorker(const FString& WorkerId)
{
	for (int i = 0; i < VirtualWorkerAssignment.Num(); ++i)
	{
		if (VirtualWorkerAssignment[i].Equals(WorkerId))
		{
			VirtualWorkerAssignment[i] = TEXT("");
			UnassignedVirtualWorkers.Enqueue(VirtualWorkers[i]);

			UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) Unassigned VirtualWorker %s from simulating on Worker %s"), *Cast<USpatialGameInstance>(GetWorld()->GetGameInstance())->GetSpatialWorkerLabel(), *VirtualWorkers[i], *WorkerId);
			break;
		}
	}
}

void USpatialVirtualWorkerTranslator::ApplyVirtualWorkerManagerData(const Worker_ComponentData& Data)
{
	UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) ApplyVirtualWorkerManagerData"), *NetDriver->Connection->GetWorkerId());

	Schema_Object* ComponentObject = Schema_GetComponentDataFields(Data.schema_type);

	VirtualWorkerAssignment.Empty();
	GetStringArrayFromSchema(ComponentObject, SpatialConstants::VIRTUAL_WORKER_MANAGER_ASSIGNMENTS_ID, VirtualWorkerAssignment);
	OnWorkerAssignmentChanged.Broadcast(VirtualWorkerAssignment);

	for (int i = 0; i < VirtualWorkerAssignment.Num(); ++i)
	{
		UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) assignment: %s"), *NetDriver->Connection->GetWorkerId(), *VirtualWorkerAssignment[i]);
	}
}

void USpatialVirtualWorkerTranslator::ApplyVirtualWorkerManagerUpdate(const Worker_ComponentUpdate& Update)
{
	UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) ApplyVirtualWorkerManagerUpdate"), *NetDriver->Connection->GetWorkerId());

	Schema_Object* ComponentObject = Schema_GetComponentUpdateFields(Update.schema_type);

	if (Schema_GetObjectCount(ComponentObject, SpatialConstants::VIRTUAL_WORKER_MANAGER_ASSIGNMENTS_ID) > 0)
	{
		VirtualWorkerAssignment.Empty();
		GetStringArrayFromSchema(ComponentObject, SpatialConstants::VIRTUAL_WORKER_MANAGER_ASSIGNMENTS_ID, VirtualWorkerAssignment);
		OnWorkerAssignmentChanged.Broadcast(VirtualWorkerAssignment);
	}

	for (int i = 0; i < VirtualWorkerAssignment.Num(); ++i)
	{
		UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) assignment: %s"), *NetDriver->Connection->GetWorkerId(), *VirtualWorkerAssignment[i]);
	}

}

void USpatialVirtualWorkerTranslator::ConstructVirtualWorkerMappingFromQueryResponse(const Worker_EntityQueryResponseOp& Op)
{
	for (uint32_t i = 0; i < Op.result_count; ++i)
	{
		for (uint32_t j = 0; j < Op.results[i].component_count; j++)
		{
			Worker_ComponentData Data = Op.results[i].components[j];
			if (Data.component_id == SpatialConstants::WORKER_COMPONENT_ID)
			{
				Schema_Object* ComponentObject = Schema_GetComponentDataFields(Data.schema_type);

				const FString& WorkerType = GetStringFromSchema(ComponentObject, SpatialConstants::WORKER_TYPE_ID);

				if (WorkerType.Equals(SpatialConstants::DefaultServerWorkerType.ToString()) &&
					!UnassignedVirtualWorkers.IsEmpty())
				{
					AssignWorker(GetStringFromSchema(ComponentObject, SpatialConstants::WORKER_ID_ID));
				}
			}
		}
	}
}

void USpatialVirtualWorkerTranslator::SendVirtualWorkerMappingUpdate()
{
	UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) SendVirtualWorkerMappingUpdate"), *NetDriver->Connection->GetWorkerId());

	check(NetDriver->StaticComponentView->HasAuthority(TranslatorEntityId, SpatialConstants::VIRTUAL_WORKER_MANAGER_COMPONENT_ID));

	Worker_ComponentUpdate Update = {};
	Update.component_id = SpatialConstants::VIRTUAL_WORKER_MANAGER_COMPONENT_ID;
	Update.schema_type = Schema_CreateComponentUpdate(SpatialConstants::VIRTUAL_WORKER_MANAGER_COMPONENT_ID);
	Schema_Object* UpdateObject = Schema_GetComponentUpdateFields(Update.schema_type);

	AddStringArrayToSchema(UpdateObject, SpatialConstants::VIRTUAL_WORKER_MANAGER_ASSIGNMENTS_ID, VirtualWorkerAssignment);

	NetDriver->Connection->SendComponentUpdate(TranslatorEntityId, &Update);

	// Broadcast locally since we won't receive the ComponentUpdate on this worker
	OnWorkerAssignmentChanged.Broadcast(VirtualWorkerAssignment);
}

void USpatialVirtualWorkerTranslator::QueryForWorkerEntities()
{
	UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) Sending query for WorkerEntities"), *NetDriver->Connection->GetWorkerId());

	checkf(!bWorkerEntityQueryInFlight, TEXT("(%s) Trying to query for worker entities while a previous query is still in flight!"), *NetDriver->Connection->GetWorkerId());

	Worker_ComponentConstraint WorkerEntityComponentConstraint{};
	WorkerEntityComponentConstraint.component_id = SpatialConstants::WORKER_COMPONENT_ID;

	Worker_Constraint WorkerEntityConstraint{};
	WorkerEntityConstraint.constraint_type = WORKER_CONSTRAINT_TYPE_COMPONENT;
	WorkerEntityConstraint.component_constraint = WorkerEntityComponentConstraint;

	Worker_EntityQuery WorkerEntityQuery{};
	WorkerEntityQuery.constraint = WorkerEntityConstraint;
	WorkerEntityQuery.result_type = WORKER_RESULT_TYPE_SNAPSHOT;

	Worker_RequestId RequestID;
	RequestID = NetDriver->Connection->SendEntityQueryRequest(&WorkerEntityQuery);

	EntityQueryDelegate WorkerEntityQueryDelegate;
	WorkerEntityQueryDelegate.BindLambda([this](const Worker_EntityQueryResponseOp& Op)
	{
		if (!NetDriver->StaticComponentView->HasAuthority(TranslatorEntityId, SpatialConstants::VIRTUAL_WORKER_MANAGER_COMPONENT_ID))
		{
			UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) Received response to WorkerEntityQuery, but don't have authority over VIRTUAL_WORKER_MANAGER_COMPONENT.  Aborting processing."), *NetDriver->Connection->GetWorkerId());
			return;
		}

		if (Op.status_code != WORKER_STATUS_CODE_SUCCESS)
		{
			UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) Could not find Worker Entities via entity query: %s"), *NetDriver->Connection->GetWorkerId(), UTF8_TO_TCHAR(Op.message));
		}
		else if (Op.result_count == 0)
		{
			UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) Worker Entity query shows that Worker Entities do not yet exist in the world."), *NetDriver->Connection->GetWorkerId());
		}
		else
		{
			UE_LOG(LogSpatialVirtualWorkerTranslator, Log, TEXT("(%s) Processing Worker Entity query response"), *NetDriver->Connection->GetWorkerId());
			ConstructVirtualWorkerMappingFromQueryResponse(Op);
			SendVirtualWorkerMappingUpdate();
		}

		bWorkerEntityQueryInFlight = false;
	});

	bWorkerEntityQueryInFlight = true;

	NetDriver->Receiver->AddEntityQueryDelegate(RequestID, WorkerEntityQueryDelegate);
}
