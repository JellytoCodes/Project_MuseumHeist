#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/Widgets/HeistForgeryWidget.h"
#include "UI/Widgets/HeistHUDWidget.h"
#include "UI/Widgets/HeistObjectAssemblyWidget.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

namespace HeistWeek7AlertPresentationTest
{
constexpr int32 AlertTestPlayerCount = 4;
constexpr int32 SurfaceOwnerPlayerId = 1;
constexpr int32 AssemblyOwnerPlayerId = 2;

struct FAlertPresentationAutomationState
{
	bool bAborted = false;
	bool bCapturedPlaySettings = false;
	EPlayNetMode OriginalNetMode = EPlayNetMode::PIE_Standalone;
	bool bOriginalRunUnderOneProcess = true;
	int32 OriginalClientCount = 1;
	EHeistAlertLevel ExpectedAlertLevel = EHeistAlertLevel::Quiet;
	int32 ExpectedAlertRevision = INDEX_NONE;
	float ExpectedTransitionEndServerTime = 0.0f;
	FName ExpectedTriggerId = NAME_None;
	FName SurfaceCaseId = NAME_None;
	FName ObjectCaseId = NAME_None;
};

TArray<UWorld*> GetPIEWorlds()
{
	TArray<UWorld*> Worlds;
	if (!IsValid(GEngine))
	{
		return Worlds;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (WorldContext.WorldType == EWorldType::PIE && IsValid(World))
		{
			Worlds.Add(World);
		}
	}

	Worlds.Sort([](const UWorld& Left, const UWorld& Right)
	{
		if (Left.GetNetMode() != Right.GetNetMode())
		{
			return static_cast<uint8>(Left.GetNetMode()) < static_cast<uint8>(Right.GetNetMode());
		}
		return Left.GetName() < Right.GetName();
	});
	return Worlds;
}

UWorld* GetServerWorld()
{
	for (UWorld* World : GetPIEWorlds())
	{
		if (IsValid(World) && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_Standalone))
		{
			return World;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetLocalController(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AHeistPlayerController* Controller = Cast<AHeistPlayerController>(It->Get());
		if (IsValid(Controller) && Controller->IsLocalController())
		{
			return Controller;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetOwningControllerById(const int32 PlayerId)
{
	for (UWorld* World : GetPIEWorlds())
	{
		AHeistPlayerController* Controller = GetLocalController(World);
		const AHeistPlayerState* PlayerState = IsValid(Controller) ? Controller->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return Controller;
		}
	}
	return nullptr;
}

AHeistPlayerCharacter* GetServerCharacterById(const int32 PlayerId)
{
	UWorld* ServerWorld = GetServerWorld();
	if (!IsValid(ServerWorld))
	{
		return nullptr;
	}

	for (TActorIterator<AHeistPlayerCharacter> It(ServerWorld); It; ++It)
	{
		AHeistPlayerCharacter* Character = *It;
		const AHeistPlayerState* PlayerState = IsValid(Character) ? Character->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return Character;
		}
	}
	return nullptr;
}

bool AreFourPlayerGameplayWorldsReady()
{
	const TArray<UWorld*> Worlds = GetPIEWorlds();
	UWorld* ServerWorld = GetServerWorld();
	const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (Worlds.Num() != AlertTestPlayerCount || !IsValid(ServerWorld) || !IsValid(GameMode) || !GameMode->IsPlayerCountGuardScalingApplied() ||
		GameMode->GetDifficultyAppliedPlayerCount() != AlertTestPlayerCount)
	{
		return false;
	}

	TSet<int32> LocalPlayerIds;
	for (UWorld* World : Worlds)
	{
		const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
		const AHeistPlayerController* Controller = GetLocalController(World);
		const AHeistPlayerState* PlayerState = IsValid(Controller) ? Controller->GetPlayerState<AHeistPlayerState>() : nullptr;
		const AHeistHUD* HUD = IsValid(Controller) ? Controller->GetHUD<AHeistHUD>() : nullptr;
		if (!IsValid(GameState) || GameState->GetMatchPhase() != EHeistMatchPhase::InGame || GameState->PlayerArray.Num() != AlertTestPlayerCount ||
			!IsValid(Controller) || !IsValid(Controller->GetPawn()) || !IsValid(PlayerState) || PlayerState->HeistPlayerId < 1 ||
			PlayerState->HeistPlayerId > AlertTestPlayerCount || !IsValid(HUD) || !IsValid(HUD->GetHUDViewModel()) || !IsValid(HUD->GetMainHUDWidget()))
		{
			return false;
		}
		LocalPlayerIds.Add(PlayerState->HeistPlayerId);
	}
	return LocalPlayerIds.Num() == AlertTestPlayerCount && IsValid(GetServerCharacterById(SurfaceOwnerPlayerId)) &&
		IsValid(GetServerCharacterById(AssemblyOwnerPlayerId));
}

FLinearColor ResolveExpectedAlertColor(const EHeistAlertLevel AlertLevel)
{
	switch (AlertLevel)
	{
	case EHeistAlertLevel::Suspicious:
		return FLinearColor(1.0f, 0.68f, 0.12f);
	case EHeistAlertLevel::Searching:
		return FLinearColor(1.0f, 0.30f, 0.05f);
	case EHeistAlertLevel::Alarmed:
		return FLinearColor(1.0f, 0.04f, 0.02f);
	case EHeistAlertLevel::Lockdown:
		return FLinearColor(0.72f, 0.0f, 0.0f);
	case EHeistAlertLevel::Quiet:
	default:
		return FLinearColor(0.45f, 0.58f, 0.70f);
	}
}

FString ResolveExpectedAlertBanner(const EHeistAlertLevel AlertLevel)
{
	const int32 SecurityLevel = FMath::Clamp(static_cast<int32>(AlertLevel), 0, 4);
	FString Stars;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (Index > 0)
		{
			Stars += TEXT(" ");
		}
		Stars += Index < SecurityLevel ? TEXT("\u2605") : TEXT("\u2606");
	}
	return FString::Printf(TEXT("경계 단계 %d/4  %s"), SecurityLevel, *Stars);
}

bool ApplyAlertSnapshot(const TSharedRef<FAlertPresentationAutomationState>& State, const EHeistAlertLevel AlertLevel, const float TransitionDelaySeconds,
	const TCHAR* TriggerSuffix)
{
	UWorld* ServerWorld = GetServerWorld();
	AHeistGameState* ServerGameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(ServerGameState) || !IsValid(GameMode) || TransitionDelaySeconds < 0.0f)
	{
		return false;
	}

	const FName TriggerId(*FString::Printf(TEXT("W7AlertPresentation_%s"), TriggerSuffix));
	const bool bEscalating = static_cast<uint8>(AlertLevel) > static_cast<uint8>(ServerGameState->GetAlertLevel());
	const bool bUseProductionEscalation = bEscalating && AlertLevel != EHeistAlertLevel::Lockdown;
	bool bApplied = false;
	if (bUseProductionEscalation)
	{
		bool bLevelChanged = false;
		bApplied = GameMode->RequestAlertEscalation(AlertLevel, TriggerId, &bLevelChanged) && bLevelChanged;
	}
	else
	{
		const float RequestedEndServerTime = TransitionDelaySeconds > 0.0f ? ServerGameState->GetServerWorldTimeSeconds() + TransitionDelaySeconds : 0.0f;
		bApplied = ServerGameState->SetAlertSnapshot(AlertLevel, RequestedEndServerTime, TriggerId);
	}
	if (!bApplied)
	{
		return false;
	}

	State->ExpectedAlertLevel = AlertLevel;
	State->ExpectedAlertRevision = ServerGameState->GetAlertRevision();
	State->ExpectedTransitionEndServerTime = ServerGameState->GetAlertNextTransitionServerTime();
	State->ExpectedTriggerId = TriggerId;
	return true;
}

bool IsAlertStageReadyOnAllPeers(const TSharedRef<FAlertPresentationAutomationState>& State)
{
	const TArray<UWorld*> Worlds = GetPIEWorlds();
	if (Worlds.Num() != AlertTestPlayerCount || State->ExpectedAlertRevision < 0)
	{
		return false;
	}

	const int32 ExpectedSecurityLevel = FMath::Clamp(static_cast<int32>(State->ExpectedAlertLevel), 0, 4);
	const FString ExpectedBanner = ResolveExpectedAlertBanner(State->ExpectedAlertLevel);
	const FLinearColor ExpectedColor = ResolveExpectedAlertColor(State->ExpectedAlertLevel);
	const bool bExpectedCountdown = State->ExpectedAlertLevel == EHeistAlertLevel::Alarmed && State->ExpectedTransitionEndServerTime > 0.0f;
	const bool bExpectedSuspense = State->ExpectedAlertLevel == EHeistAlertLevel::Suspicious || State->ExpectedAlertLevel == EHeistAlertLevel::Searching;
	const bool bExpectedAlarm = State->ExpectedAlertLevel == EHeistAlertLevel::Alarmed || State->ExpectedAlertLevel == EHeistAlertLevel::Lockdown;

	for (UWorld* World : Worlds)
	{
		const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
		const AHeistPlayerController* Controller = GetLocalController(World);
		const AHeistHUD* HUD = IsValid(Controller) ? Controller->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDViewModel* ViewModel = IsValid(HUD) ? HUD->GetHUDViewModel() : nullptr;
		const UHeistHUDWidget* Widget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
		const UTextBlock* AlertTextWidget = IsValid(Widget) ? Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("AlertText"))) : nullptr;
		const UTextBlock* CountdownTextWidget = IsValid(Widget) ? Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("LockdownCountdownText"))) : nullptr;
		const bool bCountdownDisplayed = IsValid(CountdownTextWidget) && CountdownTextWidget->GetVisibility() != ESlateVisibility::Collapsed &&
			CountdownTextWidget->GetVisibility() != ESlateVisibility::Hidden;
		if (!IsValid(GameState) || GameState->GetAlertLevel() != State->ExpectedAlertLevel || GameState->GetAlertRevision() != State->ExpectedAlertRevision ||
			GameState->GetLastAlertTriggerId() != State->ExpectedTriggerId || !IsValid(ViewModel) || ViewModel->GetAlertLevel() != State->ExpectedAlertLevel ||
			ViewModel->GetSecurityLevel() != ExpectedSecurityLevel || ViewModel->GetAlertBannerText().ToString() != ExpectedBanner ||
			!ViewModel->GetAlertColor().Equals(ExpectedColor, KINDA_SMALL_NUMBER) || ViewModel->IsLockdownCountdownVisible() != bExpectedCountdown ||
			ViewModel->IsSuspenseMusicActive() != bExpectedSuspense || ViewModel->IsAlarmMusicActive() != bExpectedAlarm || !IsValid(Widget) ||
			!IsValid(AlertTextWidget) || AlertTextWidget->GetText().ToString() != ExpectedBanner ||
			!AlertTextWidget->GetColorAndOpacity().GetSpecifiedColor().Equals(ExpectedColor, KINDA_SMALL_NUMBER) || !IsValid(CountdownTextWidget) ||
			bCountdownDisplayed != bExpectedCountdown ||
			!Widget->AreAlertAudioAssetsAssignedForDebug() || !Widget->AreAlertAudioAssetsLoopingForDebug() || Widget->IsSuspenseMusicPlayingForDebug() != bExpectedSuspense ||
			Widget->IsAlarmMusicPlayingForDebug() != bExpectedAlarm || !Widget->IsAlertPresentationContractSatisfied())
		{
			return false;
		}

		const float ExpectedEndServerTime = bExpectedCountdown ? State->ExpectedTransitionEndServerTime : 0.0f;
		if (!FMath::IsNearlyEqual(ViewModel->GetLockdownCountdownEndServerTime(), ExpectedEndServerTime, 0.1f))
		{
			return false;
		}
		if (bExpectedCountdown)
		{
			const int32 RemainingSeconds = FMath::Max(0, FMath::CeilToInt(State->ExpectedTransitionEndServerTime - GameState->GetServerWorldTimeSeconds()));
			const FString ExpectedCountdown = FString::Printf(TEXT("봉쇄까지 %02d:%02d  —  탈출 경로가 제한됩니다"), RemainingSeconds / 60, RemainingSeconds % 60);
			if (CountdownTextWidget->GetText().ToString() != ExpectedCountdown ||
				!CountdownTextWidget->GetColorAndOpacity().GetSpecifiedColor().Equals(ExpectedColor, KINDA_SMALL_NUMBER))
			{
				return false;
			}
		}
	}
	return true;
}

