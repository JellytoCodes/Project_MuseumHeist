#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "HeistHUD.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistHUD : public AHUD
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistHUD();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region ResultPresentation

public:
	bool ShowResultScreen();
	void HideResultScreen();
	class UHeistResultViewModel* GetResultViewModel() const;

private:
	void InitializeResultPresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistResultWidget> ResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHeistResultViewModel> ResultViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistResultWidget> ResultWidget;

#pragma endregion
};
