#include "Character/HeistPlayerCharacter.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistCustomizationComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/Components/HeistTagComponent.h"
#include "Character/Components/HeistVisionComponent.h"

AHeistPlayerCharacter::AHeistPlayerCharacter()
{
	TagComponent = CreateDefaultSubobject<UHeistTagComponent>(TEXT("TagComponent"));
	StatusComponent = CreateDefaultSubobject<UHeistStatusComponent>(TEXT("StatusComponent"));
	InventoryComponent = CreateDefaultSubobject<UHeistInventoryComponent>(TEXT("InventoryComponent"));
	InteractionComponent = CreateDefaultSubobject<UHeistInteractionComponent>(TEXT("InteractionComponent"));
	ActionComponent = CreateDefaultSubobject<UHeistActionComponent>(TEXT("ActionComponent"));
	VisionComponent = CreateDefaultSubobject<UHeistVisionComponent>(TEXT("VisionComponent"));
	CustomizationComponent = CreateDefaultSubobject<UHeistCustomizationComponent>(TEXT("CustomizationComponent"));
	NoiseEmitterComponent = CreateDefaultSubobject<UHeistNoiseEmitterComponent>(TEXT("NoiseEmitterComponent"));
}
