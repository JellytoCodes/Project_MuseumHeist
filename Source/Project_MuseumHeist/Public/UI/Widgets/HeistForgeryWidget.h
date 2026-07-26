#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistForgeryWidget.generated.h"

class UTextBlock;
class UWidget;
class UImage;
class UButton;

struct FHeistLocalForgeryStroke
{
	TArray<FVector2D> Points;
	uint8 PaletteIndex = 0;
};

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistForgeryWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistForgeryWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
							  const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

#pragma endregion

#pragma region Setup

  public:
	void SetupForgeryWidget(class UHeistForgeryViewModel* InForgeryViewModel);
	bool IsOwnerOnlyContractSatisfied() const;
	bool IsWidgetPresentationVisible() const;
	bool IsDrawingSurfaceReady() const;
	FVector2D GetDrawingSurfaceSize() const;
	bool AreCollectedPointsNormalized() const;
	int32 GetCollectedStrokeCount() const;
	int32 GetCollectedPointCount() const;
	int32 GetCollectedSegmentCount() const;
	int32 GetErasedStrokeCount() const;
	int32 GetVisiblePaletteButtonCount() const;
	FString GetPreviewScoreText() const;
	int32 GetConfiguredStrokeLimit() const;
	float GetConfiguredBrushSize() const;
	UFUNCTION(BlueprintPure, Category = "Heist|Forgery|Palette")
	int32 GetActivePaletteIndex() const;
	UFUNCTION(BlueprintPure, Category = "Heist|Forgery|Palette")
	FLinearColor GetActivePaletteColor() const;
	UFUNCTION(BlueprintCallable, Category = "Heist|Forgery|Palette")
	bool SelectPaletteIndex(int32 PaletteIndex);
	UFUNCTION(BlueprintCallable, Category = "Heist|Forgery|Drawing")
	bool ResetDrawingCanvas();
	UFUNCTION(BlueprintPure, Category = "Heist|Forgery|Drawing")
	float GetDrawingTimeRemainingSeconds() const;
	bool RequestSubmitCollectedStrokes();
	bool IsAlertWarningContractSatisfied() const;
	void DebugDumpAlertWarningState() const;

  private:
	void RefreshForgeryPresentation();
	void RefreshAlertWarningPresentation();
	void RefreshForgeryLockdownCountdown();
	void ApplyStateVisibility(UWidget* TargetWidget, bool bVisible) const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistForgeryViewModel> ForgeryViewModel;

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Forgery", meta = (DisplayName = "Forgery Sources Ready"))
	void BP_OnForgerySourcesReady();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Forgery", meta = (DisplayName = "Refresh Forgery Presentation"))
	void BP_RefreshForgeryPresentation(bool bObservation, bool bDrawing, bool bValidation, bool bResult, float StateEndServerTime, float ResultScore);

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Forgery|Palette", meta = (DisplayName = "Refresh Forgery Palette"))
	void BP_RefreshForgeryPalette(const TArray<FLinearColor>& AllowedPalette, int32 InActivePaletteIndex);

#pragma endregion

#pragma region DrawingCanvas

  private:
	bool IsDrawingInputEnabled() const;
	bool TryResolveNormalizedDrawingPoint(const FPointerEvent& PointerEvent, FVector2D& OutNormalizedPoint) const;
	bool BeginLocalStroke(const FVector2D& NormalizedPoint);
	bool AppendLocalStrokePoint(const FVector2D& NormalizedPoint);
	bool CompactLocalStrokesForPointBudget();
	bool EraseLocalStrokeSegments(const FVector2D& NormalizedPoint);
	bool BuildDrawableStrokePayload(TArray<FVector2D>& OutNormalizedPoints, TArray<int32>& OutStrokePointCounts, TArray<uint8>& OutStrokePaletteIndices, int32& OutIgnoredShortStrokeCount) const;
	void FinishPointerInteraction();
	void ResetLocalStrokePreview();
	void RefreshDrawingFeedback();
	void MarkPreviewScoreDirty();
	void RefreshLocalPreviewScore();
	void RefreshDrawingTimeRemaining();
	void BindPaletteButtons();
	void RefreshPaletteButtons();
	float GetNormalizedEraseRadius() const;
	static float GetPointToSegmentDistanceSquared(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd);

	TArray<FHeistLocalForgeryStroke> LocalStrokes;
	int32 ActiveStrokeIndex = INDEX_NONE;
	int32 ActivePaletteIndex = 0;
	int32 ErasedStrokeCount = 0;
	bool bErasePointerActive = false;
	bool bWasDrawingVisible = false;
	TOptional<FGeometry> DrawingInputWidgetGeometry;
	mutable bool bPendingDrawCoordinateLog = false;
	mutable FVector2D PendingDrawMouseScreen = FVector2D::ZeroVector;
	mutable FVector2D PendingDrawNormalizedPoint = FVector2D::ZeroVector;
	float PreviewScoreUpdateAccumulator = 0.0f;
	bool bPreviewScoreDirty = true;
	int32 LastDisplayedDrawingTimeSeconds = INDEX_NONE;
	int32 LastDisplayedLockdownSeconds = INDEX_NONE;

	UFUNCTION()
	void HandlePaletteButton1Clicked();

	UFUNCTION()
	void HandlePaletteButton2Clicked();

	UFUNCTION()
	void HandlePaletteButton3Clicked();

	UFUNCTION()
	void HandlePaletteButton4Clicked();

	UFUNCTION()
	void HandlePaletteButton5Clicked();

	UFUNCTION()
	void HandlePaletteButton6Clicked();

	UFUNCTION()
	void HandlePaletteButton7Clicked();

	UFUNCTION()
	void HandlePaletteButton8Clicked();

#pragma endregion

#pragma region Presentation

  private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ObservationContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> DrawingContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> DrawingSurface;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DrawingPlaceholder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DrawingHint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DrawingTimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ForgeryAlertWarningText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ForgeryLockdownCountdownText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PreviewScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PaletteButton1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PaletteButton2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PaletteButton3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PaletteButton4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PaletteButton5;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PaletteButton6;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PaletteButton7;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PaletteButton8;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PaletteButtonText1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PaletteButtonText2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PaletteButtonText3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PaletteButtonText4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PaletteButtonText5;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PaletteButtonText6;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PaletteButtonText7;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PaletteButtonText8;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ValidationContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ReferenceText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> ReferenceImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultScoreText;

#pragma endregion
};