FString DescribeAlertStage(const TSharedRef<FAlertPresentationAutomationState>& State)
{
	TArray<FString> PeerStates;
	for (UWorld* World : GetPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistPlayerController* Controller = GetLocalController(World);
		const AHeistPlayerState* PlayerState = IsValid(Controller) ? Controller->GetPlayerState<AHeistPlayerState>() : nullptr;
		const AHeistHUD* HUD = IsValid(Controller) ? Controller->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDViewModel* ViewModel = IsValid(HUD) ? HUD->GetHUDViewModel() : nullptr;
		const UHeistHUDWidget* Widget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
		PeerStates.Add(FString::Printf(
			TEXT("World=%s NetMode=%d Player=%d GS=%s/%d VM=%s/%d Banner='%s' Color=%s Countdown=%s End=%.2f Suspense=%s/%s Alarm=%s/%s AudioAssets=%s Contract=%s"),
			*GetNameSafe(World), IsValid(World) ? static_cast<int32>(World->GetNetMode()) : INDEX_NONE,
			IsValid(PlayerState) ? PlayerState->HeistPlayerId : INDEX_NONE,
			IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetAlertLevel()) : TEXT("Missing"), IsValid(GameState) ? GameState->GetAlertRevision() : INDEX_NONE,
			IsValid(ViewModel) ? *UEnum::GetValueAsString(ViewModel->GetAlertLevel()) : TEXT("Missing"), IsValid(ViewModel) ? ViewModel->GetSecurityLevel() : INDEX_NONE,
			IsValid(ViewModel) ? *ViewModel->GetAlertBannerText().ToString() : TEXT("Missing"), IsValid(ViewModel) ? *ViewModel->GetAlertColor().ToString() : TEXT("Missing"),
			IsValid(ViewModel) && ViewModel->IsLockdownCountdownVisible() ? TEXT("true") : TEXT("false"),
			IsValid(ViewModel) ? ViewModel->GetLockdownCountdownEndServerTime() : -1.0f,
			IsValid(ViewModel) && ViewModel->IsSuspenseMusicActive() ? TEXT("requested") : TEXT("inactive"),
			IsValid(Widget) && Widget->IsSuspenseMusicPlayingForDebug() ? TEXT("playing") : TEXT("stopped"),
			IsValid(ViewModel) && ViewModel->IsAlarmMusicActive() ? TEXT("requested") : TEXT("inactive"),
			IsValid(Widget) && Widget->IsAlarmMusicPlayingForDebug() ? TEXT("playing") : TEXT("stopped"),
			IsValid(Widget) && Widget->AreAlertAudioAssetsAssignedForDebug() ? TEXT("true") : TEXT("false"),
			IsValid(Widget) && Widget->IsAlertPresentationContractSatisfied() ? TEXT("PASS") : TEXT("FAIL")));
	}
	return FString::Printf(TEXT("Expected=%s Revision=%d End=%.2f Trigger=%s | %s"), *UEnum::GetValueAsString(State->ExpectedAlertLevel),
		State->ExpectedAlertRevision, State->ExpectedTransitionEndServerTime, *State->ExpectedTriggerId.ToString(), *FString::Join(PeerStates, TEXT(" | ")));
}

