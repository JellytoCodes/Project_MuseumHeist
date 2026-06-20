#include "Core/HeistPlayerController.h"

#include "Core/HeistLogChannels.h"

AHeistPlayerController::AHeistPlayerController()
{
}

void AHeistPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogHeist, Log, TEXT("HeistPlayerController BeginPlay"));
}
