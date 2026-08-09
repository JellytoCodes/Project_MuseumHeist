#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/Button.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Widgets/HeistHUDWidget.h"
#include "UI/Widgets/HeistResultWidget.h"

namespace
{
struct FHeistReplayAutomationState
{
	bool bAborted = false;
	bool bCapturedPlaySettings = false;
	EPlayNetMode OriginalNetMode = EPlayNetMode::PIE_Standalone;
	bool bOriginalRunUnderOneProcess = true;
	int32 OriginalClientCount = 1;
};

TArray<UWorld*> GetHeistPIEWorlds()
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
		return static_cast<uint8>(Left.GetNetMode()) < static_cast<uint8>(Right.GetNetMode());
	});
	return Worlds;
}

UWorld* GetHeistPIEWorld(const ENetMode NetMode)
{
	for (UWorld* World : GetHeistPIEWorlds())
	{
		if (IsValid(World) && World->GetNetMode() == NetMode)
		{
			return World;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetLocalHeistPlayerController(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator ControllerIterator = World->GetPlayerControllerIterator(); ControllerIterator; ++ControllerIterator)
	{
		AHeistPlayerController* PlayerController = Cast<AHeistPlayerController>(ControllerIterator->Get());
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			return PlayerController;
		}
	}
	return nullptr;
}

bool AreTwoPIEWorldsReady(const EHeistMatchPhase ExpectedPhase, const bool bRequirePawn)
{
	const TArray<UWorld*> Worlds = GetHeistPIEWorlds();
	if (Worlds.Num() != 2 || !IsValid(GetHeistPIEWorld(NM_ListenServer)) || !IsValid(GetHeistPIEWorld(NM_Client)))
	{
		return false;
	}

	for (UWorld* World : Worlds)
	{
		const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
		const AHeistPlayerController* PlayerController = GetLocalHeistPlayerController(World);
		if (!IsValid(GameState) || GameState->GetMatchPhase() != ExpectedPhase || GameState->PlayerArray.Num() != 2 || !IsValid(PlayerController) ||
			(bRequirePawn && !IsValid(PlayerController->GetPawn())))
		{
			return false;
		}
	}
	return true;
}

class FHeistReplayWaitCommand final : public IAutomationLatentCommand
{
  public:
	FHeistReplayWaitCommand(FAutomationTestBase* InTest, const TSharedRef<FHeistReplayAutomationState>& InState, FString InDescription,
		TFunction<bool()> InPredicate, const double InTimeoutSeconds, TFunction<FString()> InTimeoutDiagnostic = {})
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Predicate(MoveTemp(InPredicate)),
		  TimeoutDiagnostic(MoveTemp(InTimeoutDiagnostic)), TimeoutSeconds(InTimeoutSeconds)
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
			Test->AddInfo(FString::Printf(TEXT("W6-008 replay step ready: %s"), *Description));
			return true;
		}
		if (FPlatformTime::Seconds() - StartTimeSeconds >= TimeoutSeconds)
		{
			Test->AddError(FString::Printf(TEXT("W6-008 replay step timed out: %s"), *Description));
			if (TimeoutDiagnostic)
			{
				Test->AddInfo(TimeoutDiagnostic());
			}
			State->bAborted = true;
			return true;
		}
		return false;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FHeistReplayAutomationState> State;
	FString Description;
	TFunction<bool()> Predicate;
	TFunction<FString()> TimeoutDiagnostic;
	double TimeoutSeconds = 30.0;
	double StartTimeSeconds = 0.0;
};

class FHeistReplayActionCommand final : public IAutomationLatentCommand
{
  public:
	FHeistReplayActionCommand(FAutomationTestBase* InTest, const TSharedRef<FHeistReplayAutomationState>& InState, FString InDescription,
		TFunction<bool()> InAction, const bool bInRunAfterAbort = false)
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
			Test->AddError(FString::Printf(TEXT("W6-008 replay action failed: %s"), *Description));
			State->bAborted = true;
		}
		else
		{
			Test->AddInfo(FString::Printf(TEXT("W6-008 replay action passed: %s"), *Description));
		}
		return true;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FHeistReplayAutomationState> State;
	FString Description;
	TFunction<bool()> Action;
	bool bRunAfterAbort = false;
};

