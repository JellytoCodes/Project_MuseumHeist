#include "UI/Widgets/HeistSoundPingMarkerWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistTypes.h"

#pragma region Construction

UHeistSoundPingMarkerWidget::UHeistSoundPingMarkerWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Presentation

void UHeistSoundPingMarkerWidget::ShowSoundPingMarker(const FHeistSoundPingEvent& SoundPingEvent, const FVector2D& ScreenDirection, const FVector2D& ScreenEdgeTranslation)
{
	const FVector2D NormalizedDirection = ScreenDirection.GetSafeNormal();
	const float DirectionAngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(NormalizedDirection.Y, NormalizedDirection.X));

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderTranslation(ScreenEdgeTranslation);

	if (IsValid(SoundPingMarkerContainer))
	{
		SoundPingMarkerContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (IsValid(SoundPingDirectionArrow))
	{
		SoundPingDirectionArrow->SetRenderTransformAngle(DirectionAngleDegrees);
	}

	if (IsValid(SoundPingTypeText))
	{
		SoundPingTypeText->SetText(GetPingTypeText(SoundPingEvent));
	}

	if (IsValid(SoundPingDurationText))
	{
		FNumberFormattingOptions DurationFormatting;
		DurationFormatting.MinimumFractionalDigits = 1;
		DurationFormatting.MaximumFractionalDigits = 1;
		SoundPingDurationText->SetText(FText::Format(NSLOCTEXT("HeistSoundPing", "DurationFormat", "{0}s"), FText::AsNumber(FMath::Max(0.0f, SoundPingEvent.Duration), &DurationFormatting)));
	}
}

void UHeistSoundPingMarkerWidget::ReleaseSoundPingMarker()
{
	SetVisibility(ESlateVisibility::Collapsed);
	SetRenderTranslation(FVector2D::ZeroVector);

	if (IsValid(SoundPingMarkerContainer))
	{
		SoundPingMarkerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FText UHeistSoundPingMarkerWidget::GetPingTypeText(const FHeistSoundPingEvent& SoundPingEvent)
{
	switch (SoundPingEvent.PingType)
	{
	case EHeistSoundPingType::Footstep:
		return NSLOCTEXT("HeistSoundPing", "Footstep", "FOOTSTEP");
	case EHeistSoundPingType::GlassBreak:
		return NSLOCTEXT("HeistSoundPing", "GlassBreak", "GLASS BREAK");
	case EHeistSoundPingType::CoinImpact:
		return NSLOCTEXT("HeistSoundPing", "CoinImpact", "COIN IMPACT");
	case EHeistSoundPingType::StunHit:
		return NSLOCTEXT("HeistSoundPing", "StunHit", "STUN HIT");
	default:
		return NSLOCTEXT("HeistSoundPing", "Unknown", "SOUND");
	}
}

#pragma endregion
