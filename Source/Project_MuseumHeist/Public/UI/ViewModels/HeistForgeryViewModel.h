#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistForgeryViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistForgeryPresentationChanged);

class UTexture2D;

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistForgeryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistForgeryViewModel(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

public:
	void SetupViewModel(
		class AHeistGameState* InGameState,
		class UHeistActionComponent* InActionComponent,
		class UHeistForgeryComponent* InForgeryComponent);
	void RefreshPresentationState();
	FHeistForgeryPresentationChanged& GetPresentationChangedDelegate();

	void ShowResultPresentation(float Score, const FText& InResultText);
	void ClearResultPresentation();
	bool SetDebugPreviewState(FName StateName);
	FName GetDebugPreviewState() const;

private:
	void HandleActionStateChanged();
	void HandleForgerySessionStateChanged();
	void HandleObjectiveStateChanged(
		FName ArtifactId,
		FName CaseId,
		EHeistObjectiveState ObjectiveState,
		class AHeistPlayerState* CarrierCandidate);

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<UHeistActionComponent> ActionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHeistForgeryComponent> ForgeryComponent;

	FHeistForgeryPresentationChanged PresentationChangedDelegate;
	bool bResultPresentationActive = false;
	float PendingResultScore = 0.0f;
	FText PendingResultText;
	FName DebugPreviewState = NAME_None;

#pragma endregion

#pragma region Presentation

public:
	bool IsPresentationVisible() const;
	bool IsObservationVisible() const;
	bool IsDrawingVisible() const;
	bool IsValidationVisible() const;
	bool IsResultVisible() const;
	float GetStateEndServerTime() const;
	float GetResultScore() const;
	FName GetReferenceArtifactId() const;
	FName GetReferenceTemplateId() const;
	UTexture2D* GetReferenceImage() const;
	UTexture2D* GetReferenceMask() const;
	const TArray<FLinearColor>& GetAllowedPalette() const;
	float GetObservationDuration() const;
	float GetForgeryDuration() const;
	int32 GetStrokeLimit() const;
	float GetBrushSize() const;
	FName GetActiveDisplayCaseName() const;
	const FText& GetStateText() const;
	const FText& GetReferenceText() const;
	const FText& GetResultText() const;
	int32 GetVisibleStateCount() const;

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bPresentationVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bObservationVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bDrawingVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bValidationVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bResultVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	float StateEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	float ResultScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	FName ReferenceArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	FName ReferenceTemplateId = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> ReferenceImage;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> ReferenceMask;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	TArray<FLinearColor> AllowedPalette;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	float ObservationDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	float ForgeryDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	int32 StrokeLimit = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	float BrushSize = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	FName ActiveDisplayCaseName = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	FText StateText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	FText ReferenceText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	FText ResultText;

#pragma endregion
};
