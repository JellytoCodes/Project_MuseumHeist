#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "HeistDebugFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class EHeistDebugLevel : uint8
{
	Info,
	Warning,
	Error
};

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistDebugFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#pragma region Logging

public:
	UFUNCTION(
		BlueprintCallable,
		Category = "Heist|Debug",
		meta = (
			DevelopmentOnly,
			WorldContext = "WorldContextObject",
			AdvancedDisplay = "bPrintToScreen,Duration"))
	static void Message(
		const UObject* WorldContextObject,
		const FString& Message,
		EHeistDebugLevel Level = EHeistDebugLevel::Info,
		bool bPrintToScreen = false,
		float Duration = 3.0f);

#pragma endregion
};
