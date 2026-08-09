#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistResultWidget.generated.h"

class UTextBlock;
class UWidget;
class UButton;
class UTexture2D;
class UHorizontalBox;
class UBorder;
class UVerticalBox;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistResultWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistResultWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModel

  public:
	void SetupResultWidget(class UHeistResultViewModel* InResultViewModel);
	UHeistResultViewModel* GetResultViewModel() const;
	/** Clears local-only detail/recap state before the persistent HUD is hidden for travel. */
	void ResetHiddenPresentationState();
	bool IsHiddenPresentationStateReset() const;
	bool IsRewardDetailVisible() const;

	/** Builds the actual submitted painting thumbnail carried by the replicated result snapshot. */
	UFUNCTION(BlueprintCallable, Category = "Heist|Result|Replica")
	UTexture2D* CreatePaintingRecapTexture(const FHeistReplicaRecapEntry& ReplicaRecap, FLinearColor BackgroundColor);

	UFUNCTION(BlueprintPure, Category = "Heist|Result|Replica")
	static FText BuildReplicaCardTitle(const FHeistReplicaRecapEntry& ReplicaRecap);

	/** Shared 2D socket layout used to reconstruct the submitted Object Assembly recipe. */
	UFUNCTION(BlueprintPure, Category = "Heist|Result|Replica")
	static FVector2D ResolveAssemblyRecapSocketAnchor(FName SocketId);

	UFUNCTION(BlueprintPure, Category = "Heist|Result|Replica")
	static FVector2D ResolveAssemblyRecapPartSize(FName PartId);

	UFUNCTION(BlueprintPure, Category = "Heist|Result|Replica")
	static float ResolveAssemblyRecapPartAngle(uint8 QuantizedOrientation);

	static bool DecodePaintingRecapPixels(const FHeistReplicaRecapEntry& ReplicaRecap, const FColor& BackgroundColor, TArray64<uint8>& OutTextureBytes);

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistResultViewModel> ResultViewModel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> ReplicaRecapTextures;

#pragma endregion

#pragma region Presentation

  private:
	void RefreshResultPresentation();
	void RefreshRewardDetailPresentation(const FHeistTeamResult& TeamResult);
	void RefreshReplicaRecapPresentation(const TArray<FHeistReplicaRecapEntry>& ReplicaRecap);
	void RefreshContributionTablePresentation(const TArray<FHeistPlayerResult>& PlayerResults);
	UWidget* CreateReplicaVisualWidget(const FHeistReplicaRecapEntry& ReplicaRecap);
	UWidget* CreateContributionTableRow(const FHeistPlayerResult* PlayerResult, bool bHeaderRow, int32 RowIndex);
	void AddContributionTableCell(UHorizontalBox* RowContainer, const FText& CellText, float Width, bool bHeaderCell, int32 RowIndex);

	UFUNCTION()
	void HandleReturnToLobbyClicked();

	UFUNCTION()
	void HandleRewardDetailsClicked();

	UFUNCTION()
	void HandleRewardDetailsCloseClicked();

	// Blueprint events must remain overridable, so presentation hooks cannot be private.
  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Result", meta = (DisplayName = "Refresh Replica Recap"))
	void BP_RefreshReplicaRecap(const TArray<FHeistReplicaRecapEntry>& ReplicaRecap);

  private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> OutcomeTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> OutcomeReasonTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TeamRewardTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailRequiredTargetValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailTargetStatusValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailQuotaValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailSecuredValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailExtraValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailRequiredTargetRewardValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailLooseLootValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailForgeryMultiplierValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailStealthMultiplierValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailArrestPenaltyValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ReplicaRecapTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> ReplicaRecapVisualPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHorizontalBox> ReplicaRecapVisualContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UVerticalBox> ContributionTableContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> RewardDetailPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RewardDetailsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RewardDetailsCloseButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ReturnToLobbyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MyFinalScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> EscapedBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultRow1Container;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultRow2Container;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultRow3Container;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultRow4Container;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultRow1TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultRow2TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultRow3TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultRow4TextBlock;

#pragma endregion
};
