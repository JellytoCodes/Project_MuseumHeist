#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"

#include "Net/UnrealNetwork.h"

AHeistObjectDisplayCaseActor::AHeistObjectDisplayCaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AHeistObjectDisplayCaseActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyState);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyRevision);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyReplicaData);
}

FName AHeistObjectDisplayCaseActor::GetObjectCaseId() const
{
	return ObjectCaseId;
}

FName AHeistObjectDisplayCaseActor::GetTargetArtifactId() const
{
	return TargetObjectArtifactId;
}

FName AHeistObjectDisplayCaseActor::GetObjectFamilyId() const
{
	return ObjectFamilyId;
}

EHeistObjectAssemblyState AHeistObjectDisplayCaseActor::GetAssemblyState() const
{
	return AssemblyState;
}

int32 AHeistObjectDisplayCaseActor::GetAssemblyRevision() const
{
	return AssemblyRevision;
}

FHeistObjectAssemblyReplicaData AHeistObjectDisplayCaseActor::GetAssemblyReplicaData() const
{
	return AssemblyReplicaData;
}

bool AHeistObjectDisplayCaseActor::CanInteract(const AActor* /*Interactor*/) const
{
	// TASK-W5-018 owns session authority and will enable validated interaction.
	return false;
}

void AHeistObjectDisplayCaseActor::SetObjectIdentityForLegacyMigration(const FName InObjectCaseId, const FName InTargetArtifactId, const FName InObjectFamilyId)
{
	if (!InObjectCaseId.IsNone())
	{
		ObjectCaseId = InObjectCaseId;
	}
	if (!InTargetArtifactId.IsNone())
	{
		TargetObjectArtifactId = InTargetArtifactId;
	}
	if (!InObjectFamilyId.IsNone())
	{
		ObjectFamilyId = InObjectFamilyId;
	}
}

void AHeistObjectDisplayCaseActor::OnRep_AssemblyState()
{
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_AssemblyRevision()
{
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_AssemblyReplicaData()
{
	BP_ObjectAssemblySnapshotChanged();
}