AHeistPaintingDisplayCaseActor* FindSurfaceCase(const FName CaseId = NAME_None)
{
	UWorld* ServerWorld = GetServerWorld();
	const AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	const FName ResolvedCaseId = !CaseId.IsNone() ? CaseId : (IsValid(GameState) ? GameState->GetContractSnapshot().RequiredTargetCaseId : NAME_None);
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		if (IsValid(*It) && It->IsContractExhibitActive() && It->GetDisplayCaseId() == ResolvedCaseId)
		{
			return *It;
		}
	}
	return nullptr;
}

AHeistObjectDisplayCaseActor* FindObjectCase(const FName CaseId = NAME_None)
{
	UWorld* ServerWorld = GetServerWorld();
	for (TActorIterator<AHeistObjectDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		if (!IsValid(*It) || !It->IsContractExhibitActive() || (!CaseId.IsNone() && It->GetObjectCaseId() != CaseId))
		{
			continue;
		}
		const EHeistObjectAssemblyState State = It->GetAssemblyState();
		if (State == EHeistObjectAssemblyState::Secured || State == EHeistObjectAssemblyState::Observed)
		{
			return *It;
		}
	}
	return nullptr;
}

bool StartSurfaceSession(const TSharedRef<FAlertPresentationAutomationState>& State)
{
	AHeistPlayerCharacter* Character = GetServerCharacterById(SurfaceOwnerPlayerId);
	AHeistPlayerState* PlayerState = IsValid(Character) ? Character->GetPlayerState<AHeistPlayerState>() : nullptr;
	UHeistForgeryComponent* Forgery = IsValid(Character) ? Character->GetForgeryComponent() : nullptr;
	AHeistPaintingDisplayCaseActor* DisplayCase = FindSurfaceCase(State->SurfaceCaseId);
	if (!IsValid(Character) || !IsValid(PlayerState) || !IsValid(Forgery) || Forgery->IsSessionActive() || !IsValid(DisplayCase))
	{
		return false;
	}

	State->SurfaceCaseId = DisplayCase->GetDisplayCaseId();
	Character->SetActorLocation(DisplayCase->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	Character->ForceNetUpdate();
	if (!DisplayCase->IsSessionLocked() && !DisplayCase->TryBeginSession(PlayerState))
	{
		return false;
	}
	if (DisplayCase->GetSessionOwner() != PlayerState)
	{
		return false;
	}
	if (DisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::Observed &&
		!DisplayCase->TryTransitionToDisplayCaseState(EHeistDisplayCaseState::Observed))
	{
		return false;
	}
	return Forgery->TryBeginForgerySession(DisplayCase, 40.0f);
}

bool StartObjectSession(const TSharedRef<FAlertPresentationAutomationState>& State)
{
	AHeistPlayerCharacter* Character = GetServerCharacterById(AssemblyOwnerPlayerId);
	UHeistObjectAssemblyComponent* Assembly = IsValid(Character) ? Character->GetObjectAssemblyComponent() : nullptr;
	AHeistObjectDisplayCaseActor* DisplayCase = FindObjectCase(State->ObjectCaseId);
	if (!IsValid(Character) || !IsValid(Assembly) || Assembly->IsSessionActive() || !IsValid(DisplayCase))
	{
		return false;
	}

	State->ObjectCaseId = DisplayCase->GetObjectCaseId();
	Character->SetActorLocation(DisplayCase->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	Character->ForceNetUpdate();
	return Assembly->TryBeginAssemblySession(DisplayCase, 30.0f);
}

bool IsSurfaceSessionPresentationReady()
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(SurfaceOwnerPlayerId);
	const UHeistForgeryComponent* ServerForgery = IsValid(ServerCharacter) ? ServerCharacter->GetForgeryComponent() : nullptr;
	const AHeistPlayerController* OwnerController = GetOwningControllerById(SurfaceOwnerPlayerId);
	const AHeistPlayerCharacter* OwnerCharacter = IsValid(OwnerController) ? OwnerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	const UHeistForgeryComponent* OwnerForgery = IsValid(OwnerCharacter) ? OwnerCharacter->GetForgeryComponent() : nullptr;
	const AHeistHUD* HUD = IsValid(OwnerController) ? OwnerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistForgeryWidget* Widget = IsValid(HUD) ? HUD->GetForgeryWidget() : nullptr;
	return IsValid(ServerForgery) && ServerForgery->IsSessionActive() && IsValid(OwnerForgery) && OwnerForgery->IsSessionActive() && IsValid(OwnerController) &&
		OwnerController->GetLocalInputMode() == EHeistInputMode::Forgery && OwnerController->IsLocalInputModeContractSatisfied() && IsValid(Widget) &&
		Widget->IsWidgetPresentationVisible() && Widget->IsOwnerOnlyContractSatisfied() && Widget->GetWidgetFromName(TEXT("ForgeryAlertWarningText")) == nullptr &&
		Widget->GetWidgetFromName(TEXT("ForgeryLockdownCountdownText")) == nullptr;
}

bool IsObjectSessionPresentationReady()
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(AssemblyOwnerPlayerId);
	const UHeistObjectAssemblyComponent* ServerAssembly = IsValid(ServerCharacter) ? ServerCharacter->GetObjectAssemblyComponent() : nullptr;
	const AHeistPlayerController* OwnerController = GetOwningControllerById(AssemblyOwnerPlayerId);
	const AHeistPlayerCharacter* OwnerCharacter = IsValid(OwnerController) ? OwnerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	const UHeistObjectAssemblyComponent* OwnerAssembly = IsValid(OwnerCharacter) ? OwnerCharacter->GetObjectAssemblyComponent() : nullptr;
	const AHeistHUD* HUD = IsValid(OwnerController) ? OwnerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistObjectAssemblyWidget* Widget = IsValid(HUD) ? HUD->GetObjectAssemblyWidget() : nullptr;
	return IsValid(ServerAssembly) && ServerAssembly->IsSessionActive() && IsValid(OwnerAssembly) && OwnerAssembly->IsSessionActive() && IsValid(OwnerController) &&
		OwnerController->GetLocalInputMode() == EHeistInputMode::Forgery && OwnerController->IsLocalInputModeContractSatisfied() && IsValid(Widget) &&
		Widget->IsWidgetPresentationVisible() && Widget->IsOwnerOnlyContractSatisfied() && Widget->GetWidgetFromName(TEXT("AssemblyAlertWarningText")) == nullptr &&
		Widget->GetWidgetFromName(TEXT("AssemblyLockdownCountdownText")) == nullptr;
}

