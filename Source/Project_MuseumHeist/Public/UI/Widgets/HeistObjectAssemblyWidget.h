#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistObjectAssemblyWidget.generated.h"

class AActor;
class AHeistPlayerController;
class UButton;
class UHeistObjectAssemblyViewModel;
class UStaticMeshComponent;
class UTextBlock;
class UViewport;

/**
 * Owner-only Object Assembly screen.
 *
 * The UViewport scene and every preview component are local presentation
 * objects. Only the compact entry payload owned by the ViewModel is submitted.
 */
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

#pragma endregion

#pragma region Setup

  public:
	void SetupObjectAssemblyWidget(UHeistObjectAssemblyViewModel* InObjectAssemblyViewModel, AHeistPlayerController* InPlayerController);
	bool IsOwnerOnlyContractSatisfied() const;
	bool IsWidgetPresentationVisible() const;
	bool IsPreviewReady() const;
	int32 GetPreviewComponentCount() const;
	int32 GetUnresolvedPreviewSocketCount() const;

  private:
	void BindButtons();
	void RefreshObjectAssemblyPresentation();
	void RefreshCountdownPresentation();
	void RefreshLocalPreview();
	void DestroyLocalPreview();
	FTransform ResolveFallbackPartTransform(FName SocketId, int32 PlacementIndex, uint8 QuantizedOrientation) const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistObjectAssemblyViewModel> ObjectAssemblyViewModel;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PreviewCoreComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PreviewPartComponents;

	int32 DisplayedPreviewRevision = INDEX_NONE;
	int32 UnresolvedPreviewSocketCount = 0;
	int32 LastDisplayedAssemblyTimeSeconds = INDEX_NONE;
	int32 LastDisplayedLockdownSeconds = INDEX_NONE;

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Object Assembly", meta = (DisplayName = "Object Assembly Sources Ready"))
	void BP_OnObjectAssemblySourcesReady();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Object Assembly", meta = (DisplayName = "Refresh Object Assembly Presentation"))
	void BP_RefreshObjectAssemblyPresentation(bool bVisible, bool bDataReady, int32 PlacedPartCount, int32 RequiredPartCount);

#pragma endregion

#pragma region Input

  private:
	UFUNCTION()
	void HandlePreviousPartClicked();

	UFUNCTION()
	void HandleNextPartClicked();

	UFUNCTION()
	void HandlePreviousSocketClicked();

	UFUNCTION()
	void HandleNextSocketClicked();

	UFUNCTION()
	void HandleRotateLeftClicked();

	UFUNCTION()
	void HandleRotateRightClicked();

	UFUNCTION()
	void HandlePlacePartClicked();

	UFUNCTION()
	void HandleRemovePartClicked();

	UFUNCTION()
	void HandleResetAssemblyClicked();

	UFUNCTION()
	void HandleSubmitClicked();

	UFUNCTION()
	void HandleCancelClicked();

#pragma endregion

#pragma region Presentation

  private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UViewport> AssemblyViewport;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TemplateNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SelectedPartText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SelectedSocketText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SelectedOrientationText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlacementProgressText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AssemblyStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AssemblyTimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AssemblyAlertWarningText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AssemblyLockdownCountdownText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PreviousPartButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> NextPartButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PreviousSocketButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> NextSocketButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RotateLeftButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RotateRightButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PlacePartButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RemovePartButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ResetAssemblyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SubmitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CancelButton;

#pragma endregion
};