bool IsResultPresentationReady(UWorld* World)
{
	const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerController* PlayerController = GetLocalHeistPlayerController(World);
	const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
	const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
	return IsValid(GameState) && GameState->GetMatchPhase() == EHeistMatchPhase::End && GameState->GetTeamResult().IsValid() &&
		GameState->GetPlayerResults().Num() == 2 && IsValid(ResultWidget) && ResultWidget->GetVisibility() == ESlateVisibility::Visible &&
		IsValid(MainHUDWidget) && MainHUDWidget->IsHiddenPresentationStateReset();
}

FString DescribeResultPresentationState(UWorld* World)
{
	const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerController* PlayerController = GetLocalHeistPlayerController(World);
	const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
	const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
	return FString::Printf(
		TEXT("World=%s NetMode=%s Phase=%s TeamResult=%s PlayerResults=%d Controller=%s HUD=%s ResultWidget=%s ResultVisibility=%s MainHUD=%s MainHUDReset=%s"),
		*GetNameSafe(World), IsValid(World) ? *FString::FromInt(static_cast<int32>(World->GetNetMode())) : TEXT("Invalid"),
		IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetMatchPhase()) : TEXT("Invalid"),
		IsValid(GameState) && GameState->GetTeamResult().IsValid() ? TEXT("valid") : TEXT("invalid"),
		IsValid(GameState) ? GameState->GetPlayerResults().Num() : INDEX_NONE, *GetNameSafe(PlayerController), *GetNameSafe(HUD), *GetNameSafe(ResultWidget),
		IsValid(ResultWidget) ? *UEnum::GetValueAsString(ResultWidget->GetVisibility()) : TEXT("Invalid"), *GetNameSafe(MainHUDWidget),
		IsValid(MainHUDWidget) && MainHUDWidget->IsHiddenPresentationStateReset() ? TEXT("true") : TEXT("false"));
}

bool IsLobbyReplayStateClean(UWorld* World)
{
	const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerController* PlayerController = GetLocalHeistPlayerController(World);
	const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
	const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
	const bool bResultPresentationClean = !IsValid(ResultWidget) ||
		(ResultWidget->IsHiddenPresentationStateReset() && ResultWidget->GetVisibility() == ESlateVisibility::Collapsed);
	const bool bMainHUDPresentationClean = !IsValid(MainHUDWidget) || MainHUDWidget->IsHiddenPresentationStateReset();
	if (!IsValid(GameState) || !IsValid(PlayerController) || !IsValid(HUD) ||
		GameState->GetMatchPhase() != EHeistMatchPhase::Lobby || GameState->GetAlertLevel() != EHeistAlertLevel::Quiet || GameState->GetTeamResult().IsValid() ||
		!GameState->GetPlayerResults().IsEmpty() || GameState->GetContractSnapshot().Outcome != EHeistContractOutcome::None ||
		GameState->IsEscapePhaseOpen() || !bResultPresentationClean || !bMainHUDPresentationClean || !PlayerController->bShowMouseCursor)
	{
		return false;
	}

	const FHeistPlayerContribution EmptyContribution;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
		if (!IsValid(HeistPlayerState) || HeistPlayerState->GetTotalLootScore() != 0 || !FMath::IsNearlyZero(HeistPlayerState->GetTotalLootWeight()) ||
			HeistPlayerState->IsEscaped() || HeistPlayerState->IsArrested() || !(HeistPlayerState->GetContribution() == EmptyContribution))
		{
			return false;
		}
	}
	return true;
}

