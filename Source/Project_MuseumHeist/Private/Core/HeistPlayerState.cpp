#include "Core/HeistPlayerState.h"

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

#pragma region ScoreAndWeight

int32 AHeistPlayerState::GetTotalLootScore() const
{
	return TotalLootScore;
}

float AHeistPlayerState::GetTotalLootWeight() const
{
	return TotalLootWeight;
}

FHeistLootTotalsChanged& AHeistPlayerState::GetLootTotalsChangedDelegate()
{
	return LootTotalsChangedDelegate;
}

bool AHeistPlayerState::CanAddLootScoreAndWeight(int32 ScoreDelta, float WeightDelta) const
{
	if (!HasAuthority() || bEscaped || bArrested || ScoreDelta < 0 || WeightDelta < 0.0f || !FMath::IsFinite(WeightDelta))
	{
		return false;
	}

	const int64 NewScore = static_cast<int64>(TotalLootScore) + static_cast<int64>(ScoreDelta);
	const double NewWeight = static_cast<double>(TotalLootWeight) + static_cast<double>(WeightDelta);

	return NewScore <= MAX_int32 && FMath::IsFinite(NewWeight) && NewWeight <= static_cast<double>(TNumericLimits<float>::Max());
}

bool AHeistPlayerState::AddLootScoreAndWeight(int32 ScoreDelta, float WeightDelta)
{
	if (!HasAuthority())
	{
		UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(this, TEXT("NotAuthority"));
		return false;
	}

	if (bEscaped)
	{
		UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(this, TEXT("AlreadyEscaped"));
		return false;
	}

	if (bArrested)
	{
		UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(this, TEXT("PlayerArrested"));
		return false;
	}

	if (!CanAddLootScoreAndWeight(ScoreDelta, WeightDelta))
	{
		UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(this, TEXT("InvalidLootValues"), ScoreDelta, WeightDelta);
		return false;
	}

	TotalLootScore += ScoreDelta;
	TotalLootWeight += WeightDelta;
	ForceNetUpdate();
	BroadcastLootTotalsChanged();

	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->RefreshMovementSpeedFromWeight();
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugWeightMovementSkipped(this, TEXT("MissingCharacter"));
	}

	UHeistDebugFunctionLibrary::DebugLootScoreWeightApplied(this, ScoreDelta, WeightDelta, TotalLootScore, TotalLootWeight);

	return true;
}

bool AHeistPlayerState::CanRemoveLootScoreAndWeight(const int32 ScoreDelta, const float WeightDelta) const
{
	return HasAuthority() && !bEscaped && !bArrested && ScoreDelta >= 0 && WeightDelta >= 0.0f && FMath::IsFinite(WeightDelta) && ScoreDelta <= TotalLootScore &&
		   WeightDelta <= TotalLootWeight + KINDA_SMALL_NUMBER;
}

bool AHeistPlayerState::RemoveLootScoreAndWeight(const int32 ScoreDelta, const float WeightDelta)
{
	if (!CanRemoveLootScoreAndWeight(ScoreDelta, WeightDelta))
	{
		return false;
	}

	CommitLootScoreAndWeightRemoval(ScoreDelta, WeightDelta, true);
	return true;
}

