#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistForgeryComponent.generated.h"

class AHeistDisplayCaseActor;
class AHeistPlayerState;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE(FHeistForgerySessionStateChanged);

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistForgeryComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistForgeryComponent();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region Session

public:
	bool TryBeginForgerySession(
		AHeistDisplayCaseActor* TargetDisplayCase,
		float DurationSeconds = -1.0f);
	bool TryPrepareForgeryTemplate(
		AHeistDisplayCaseActor* TargetDisplayCase,
		float& OutObservationDuration);
	bool ClearPreparedForgeryTemplate(FName Reason);
	bool TryBeginSubmit();
	bool CancelForgerySession(FName Reason);
	bool ForceTimeoutForDebug();

	bool IsSessionActive() const;
	bool IsSubmitPending() const;
	float GetSessionEndServerTime() const;
	int32 GetSessionRevision() const;
	AHeistDisplayCaseActor* GetActiveDisplayCase() const;
	FName GetLastCleanupReason() const;
	bool HasPreparedForgeryTemplate() const;
	FName GetActiveArtifactId() const;
	FName GetActiveTemplateId() const;
	const TSoftObjectPtr<UTexture2D>& GetReferenceImageAsset() const;
	const TSoftObjectPtr<UTexture2D>& GetReferenceMaskAsset() const;
	UTexture2D* LoadReferenceImage() const;
	UTexture2D* LoadReferenceMask() const;
	float GetTemplateObservationDuration() const;
	float GetTemplateForgeryDuration() const;
	int32 GetTemplateStrokeLimit() const;
	float GetTemplateBrushSize() const;
	FHeistForgerySessionStateChanged& GetSessionStateChangedDelegate();

private:
	bool ValidateActiveSession(FName& OutRejectReason) const;
	void HandleSessionTimeout();
	void ClearSession(FName Reason, bool bReleaseCaseLock);
	void BroadcastSessionSnapshot(const TCHAR* ChangeSource, FName Reason);
	void UnbindActiveDisplayCase();
	void ResetPreparedTemplateSnapshot();

	UFUNCTION()
	void HandleDisplayCaseSessionChanged(
		AHeistPlayerState* SessionOwner,
		bool bLocked,
		int32 Revision);

	UFUNCTION()
	void OnRep_SessionRevision();

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistDisplayCaseActor> ActiveDisplayCase;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery",
		meta = (AllowPrivateAccess = "true"))
	bool bSessionActive = false;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery",
		meta = (AllowPrivateAccess = "true"))
	bool bSubmitPending = false;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery",
		meta = (AllowPrivateAccess = "true"))
	float SessionEndServerTime = 0.0f;

	UPROPERTY(
		ReplicatedUsing = OnRep_SessionRevision,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery",
		meta = (AllowPrivateAccess = "true"))
	int32 SessionRevision = 0;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	bool bTemplatePrepared = false;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	FName ActiveArtifactId = NAME_None;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	FName ActiveTemplateId = NAME_None;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> ReferenceImageAsset;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> ReferenceMaskAsset;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	float TemplateObservationDuration = 0.0f;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	float TemplateForgeryDuration = 0.0f;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	int32 TemplateStrokeLimit = 0;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery|Template",
		meta = (AllowPrivateAccess = "true"))
	float TemplateBrushSize = 0.0f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Heist|Forgery",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "s"))
	float DefaultSessionDurationSeconds = 60.0f;

	UPROPERTY(Transient)
	FName LastCleanupReason = NAME_None;

	FTimerHandle SessionTimeoutTimerHandle;
	FHeistForgerySessionStateChanged SessionStateChangedDelegate;
	bool bHandlingCaseSessionCallback = false;
	TWeakObjectPtr<AHeistDisplayCaseActor> PreparedDisplayCase;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