FString DescribeLobbyReplayState(UWorld* World)
{
	const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerController* PlayerController = GetLocalHeistPlayerController(World);
	const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
	const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
	return FString::Printf(
		TEXT("World=%s Phase=%s Alert=%s TeamResult=%s PlayerResults=%d ContractOutcome=%s EscapeOpen=%s Controller=%s Cursor=%s "
			 "ResultWidget=%s ResultVisibility=%s ResultReset=%s MainHUD=%s MainHUDReset=%s PlayerStates=%d Clean=%s"),
		*GetNameSafe(World), IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetMatchPhase()) : TEXT("Invalid"),
		IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetAlertLevel()) : TEXT("Invalid"),
		IsValid(GameState) && GameState->GetTeamResult().IsValid() ? TEXT("valid") : TEXT("invalid"),
		IsValid(GameState) ? GameState->GetPlayerResults().Num() : INDEX_NONE,
		IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetContractSnapshot().Outcome) : TEXT("Invalid"),
		IsValid(GameState) && GameState->IsEscapePhaseOpen() ? TEXT("true") : TEXT("false"), *GetNameSafe(PlayerController),
		IsValid(PlayerController) && PlayerController->bShowMouseCursor ? TEXT("true") : TEXT("false"), *GetNameSafe(ResultWidget),
		IsValid(ResultWidget) ? *UEnum::GetValueAsString(ResultWidget->GetVisibility()) : TEXT("NotCreated"),
		!IsValid(ResultWidget) || ResultWidget->IsHiddenPresentationStateReset() ? TEXT("true") : TEXT("false"), *GetNameSafe(MainHUDWidget),
		!IsValid(MainHUDWidget) || MainHUDWidget->IsHiddenPresentationStateReset() ? TEXT("true") : TEXT("false"),
		IsValid(GameState) ? GameState->PlayerArray.Num() : INDEX_NONE, IsLobbyReplayStateClean(World) ? TEXT("true") : TEXT("false"));
}

