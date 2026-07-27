#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HeistTypes.h"
#include "Data/HeistArtifactDataTypes.h"

#include "HeistObjectAssemblyComponent.generated.h"

class AHeistObjectDisplayCaseActor;
class AHeistPlayerState;

DECLARE_MULTICAST_DELEGATE(FHeistObjectAssemblySessionStateChanged);

/**
 * Player-owned, server-authoritative Object Assembly session and scoring.
 *
 * This component transports stable identifiers only. It never receives mesh
 * data, arbitrary transforms, preview actors, or physics state from clients.
 */
UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistObjectAssemblyComponent : public UActorComponent
{
	GENERATED_BODY()

  public:
	UHeistObjectAssemblyComponent();

  protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  public:
	bool TryBeginAssemblySession(AHeistObjectDisplayCaseActor* TargetDisplayCase, float DurationSeconds = -1.0f);
	bool TrySubmitAssemblyPayload(const TArray<FHeistObjectAssemblyEntry>& Entries, int32 ClientSessionRevision);
	bool CancelAssemblySession(FName Reason);
	bool ForceTimeoutForDebug();

	bool IsSessionActive() const;
	float GetSessionEndServerTime() const;
	int32 GetSessionRevision() const;
	AHeistObjectDisplayCaseActor* GetActiveDisplayCase() const;
	FName GetActiveArtifactId() const;
	FName GetActiveTemplateId() const;
	FName GetActiveFamilyId() const;
	FName GetLastCleanupReason() const;
	bool WasLastPayloadAccepted() const;
	FName GetLastPayloadReason() const;
	int32 GetPayloadValidationRevision() const;
	int32 GetValidatedEntryCount() const;
	int32 GetValidatedPayloadBytes() const;
	const TArray<FHeistObjectAssemblyEntry>& GetValidatedEntries() const;
	bool HasAuthoritativeResult() const;
	const FHeistObjectAssemblyResult& GetAuthoritativeResult() const;
	int32 GetScoreRevision() const;
	const FHeistObjectAssemblyTemplateRow* GetPreparedTemplateForDebug() const;
	FHeistObjectAssemblySessionStateChanged& GetSessionStateChangedDelegate();

  private:
	bool TryPrepareTemplate(AHeistObjectDisplayCaseActor* TargetDisplayCase, FName& OutRejectReason);
	bool ValidateActiveSession(FName& OutRejectReason) const;
	bool ValidatePayload(const TArray<FHeistObjectAssemblyEntry>& Entries, int32 ClientSessionRevision, FName& OutRejectReason, int32& OutPayloadBytes) const;
	bool CalculateDeterministicScore(const TArray<FHeistObjectAssemblyEntry>& Entries, FHeistObjectAssemblyResult& OutResult) const;
	void RecordPayloadValidation(bool bAccepted, FName Reason, int32 EntryCount, int32 PayloadBytes);
	void ResetPayloadState(bool bResetLastValidation);
	void ResetPreparedTemplate();
	void ResetAuthoritativeResult();
	void CompleteSuccessfulSession();
	void HandleSessionTimeout();
	void ClearSession(FName Reason, bool bReleaseCaseLock, bool bPreserveResult);
	void BroadcastSessionSnapshot(FName EventName, FName Reason, bool bResult);

	UFUNCTION()
	void HandleDisplayCaseSessionChanged(AHeistPlayerState* SessionOwner, bool bLocked, int32 Revision);

	UFUNCTION()
	void OnRep_SessionRevision();

	UFUNCTION()
	void OnRep_PayloadValidationRevision();

	UFUNCTION()
	void OnRep_ScoreRevision();

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistObjectDisplayCaseActor> ActiveDisplayCase;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	bool bSessionActive = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	float SessionEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SessionRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	int32 SessionRevision = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Template", meta = (AllowPrivateAccess = "true"))
	FName ActiveArtifactId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Template", meta = (AllowPrivateAccess = "true"))
	FName ActiveTemplateId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Template", meta = (AllowPrivateAccess = "true"))
	FName ActiveFamilyId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Payload", meta = (AllowPrivateAccess = "true"))
	bool bLastPayloadAccepted = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Payload", meta = (AllowPrivateAccess = "true"))
	FName LastPayloadReason = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_PayloadValidationRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Payload", meta = (AllowPrivateAccess = "true"))
	int32 PayloadValidationRevision = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Payload", meta = (AllowPrivateAccess = "true"))
	int32 ValidatedEntryCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Payload", meta = (AllowPrivateAccess = "true"))
	int32 ValidatedPayloadBytes = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Score", meta = (AllowPrivateAccess = "true"))
	bool bHasAuthoritativeResult = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Score", meta = (AllowPrivateAccess = "true"))
	FHeistObjectAssemblyResult AuthoritativeResult;

	UPROPERTY(ReplicatedUsing = OnRep_ScoreRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Score", meta = (AllowPrivateAccess = "true"))
	int32 ScoreRevision = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "s"))
	float DefaultSessionDurationSeconds = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Payload", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "32"))
	int32 MaximumPayloadEntries = 16;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Payload", meta = (AllowPrivateAccess = "true", ClampMin = "64", ClampMax = "16384"))
	int32 MaximumPayloadBytes = 4096;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Payload", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float SubmissionTimeoutToleranceSeconds = 0.25f;

	FHeistObjectAssemblyTemplateRow PreparedTemplate;
	TMap<FName, FHeistObjectAssemblyPartRow> PreparedPartDefinitions;
	TArray<FHeistObjectAssemblyEntry> ValidatedEntries;
	float SessionStartServerTime = 0.0f;
	float ActiveSessionDurationSeconds = 0.0f;
	FName LastCleanupReason = NAME_None;
	FTimerHandle SessionTimeoutTimerHandle;
	FHeistObjectAssemblySessionStateChanged SessionStateChangedDelegate;
	bool bHandlingCaseSessionCallback = false;

  public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
