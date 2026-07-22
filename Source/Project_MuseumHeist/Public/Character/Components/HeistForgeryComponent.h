#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HeistTypes.h"
#include "Data/HeistArtifactDataTypes.h"

#include "HeistForgeryComponent.generated.h"

class AHeistPaintingDisplayCaseActor;
class AHeistPlayerState;
class UTexture2D;
struct FHeistReplicaPaintingData;

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
	bool TryBeginForgerySession(AHeistPaintingDisplayCaseActor* TargetDisplayCase, float DurationSeconds = -1.0f);
	bool TryPrepareForgeryTemplate(AHeistPaintingDisplayCaseActor* TargetDisplayCase, float& OutObservationDuration);
	bool ClearPreparedForgeryTemplate(FName Reason);
	bool TryBeginSubmit();
	bool TrySubmitStrokePayload(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices, float ClientBrushSize,
								int32 ClientSessionRevision);
	bool CancelForgerySession(FName Reason);
	bool ForceTimeoutForDebug();
	bool ForceExpireSubmissionWindowForDebug();
	bool ForceNearExpirySubmissionWindowForDebug();

	bool IsSessionActive() const;
	bool IsSubmitPending() const;
	float GetSessionEndServerTime() const;
	int32 GetSessionRevision() const;
	AHeistPaintingDisplayCaseActor* GetActiveDisplayCase() const;
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
	const TArray<FLinearColor>& GetTemplateAllowedPalette() const;
	bool HasValidatedStrokePayload() const;
	bool WasLastStrokeValidationAccepted() const;
	FName GetLastStrokeValidationReason() const;
	int32 GetStrokeValidationRevision() const;
	int32 GetValidatedStrokeCount() const;
	int32 GetValidatedPointCount() const;
	int32 GetValidatedPayloadBytes() const;
	float GetValidatedBrushSize() const;
	const TArray<FVector2D>& GetValidatedStrokePoints() const;
	const TArray<int32>& GetValidatedStrokePointCounts() const;
	const TArray<uint8>& GetValidatedStrokePaletteIndices() const;
	bool HasAuthoritativeForgeryResult() const;
	const FHeistForgeryResult& GetAuthoritativeForgeryResult() const;
	int32 GetForgeryScoreRevision() const;
	int32 GetForgeryScoreResolution() const;
	int32 GetReferenceMaskPixelCount() const;
	int32 GetSubmittedMaskPixelCount() const;
	bool RecalculateValidatedForgeryScoreForDebug(FHeistForgeryResult& OutResult, int32& OutReferenceMaskPixels, int32& OutSubmittedMaskPixels) const;
	bool RunOpenCVScoringSelfTestForDebug(FString& OutSummary) const;
	bool CalculateLocalForgeryPreview(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices, float BrushSize,
									  FHeistForgeryResult& OutResult, int32& OutReferenceMaskPixels, int32& OutSubmittedMaskPixels) const;
	FHeistForgerySessionStateChanged& GetSessionStateChangedDelegate();

  private:
	bool ValidateActiveSession(FName& OutRejectReason) const;
	bool ValidateStrokePayload(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices, float ClientBrushSize,
							   int32 ClientSessionRevision, FName& OutRejectReason, int32& OutPayloadBytes) const;
	void RecordStrokeValidationResult(bool bAccepted, FName Reason);
	void ResetStrokeTransportState(bool bResetLastValidation);
	bool TryCalculateAndCommitForgeryScore();
	bool BuildReplicaPaintingData(FHeistReplicaPaintingData& OutPaintingData) const;
	bool CalculateForgeryScore(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices, float BrushSize,
							   FHeistForgeryResult& OutResult, int32& OutReferenceMaskPixels, int32& OutSubmittedMaskPixels) const;
	bool BuildScoringReferenceCache() const;
	void ResetScoringReferenceCache() const;
	void ResetForgeryScoreState();
	void CompleteSuccessfulForgerySession();
	void HandleSessionTimeout();
	void ClearSession(FName Reason, bool bReleaseCaseLock);
	void BroadcastSessionSnapshot(const TCHAR* ChangeSource, FName Reason);
	void UnbindActiveDisplayCase();
	void ResetPreparedTemplateSnapshot();

	UFUNCTION()
	void HandleDisplayCaseSessionChanged(AHeistPlayerState* SessionOwner, bool bLocked, int32 Revision);

	UFUNCTION()
	void OnRep_SessionRevision();

	UFUNCTION()
	void OnRep_StrokeValidationRevision();

	UFUNCTION()
	void OnRep_ForgeryScoreRevision();

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPaintingDisplayCaseActor> ActiveDisplayCase;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bSessionActive = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bSubmitPending = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	float SessionEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SessionRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	int32 SessionRevision = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	bool bTemplatePrepared = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	FName ActiveArtifactId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	FName ActiveTemplateId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> ReferenceImageAsset;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> ReferenceMaskAsset;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	float TemplateObservationDuration = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	float TemplateForgeryDuration = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	int32 TemplateStrokeLimit = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	float TemplateBrushSize = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Template", meta = (AllowPrivateAccess = "true"))
	TArray<FLinearColor> TemplateAllowedPalette;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "s"))
	float DefaultSessionDurationSeconds = 60.0f;

	UPROPERTY(Transient)
	FName LastCleanupReason = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Transport", meta = (AllowPrivateAccess = "true"))
	bool bHasValidatedStrokePayload = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Transport", meta = (AllowPrivateAccess = "true"))
	bool bLastStrokeValidationAccepted = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Transport", meta = (AllowPrivateAccess = "true"))
	FName LastStrokeValidationReason = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_StrokeValidationRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Transport", meta = (AllowPrivateAccess = "true"))
	int32 StrokeValidationRevision = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Transport", meta = (AllowPrivateAccess = "true"))
	int32 ValidatedStrokeCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Transport", meta = (AllowPrivateAccess = "true"))
	int32 ValidatedPointCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Transport", meta = (AllowPrivateAccess = "true"))
	int32 ValidatedPayloadBytes = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Transport", meta = (AllowPrivateAccess = "true"))
	float ValidatedBrushSize = 0.0f;

	TArray<FVector2D> ValidatedStrokePoints;
	TArray<int32> ValidatedStrokePointCounts;
	TArray<uint8> ValidatedStrokePaletteIndices;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Score", meta = (AllowPrivateAccess = "true"))
	bool bHasAuthoritativeForgeryResult = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Score", meta = (AllowPrivateAccess = "true"))
	FHeistForgeryResult AuthoritativeForgeryResult;

	UPROPERTY(ReplicatedUsing = OnRep_ForgeryScoreRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Score", meta = (AllowPrivateAccess = "true"))
	int32 ForgeryScoreRevision = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Score", meta = (AllowPrivateAccess = "true"))
	int32 ForgeryScoreResolution = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Score", meta = (AllowPrivateAccess = "true"))
	int32 ReferenceMaskPixelCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery|Score", meta = (AllowPrivateAccess = "true"))
	int32 SubmittedMaskPixelCount = 0;

	UPROPERTY(Replicated)
	float TemplateCoverageWeight = 0.0f;

	UPROPERTY(Replicated)
	float TemplateMajorShapeWeight = 0.0f;

	UPROPERTY(Replicated)
	float TemplateExtraStrokePenaltyWeight = 0.0f;

	UPROPERTY(Replicated)
	float TemplateTimeoutPenalty = 0.0f;

	UPROPERTY(Replicated)
	EHeistForgeryBackgroundFilter TemplateBackgroundFilterMode = EHeistForgeryBackgroundFilter::None;

	UPROPERTY(Replicated)
	float TemplateBackgroundColorTolerance = 0.0f;

	UPROPERTY(Replicated)
	float TemplateShapeAccuracyWeight = 0.0f;

	UPROPERTY(Replicated)
	float TemplateColorAccuracyWeight = 0.0f;

	UPROPERTY(Replicated)
	float TemplateMaximumPaintToReferenceRatio = 0.0f;

	UPROPERTY(Replicated)
	float TemplateOverpaintScoreCap = 0.0f;
	float ActiveSessionDurationSeconds = 0.0f;

	mutable FName CachedScoringTemplateId = NAME_None;
	mutable TArray<uint8> CachedReferenceMask;
	mutable TArray<uint8> CachedReferencePaletteMap;
	mutable bool bLastScoringReferenceCacheHit = false;
	mutable double LastScoringReferenceMilliseconds = 0.0;
	mutable double LastOpenCVScoringMilliseconds = 0.0;

	FTimerHandle SessionTimeoutTimerHandle;
	FHeistForgerySessionStateChanged SessionStateChangedDelegate;
	bool bHandlingCaseSessionCallback = false;
	TWeakObjectPtr<AHeistPaintingDisplayCaseActor> PreparedDisplayCase;

#pragma endregion

#pragma region Replication

  public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
