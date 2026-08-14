#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistObjectAssemblyWidget.generated.h"

class AHeistPlayerController;
class UBorder;
class UButton;
class UCanvasPanel;
class UHeistObjectAssemblyViewModel;
class UFont;
class UTextBlock;

/** Owner-only 2D memory assembly screen. Intermediate drag positions stay local. */
UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistObjectAssemblyWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistObjectAssemblyWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

#pragma endregion

#pragma region Setup

  public:
	void SetupObjectAssemblyWidget(UHeistObjectAssemblyViewModel* InObjectAssemblyViewModel, AHeistPlayerController* InPlayerController);
	bool IsOwnerOnlyContractSatisfied() const;
	bool IsWidgetPresentationVisible() const;
	bool IsAlertWarningContractSatisfied() const;
	bool IsCanvasReady() const;
	int32 GetPartTileCount() const;

  private:
	void BindActionButtons();
	void RefreshObjectAssemblyPresentation();
	void RefreshCountdownPresentation();
	bool TryForceCloseForAlert();
	void RebuildPartTiles();
	void ClearPartTiles();
	void RefreshPartTilePresentation();
	FName FindPartTileAtScreenPosition(const FVector2D& ScreenPosition) const;
	FName ResolveClosestCompatibleSocket(FName PartId, const FVector2D& CanvasPoint) const;
	FVector2D ResolveSocketAnchorNormalized(FName SocketId) const;
	FVector2D ResolvePartTileSize(FName PartId) const;
	FVector2D ResolveTrayPosition(int32 CandidateIndex, const FVector2D& CanvasSize, const FVector2D& TileSize) const;
	FVector2D GetAssemblyCanvasSize() const;
	void SetPartTilePosition(FName PartId, const FVector2D& Position);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistObjectAssemblyViewModel> ObjectAssemblyViewModel;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UBorder>> PartTiles;

	FName DraggedPartId = NAME_None;
	FVector2D DragOffset = FVector2D::ZeroVector;
	int32 DisplayedSessionRevision = INDEX_NONE;
	int32 DisplayedPreviewRevision = INDEX_NONE;
	int32 LastDisplayedAssemblyTimeSeconds = INDEX_NONE;
	bool bAlertExitRequested = false;

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Object Assembly", meta = (DisplayName = "Object Assembly Sources Ready"))
	void BP_OnObjectAssemblySourcesReady();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Object Assembly", meta = (DisplayName = "Refresh Object Assembly Presentation"))
	void BP_RefreshObjectAssemblyPresentation(bool bVisible, bool bDataReady, int32 PlacedPartCount, int32 RequiredPartCount);

#pragma endregion

#pragma region Input

  private:
	UFUNCTION()
	void HandleSubmitClicked();

	UFUNCTION()
	void HandleCancelClicked();

#pragma endregion

#pragma region Presentation

  private:
	UPROPERTY(EditDefaultsOnly, Category = "Heist|Object Assembly|Presentation")
	TObjectPtr<UFont> KoreanUIFont;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCanvasPanel> AssemblyCanvas;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PreviewScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AssemblyTimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> FooterHint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SubmitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SubmitButtonLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CancelButtonLabel;

#pragma endregion
};
