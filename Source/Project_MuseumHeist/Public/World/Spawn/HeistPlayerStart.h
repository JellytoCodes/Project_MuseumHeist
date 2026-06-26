#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"

#include "HeistPlayerStart.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	AHeistPlayerStart(const FObjectInitializer& ObjectInitializer);
};
