#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "HeistPlayerState.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AHeistPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeVerificationIdentity(int32 InHeistPlayerId, const FLinearColor& InPlayerColor);

	UPROPERTY(ReplicatedUsing = OnRep_HeistPlayerId, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Verification")
	int32 HeistPlayerId = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerColor, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Verification")
	FLinearColor PlayerColor = FLinearColor::White;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_HeistPlayerId();

	UFUNCTION()
	void OnRep_PlayerColor();
};
