#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/PlayerController.h"

#include "HeistPlayerController.generated.h"

class AHeistPlayerCharacter;
class AHeistObjectDisplayCaseActor;
class AHeistPaintingDisplayCaseActor;
class AHeistSecurityHoldButtonActor;
class AHeistGuardCharacter;
class AHeistGameState;
class AHeistLootActor;
class AHeistDroppedOriginalActor;
class AHeistPlayerState;
class AHeistThrowableProjectile;
class AHeistVentActor;
class UHeistForgeryComponent;
class UHeistInventoryComponent;
class UHeistObjectAssemblyComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistPopupFeedbackRequested, const FText&, float);
DECLARE_MULTICAST_DELEGATE(FHeistTutorialPresentationChanged);

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerController : public APlayerController
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistPlayerController();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostSeamlessTravel() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_Pawn() override;
	virtual void OnRep_PlayerState() override;
	virtual void PawnLeavingGame() override;
	virtual void SetupInputComponent() override;

  private:
	void ResetLocalHeldInteractionInputState();
	void UnbindMatchPhasePresentationState();
	void RefreshLocalHUDPresentation();
	void RefreshLocalPresentationAfterSeamlessTravel();
	void RefreshMatchPhasePresentationBinding();
	void HandleMatchPhasePresentationChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);
	void HandlePlayerResultsPresentationChanged();
	void HandleAlertStatePresentationChanged(EHeistAlertLevel PreviousAlertLevel, EHeistAlertLevel NewAlertLevel, int32 Revision, FName TriggerId);
	void HandleEscapePhasePresentationChanged(bool bEscapePhaseOpen);
	void RefreshLocalPlayerTerminalState();
	void RefreshLocalSpectateTarget();
	AActor* FindLocalSpectateTarget() const;
	bool IsLocalPlayerTerminalInputBlocked() const;
	TWeakObjectPtr<AHeistGameState> BoundMatchPhaseGameState;
	TWeakObjectPtr<AActor> LocalSpectateTarget;
	bool bLocalAwaitingCrew = false;

#pragma endregion

#pragma region Input

  private:
	void HandleLookInput(const FInputActionValue& InputValue);
	void HandleMoveInput(const FInputActionValue& InputValue);
	void HandleSprintStarted();
	void HandleSprintStopped();
	void HandleMapToggle();
	void HandleInventoryToggle();
	void HandleForgeryCancel();
	void HandleReplicaRedraw();
	void UpdateFlashlightAimDirection();
	void RefreshLocalForgeryInputBinding();
	void UnbindLocalForgeryInputState();
	void HandleForgerySessionStateChanged();
	void RefreshLocalObjectAssemblyInputBinding();
	void UnbindLocalObjectAssemblyInputState();
	void HandleObjectAssemblySessionStateChanged();
	void CloseFloorPlanMapForStateTransition();
	void RefreshLocalInputModeFromPawn();
	void ApplyLocalInputMode(EHeistInputMode NewInputMode);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InteractInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InventoryInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ForgeryCancelInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> SprintInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MapInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> GameplayInputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InventoryInputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> ForgeryInputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> MapInputMappingContext;

	TWeakObjectPtr<UHeistForgeryComponent> BoundForgeryComponent;
	TWeakObjectPtr<UHeistObjectAssemblyComponent> BoundObjectAssemblyComponent;
	EHeistInputMode LocalInputMode = EHeistInputMode::Gameplay;
	bool bLocalForgerySessionActive = false;
	bool bLocalObjectAssemblySessionActive = false;

  public:
	bool ToggleFloorPlanMap();
	void RequestSetSprintRequested(bool bRequested);
	void HandleInventoryOpenStateChanged(bool bInventoryOpen);
	void HandleArrestStateChanged(bool bArrested);
	void HandlePlayerTerminalStateChanged(bool bEscaped, bool bArrested);
	void HandlePlayerStunStateChanged(bool bStunned);
	void ApplyLocalUserSettings();
	float GetLocalMouseSensitivity() const;
	EHeistInputMode GetLocalInputMode() const;
	bool IsLocalInputMappingContextActive(EHeistInputMode InputMode) const;
	int32 GetActiveHeistInputMappingContextCount() const;
	bool IsLocalInputModeContractSatisfied() const;
	bool IsLocalAwaitingCrew() const;
	AActor* GetLocalSpectateTarget() const;
	bool AreW7InputAssetsConfigured() const;

  private:
	float LocalMouseSensitivity = 1.0f;

