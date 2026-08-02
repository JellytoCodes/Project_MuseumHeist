#include "UI/ViewModels/HeistHUDViewModel.h"

#include "Character/Components/HeistActionComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"

#pragma region Construction

UHeistHUDViewModel::UHeistHUDViewModel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistHUDViewModel::BeginDestroy()
{
	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetEscapePhaseStateChangedDelegate().RemoveAll(this);
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
		GameState->GetObjectiveStateChangedDelegate().RemoveAll(this);
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
	}

	if (IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetLootTotalsChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetEscapeStateChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetArrestStateChangedDelegate().RemoveAll(this);
	}

	if (IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistHUDViewModel::SetupViewModel(AHeistGameState* InGameState, AHeistPlayerState* InLocalPlayerState, UHeistActionComponent* InActionComponent)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetEscapePhaseStateChangedDelegate().RemoveAll(this);
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
		GameState->GetObjectiveStateChangedDelegate().RemoveAll(this);
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
	}

	if (LocalPlayerState != InLocalPlayerState && IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetLootTotalsChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetEscapeStateChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetArrestStateChangedDelegate().RemoveAll(this);
	}

	if (ActionComponent != InActionComponent && IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;
	ActionComponent = InActionComponent;

	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetPlayerConnectionsChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandlePlayerConnectionsChanged);
		GameState->GetEscapePhaseStateChangedDelegate().RemoveAll(this);
		GameState->GetEscapePhaseStateChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandleEscapePhaseStateChanged);
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
		GameState->GetRareLootEventStateChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandleRareLootEventStateChanged);
		GameState->GetObjectiveStateChangedDelegate().RemoveAll(this);
		GameState->GetObjectiveStateChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandleObjectiveStateChanged);
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
		GameState->GetAlertStateChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandleAlertStateChanged);
	}

	if (IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetPlayerIdentityChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandlePlayerIdentityChanged);
		LocalPlayerState->GetLootTotalsChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetLootTotalsChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandleLootTotalsChanged);
		LocalPlayerState->GetEscapeStateChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetEscapeStateChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandleEscapeStateChanged);
		LocalPlayerState->GetArrestStateChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetArrestStateChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandleArrestStateChanged);
	}

	if (IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
		ActionComponent->GetActionStateChangedDelegate().AddUObject(this, &UHeistHUDViewModel::HandleActionStateChanged);
	}

	RefreshPresentationState();
	RefreshRareLootState();
}

