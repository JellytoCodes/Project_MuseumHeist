#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistLobbyMapCardWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FHeistLobbyMapCardSelected, FName);

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistLobbyMapCardWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

  public:
	void ConfigureMapCard(FName InMapId, const FText& InMapDisplayName);
	void SetMapThumbnail(UTexture2D* InMapThumbnail);
	void ApplySelectionState(bool bSelected, bool bCanSelect);
	FName GetMapId() const;
	FHeistLobbyMapCardSelected& GetMapSelectedDelegate();

  private:
	UFUNCTION()
	void HandleSelectMapClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SelectMapButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MapNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> SelectedCheckImage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> MapThumbnail;

	FHeistLobbyMapCardSelected MapSelectedDelegate;
	FName MapId = NAME_None;
};