bool IsSurfaceSessionClosedForReason(const FName ExpectedCleanupReason)
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(SurfaceOwnerPlayerId);
	const UHeistForgeryComponent* ServerForgery = IsValid(ServerCharacter) ? ServerCharacter->GetForgeryComponent() : nullptr;
	const AHeistPlayerController* OwnerController = GetOwningControllerById(SurfaceOwnerPlayerId);
	const AHeistPlayerCharacter* OwnerCharacter = IsValid(OwnerController) ? OwnerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	const UHeistForgeryComponent* OwnerForgery = IsValid(OwnerCharacter) ? OwnerCharacter->GetForgeryComponent() : nullptr;
	const AHeistHUD* HUD = IsValid(OwnerController) ? OwnerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistForgeryWidget* Widget = IsValid(HUD) ? HUD->GetForgeryWidget() : nullptr;
	return IsValid(ServerForgery) && !ServerForgery->IsSessionActive() && ServerForgery->GetLastCleanupReason() == ExpectedCleanupReason && IsValid(OwnerForgery) &&
		!OwnerForgery->IsSessionActive() && IsValid(OwnerController) &&
		OwnerController->GetLocalInputMode() == EHeistInputMode::Gameplay && OwnerController->IsLocalInputModeContractSatisfied() && !OwnerController->bShowMouseCursor &&
		!OwnerController->IsMoveInputIgnored() && !OwnerController->IsLookInputIgnored() && IsValid(Widget) && !Widget->IsWidgetPresentationVisible() &&
		Widget->IsAlertWarningContractSatisfied();
}