void UHeistHUDViewModel::RefreshPresentationState()
{
	UE_MVVM_SET_PROPERTY_VALUE(LocalLootScore, IsValid(LocalPlayerState) ? LocalPlayerState->GetTotalLootScore() : 0);
	UE_MVVM_SET_PROPERTY_VALUE(LocalLootWeight, IsValid(LocalPlayerState) ? LocalPlayerState->GetTotalLootWeight() : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerId, IsValid(LocalPlayerState) ? LocalPlayerState->HeistPlayerId : INDEX_NONE);
	UE_MVVM_SET_PROPERTY_VALUE(ConnectedPlayerCount, IsValid(GameState) ? GameState->GetConnectedPlayerCount() : 0);
	UE_MVVM_SET_PROPERTY_VALUE(bLocalPlayerEscaped, IsValid(LocalPlayerState) && LocalPlayerState->IsEscaped());
	UE_MVVM_SET_PROPERTY_VALUE(bLocalPlayerArrested, IsValid(LocalPlayerState) && LocalPlayerState->IsArrested());
	UE_MVVM_SET_PROPERTY_VALUE(bEscapePhaseOpen, IsValid(GameState) && GameState->IsEscapePhaseOpen());
	UE_MVVM_SET_PROPERTY_VALUE(bEscapeCastActive, IsValid(ActionComponent) && ActionComponent->IsEscapeCastActive());
	UE_MVVM_SET_PROPERTY_VALUE(EscapeCastEndServerTime, IsValid(ActionComponent) ? ActionComponent->GetEscapeCastEndServerTime() : 0.0f);
	const bool bLocalObservationCastActive = IsValid(ActionComponent) && ActionComponent->IsObservationCastActive();
	const FName ActiveObjectiveArtifactId = IsValid(GameState) ? GameState->GetActiveTargetArtifactId() : NAME_None;
	const FName ActiveObjectiveCaseId = IsValid(GameState) ? GameState->GetActiveTargetCaseId() : NAME_None;
	const EHeistObjectiveState ActiveObjectiveState = IsValid(GameState) ? GameState->GetObjectiveState() : EHeistObjectiveState::Inactive;
	const EHeistAlertLevel ActiveAlertLevel = IsValid(GameState) ? GameState->GetAlertLevel() : EHeistAlertLevel::Quiet;

	UE_MVVM_SET_PROPERTY_VALUE(bObservationCastActive, bLocalObservationCastActive);
	UE_MVVM_SET_PROPERTY_VALUE(ObservationCastEndServerTime, bLocalObservationCastActive ? ActionComponent->GetObservationCastEndServerTime() : 0.0f);
	// This ViewModel is constructed from the locally owned PlayerState and ActionComponent.
	// Remote players can replicate the cast state, but their HUD never consumes this instance.
	UE_MVVM_SET_PROPERTY_VALUE(bObservationReferenceVisible, bLocalObservationCastActive);
	UE_MVVM_SET_PROPERTY_VALUE(ObservationReferenceArtifactId, bLocalObservationCastActive ? ActiveObjectiveArtifactId : NAME_None);
	UE_MVVM_SET_PROPERTY_VALUE(ObjectiveArtifactId, ActiveObjectiveArtifactId);
	UE_MVVM_SET_PROPERTY_VALUE(ObjectiveCaseId, ActiveObjectiveCaseId);
	UE_MVVM_SET_PROPERTY_VALUE(ObjectiveState, ActiveObjectiveState);

	const int32 NewSecurityLevel = FMath::Clamp(static_cast<int32>(ActiveAlertLevel), 0, 4);
	FString SecurityLevelStars;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (Index > 0)
		{
			SecurityLevelStars += TEXT(" ");
		}
		SecurityLevelStars += Index < NewSecurityLevel ? TEXT("\u2605") : TEXT("\u2606");
	}
	const FText NewAlertBannerText =
		FText::Format(NSLOCTEXT("HeistHUD", "SecurityLevelFormat", "경계 단계 {0}/4  {1}"), FText::AsNumber(NewSecurityLevel), FText::FromString(SecurityLevelStars));
	FLinearColor NewAlertColor;
	switch (ActiveAlertLevel)
	{
	case EHeistAlertLevel::Suspicious:
		NewAlertColor = FLinearColor(1.0f, 0.68f, 0.12f);
		break;
	case EHeistAlertLevel::Searching:
		NewAlertColor = FLinearColor(1.0f, 0.30f, 0.05f);
		break;
	case EHeistAlertLevel::Alarmed:
		NewAlertColor = FLinearColor(1.0f, 0.04f, 0.02f);
		break;
	case EHeistAlertLevel::Lockdown:
		NewAlertColor = FLinearColor(0.72f, 0.0f, 0.0f);
		break;
	case EHeistAlertLevel::Quiet:
	default:
		NewAlertColor = FLinearColor(0.45f, 0.58f, 0.70f);
		break;
	}

	const bool bLockdownCountdownActive = IsValid(GameState) && GameState->IsLockdownCountdownActive();
	UE_MVVM_SET_PROPERTY_VALUE(AlertLevel, ActiveAlertLevel);
	UE_MVVM_SET_PROPERTY_VALUE(SecurityLevel, NewSecurityLevel);
	UE_MVVM_SET_PROPERTY_VALUE(AlertBannerText, NewAlertBannerText);
	UE_MVVM_SET_PROPERTY_VALUE(AlertColor, NewAlertColor);
	UE_MVVM_SET_PROPERTY_VALUE(bLockdownCountdownVisible, bLockdownCountdownActive);
	UE_MVVM_SET_PROPERTY_VALUE(LockdownCountdownEndServerTime, bLockdownCountdownActive ? GameState->GetAlertNextTransitionServerTime() : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bSuspenseMusicActive, ActiveAlertLevel == EHeistAlertLevel::Suspicious || ActiveAlertLevel == EHeistAlertLevel::Searching);
	UE_MVVM_SET_PROPERTY_VALUE(bAlarmMusicActive, ActiveAlertLevel == EHeistAlertLevel::Alarmed || ActiveAlertLevel == EHeistAlertLevel::Lockdown);

	FString ArtifactDisplayName = ActiveObjectiveArtifactId.ToString();
	ArtifactDisplayName.ReplaceInline(TEXT("_"), TEXT(" "));
	const FText ArtifactLabel =
		ActiveObjectiveArtifactId.IsNone() ? NSLOCTEXT("HeistHUD", "UnknownObjectiveArtifact", "목표 유물") : FText::FromString(ArtifactDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(ObservationReferenceText,
							   bLocalObservationCastActive ? FText::Format(NSLOCTEXT("HeistHUD", "ObservationReferenceFormat", "참고 작품  {0}"), ArtifactLabel)
														   : FText::GetEmpty());

	FText NewObjectiveStateText;
	switch (ActiveObjectiveState)
	{
	case EHeistObjectiveState::Available:
		NewObjectiveStateText = FText::Format(NSLOCTEXT("HeistHUD", "ObjectiveAvailable", "목표  {0} 훔치기"), ArtifactLabel);
		break;
	case EHeistObjectiveState::InProgress:
		NewObjectiveStateText = FText::Format(NSLOCTEXT("HeistHUD", "ObjectiveInProgress", "목표  {0} 진행 중"), ArtifactLabel);
		break;
	case EHeistObjectiveState::Completed:
		NewObjectiveStateText = NSLOCTEXT("HeistHUD", "ObjectiveCompleted", "목표 완료");
		break;
	case EHeistObjectiveState::Failed:
		NewObjectiveStateText = NSLOCTEXT("HeistHUD", "ObjectiveFailed", "목표 실패");
		break;
	case EHeistObjectiveState::Inactive:
	default:
		NewObjectiveStateText = FText::GetEmpty();
		break;
	}
	UE_MVVM_SET_PROPERTY_VALUE(ObjectiveStateText, NewObjectiveStateText);

	PresentationChangedDelegate.Broadcast();
}