bool AHeistPlayerState::RemoveLootScoreAndWeightForDisconnect(const int32 ScoreDelta, const float WeightDelta)
{
	if (!HasAuthority() || ScoreDelta < 0 || WeightDelta < 0.0f || !FMath::IsFinite(WeightDelta) || ScoreDelta > TotalLootScore ||
		WeightDelta > TotalLootWeight + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	CommitLootScoreAndWeightRemoval(ScoreDelta, WeightDelta, false);
	return true;
}

bool AHeistPlayerState::RemoveCarriedOriginalWeight(const float WeightDelta)
{
	if (!HasAuthority() || !FMath::IsFinite(WeightDelta) || WeightDelta < 0.0f || WeightDelta > TotalLootWeight + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	CommitLootScoreAndWeightRemoval(0, WeightDelta, true);
	return true;
}

void AHeistPlayerState::CommitLootScoreAndWeightRemoval(const int32 ScoreDelta, const float WeightDelta, const bool bRefreshMovement)
{
	TotalLootScore -= ScoreDelta;
	TotalLootWeight = FMath::Max(0.0f, TotalLootWeight - WeightDelta);
	ForceNetUpdate();
	BroadcastLootTotalsChanged();

	if (bRefreshMovement)
	{
		if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
		{
			HeistPlayerCharacter->RefreshMovementSpeedFromWeight();
		}
	}

	UHeistDebugFunctionLibrary::DebugLootScoreWeightRemoved(this, ScoreDelta, WeightDelta, TotalLootScore, TotalLootWeight);
}

void AHeistPlayerState::BroadcastLootTotalsChanged()
{
	LootTotalsChangedDelegate.Broadcast(TotalLootScore, TotalLootWeight);
	RefreshCrewStatus();
}

#pragma endregion

#pragma region Contribution

const FHeistPlayerContribution& AHeistPlayerState::GetContribution() const
{
	return Contribution;
}

void AHeistPlayerState::RecordSurfaceForgeryContribution(const float QualityScore)
{
	if (!HasAuthority() || !FMath::IsFinite(QualityScore))
	{
		return;
	}
	Contribution.SurfaceForgeries = Contribution.SurfaceForgeries == MAX_int32 ? MAX_int32 : Contribution.SurfaceForgeries + 1;
	Contribution.BestSurfaceQuality = FMath::Max(Contribution.BestSurfaceQuality, FMath::Clamp(QualityScore, 0.0f, 100.0f));
	CommitContributionMutation();
}

void AHeistPlayerState::RecordAssemblyContribution(const float QualityScore)
{
	if (!HasAuthority() || !FMath::IsFinite(QualityScore))
	{
		return;
	}
	Contribution.Assemblies = Contribution.Assemblies == MAX_int32 ? MAX_int32 : Contribution.Assemblies + 1;
	Contribution.BestAssemblyQuality = FMath::Max(Contribution.BestAssemblyQuality, FMath::Clamp(QualityScore, 0.0f, 100.0f));
	CommitContributionMutation();
}

void AHeistPlayerState::BeginOriginalCarryContribution()
{
	if (HasAuthority() && OriginalCarryContributionStartServerTime < 0.0f && GetWorld())
	{
		OriginalCarryContributionStartServerTime = GetWorld()->GetTimeSeconds();
	}
}

void AHeistPlayerState::EndOriginalCarryContribution(const int32 RecoveredArtifactCount)
{
	if (!HasAuthority())
	{
		return;
	}
	if (OriginalCarryContributionStartServerTime >= 0.0f && GetWorld())
	{
		Contribution.CarryTimeSeconds += FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - OriginalCarryContributionStartServerTime);
		OriginalCarryContributionStartServerTime = -1.0f;
	}
	if (RecoveredArtifactCount > 0)
	{
		Contribution.ArtifactsRecovered = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Contribution.ArtifactsRecovered) + RecoveredArtifactCount));
	}
	CommitContributionMutation();
}

void AHeistPlayerState::RecordSecuredLootContribution(const int32 SecuredLootValue)
{
	if (!HasAuthority() || SecuredLootValue <= 0)
	{
		return;
	}
	Contribution.SecuredLootValue = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Contribution.SecuredLootValue) + SecuredLootValue));
	CommitContributionMutation();
}

void AHeistPlayerState::RecordGuardDistractionContribution(const int32 DistractedGuardCount)
{
	if (HasAuthority() && DistractedGuardCount > 0)
	{
		Contribution.GuardsDistracted = static_cast<int32>(
			FMath::Min<int64>(MAX_int32, static_cast<int64>(Contribution.GuardsDistracted) + DistractedGuardCount));
		CommitContributionMutation();
	}
}

void AHeistPlayerState::RecordTeammateRescueContribution()
{
	if (HasAuthority())
	{
		Contribution.TeammatesRescued = Contribution.TeammatesRescued == MAX_int32 ? MAX_int32 : Contribution.TeammatesRescued + 1;
		CommitContributionMutation();
	}
}

void AHeistPlayerState::RecordAlarmContribution()
{
	if (HasAuthority())
	{
		Contribution.AlarmsTriggered = Contribution.AlarmsTriggered == MAX_int32 ? MAX_int32 : Contribution.AlarmsTriggered + 1;
		CommitContributionMutation();
	}
}