#pragma endregion

#pragma region Voice

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Voice")
	bool IsVoicePushToTalkHeld() const { return bLocalVoicePushToTalkHeld; }

	UFUNCTION(BlueprintPure, Category = "Heist|Voice")
	bool IsLocalVoiceSpeaking() const { return bLocalVoiceSpeaking; }

	UFUNCTION(BlueprintPure, Category = "Heist|Voice")
	float GetLocalVoiceActivityLevel() const { return LocalVoiceActivityLevel; }

	UFUNCTION(BlueprintCallable, Category = "Heist|Voice")
	void SetPlayerVoiceMuted(AHeistPlayerState* TargetPlayerState, bool bMuted);

	UFUNCTION(BlueprintPure, Category = "Heist|Voice")
	bool IsPlayerVoiceMuted(AHeistPlayerState* TargetPlayerState);

  private:
	void HandleVoicePushToTalkPressed();
	void HandleVoicePushToTalkReleased();
	void PollLocalVoiceActivity();
	void ResetLocalVoicePushToTalk(bool bNotifyServer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Voice", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", Units = "s"))
	float VoiceActivityPollInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Voice", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float MinimumVoiceActivityDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Voice", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float VoiceActivityReportInterval = 0.8f;

	FTimerHandle LocalVoiceActivityTimerHandle;
	float LocalVoiceActivityLevel = 0.0f;
	float LocalVoiceActivityDuration = 0.0f;
	double LastLocalVoiceActivityReportTime = -1.0;
	bool bLocalVoicePushToTalkHeld = false;
	bool bLocalVoiceSpeaking = false;
	bool bServerVoicePushToTalkHeld = false;

#pragma endregion

#pragma region Interaction

  private:
	void HandleInteractPressed();
	void HandleInteractReleased();
	bool bLocalObservationInputHeld = false;
	TWeakObjectPtr<AHeistSecurityHoldButtonActor> LocalSecurityHoldButton;

#pragma endregion

#pragma region Tutorial

  public:
	FHeistTutorialPresentationChanged& GetTutorialPresentationChangedDelegate();
	bool IsLocalTutorialActive() const;
	bool HasCompletedLocalTutorial() const;
	int32 GetLocalTutorialStepIndex() const;
	int32 GetLocalTutorialStepCount() const;
	FName GetLocalTutorialStepId() const;
	FText GetLocalTutorialTitleText() const;
	FText GetLocalTutorialBodyText() const;

	void DebugResetLocalTutorial();
	void DebugAdvanceLocalTutorial();
	void DebugSkipLocalTutorial();

  private:
	void RefreshLocalTutorialFromMatchPhase();
	void TryStartLocalTutorial(bool bForceRestart = false);
	void StopLocalTutorial(FName TriggerId);
	void CompleteLocalTutorial(FName TriggerId);
	void AdvanceLocalTutorial(FName TriggerId);
	void NotifyLocalTutorialMilestone(FName StepId, FName TriggerId);
	void ScheduleLocalTutorialAutoAdvance(float OverrideDelaySeconds = -1.0f);
	void HandleLocalTutorialAutoAdvance();
	bool IsCurrentLocalTutorialStep(FName StepId) const;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Tutorial", meta = (ClampMin = "2.0", ClampMax = "30.0"))
	float TutorialCardDurationSeconds = 8.0f;

	FHeistTutorialPresentationChanged TutorialPresentationChangedDelegate;
	FTimerHandle LocalTutorialStepTimerHandle;
	int32 LocalTutorialStepIndex = INDEX_NONE;
	bool bLocalTutorialActive = false;
	bool bLocalTutorialObservedForgerySession = false;