bool IsSecondGameplayStateClean(UWorld* World)
{
	const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerController* PlayerController = GetLocalHeistPlayerController(World);
	const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
	const bool bResultPresentationClean = !IsValid(ResultWidget) ||
		(ResultWidget->GetVisibility() == ESlateVisibility::Collapsed && ResultWidget->IsHiddenPresentationStateReset());
	return IsValid(GameState) && IsValid(PlayerController) && IsValid(PlayerController->GetPawn()) && IsValid(HUD) &&
		GameState->GetMatchPhase() == EHeistMatchPhase::InGame && GameState->GetContractSnapshot().IsInitialized() &&
		GameState->GetContractSnapshot().Outcome == EHeistContractOutcome::None && GameState->GetAlertLevel() == EHeistAlertLevel::Quiet &&
		!GameState->GetTeamResult().IsValid() && GameState->GetPlayerResults().IsEmpty() && PlayerController->GetLocalInputMode() == EHeistInputMode::Gameplay &&
		PlayerController->GetActiveHeistInputMappingContextCount() == 1 && PlayerController->IsLocalInputModeContractSatisfied() &&
		!PlayerController->bShowMouseCursor && !PlayerController->IsMoveInputIgnored() && !PlayerController->IsLookInputIgnored() && bResultPresentationClean;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistEndLobbyReplayIntegrationTest, "ProjectMuseumHeist.Replay.EndLobbyReturnTwoPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistEndLobbyReplayIntegrationTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FHeistReplayAutomationState> State = MakeShared<FHeistReplayAutomationState>();

	AddCommand(new FEditorLoadMap(TEXT("/Game/Maps/TitleMenuMap")));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("configure two-player listen-server PIE"), [State]()
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
		PlaySettings->SetPlayNumberOfClients(2);
		return true;
	}));
	AddCommand(new FStartPIECommand(false));
	AddCommand(new FHeistReplayWaitCommand(this, State, TEXT("title menu host/client worlds"), []()
	{
		const TArray<UWorld*> Worlds = GetHeistPIEWorlds();
		return Worlds.Num() == 2 && IsValid(GetHeistPIEWorld(NM_ListenServer)) && IsValid(GetHeistPIEWorld(NM_Client)) &&
			IsValid(GetLocalHeistPlayerController(GetHeistPIEWorld(NM_ListenServer))) && IsValid(GetLocalHeistPlayerController(GetHeistPIEWorld(NM_Client)));
	}, 45.0));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("create host session"), []()
	{
		UWorld* HostWorld = GetHeistPIEWorld(NM_ListenServer);
		UHeistGameInstance* GameInstance = IsValid(HostWorld) ? Cast<UHeistGameInstance>(HostWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->RequestHostSession();
	}));
	AddCommand(new FHeistReplayWaitCommand(this, State, TEXT("first lobby with preserved two-player connection"), []()
	{
		UWorld* HostWorld = GetHeistPIEWorld(NM_ListenServer);
		UHeistGameInstance* HostGameInstance = IsValid(HostWorld) ? Cast<UHeistGameInstance>(HostWorld->GetGameInstance()) : nullptr;
		return AreTwoPIEWorldsReady(EHeistMatchPhase::Lobby, false) && IsValid(HostGameInstance) && HostGameInstance->IsHostingOnlineSession() &&
			HostGameInstance->HasActiveNamedOnlineSession() && HostGameInstance->IsCurrentWorldLobby();
	}, 45.0));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("start first gameplay travel"), []()
	{
		UWorld* HostWorld = GetHeistPIEWorld(NM_ListenServer);
		UHeistGameInstance* GameInstance = IsValid(HostWorld) ? Cast<UHeistGameInstance>(HostWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->RequestStartSelectedGameplayMap();
	}));
	AddCommand(new FHeistReplayWaitCommand(this, State, TEXT("first gameplay host/client worlds"), []()
	{
		return AreTwoPIEWorldsReady(EHeistMatchPhase::InGame, true);
	}, 60.0));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("raise alert with an active transition timer"), []()
	{
		AHeistPlayerController* HostPlayerController = GetLocalHeistPlayerController(GetHeistPIEWorld(NM_ListenServer));
		if (!IsValid(HostPlayerController))
		{
			return false;
		}
		UHeistDebugFunctionLibrary::DebugAlertRequest(HostPlayerController, TEXT("Alarmed"));
		return true;
	}));
	AddCommand(new FHeistReplayWaitCommand(this, State, TEXT("alert presentation replicated to both players"), []()
	{
		for (UWorld* World : GetHeistPIEWorlds())
		{
			const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
			const AHeistPlayerController* PlayerController = GetLocalHeistPlayerController(World);
			const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
			const UHeistHUDWidget* HUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
			if (!IsValid(GameState) || GameState->GetAlertLevel() != EHeistAlertLevel::Alarmed || !IsValid(HUDWidget) || HUDWidget->IsHiddenPresentationStateReset())
			{
				return false;
			}
		}
		return true;
	}, 20.0));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("resolve the first result"), []()
	{
		AHeistPlayerController* HostPlayerController = GetLocalHeistPlayerController(GetHeistPIEWorld(NM_ListenServer));
		if (!IsValid(HostPlayerController))
		{
			return false;
		}
		UHeistDebugFunctionLibrary::DebugOnlineSessionComplete(HostPlayerController);
		return true;
	}));
	AddCommand(new FHeistReplayWaitCommand(this, State, TEXT("result shown and alert HUD reset on both players"), []()
	{
		const TArray<UWorld*> Worlds = GetHeistPIEWorlds();
		return Worlds.Num() == 2 && IsResultPresentationReady(Worlds[0]) && IsResultPresentationReady(Worlds[1]);
	}, 30.0, []()
	{
		FString Diagnostic(TEXT("W6-008 result readiness diagnostic:"));
		for (UWorld* World : GetHeistPIEWorlds())
		{
			Diagnostic += FString::Printf(TEXT("\n  %s"), *DescribeResultPresentationState(World));
		}
		return Diagnostic;
	}));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("open reward detail on both result widgets before lobby return"), []()
	{
		for (UWorld* World : GetHeistPIEWorlds())
		{
			AHeistPlayerController* PlayerController = GetLocalHeistPlayerController(World);
			AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
			UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
			UButton* RewardDetailsButton = IsValid(ResultWidget) ? Cast<UButton>(ResultWidget->GetWidgetFromName(TEXT("RewardDetailsButton"))) : nullptr;
			if (!IsValid(RewardDetailsButton))
			{
				return false;
			}
			RewardDetailsButton->OnClicked.Broadcast();
			if (!ResultWidget->IsRewardDetailVisible())
			{
				return false;
			}
		}
		return true;
	}));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("return to lobby without destroying the host session"), []()
	{
		UWorld* HostWorld = GetHeistPIEWorld(NM_ListenServer);
		UHeistGameInstance* GameInstance = IsValid(HostWorld) ? Cast<UHeistGameInstance>(HostWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->RequestReturnToLobby();
	}));
	AddCommand(new FHeistReplayWaitCommand(this, State, TEXT("clean lobby replay state on host and client"), []()
	{
		UWorld* HostWorld = GetHeistPIEWorld(NM_ListenServer);
		UHeistGameInstance* HostGameInstance = IsValid(HostWorld) ? Cast<UHeistGameInstance>(HostWorld->GetGameInstance()) : nullptr;
		const TArray<UWorld*> Worlds = GetHeistPIEWorlds();
		return Worlds.Num() == 2 && AreTwoPIEWorldsReady(EHeistMatchPhase::Lobby, false) && IsValid(HostGameInstance) && HostGameInstance->IsHostingOnlineSession() &&
			HostGameInstance->HasActiveNamedOnlineSession() && IsLobbyReplayStateClean(Worlds[0]) && IsLobbyReplayStateClean(Worlds[1]);
	}, 60.0, []()
	{
		FString Diagnostic(TEXT("W6-008 lobby cleanup diagnostic:"));
		for (UWorld* World : GetHeistPIEWorlds())
		{
			Diagnostic += FString::Printf(TEXT("\n  %s"), *DescribeLobbyReplayState(World));
		}
		return Diagnostic;
	}));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("start second gameplay in the same session"), []()
	{
		UWorld* HostWorld = GetHeistPIEWorld(NM_ListenServer);
		UHeistGameInstance* GameInstance = IsValid(HostWorld) ? Cast<UHeistGameInstance>(HostWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->HasActiveNamedOnlineSession() && GameInstance->RequestStartSelectedGameplayMap();
	}));
	AddCommand(new FHeistReplayWaitCommand(this, State, TEXT("second gameplay has clean input and match state"), []()
	{
		UWorld* HostWorld = GetHeistPIEWorld(NM_ListenServer);
		UHeistGameInstance* HostGameInstance = IsValid(HostWorld) ? Cast<UHeistGameInstance>(HostWorld->GetGameInstance()) : nullptr;
		const TArray<UWorld*> Worlds = GetHeistPIEWorlds();
		return Worlds.Num() == 2 && AreTwoPIEWorldsReady(EHeistMatchPhase::InGame, true) && IsValid(HostGameInstance) && HostGameInstance->HasActiveNamedOnlineSession() &&
			IsSecondGameplayStateClean(Worlds[0]) && IsSecondGameplayStateClean(Worlds[1]);
	}, 60.0));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("record replay PASS evidence"), [this]()
	{
		AddInfo(TEXT("W6-008 replay audit: Players=2 SessionPreserved=true LobbyStateReset=true AlertAudioReset=true ResultDetailReset=true "
			"PlayerMatchStateReset=true SecondGameplayInput=true Softlock=false Result=PASS"));
		return true;
	}));
	AddCommand(new FEndPlayMapCommand());
	AddCommand(new FWaitLatentCommand(1.0f));
	AddCommand(new FHeistReplayActionCommand(this, State, TEXT("restore editor play settings"), [State]()
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

#endif