void AHeistPlayerState::DebugSetContributionState(const FHeistPlayerContribution& NewContribution)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		return;
	}

	Contribution = NewContribution;
	Contribution.SurfaceForgeries = FMath::Max(0, Contribution.SurfaceForgeries);
	Contribution.BestSurfaceQuality = FMath::IsFinite(Contribution.BestSurfaceQuality) ? FMath::Clamp(Contribution.BestSurfaceQuality, 0.0f, 100.0f) : 0.0f;
	Contribution.Assemblies = FMath::Max(0, Contribution.Assemblies);
	Contribution.BestAssemblyQuality = FMath::IsFinite(Contribution.BestAssemblyQuality) ? FMath::Clamp(Contribution.BestAssemblyQuality, 0.0f, 100.0f) : 0.0f;
	Contribution.ArtifactsRecovered = FMath::Max(0, Contribution.ArtifactsRecovered);
	Contribution.CarryTimeSeconds = FMath::IsFinite(Contribution.CarryTimeSeconds) ? FMath::Max(0.0f, Contribution.CarryTimeSeconds) : 0.0f;
	Contribution.SecuredLootValue = FMath::Max(0, Contribution.SecuredLootValue);
	Contribution.GuardsDistracted = FMath::Max(0, Contribution.GuardsDistracted);
	Contribution.TeammatesRescued = FMath::Max(0, Contribution.TeammatesRescued);
	Contribution.AlarmsTriggered = FMath::Max(0, Contribution.AlarmsTriggered);
	OriginalCarryContributionStartServerTime = -1.0f;
	CommitContributionMutation();
#endif
}

void AHeistPlayerState::CommitContributionMutation()
{
	Contribution.bEscaped = bEscaped;
	Contribution.bArrested = bArrested;
	ForceNetUpdate();
}

#pragma endregion

#pragma region EscapeState

bool AHeistPlayerState::IsEscaped() const
{
	return bEscaped;
}

bool AHeistPlayerState::MarkEscaped()
{
	if (!HasAuthority())
	{
		UHeistDebugFunctionLibrary::DebugPlayerEscapeStateRejected(this, TEXT("NotAuthority"));
		return false;
	}

	if (bEscaped)
	{
		UHeistDebugFunctionLibrary::DebugPlayerEscapeStateRejected(this, TEXT("AlreadyEscaped"));
		return false;
	}

	if (bArrested)
	{
		UHeistDebugFunctionLibrary::DebugPlayerEscapeStateRejected(this, TEXT("PlayerArrested"));
		return false;
	}

	bEscaped = true;
	RefreshCrewStatus();
	CommitContributionMutation();

	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->ApplyPlayerStateGameplayRestrictions();
	}

	if (AHeistGameState* MutableHeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		MutableHeistGameState->RebuildPlayerResults();
	}

	ForceNetUpdate();
	EscapeStateChangedDelegate.Broadcast(bEscaped);

	UHeistDebugFunctionLibrary::DebugPlayerEscapeStateCommitted(this, HeistPlayerId);
	if (AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
	{
		HeistGameMode->NotifyPlayerTerminalStateChanged(this, FName(TEXT("PlayerEscaped")));
	}

	return true;
}

FHeistPlayerEscapeStateChanged& AHeistPlayerState::GetEscapeStateChangedDelegate()
{
	return EscapeStateChangedDelegate;
}

void AHeistPlayerState::OnRep_Escaped()
{
	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->ApplyPlayerStateGameplayRestrictions();
	}

	EscapeStateChangedDelegate.Broadcast(bEscaped);

	UHeistDebugFunctionLibrary::DebugPlayerEscapeStateReplicated(this, HeistPlayerId, bEscaped);
}

#pragma endregion

#pragma region ArrestState

bool AHeistPlayerState::IsArrested() const
{
	return bArrested;
}

bool AHeistPlayerState::MarkArrested(AActor* ArrestingGuard)
{
	return SetArrestedInternal(true, ArrestingGuard);
}

bool AHeistPlayerState::ClearArrested()
{
	return SetArrestedInternal(false, nullptr);
}

FHeistPlayerArrestStateChanged& AHeistPlayerState::GetArrestStateChangedDelegate()
{
	return ArrestStateChangedDelegate;
}