#pragma endregion

#pragma region GameplayRequests

  public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	void RequestLeaveOnlineSession();

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	void RequestSetLobbyMapSelection(FName RequestedMapId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	void RequestSetLobbyReady(bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	void RequestStartSelectedGameplayMap();

	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	void RequestReturnToLobby();

	UFUNCTION(Client, Reliable)
	void Client_NotifyOnlineSessionEnded(FName Reason);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestSetInventoryOpen(bool bInventoryOpen);

	UFUNCTION(BlueprintCallable, Category = "Heist|Forgery")
	void RequestCancelForgery();

	void RequestSubmitForgeryStrokes(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices,
									 const TArray<uint8>& StrokeBrushPresetIndices, int32 ClientSessionRevision = INDEX_NONE);
	void RequestConfirmForgeryReplicaSwap();
	void RequestRestartForgeryFromPreview();

	void RequestBeginObjectAssembly(AHeistObjectDisplayCaseActor* TargetDisplayCase, float DurationSeconds = -1.0f);
	void RequestCancelObjectAssembly();
	void RequestSubmitObjectAssembly(const TArray<FHeistObjectAssemblyEntry>& Entries, int32 ClientSessionRevision = INDEX_NONE);
	void RequestConfirmObjectAssemblyReplicaSwap();
	void RequestRestartObjectAssemblyFromPreview();
	void RestoreGameplayInputAfterForcedForgeryClose();

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestMoveInventoryItem(int32 InstanceId, FIntPoint TargetGridPosition);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestRotateInventoryItem(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestDropInventoryItem(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Objective")
	void RequestTakeOriginal(AHeistPaintingDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(BlueprintCallable, Category = "Heist|Objective")
	void RequestTakeObjectOriginal(AHeistObjectDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(BlueprintCallable, Category = "Heist|Objective")
	void RequestDropCarriedOriginal();

	UFUNCTION(BlueprintCallable, Category = "Heist|Player")
	void RequestRescuePlayer(AHeistPlayerCharacter* TargetPlayerCharacter);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestAssignQuickSlot(EHeistQuickSlotType SlotType, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestClearQuickSlot(EHeistQuickSlotType SlotType);

	UFUNCTION(BlueprintCallable, Category = "Heist|QuickSlot")
	void RequestUseQuickSlot(EHeistQuickSlotType SlotType);

	bool TryBuildCameraForwardAim(float Distance, FVector& OutViewLocation, FVector& OutCameraForward, FVector& OutTargetWorldLocation) const;

  private:
	UFUNCTION(Server, Reliable)
	void Server_RequestSetLobbyMapSelection(FName RequestedMapId);

	UFUNCTION(Server, Reliable)
	void Server_RequestSetLobbyReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void Server_RequestStartSelectedGameplayMap();

	UFUNCTION(Server, Reliable)
	void Server_RequestReturnToLobby();

	UFUNCTION(Server, Reliable)
	void Server_RequestLootPickup(AHeistLootActor* TargetLootActor);

	UFUNCTION(Server, Reliable)
	void Server_RequestDroppedOriginalPickup(AHeistDroppedOriginalActor* TargetDroppedOriginal);

	UFUNCTION(Server, Reliable)
	void Server_RequestEscape(AHeistVentActor* TargetVentActor);

	UFUNCTION(Server, Reliable)
	void Server_RequestObservation(AHeistPaintingDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(Server, Reliable)
	void Server_RequestObjectObservation(AHeistObjectDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(Server, Reliable)
	void Server_CancelObservation();

	UFUNCTION(Server, Reliable)
	void Server_RequestBeginSecurityHold(AHeistSecurityHoldButtonActor* TargetButton);

	UFUNCTION(Server, Reliable)
	void Server_RequestEndSecurityHold(AHeistSecurityHoldButtonActor* TargetButton);

	UFUNCTION(Server, Reliable)
	void Server_SetInventoryOpen(bool bInventoryOpen);

	UFUNCTION(Server, Reliable)
	void Server_CancelForgery();

	UFUNCTION(Server, Reliable)
	void Server_SubmitForgeryStrokes(const TArray<uint32>& PackedNormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices,
									 const TArray<uint8>& StrokeBrushPresetIndices, int32 ClientSessionRevision);

	UFUNCTION(Server, Reliable)
	void Server_ConfirmForgeryReplicaSwap();

	UFUNCTION(Server, Reliable)
	void Server_RestartForgeryFromPreview();

	UFUNCTION(Server, Reliable)
	void Server_RequestBeginObjectAssembly(AHeistObjectDisplayCaseActor* TargetDisplayCase, float DurationSeconds);

	UFUNCTION(Server, Reliable)
	void Server_CancelObjectAssembly();

	UFUNCTION(Server, Reliable)
	void Server_SubmitObjectAssembly(const TArray<FHeistObjectAssemblyEntry>& Entries, int32 ClientSessionRevision);

	UFUNCTION(Server, Reliable)
	void Server_ConfirmObjectAssemblyReplicaSwap();

	UFUNCTION(Server, Reliable)
	void Server_RestartObjectAssemblyFromPreview();

	UFUNCTION(Server, Reliable)
	void Server_RequestMoveInventoryItem(int32 InstanceId, FIntPoint TargetGridPosition);

	UFUNCTION(Server, Reliable)
	void Server_RequestRotateInventoryItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestDropInventoryItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestTakeOriginal(AHeistPaintingDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(Server, Reliable)
	void Server_RequestTakeObjectOriginal(AHeistObjectDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(Server, Reliable)
	void Server_RequestDropCarriedOriginal();

	UFUNCTION(Server, Reliable)
	void Server_RequestRescuePlayer(AHeistPlayerCharacter* TargetPlayerCharacter);

	UFUNCTION(Server, Reliable)
	void Server_RequestAssignQuickSlot(EHeistQuickSlotType SlotType, int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestClearQuickSlot(EHeistQuickSlotType SlotType);

	UFUNCTION(Server, Reliable)
	void Server_RequestUseQuickSlot(EHeistQuickSlotType SlotType, FVector TargetWorldLocation);

	UFUNCTION(Server, Unreliable)
	void Server_UpdateFlashlightAimDirection(FVector_NetQuantizeNormal ClientCameraForward);

	UFUNCTION(Server, Reliable)
	void Server_SetSprintRequested(bool bRequested);

	UFUNCTION(Server, Reliable)
	void Server_SetVoicePushToTalk(bool bHeld);

	UFUNCTION(Server, Unreliable)
	void Server_ReportVoiceActivity();

#pragma endregion

#pragma region Debug

  public:
	void DebugRequestAddInventoryItem(FName ItemId);
	void DebugRequestThrowCoinAtWorldLocation(FVector TargetWorldLocation);
	void DebugRequestSpawnGuard(float Distance);
	void DebugRequestSetNearestGuardState(EHeistGuardState GuardState, float DurationSeconds);
	void DebugRequestEvaluateNearestGuardSight();
	void DebugRequestSetNearestGuardAutomaticSight(bool bEnabled);
	void DebugRequestReportGuardNoise(float Distance);
	void DebugRequestRebuildResults();
	void DebugRequestSeedResult(bool bEscaped);
	void DebugRequestSeedContribution(int32 SurfaceForgeries, float BestSurfaceQuality, int32 Assemblies, float BestAssemblyQuality, int32 ArtifactsRecovered,
		float CarryTimeSeconds, int32 SecuredLootValue, int32 GuardsDistracted, int32 TeammatesRescued, int32 AlarmsTriggered);
	void DebugRequestFeedbackTest();
	void DebugRequestFillInventoryForFeedback(FName ItemId);
	void DebugRequestSetArrested(bool bArrested);
	void DebugRequestDumpArrestState();
	void DebugRequestSetFootstepWeight(float TotalLootWeight);
	void DebugRequestDumpDifficultyBaseline();
	void DebugRequestForgeryScoreTest();

  private:
	UFUNCTION(Server, Reliable)
	void Server_DebugRequestAddInventoryItem(FName ItemId);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestThrowCoinAtWorldLocation(FVector TargetWorldLocation);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSpawnGuard(float Distance);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSetNearestGuardState(EHeistGuardState GuardState, float DurationSeconds);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestEvaluateNearestGuardSight();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSetNearestGuardAutomaticSight(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestReportGuardNoise(float Distance);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestRebuildResults();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSeedResult(bool bEscaped);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSeedContribution(int32 SurfaceForgeries, float BestSurfaceQuality, int32 Assemblies, float BestAssemblyQuality, int32 ArtifactsRecovered,
		float CarryTimeSeconds, int32 SecuredLootValue, int32 GuardsDistracted, int32 TeammatesRescued, int32 AlarmsTriggered);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSetArrested(bool bArrested);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestDumpArrestState();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSetFootstepWeight(float TotalLootWeight);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestDumpDifficultyBaseline();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestFeedbackTest();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestFillInventoryForFeedback(FName ItemId);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestForgeryScoreTest();

#pragma endregion

#pragma region Feedback

  public:
	FHeistPopupFeedbackRequested& GetPopupFeedbackRequestedDelegate();

  private:
	UFUNCTION(Client, Reliable)
	void Client_ReceivePopupFeedback(const FText& Message, float DurationSeconds);

	void SendPopupFeedback(const FText& Message, float DurationSeconds = 2.0f);
	void SendPopupFeedbackForRejection(const TCHAR* RequestName, const TCHAR* Reason);
	static FText ResolvePopupFeedbackText(const TCHAR* RequestName, const TCHAR* Reason);

	FHeistPopupFeedbackRequested PopupFeedbackRequestedDelegate;

#pragma endregion

#pragma region InternalHelpers

  private:
	struct FHeistGameplayRequestContext
	{
		AHeistPlayerCharacter* Character = nullptr;
		AHeistPlayerState* PlayerState = nullptr;
		UHeistInventoryComponent* InventoryComponent = nullptr;
	};

	bool TryBuildGameplayRequestContext(FHeistGameplayRequestContext& OutContext, const TCHAR*& OutRejectReason) const;
	bool TryBuildInventoryMutationRequestContext(FHeistGameplayRequestContext& OutContext, const TCHAR*& OutRejectReason) const;

	bool TryResolveQuickSlotItem(const FHeistGameplayRequestContext& RequestContext, EHeistQuickSlotType SlotType, FName& OutItemId, const TCHAR*& OutRejectReason) const;
	AHeistGuardCharacter* FindNearestGuard() const;
	bool TrySpawnThrowableProjectile(const FHeistGameplayRequestContext& RequestContext, FName ItemId, const FVector& TargetWorldLocation, bool bDebugBypassInventory,
									 AHeistThrowableProjectile*& OutProjectile, const TCHAR*& OutRejectReason) const;
	static FName GetExpectedQuickSlotItemId(EHeistQuickSlotType SlotType);

	void LogLootPickupRejected(const AHeistLootActor* TargetLootActor, const TCHAR* Reason, float Distance = -1.0f);
	void LogEscapeRequestRejected(const AHeistVentActor* TargetVentActor, const TCHAR* Reason, float Distance = -1.0f);
	void LogInventoryRequestRejected(const TCHAR* RequestName, int32 InstanceId, const TCHAR* Reason);
	void LogThrowableUseRejected(EHeistQuickSlotType SlotType, FName ItemId, const TCHAR* Reason);

#pragma endregion
};