bool IsObjectSessionClosedForReason(const FName ExpectedCleanupReason)
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(AssemblyOwnerPlayerId);
	const UHeistObjectAssemblyComponent* ServerAssembly = IsValid(ServerCharacter) ? ServerCharacter->GetObjectAssemblyComponent() : nullptr;
	const AHeistPlayerController* OwnerController = GetOwningControllerById(AssemblyOwnerPlayerId);
	const AHeistPlayerCharacter* OwnerCharacter = IsValid(OwnerController) ? OwnerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	const UHeistObjectAssemblyComponent* OwnerAssembly = IsValid(OwnerCharacter) ? OwnerCharacter->GetObjectAssemblyComponent() : nullptr;
	const AHeistHUD* HUD = IsValid(OwnerController) ? OwnerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistObjectAssemblyWidget* Widget = IsValid(HUD) ? HUD->GetObjectAssemblyWidget() : nullptr;
	return IsValid(ServerAssembly) && !ServerAssembly->IsSessionActive() && ServerAssembly->GetLastCleanupReason() == ExpectedCleanupReason && IsValid(OwnerAssembly) &&
		!OwnerAssembly->IsSessionActive() && IsValid(OwnerController) &&
		OwnerController->GetLocalInputMode() == EHeistInputMode::Gameplay && OwnerController->IsLocalInputModeContractSatisfied() && !OwnerController->bShowMouseCursor &&
		!OwnerController->IsMoveInputIgnored() && !OwnerController->IsLookInputIgnored() && IsValid(Widget) && !Widget->IsWidgetPresentationVisible() &&
		Widget->IsAlertWarningContractSatisfied();
}