bool AHeistPlayerState::SetArrestedInternal(const bool bNewArrested, AActor* ArrestingGuard)
{
	if (!HasAuthority())
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Player arrest state rejected: Reason=NotAuthority"), EHeistDebugLevel::Warning);
		return false;
	}

	if (bNewArrested && bEscaped)
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Player arrest state rejected: Reason=AlreadyEscaped"), EHeistDebugLevel::Warning);
		return false;
	}

	if (bArrested == bNewArrested)
	{
		UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Player arrest state rejected: Reason=NoStateChange Arrested=%s"), bArrested ? TEXT("true") : TEXT("false")),
											EHeistDebugLevel::Warning);
		return false;
	}

	bArrested = bNewArrested;
	RefreshCrewStatus();
	CommitContributionMutation();
	AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn());
	if (IsValid(HeistPlayerCharacter))
	{
		if (bArrested)
		{
			if (UHeistActionComponent* ActionComponent = HeistPlayerCharacter->GetActionComponent())
			{
				ActionComponent->CancelGameplayActions(TEXT("PlayerArrested"));
			}
			if (UHeistInventoryComponent* InventoryComponent = HeistPlayerCharacter->GetInventoryComponent(); IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen())
			{
				InventoryComponent->TrySetInventoryOpen(false);
			}
		}

		HeistPlayerCharacter->ApplyPlayerStateGameplayRestrictions();
	}

	if (AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		HeistGameState->RebuildPlayerResults();
	}

	ForceNetUpdate();
	ArrestStateChangedDelegate.Broadcast(bArrested);
	UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Player arrest state committed: PlayerId=%d Arrested=%s Guard=%s Authority=true"), HeistPlayerId,
														  bArrested ? TEXT("true") : TEXT("false"), *GetNameSafe(ArrestingGuard)));
	if (bArrested)
	{
		if (AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
		{
			HeistGameMode->NotifyPlayerTerminalStateChanged(this, FName(TEXT("PlayerArrested")));
		}
	}
	return true;
}

void AHeistPlayerState::OnRep_Arrested()
{
	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->ApplyPlayerStateGameplayRestrictions();
	}

	ArrestStateChangedDelegate.Broadcast(bArrested);
	UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Player arrest state replicated: PlayerId=%d Arrested=%s"), HeistPlayerId, bArrested ? TEXT("true") : TEXT("false")));
}

#pragma endregion

#pragma region LobbyReady

bool AHeistPlayerState::IsLobbyReady() const
{
	return bLobbyReady;
}

bool AHeistPlayerState::SetLobbyReady(const bool bNewLobbyReady)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (bLobbyReady == bNewLobbyReady)
	{
		return true;
	}

	bLobbyReady = bNewLobbyReady;
	ForceNetUpdate();
	LobbyReadyChangedDelegate.Broadcast(bLobbyReady);
	return true;
}

FHeistLobbyReadyChanged& AHeistPlayerState::GetLobbyReadyChangedDelegate()
{
	return LobbyReadyChangedDelegate;
}

void AHeistPlayerState::OnRep_LobbyReady()
{
	LobbyReadyChangedDelegate.Broadcast(bLobbyReady);
}

#pragma endregion

#pragma region Replication

void AHeistPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	PlayerIdentityChangedDelegate.Broadcast(HeistPlayerId);
}

void AHeistPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	AHeistPlayerState* NewHeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
	if (IsValid(NewHeistPlayerState) && HeistPlayerId >= 1 && HeistPlayerId <= 4)
	{
		NewHeistPlayerState->InitializePlayerIdentity(HeistPlayerId, PlayerColor);
	}
}

void AHeistPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistPlayerState, HeistPlayerId);
	DOREPLIFETIME(AHeistPlayerState, PlayerColor);
	DOREPLIFETIME(AHeistPlayerState, bLobbyReady);
	DOREPLIFETIME_CONDITION(AHeistPlayerState, TotalLootScore, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHeistPlayerState, TotalLootWeight, COND_OwnerOnly);
	DOREPLIFETIME(AHeistPlayerState, bEscaped);
	DOREPLIFETIME(AHeistPlayerState, bArrested);
	DOREPLIFETIME(AHeistPlayerState, Contribution);
	DOREPLIFETIME(AHeistPlayerState, CrewStatus);
}

#pragma region CrewStatus

EHeistCrewStatus AHeistPlayerState::GetCrewStatus() const
{
	return CrewStatus;
}

FText AHeistPlayerState::GetHeistDisplayName() const
{
	const FString ReplicatedName = GetPlayerName();
	if (!ReplicatedName.TrimStartAndEnd().IsEmpty())
	{
		return FText::FromString(ReplicatedName);
	}
	return HeistPlayerId >= 1 ? FText::Format(NSLOCTEXT("HeistCrewStatus", "FallbackPlayerName", "PLAYER {0}"), FText::AsNumber(HeistPlayerId))
							  : NSLOCTEXT("HeistCrewStatus", "UnknownPlayerName", "PLAYER");
}

bool AHeistPlayerState::RefreshCrewStatus()
{
	if (!HasAuthority())
	{
		return false;
	}

	const EHeistCrewStatus NewStatus = ResolveCrewStatusFromPawn();
	if (CrewStatus == NewStatus)
	{
		return false;
	}

	CrewStatus = NewStatus;
	ForceNetUpdate();
	CrewStatusChangedDelegate.Broadcast(CrewStatus);
	return true;
}