void UHeistHUDViewModel::RefreshRareLootState()
{
	const FHeistRareLootEventState State = IsValid(GameState) ? GameState->GetRareLootEventState() : FHeistRareLootEventState();

	UE_MVVM_SET_PROPERTY_VALUE(bRareLootIncoming, State.bIncomingWarningActive);
	UE_MVVM_SET_PROPERTY_VALUE(bRareLootDirectionMarkerVisible, State.bDirectionMarkerActive);
	UE_MVVM_SET_PROPERTY_VALUE(RareLootEventIndex, State.EventIndex);
	UE_MVVM_SET_PROPERTY_VALUE(RareLootItemId, State.ItemId);
	UE_MVVM_SET_PROPERTY_VALUE(RareLootWorldLocation, FVector(State.WorldLocation));
	UE_MVVM_SET_PROPERTY_VALUE(RareLootSpawnServerTime, State.SpawnServerTime);
	RareLootPresentationChangedDelegate.Broadcast();
}

void UHeistHUDViewModel::HandleRareLootEventStateChanged(const FHeistRareLootEventState&)
{
	RefreshRareLootState();
}

void UHeistHUDViewModel::HandlePlayerConnectionsChanged(const int32)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandlePlayerIdentityChanged(const int32)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleAlertStateChanged(const EHeistAlertLevel, const EHeistAlertLevel, const int32, const FName)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleObjectiveStateChanged(const FName, const FName, const EHeistObjectiveState, AHeistPlayerState*)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleEscapePhaseStateChanged(const bool)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleLootTotalsChanged(const int32, const float)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleEscapeStateChanged(const bool)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleArrestStateChanged(const bool)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleActionStateChanged()
{
	RefreshPresentationState();
	UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Observation presentation refreshed: LocalPlayerId=%d Active=%s ReferenceVisible=%s Artifact=%s EndServerTime=%.2f ObjectiveState=%d OwnerOnly=true"),
		   *GetName(), LocalPlayerId, bObservationCastActive ? TEXT("true") : TEXT("false"), bObservationReferenceVisible ? TEXT("true") : TEXT("false"), *ObservationReferenceArtifactId.ToString(),
		   ObservationCastEndServerTime, static_cast<int32>(ObjectiveState));
}

