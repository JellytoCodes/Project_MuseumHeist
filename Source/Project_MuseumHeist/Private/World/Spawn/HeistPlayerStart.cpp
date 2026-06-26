#include "World/Spawn/HeistPlayerStart.h"

AHeistPlayerStart::AHeistPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}