FHeistCrewStatusChanged& AHeistPlayerState::GetCrewStatusChangedDelegate()
{
	return CrewStatusChangedDelegate;
}

EHeistCrewStatus AHeistPlayerState::ResolveCrewStatusFromPawn() const
{
	if (bEscaped)
	{
		return EHeistCrewStatus::Escaped;
	}
	if (bArrested)
	{
		return EHeistCrewStatus::Arrested;
	}

	const AHeistPlayerCharacter* Character = Cast<AHeistPlayerCharacter>(GetPawn());
	if (!IsValid(Character))
	{
		return EHeistCrewStatus::Active;
	}
	const UHeistStatusComponent* Status = Character->GetStatusComponent();
	if (IsValid(Status) && Status->HasStatusTag(FHeistGameplayTags::Get().Event_Player_Stunned))
	{
		return EHeistCrewStatus::Stunned;
	}
	const UHeistInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (IsValid(Inventory) && Inventory->GetOriginalArtifactCount() > 0)
	{
		return EHeistCrewStatus::CarryingOriginal;
	}
	const UHeistNoiseEmitterComponent* NoiseEmitter = Character->GetNoiseEmitterComponent();
	if (IsValid(NoiseEmitter) && NoiseEmitter->IsHeavyWeight(TotalLootWeight))
	{
		return EHeistCrewStatus::Heavy;
	}
	const UHeistObjectAssemblyComponent* Assembly = Character->GetObjectAssemblyComponent();
	if (IsValid(Assembly) && Assembly->IsSessionActive())
	{
		return EHeistCrewStatus::Assembling;
	}
	const UHeistForgeryComponent* Forgery = Character->GetForgeryComponent();
	return IsValid(Forgery) && Forgery->IsSessionActive() ? EHeistCrewStatus::Forging : EHeistCrewStatus::Active;
}

void AHeistPlayerState::OnRep_CrewStatus()
{
	CrewStatusChangedDelegate.Broadcast(CrewStatus);
}

#pragma endregion

void AHeistPlayerState::OnRep_TotalLootScore()
{
	BroadcastLootTotalsChanged();
	UHeistDebugFunctionLibrary::DebugPlayerStateScoreReplicated(this, TotalLootScore);
}

void AHeistPlayerState::OnRep_TotalLootWeight()
{
	BroadcastLootTotalsChanged();
	UHeistDebugFunctionLibrary::DebugPlayerStateWeightReplicated(this, TotalLootWeight);
}

#pragma endregion

#pragma region Debug

void AHeistPlayerState::DebugSetTotalLootWeight(const float InWeight)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		return;
	}

	TotalLootWeight = FMath::IsFinite(InWeight) ? FMath::Max(0.0f, InWeight) : 0.0f;
	RefreshCrewStatus();
	ForceNetUpdate();
	BroadcastLootTotalsChanged();
	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->RefreshMovementSpeedFromWeight();
	}
	UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Footstep debug weight committed: PlayerId=%d TotalLootWeight=%.1f Authority=true"), HeistPlayerId, TotalLootWeight));
#endif
}

void AHeistPlayerState::DebugSetResultState(const bool bInEscaped)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		return;
	}

	bEscaped = bInEscaped;
	RefreshCrewStatus();
	CommitContributionMutation();
	ForceNetUpdate();
	EscapeStateChangedDelegate.Broadcast(bEscaped);

	UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Result debug state seeded: PlayerId=%d Escaped=%s"), HeistPlayerId,
															  bEscaped ? TEXT("true") : TEXT("false")));
#endif
}

#pragma endregion

#pragma region PlayerIdentity

void AHeistPlayerState::InitializePlayerIdentity(int32 InHeistPlayerId, const FLinearColor& InPlayerColor)
{
	check(HasAuthority());

	HeistPlayerId = InHeistPlayerId;
	PlayerColor = InPlayerColor;
	ForceNetUpdate();
	PlayerIdentityChangedDelegate.Broadcast(HeistPlayerId);
}

FHeistPlayerIdentityChanged& AHeistPlayerState::GetPlayerIdentityChangedDelegate()
{
	return PlayerIdentityChangedDelegate;
}

void AHeistPlayerState::OnRep_HeistPlayerId()
{
	PlayerIdentityChangedDelegate.Broadcast(HeistPlayerId);
}

void AHeistPlayerState::OnRep_PlayerColor()
{
	PlayerIdentityChangedDelegate.Broadcast(HeistPlayerId);
}

#pragma endregion