FHeistHUDPresentationChanged& UHeistHUDViewModel::GetPresentationChangedDelegate()
{
	return PresentationChangedDelegate;
}

FHeistRareLootPresentationChanged& UHeistHUDViewModel::GetRareLootPresentationChangedDelegate()
{
	return RareLootPresentationChangedDelegate;
}

#pragma endregion

#pragma region GeneralPresentation

int32 UHeistHUDViewModel::GetLocalLootScore() const
{
	return LocalLootScore;
}

float UHeistHUDViewModel::GetLocalLootWeight() const
{
	return LocalLootWeight;
}

int32 UHeistHUDViewModel::GetLocalPlayerId() const
{
	return LocalPlayerId;
}

int32 UHeistHUDViewModel::GetConnectedPlayerCount() const
{
	return ConnectedPlayerCount;
}

bool UHeistHUDViewModel::IsLocalPlayerEscaped() const
{
	return bLocalPlayerEscaped;
}

bool UHeistHUDViewModel::IsLocalPlayerArrested() const
{
	return bLocalPlayerArrested;
}

bool UHeistHUDViewModel::IsEscapePhaseOpen() const
{
	return bEscapePhaseOpen;
}

bool UHeistHUDViewModel::IsEscapeCastActive() const
{
	return bEscapeCastActive;
}

float UHeistHUDViewModel::GetEscapeCastEndServerTime() const
{
	return EscapeCastEndServerTime;
}

bool UHeistHUDViewModel::IsObservationCastActive() const
{
	return bObservationCastActive;
}

float UHeistHUDViewModel::GetObservationCastEndServerTime() const
{
	return ObservationCastEndServerTime;
}

bool UHeistHUDViewModel::IsObservationReferenceVisible() const
{
	return bObservationReferenceVisible;
}

FName UHeistHUDViewModel::GetObservationReferenceArtifactId() const
{
	return ObservationReferenceArtifactId;
}

FName UHeistHUDViewModel::GetObjectiveArtifactId() const
{
	return ObjectiveArtifactId;
}

FName UHeistHUDViewModel::GetObjectiveCaseId() const
{
	return ObjectiveCaseId;
}

EHeistObjectiveState UHeistHUDViewModel::GetObjectiveState() const
{
	return ObjectiveState;
}

const FText& UHeistHUDViewModel::GetObservationReferenceText() const
{
	return ObservationReferenceText;
}

const FText& UHeistHUDViewModel::GetObjectiveStateText() const
{
	return ObjectiveStateText;
}

EHeistAlertLevel UHeistHUDViewModel::GetAlertLevel() const
{
	return AlertLevel;
}

int32 UHeistHUDViewModel::GetSecurityLevel() const
{
	return SecurityLevel;
}

const FText& UHeistHUDViewModel::GetAlertBannerText() const
{
	return AlertBannerText;
}

FLinearColor UHeistHUDViewModel::GetAlertColor() const
{
	return AlertColor;
}

bool UHeistHUDViewModel::IsLockdownCountdownVisible() const
{
	return bLockdownCountdownVisible;
}

float UHeistHUDViewModel::GetLockdownCountdownEndServerTime() const
{
	return LockdownCountdownEndServerTime;
}

bool UHeistHUDViewModel::IsSuspenseMusicActive() const
{
	return bSuspenseMusicActive;
}

bool UHeistHUDViewModel::IsAlarmMusicActive() const
{
	return bAlarmMusicActive;
}

#pragma endregion

#pragma region RareLootPresentation

bool UHeistHUDViewModel::IsRareLootIncoming() const
{
	return bRareLootIncoming;
}

bool UHeistHUDViewModel::IsRareLootDirectionMarkerVisible() const
{
	return bRareLootDirectionMarkerVisible;
}

int32 UHeistHUDViewModel::GetRareLootEventIndex() const
{
	return RareLootEventIndex;
}

FName UHeistHUDViewModel::GetRareLootItemId() const
{
	return RareLootItemId;
}

FVector UHeistHUDViewModel::GetRareLootWorldLocation() const
{
	return RareLootWorldLocation;
}

float UHeistHUDViewModel::GetRareLootSpawnServerTime() const
{
	return RareLootSpawnServerTime;
}

#pragma endregion
