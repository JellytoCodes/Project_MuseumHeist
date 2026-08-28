#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Data/HeistArtifactDataTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistForgeryViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistForgeryPresentationChanged);

class UTexture2D;

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistForgeryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Lifecycle

  protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

  public:
	void SetupViewModel(class UHeistForgeryComponent* InForgeryComponent);
	void RefreshPresentationState();
	FHeistForgeryPresentationChanged& GetPresentationChangedDelegate();

  private:
	void HandleForgerySessionStateChanged();

	UPROPERTY(Transient)
	TObjectPtr<UHeistForgeryComponent> ForgeryComponent;

	FHeistForgeryPresentationChanged PresentationChangedDelegate;

#pragma endregion

#pragma region Presentation

  public:
	bool IsPresentationVisible() const;
	bool IsDrawingVisible() const;
	float GetStateEndServerTime() const;
	UTexture2D* GetReferenceImage() const;
	const TArray<FLinearColor>& GetAllowedPalette() const;
	int32 GetStrokeLimit() const;
	float GetBrushSize() const;
	float GetBrushSizeForPreset(int32 BrushPresetIndex) const;
	bool IsSubmitPending() const;
	FName GetLastSubmissionRejectReason() const;
	int32 GetStrokeValidationRevision() const;
	float GetMinimumAcceptedQualityScore() const;
	int32 GetScoreRasterResolution() const;
	bool CalculatePreviewScore(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices,
		const TArray<uint8>& StrokeBrushPresetIndices, FHeistForgeryResult& OutResult, int32& OutReferenceMaskPixels, int32& OutSubmittedMaskPixels) const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bPresentationVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	bool bDrawingVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	float StateEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> ReferenceImage;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	TArray<FLinearColor> AllowedPalette;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	int32 StrokeLimit = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	float BrushSize = 0.0f;

#pragma endregion
};
