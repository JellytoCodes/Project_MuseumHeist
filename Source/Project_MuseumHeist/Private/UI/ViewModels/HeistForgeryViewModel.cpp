#include "UI/ViewModels/HeistForgeryViewModel.h"

#include "Character/Components/HeistForgeryComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Engine/Texture2D.h"

UHeistForgeryViewModel::UHeistForgeryViewModel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UHeistForgeryViewModel::BeginDestroy()
{
	if (IsValid(GameState))
	{
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
	}
	if (IsValid(ForgeryComponent))
	{
		ForgeryComponent->GetSessionStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

void UHeistForgeryViewModel::SetupViewModel(AHeistGameState* InGameState, UHeistForgeryComponent* InForgeryComponent)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
	}
	if (ForgeryComponent != InForgeryComponent && IsValid(ForgeryComponent))
	{
		ForgeryComponent->GetSessionStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	ForgeryComponent = InForgeryComponent;

	if (IsValid(GameState))
	{
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
		GameState->GetAlertStateChangedDelegate().AddUObject(this, &UHeistForgeryViewModel::HandleAlertStateChanged);
	}
	if (IsValid(ForgeryComponent))
	{
		ForgeryComponent->GetSessionStateChangedDelegate().RemoveAll(this);
		ForgeryComponent->GetSessionStateChangedDelegate().AddUObject(this, &UHeistForgeryViewModel::HandleForgerySessionStateChanged);
	}

	RefreshPresentationState();
}

void UHeistForgeryViewModel::RefreshPresentationState()
{
	// Observation stays in the world HUD. WBP_HeistForgery has one job:
	// present the active drawing session. A rejected score keeps the same local
	// canvas open; a successful submit closes when the server ends the session.
	const bool bShowDrawing = IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive();
	const bool bTemplatePrepared = bShowDrawing && ForgeryComponent->HasPreparedForgeryTemplate();
	const float NewStateEndServerTime = bShowDrawing ? ForgeryComponent->GetSessionEndServerTime() : 0.0f;
	UTexture2D* NewReferenceImage = bTemplatePrepared ? ForgeryComponent->LoadReferenceImage() : nullptr;
	const TArray<FLinearColor> NewAllowedPalette = bTemplatePrepared ? ForgeryComponent->GetTemplateAllowedPalette() : TArray<FLinearColor>();
	const int32 NewStrokeLimit = bTemplatePrepared ? ForgeryComponent->GetTemplateStrokeLimit() : 0;
	const float NewBrushSize = bTemplatePrepared ? ForgeryComponent->GetTemplateBrushSize() : 0.0f;

	const EHeistAlertLevel NewAlertLevel = IsValid(GameState) ? GameState->GetAlertLevel() : EHeistAlertLevel::Quiet;
	const bool bShowDangerWarning = false;
	const FText NewDangerWarningText = FText::GetEmpty();
	FLinearColor NewDangerWarningColor = FLinearColor::White;

	switch (NewAlertLevel)
	{
	case EHeistAlertLevel::Suspicious:
		NewDangerWarningColor = FLinearColor(1.0f, 0.68f, 0.12f);
		break;
	case EHeistAlertLevel::Searching:
		NewDangerWarningColor = FLinearColor(1.0f, 0.30f, 0.05f);
		break;
	case EHeistAlertLevel::Alarmed:
		NewDangerWarningColor = FLinearColor(1.0f, 0.04f, 0.02f);
		break;
	case EHeistAlertLevel::Lockdown:
		NewDangerWarningColor = FLinearColor(0.72f, 0.0f, 0.0f);
		break;
	case EHeistAlertLevel::Quiet:
	default:
		break;
	}

	UE_MVVM_SET_PROPERTY_VALUE(bPresentationVisible, bShowDrawing);
	UE_MVVM_SET_PROPERTY_VALUE(bDrawingVisible, bShowDrawing);
	UE_MVVM_SET_PROPERTY_VALUE(StateEndServerTime, NewStateEndServerTime);
	UE_MVVM_SET_PROPERTY_VALUE(ReferenceImage, NewReferenceImage);
	UE_MVVM_SET_PROPERTY_VALUE(AllowedPalette, NewAllowedPalette);
	UE_MVVM_SET_PROPERTY_VALUE(StrokeLimit, NewStrokeLimit);
	UE_MVVM_SET_PROPERTY_VALUE(BrushSize, NewBrushSize);

	UE_MVVM_SET_PROPERTY_VALUE(AlertLevel, NewAlertLevel);
	UE_MVVM_SET_PROPERTY_VALUE(bDangerWarningVisible, bShowDangerWarning);
	UE_MVVM_SET_PROPERTY_VALUE(DangerWarningText, NewDangerWarningText);
	UE_MVVM_SET_PROPERTY_VALUE(DangerWarningColor, NewDangerWarningColor);
	UE_MVVM_SET_PROPERTY_VALUE(bLockdownCountdownVisible, false);
	UE_MVVM_SET_PROPERTY_VALUE(LockdownCountdownEndServerTime, 0.0f);

	PresentationChangedDelegate.Broadcast();
	UE_LOG(LogHeistUI, Verbose, TEXT("Forgery presentation refreshed: Visible=%s Drawing=%s ReferenceImage=%s PaletteColors=%d StrokeLimit=%d Brush=%.4f EndServerTime=%.2f OwnerOnly=true"),
		bPresentationVisible ? TEXT("true") : TEXT("false"), bDrawingVisible ? TEXT("true") : TEXT("false"), *GetNameSafe(ReferenceImage), AllowedPalette.Num(), StrokeLimit, BrushSize,
		StateEndServerTime);
}

FHeistForgeryPresentationChanged& UHeistForgeryViewModel::GetPresentationChangedDelegate()
{
	return PresentationChangedDelegate;
}

void UHeistForgeryViewModel::HandleForgerySessionStateChanged()
{
	RefreshPresentationState();
}

void UHeistForgeryViewModel::HandleAlertStateChanged(const EHeistAlertLevel, const EHeistAlertLevel, const int32, const FName)
{
	RefreshPresentationState();
}

bool UHeistForgeryViewModel::IsPresentationVisible() const
{
	return bPresentationVisible;
}

bool UHeistForgeryViewModel::IsDrawingVisible() const
{
	return bDrawingVisible;
}

float UHeistForgeryViewModel::GetStateEndServerTime() const
{
	return StateEndServerTime;
}

UTexture2D* UHeistForgeryViewModel::GetReferenceImage() const
{
	return ReferenceImage.Get();
}

const TArray<FLinearColor>& UHeistForgeryViewModel::GetAllowedPalette() const
{
	return AllowedPalette;
}

int32 UHeistForgeryViewModel::GetStrokeLimit() const
{
	return StrokeLimit;
}

float UHeistForgeryViewModel::GetBrushSize() const
{
	return BrushSize;
}

float UHeistForgeryViewModel::GetBrushSizeForPreset(const int32 BrushPresetIndex) const
{
	return IsValid(ForgeryComponent) && FMath::IsWithinInclusive(BrushPresetIndex, 0, 2)
			   ? ForgeryComponent->ResolveBrushSizeForPreset(static_cast<uint8>(BrushPresetIndex))
			   : 0.0f;
}

bool UHeistForgeryViewModel::IsSubmitPending() const
{
	return IsValid(ForgeryComponent) && ForgeryComponent->IsSubmitPending();
}

FName UHeistForgeryViewModel::GetLastSubmissionRejectReason() const
{
	return IsValid(ForgeryComponent) && !ForgeryComponent->WasLastStrokeValidationAccepted() ? ForgeryComponent->GetLastStrokeValidationReason() : NAME_None;
}

int32 UHeistForgeryViewModel::GetStrokeValidationRevision() const
{
	return IsValid(ForgeryComponent) ? ForgeryComponent->GetStrokeValidationRevision() : INDEX_NONE;
}

float UHeistForgeryViewModel::GetMinimumAcceptedQualityScore() const
{
	return HeistReplicaAcceptance::MinimumQualityScore;
}

int32 UHeistForgeryViewModel::GetScoreRasterResolution() const
{
	return IsValid(ForgeryComponent) ? ForgeryComponent->GetForgeryScoreResolution() : 0;
}

bool UHeistForgeryViewModel::CalculatePreviewScore(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices,
	const TArray<uint8>& StrokeBrushPresetIndices, FHeistForgeryResult& OutResult, int32& OutReferenceMaskPixels, int32& OutSubmittedMaskPixels) const
{
	return IsValid(ForgeryComponent) &&
		ForgeryComponent->CalculateLocalForgeryPreview(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, StrokeBrushPresetIndices, OutResult, OutReferenceMaskPixels,
			OutSubmittedMaskPixels);
}

EHeistAlertLevel UHeistForgeryViewModel::GetAlertLevel() const
{
	return AlertLevel;
}

bool UHeistForgeryViewModel::IsDangerWarningVisible() const
{
	return bDangerWarningVisible;
}

const FText& UHeistForgeryViewModel::GetDangerWarningText() const
{
	return DangerWarningText;
}

FLinearColor UHeistForgeryViewModel::GetDangerWarningColor() const
{
	return DangerWarningColor;
}

bool UHeistForgeryViewModel::IsLockdownCountdownVisible() const
{
	return bLockdownCountdownVisible;
}

float UHeistForgeryViewModel::GetLockdownCountdownEndServerTime() const
{
	return LockdownCountdownEndServerTime;
}
