#include "Debug/HeistDebugFunctionLibrary.h"

#include "Core/HeistLogChannels.h"
#include "Engine/Engine.h"

#pragma region Logging

void UHeistDebugFunctionLibrary::Message(
	const UObject* WorldContextObject,
	const FString& Message,
	EHeistDebugLevel Level,
	bool bPrintToScreen,
	float Duration)
{
	const FString ContextName = GetNameSafe(WorldContextObject);
	const FString FormattedMessage = FString::Printf(TEXT("[%s] %s"), *ContextName, *Message);

	switch (Level)
	{
	case EHeistDebugLevel::Warning:
		UE_LOG(LogHeist, Warning, TEXT("%s"), *FormattedMessage);
		break;
	case EHeistDebugLevel::Error:
		UE_LOG(LogHeist, Error, TEXT("%s"), *FormattedMessage);
		break;
	default:
		UE_LOG(LogHeist, Log, TEXT("%s"), *FormattedMessage);
		break;
	}

	if (!bPrintToScreen || GEngine == nullptr)
	{
		return;
	}

	FColor MessageColor = FColor::White;
	if (Level == EHeistDebugLevel::Warning)
	{
		MessageColor = FColor::Yellow;
	}
	else if (Level == EHeistDebugLevel::Error)
	{
		MessageColor = FColor::Red;
	}

	GEngine->AddOnScreenDebugMessage(
		INDEX_NONE,
		FMath::Max(0.0f, Duration),
		MessageColor,
		FormattedMessage);
}

#pragma endregion