bool AreAlarmedReentryRequestsRejected(const TSharedRef<FAlertPresentationAutomationState>& State)
{
	UWorld* ServerWorld = GetServerWorld();
	const AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerCharacter* SurfaceCharacter = GetServerCharacterById(SurfaceOwnerPlayerId);
	UHeistActionComponent* SurfaceAction = IsValid(SurfaceCharacter) ? SurfaceCharacter->GetActionComponent() : nullptr;
	UHeistForgeryComponent* SurfaceForgery = IsValid(SurfaceCharacter) ? SurfaceCharacter->GetForgeryComponent() : nullptr;
	AHeistPaintingDisplayCaseActor* SurfaceCase = FindSurfaceCase(State->SurfaceCaseId);
	AHeistPlayerCharacter* ObjectCharacter = GetServerCharacterById(AssemblyOwnerPlayerId);
	UHeistActionComponent* ObjectAction = IsValid(ObjectCharacter) ? ObjectCharacter->GetActionComponent() : nullptr;
	UHeistObjectAssemblyComponent* ObjectAssembly = IsValid(ObjectCharacter) ? ObjectCharacter->GetObjectAssemblyComponent() : nullptr;
	AHeistObjectDisplayCaseActor* ObjectCase = FindObjectCase(State->ObjectCaseId);
	if (!IsValid(GameState) || GameState->GetAlertLevel() != EHeistAlertLevel::Alarmed || !IsValid(SurfaceCharacter) || !IsValid(SurfaceAction) ||
		!IsValid(SurfaceForgery) || !IsValid(SurfaceCase) || !IsValid(ObjectCharacter) || !IsValid(ObjectAction) || !IsValid(ObjectAssembly) || !IsValid(ObjectCase) ||
		SurfaceAction->IsObservationCastActive() || SurfaceForgery->IsSessionActive() || SurfaceCase->IsSessionLocked() || ObjectAction->IsObservationCastActive() ||
		ObjectAssembly->IsSessionActive() || ObjectCase->IsSessionLocked())
	{
		return false;
	}

	State->ObjectCaseId = ObjectCase->GetObjectCaseId();
	SurfaceCharacter->SetActorLocation(SurfaceCase->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	ObjectCharacter->SetActorLocation(ObjectCase->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	SurfaceCharacter->ForceNetUpdate();
	ObjectCharacter->ForceNetUpdate();

	const bool bSurfaceObservationRejected = !SurfaceAction->TryBeginObservationRequest(SurfaceCase);
	const bool bSurfaceComponentBeginRejected = !SurfaceForgery->TryBeginForgerySession(SurfaceCase, 40.0f);
	const bool bObjectObservationRejected = !ObjectAction->TryBeginObservationRequest(ObjectCase);
	const bool bObjectComponentBeginRejected = !ObjectAssembly->TryBeginAssemblySession(ObjectCase, 30.0f);
	return bSurfaceObservationRejected && bSurfaceComponentBeginRejected && bObjectObservationRejected && bObjectComponentBeginRejected &&
		!SurfaceAction->IsObservationCastActive() && !SurfaceForgery->IsSessionActive() && !SurfaceCase->IsSessionLocked() &&
		!ObjectAction->IsObservationCastActive() && !ObjectAssembly->IsSessionActive() && !ObjectCase->IsSessionLocked();
}

bool IsSurfaceSessionTerminatedForLockdown()
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(SurfaceOwnerPlayerId);
	const UHeistForgeryComponent* ServerForgery = IsValid(ServerCharacter) ? ServerCharacter->GetForgeryComponent() : nullptr;
	const AHeistPlayerController* OwnerController = GetOwningControllerById(SurfaceOwnerPlayerId);
	const AHeistPlayerCharacter* OwnerCharacter = IsValid(OwnerController) ? OwnerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	const UHeistForgeryComponent* OwnerForgery = IsValid(OwnerCharacter) ? OwnerCharacter->GetForgeryComponent() : nullptr;
	const AHeistHUD* HUD = IsValid(OwnerController) ? OwnerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistForgeryWidget* Widget = IsValid(HUD) ? HUD->GetForgeryWidget() : nullptr;
	return IsValid(ServerForgery) && !ServerForgery->IsSessionActive() && ServerForgery->GetLastCleanupReason() == FName(TEXT("AlertDanger")) &&
		IsValid(OwnerForgery) && !OwnerForgery->IsSessionActive() && IsValid(Widget) && !Widget->IsWidgetPresentationVisible();
}

bool AreAllMainHUDsHiddenAndReset()
{
	const TArray<UWorld*> Worlds = GetPIEWorlds();
	if (Worlds.Num() != AlertTestPlayerCount)
	{
		return false;
	}
	for (UWorld* World : Worlds)
	{
		const AHeistPlayerController* Controller = GetLocalController(World);
		const AHeistHUD* HUD = IsValid(Controller) ? Controller->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDWidget* Widget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
		if (!IsValid(Widget) || Widget->GetVisibility() != ESlateVisibility::Collapsed || !Widget->IsHiddenPresentationStateReset())
		{
			return false;
		}
	}
	return true;
}

bool CancelActiveSessionsForCleanup()
{
	bool bPassed = true;
	if (AHeistPlayerCharacter* SurfaceCharacter = GetServerCharacterById(SurfaceOwnerPlayerId))
	{
		if (UHeistForgeryComponent* Forgery = SurfaceCharacter->GetForgeryComponent(); IsValid(Forgery) && Forgery->IsSessionActive())
		{
			bPassed &= Forgery->CancelForgerySession(FName(TEXT("W7AlertPresentationCleanup")));
		}
	}
	if (AHeistPlayerCharacter* AssemblyCharacter = GetServerCharacterById(AssemblyOwnerPlayerId))
	{
		if (UHeistObjectAssemblyComponent* Assembly = AssemblyCharacter->GetObjectAssemblyComponent(); IsValid(Assembly) && Assembly->IsSessionActive())
		{
			bPassed &= Assembly->CancelAssemblySession(FName(TEXT("W7AlertPresentationCleanup")));
		}
	}
	return bPassed;
}

bool ApplyProductionLockdown()
{
	UWorld* ServerWorld = GetServerWorld();
	AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	bool bLevelChanged = false;
	return IsValid(GameMode) && GameMode->RequestAlertEscalation(EHeistAlertLevel::Lockdown, FName(TEXT("W7AlertPresentation_ProductionLockdown")), &bLevelChanged) &&
		bLevelChanged;
}

class FAlertWaitCommand final : public IAutomationLatentCommand
{
  public:
	FAlertWaitCommand(FAutomationTestBase* InTest, const TSharedRef<FAlertPresentationAutomationState>& InState, FString InDescription, TFunction<bool()> InPredicate,
		const double InTimeoutSeconds, TFunction<FString()> InDiagnostic = {})
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Predicate(MoveTemp(InPredicate)), TimeoutSeconds(InTimeoutSeconds), Diagnostic(MoveTemp(InDiagnostic))
	{
	}

	virtual bool Update() override
	{
		if (State->bAborted)
		{
			return true;
		}
		if (StartTimeSeconds <= 0.0)
		{
			StartTimeSeconds = FPlatformTime::Seconds();
		}
		if (Predicate())
		{
			Test->AddInfo(FString::Printf(TEXT("W7-009 wait passed: %s"), *Description));
			return true;
		}
		if (FPlatformTime::Seconds() - StartTimeSeconds < TimeoutSeconds)
		{
			return false;
		}

		const FString DiagnosticText = Diagnostic ? Diagnostic() : FString();
		Test->AddError(FString::Printf(TEXT("W7-009 wait timed out: %s%s%s"), *Description, DiagnosticText.IsEmpty() ? TEXT("") : TEXT(" | "), *DiagnosticText));
		State->bAborted = true;
		return true;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FAlertPresentationAutomationState> State;
	FString Description;
	TFunction<bool()> Predicate;
	double TimeoutSeconds = 0.0;
	double StartTimeSeconds = 0.0;
	TFunction<FString()> Diagnostic;
};

class FAlertActionCommand final : public IAutomationLatentCommand
{
  public:
	FAlertActionCommand(FAutomationTestBase* InTest, const TSharedRef<FAlertPresentationAutomationState>& InState, FString InDescription, TFunction<bool()> InAction,
		const bool bInRunAfterAbort = false)
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Action(MoveTemp(InAction)), bRunAfterAbort(bInRunAfterAbort)
	{
	}

	virtual bool Update() override
	{
		if (State->bAborted && !bRunAfterAbort)
		{
			return true;
		}
		if (!Action())
		{
			Test->AddError(FString::Printf(TEXT("W7-009 action failed: %s"), *Description));
			State->bAborted = true;
		}
		else
		{
			Test->AddInfo(FString::Printf(TEXT("W7-009 action passed: %s"), *Description));
		}
		return true;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FAlertPresentationAutomationState> State;
	FString Description;
	TFunction<bool()> Action;
	bool bRunAfterAbort = false;
};

void EnqueueAlertStage(FAutomationTestBase* Test, const TSharedRef<FAlertPresentationAutomationState>& State, const EHeistAlertLevel AlertLevel,
	const float TransitionDelaySeconds, const TCHAR* TriggerSuffix, const TCHAR* Description)
{
	Test->AddCommand(new FAlertActionCommand(Test, State, FString::Printf(TEXT("apply %s"), Description),
		[State, AlertLevel, TransitionDelaySeconds, TriggerSuffix]() { return ApplyAlertSnapshot(State, AlertLevel, TransitionDelaySeconds, TriggerSuffix); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, FString::Printf(TEXT("%s exact HUD and audio on all four peers"), Description),
		[State]() { return IsAlertStageReadyOnAllPeers(State); }, 10.0, [State]() { return DescribeAlertStage(State); }));
}

bool EnqueueFourPlayerAlertScenario(FAutomationTestBase* Test)
{
	const TSharedRef<FAlertPresentationAutomationState> State = MakeShared<FAlertPresentationAutomationState>();
	Test->AddCommand(new FEditorLoadMap(TEXT("/Game/Maps/M01_ClassicalPrototype")));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("configure four-player listen-server PIE"), [State]()
	{
		ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
		if (!IsValid(PlaySettings))
		{
			return false;
		}
		PlaySettings->GetPlayNetMode(State->OriginalNetMode);
		PlaySettings->GetRunUnderOneProcess(State->bOriginalRunUnderOneProcess);
		PlaySettings->GetPlayNumberOfClients(State->OriginalClientCount);
		State->bCapturedPlaySettings = true;
		PlaySettings->SetRunUnderOneProcess(true);
		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
		PlaySettings->SetPlayNumberOfClients(AlertTestPlayerCount);
		return true;
	}));
	Test->AddCommand(new FStartPIECommand(false));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("M01 four-player gameplay worlds and local HUDs"), []() { return AreFourPlayerGameplayWorldsReady(); }, 60.0));

	EnqueueAlertStage(Test, State, EHeistAlertLevel::Quiet, 0.0f, TEXT("Quiet"), TEXT("Alert 0 Quiet"));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("start Player 1 Surface session at Quiet"), [State]() { return StartSurfaceSession(State); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Player 1 Surface owner-only presentation"), []() { return IsSurfaceSessionPresentationReady(); }, 10.0));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Suspicious, 60.0f, TEXT("SurfaceSuspicious"), TEXT("Alert 1 Suspicious"));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Surface remains active at Suspicious"), []() { return IsSurfaceSessionPresentationReady(); }, 5.0));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Searching, 60.0f, TEXT("SurfaceSearching"), TEXT("Alert 2 Searching"));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Surface remains active at Searching"), []() { return IsSurfaceSessionPresentationReady(); }, 5.0));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Alarmed, 60.0f, TEXT("SurfaceAlarmed"), TEXT("Alert 3 Alarmed countdown"));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Surface closes only at Alarmed and Gameplay input restores"),
		[]() { return IsSurfaceSessionClosedForReason(FName(TEXT("AlertDanger"))); }, 10.0));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("Alarmed rejects new Surface/Object observation and component begin without leaving a Case lock"),
		[State]() { return AreAlarmedReentryRequestsRejected(State); }));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Quiet, 0.0f, TEXT("SurfaceReset"), TEXT("Alert 0 Surface reset"));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("re-enter Player 1 Surface after Alarmed close"), [State]() { return StartSurfaceSession(State); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Player 1 Surface re-entry presentation"), []() { return IsSurfaceSessionPresentationReady(); }, 10.0));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("clean re-entered Surface session"), []() { return CancelActiveSessionsForCleanup(); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Surface cleanup restores Gameplay input"),
		[]() { return IsSurfaceSessionClosedForReason(FName(TEXT("W7AlertPresentationCleanup"))); }, 10.0));

	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("start Player 2 Object Assembly session at Quiet"), [State]() { return StartObjectSession(State); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Player 2 Object Assembly owner-only presentation"), []() { return IsObjectSessionPresentationReady(); }, 10.0));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Suspicious, 60.0f, TEXT("ObjectSuspicious"), TEXT("Alert 1 Object Suspicious"));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Object Assembly remains active at Suspicious"), []() { return IsObjectSessionPresentationReady(); }, 5.0));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Searching, 60.0f, TEXT("ObjectSearching"), TEXT("Alert 2 Object Searching"));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Object Assembly remains active at Searching"), []() { return IsObjectSessionPresentationReady(); }, 5.0));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Alarmed, 60.0f, TEXT("ObjectAlarmed"), TEXT("Alert 3 Object Alarmed countdown"));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Object Assembly closes only at Alarmed and Gameplay input restores"),
		[]() { return IsObjectSessionClosedForReason(FName(TEXT("AlertDanger"))); }, 10.0));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Quiet, 0.0f, TEXT("ObjectReset"), TEXT("Alert 0 Object reset"));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("re-enter Player 2 Object Assembly after Alarmed close"), [State]() { return StartObjectSession(State); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Player 2 Object Assembly re-entry presentation"), []() { return IsObjectSessionPresentationReady(); }, 10.0));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("clean re-entered Object Assembly session"), []() { return CancelActiveSessionsForCleanup(); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Object Assembly cleanup restores Gameplay input"),
		[]() { return IsObjectSessionClosedForReason(FName(TEXT("W7AlertPresentationCleanup"))); }, 10.0));

	EnqueueAlertStage(Test, State, EHeistAlertLevel::Lockdown, 0.0f, TEXT("Lockdown"), TEXT("Alert 4 Lockdown presentation"));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("record W7-009 four-player evidence"), [Test, State]()
	{
		if (!IsAlertStageReadyOnAllPeers(State))
		{
			return false;
		}
		Test->AddInfo(FString::Printf(
			TEXT("W7-009 4P alert gate: Peers=4 Levels=0>1>2>3>4 ExactKoreanBanner=true ExactColor=true AlarmedCountdown=true "
				 "SuspenseRequestedAndPlaying=Alert1+2 AlarmRequestedAndPlaying=Alert3+4 SurfaceOwner=1 AssemblyOwner=2 "
				 "SuspiciousSearchingSessionRetained=true AlarmedForceClose=true AlarmedReentryRejected=true NoCaseLockLeak=true GameplayInputRestored=true Reentry=true "
				 "WorkScreenAlertDetailTextAbsent=true AudioAssetsAssigned=true AudioAssetsLooping=true PlaybackStateVerified=true Result=PASS | %s"),
			*DescribeAlertStage(State)));
		return true;
	}));
	EnqueueAlertStage(Test, State, EHeistAlertLevel::Quiet, 0.0f, TEXT("PreProductionLockdownReset"), TEXT("Alert 0 pre-production Lockdown reset"));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("start Player 1 Surface session before production Lockdown"), [State]() { return StartSurfaceSession(State); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("Player 1 Surface presentation before production Lockdown"), []() { return IsSurfaceSessionPresentationReady(); }, 10.0));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("apply production Lockdown escalation"), []() { return ApplyProductionLockdown(); }));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("production Lockdown terminates the active Surface session on the server"),
		[]() { return IsSurfaceSessionTerminatedForLockdown(); }, 10.0));
	Test->AddCommand(new FAlertWaitCommand(Test, State, TEXT("match-end Main HUD cleanup clears Alert text countdown and audio components on every peer"),
		[]() { return AreAllMainHUDsHiddenAndReset(); }, 10.0));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("record production Lockdown cleanup evidence"), [Test]()
	{
		Test->AddInfo(TEXT("W7-009 production Lockdown gate: ServerCleanupReason=AlertDanger ActiveSurfaceSession=false MainHUDHidden=true AlertAudioComponentsDestroyed=true Result=PASS"));
		return true;
	}));

	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("cleanup active sessions after alert test"), []() { return CancelActiveSessionsForCleanup(); }, true));
	Test->AddCommand(new FEndPlayMapCommand());
	Test->AddCommand(new FWaitLatentCommand(1.0f));
	Test->AddCommand(new FAlertActionCommand(Test, State, TEXT("restore editor play settings"), [State]()
	{
		if (!State->bCapturedPlaySettings)
		{
			return true;
		}
		ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
		if (!IsValid(PlaySettings))
		{
			return false;
		}
		PlaySettings->SetRunUnderOneProcess(State->bOriginalRunUnderOneProcess);
		PlaySettings->SetPlayNetMode(State->OriginalNetMode);
		PlaySettings->SetPlayNumberOfClients(State->OriginalClientCount);
		return true;
	}, true));
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7AlertPresentationFourPlayerTest, "ProjectMuseumHeist.W7.AlertPresentationFourPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7AlertPresentationFourPlayerTest::RunTest(const FString& Parameters)
{
	return HeistWeek7AlertPresentationTest::EnqueueFourPlayerAlertScenario(this);
}

#endif
