#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "HeistPlayerCharacter.generated.h"

class UHeistActionComponent;
class UHeistCustomizationComponent;
class UHeistInteractionComponent;
class UHeistInventoryComponent;
class UHeistNoiseEmitterComponent;
class UHeistStatusComponent;
class UHeistTagComponent;
class UHeistVisionComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHeistPlayerCharacter();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistTagComponent> TagComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistStatusComponent> StatusComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistActionComponent> ActionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistVisionComponent> VisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistCustomizationComponent> CustomizationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistNoiseEmitterComponent> NoiseEmitterComponent;
};
