#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistForgeryWidget.generated.h"

class UTextBlock;
class UWidget;
class UImage;

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
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(
		const FCaptureLostEvent& CaptureLostEvent) override;

#pragma endregion

#pragma region Setup

public:
	void SetupForgeryWidget(class UHeistForgeryViewModel* InForgeryViewModel);
	bool IsOwnerOnlyContractSatisfied() const;
	bool IsWidgetPresentationVisible() const;
	bool IsDrawingSurfaceReady() const;
	bool AreCollectedPointsNormalized() const;
	int32 GetCollectedStrokeCount() const;
	int32 GetCollectedPointCount() const;
	int32 GetCollectedSegmentCount() const;
	int32 GetErasedStrokeCount() const;
	int32 GetConfiguredStrokeLimit() const;
	float GetConfiguredBrushSize() const;

private:
	void RefreshForgeryPresentation();
	void ApplyStateVisibility(UWidget* TargetWidget, bool bVisible) const;

	UPROPERTY(
		Transient,
		BlueprintReadOnly,
		Category = "Heist|Forgery",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistForgeryViewModel> ForgeryViewModel;

protected:
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "Heist|Forgery",
		meta = (DisplayName = "Forgery Sources Ready"))
	void BP_OnForgerySourcesReady();

	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "Heist|Forgery",
		meta = (DisplayName = "Refresh Forgery Presentation"))
	void BP_RefreshForgeryPresentation(
		bool bObservation,
		bool bDrawing,
		bool bValidation,
		bool bResult,
		float StateEndServerTime,
		float ResultScore);

#pragma endregion

#pragma region DrawingCanvas

private:
	bool IsDrawingInputEnabled() const;
	bool TryResolveNormalizedDrawingPoint(
		const FPointerEvent& PointerEvent,
		FVector2D& OutNormalizedPoint) const;
	bool BeginLocalStroke(const FVector2D& NormalizedPoint);
	bool AppendLocalStrokePoint(const FVector2D& NormalizedPoint);
	bool CompactLocalStrokesForPointBudget();
	bool EraseLocalStrokeSegments(const FVector2D& NormalizedPoint);
	void FinishPointerInteraction();
	void ResetLocalStrokePreview();
	void RefreshDrawingFeedback();
	float GetNormalizedEraseRadius() const;
	static float GetPointToSegmentDistanceSquared(
		const FVector2D& Point,
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd);

	TArray<TArray<FVector2D>> LocalStrokes;
	int32 ActiveStrokeIndex = INDEX_NONE;
	int32 ErasedStrokeCount = 0;
	bool bErasePointerActive = false;
	bool bWasDrawingVisible = false;
	TOptional<FGeometry> DrawingInputWidgetGeometry;
	mutable bool bPendingDrawCoordinateLog = false;
	mutable FVector2D PendingDrawMouseScreen = FVector2D::ZeroVector;
	mutable FVector2D PendingDrawNormalizedPoint = FVector2D::ZeroVector;

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
