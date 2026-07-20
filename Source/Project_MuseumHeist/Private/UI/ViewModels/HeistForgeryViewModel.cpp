#include "UI/ViewModels/HeistForgeryViewModel.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistTypes.h"
#include "Engine/Texture2D.h"
#include "World/Actors/Loot/HeistDisplayCaseActor.h"

namespace
{
	const FName PreviewObservation(TEXT("Observation"));
	const FName PreviewDrawing(TEXT("Drawing"));
	const FName PreviewValidation(TEXT("Validation"));
	const FName PreviewResult(TEXT("Result"));
}

UHeistForgeryViewModel::UHeistForgeryViewModel(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHeistForgeryViewModel::BeginDestroy()
{
	if (IsValid(GameState))
	{
		GameState->GetObjectiveStateChangedDelegate().RemoveAll(this);
	}
	if (IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
	}
	if (IsValid(ForgeryComponent))
	{
		ForgeryComponent->GetSessionStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

void UHeistForgeryViewModel::SetupViewModel(
	AHeistGameState* InGameState,
	UHeistActionComponent* InActionComponent,
	UHeistForgeryComponent* InForgeryComponent)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetObjectiveStateChangedDelegate().RemoveAll(this);
	}
	if (ActionComponent != InActionComponent && IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
	}
	if (ForgeryComponent != InForgeryComponent && IsValid(ForgeryComponent))
	{
		ForgeryComponent->GetSessionStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	ActionComponent = InActionComponent;
	ForgeryComponent = InForgeryComponent;

	if (IsValid(GameState))
	{
		GameState->GetObjectiveStateChangedDelegate().RemoveAll(this);
		GameState->GetObjectiveStateChangedDelegate().AddUObject(
			this,
			&UHeistForgeryViewModel::HandleObjectiveStateChanged);
	}
	if (IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
		ActionComponent->GetActionStateChangedDelegate().AddUObject(
			this,
			&UHeistForgeryViewModel::HandleActionStateChanged);
	}
	if (IsValid(ForgeryComponent))
	{
		ForgeryComponent->GetSessionStateChangedDelegate().RemoveAll(this);
		ForgeryComponent->GetSessionStateChangedDelegate().AddUObject(
			this,
			&UHeistForgeryViewModel::HandleForgerySessionStateChanged);
	}

	RefreshPresentationState();
}

void UHeistForgeryViewModel::RefreshPresentationState()
{
	// Runtime observation uses the main HUD cast progress only. The full-screen
	// forgery presentation must not cover the world until observation succeeds
	// and the owner enters the drawing session.
	bool bShowObservation = false;
	bool bShowDrawing = IsValid(ForgeryComponent)
		&& ForgeryComponent->IsSessionActive()
		&& !ForgeryComponent->IsSubmitPending();
	bool bShowValidation = IsValid(ForgeryComponent)
		&& ForgeryComponent->IsSessionActive()
		&& ForgeryComponent->IsSubmitPending();
	const bool bHasAuthoritativeResult = IsValid(ForgeryComponent)
		&& ForgeryComponent->IsSessionActive()
		&& ForgeryComponent->HasAuthoritativeForgeryResult();
	bool bShowResult = bResultPresentationActive || bHasAuthoritativeResult;
	float NewResultScore = bHasAuthoritativeResult
		? ForgeryComponent->GetAuthoritativeForgeryResult().SimilarityScore
		: bShowResult
			? PendingResultScore
			: 0.0f;
	FText NewResultText = bHasAuthoritativeResult
		? NSLOCTEXT(
			"HeistForgery",
			"AuthoritativeResult",
			"FORGERY SCORE CONFIRMED")
		: bShowResult
			? PendingResultText
			: FText::GetEmpty();

	if (!DebugPreviewState.IsNone())
	{
		bShowObservation = DebugPreviewState == PreviewObservation;
		bShowDrawing = DebugPreviewState == PreviewDrawing;
		bShowValidation = DebugPreviewState == PreviewValidation;
		bShowResult = DebugPreviewState == PreviewResult;
		NewResultScore = bShowResult ? 87.0f : 0.0f;
		NewResultText = bShowResult
			? NSLOCTEXT("HeistForgery", "DebugResult", "FORGERY ACCEPTED")
			: FText::GetEmpty();
	}

	if (bShowResult)
	{
		bShowObservation = false;
		bShowDrawing = false;
		bShowValidation = false;
	}
	else if (bShowValidation)
	{
		bShowObservation = false;
		bShowDrawing = false;
	}
	else if (bShowDrawing)
	{
		bShowObservation = false;
	}

	const bool bShowAnyState = bShowObservation
		|| bShowDrawing
		|| bShowValidation
		|| bShowResult;
	const float NewStateEndServerTime = bShowObservation && IsValid(ActionComponent)
		? ActionComponent->GetObservationCastEndServerTime()
		: (bShowDrawing || bShowValidation) && IsValid(ForgeryComponent)
			? ForgeryComponent->GetSessionEndServerTime()
			: 0.0f;
	const bool bTemplatePrepared = bShowAnyState
		&& IsValid(ForgeryComponent)
		&& ForgeryComponent->HasPreparedForgeryTemplate();
	const FName NewReferenceArtifactId = bTemplatePrepared
		? ForgeryComponent->GetActiveArtifactId()
		: bShowAnyState && IsValid(GameState)
			? GameState->GetActiveTargetArtifactId()
			: NAME_None;
	const FName NewReferenceTemplateId = bTemplatePrepared
		? ForgeryComponent->GetActiveTemplateId()
		: NAME_None;
	UTexture2D* NewReferenceImage = bTemplatePrepared
		? ForgeryComponent->LoadReferenceImage()
		: nullptr;
	UTexture2D* NewReferenceMask = bTemplatePrepared
		? ForgeryComponent->LoadReferenceMask()
		: nullptr;
	const TArray<FLinearColor> NewAllowedPalette = bTemplatePrepared
		? ForgeryComponent->GetTemplateAllowedPalette()
		: TArray<FLinearColor>();
	const float NewObservationDuration = bTemplatePrepared
		? ForgeryComponent->GetTemplateObservationDuration()
		: 0.0f;
	const float NewForgeryDuration = bTemplatePrepared
		? ForgeryComponent->GetTemplateForgeryDuration()
		: 0.0f;
	const int32 NewStrokeLimit = bTemplatePrepared
		? ForgeryComponent->GetTemplateStrokeLimit()
		: 0;
	const float NewBrushSize = bTemplatePrepared
		? ForgeryComponent->GetTemplateBrushSize()
		: 0.0f;
	const AHeistDisplayCaseActor* ActiveDisplayCase =
		IsValid(ForgeryComponent)
			? ForgeryComponent->GetActiveDisplayCase()
			: nullptr;
	if (!IsValid(ActiveDisplayCase) && IsValid(ActionComponent))
	{
		ActiveDisplayCase =
			ActionComponent->GetPendingObservationDisplayCase();
	}
	const FName NewActiveDisplayCaseName = IsValid(ActiveDisplayCase)
		? ActiveDisplayCase->GetFName()
		: NAME_None;

	FText NewStateText;
	if (bShowResult)
	{
		NewStateText = NSLOCTEXT("HeistForgery", "ResultState", "RESULT");
	}
	else if (bShowValidation)
	{
		NewStateText = NSLOCTEXT("HeistForgery", "ValidationState", "VALIDATING");
	}
	else if (bShowDrawing)
	{
		NewStateText = NSLOCTEXT("HeistForgery", "DrawingState", "DRAWING");
	}
	else if (bShowObservation)
	{
		NewStateText = NSLOCTEXT("HeistForgery", "ObservationState", "OBSERVATION");
	}

	const FText NewReferenceText = NewReferenceArtifactId.IsNone()
		? FText::GetEmpty()
		: FText::Format(
			NSLOCTEXT("HeistForgery", "ReferenceFormat", "REFERENCE  {0}"),
			FText::FromName(NewReferenceArtifactId));

	UE_MVVM_SET_PROPERTY_VALUE(bPresentationVisible, bShowAnyState);
	UE_MVVM_SET_PROPERTY_VALUE(bObservationVisible, bShowObservation);
	UE_MVVM_SET_PROPERTY_VALUE(bDrawingVisible, bShowDrawing);
	UE_MVVM_SET_PROPERTY_VALUE(bValidationVisible, bShowValidation);
	UE_MVVM_SET_PROPERTY_VALUE(bResultVisible, bShowResult);
	UE_MVVM_SET_PROPERTY_VALUE(StateEndServerTime, NewStateEndServerTime);
	UE_MVVM_SET_PROPERTY_VALUE(ResultScore, NewResultScore);
	UE_MVVM_SET_PROPERTY_VALUE(ReferenceArtifactId, NewReferenceArtifactId);
	UE_MVVM_SET_PROPERTY_VALUE(ReferenceTemplateId, NewReferenceTemplateId);
	UE_MVVM_SET_PROPERTY_VALUE(ReferenceImage, NewReferenceImage);
	UE_MVVM_SET_PROPERTY_VALUE(ReferenceMask, NewReferenceMask);
	UE_MVVM_SET_PROPERTY_VALUE(AllowedPalette, NewAllowedPalette);
	UE_MVVM_SET_PROPERTY_VALUE(ObservationDuration, NewObservationDuration);
	UE_MVVM_SET_PROPERTY_VALUE(ForgeryDuration, NewForgeryDuration);
	UE_MVVM_SET_PROPERTY_VALUE(StrokeLimit, NewStrokeLimit);
	UE_MVVM_SET_PROPERTY_VALUE(BrushSize, NewBrushSize);
	UE_MVVM_SET_PROPERTY_VALUE(
		ActiveDisplayCaseName,
		NewActiveDisplayCaseName);
	UE_MVVM_SET_PROPERTY_VALUE(StateText, NewStateText);
	UE_MVVM_SET_PROPERTY_VALUE(ReferenceText, NewReferenceText);
	UE_MVVM_SET_PROPERTY_VALUE(ResultText, NewResultText);

	PresentationChangedDelegate.Broadcast();
	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("Forgery presentation refreshed: Visible=%s Observation=%s Drawing=%s Validation=%s Result=%s Artifact=%s Template=%s ReferenceImage=%s ReferenceMask=%s ObservationDuration=%.2f ForgeryDuration=%.2f StrokeLimit=%d Brush=%.4f Case=%s EndServerTime=%.2f Preview=%s OwnerOnly=true"),
		bPresentationVisible ? TEXT("true") : TEXT("false"),
		bObservationVisible ? TEXT("true") : TEXT("false"),
		bDrawingVisible ? TEXT("true") : TEXT("false"),
		bValidationVisible ? TEXT("true") : TEXT("false"),
		bResultVisible ? TEXT("true") : TEXT("false"),
		*ReferenceArtifactId.ToString(),
		*ReferenceTemplateId.ToString(),
		*GetNameSafe(ReferenceImage),
		*GetNameSafe(ReferenceMask),
		ObservationDuration,
		ForgeryDuration,
		StrokeLimit,
		BrushSize,
		*ActiveDisplayCaseName.ToString(),
		StateEndServerTime,
		DebugPreviewState.IsNone()
			? TEXT("None")
			: *DebugPreviewState.ToString());
}

FHeistForgeryPresentationChanged&
UHeistForgeryViewModel::GetPresentationChangedDelegate()
{
	return PresentationChangedDelegate;
}

void UHeistForgeryViewModel::ShowResultPresentation(
	const float Score,
	const FText& InResultText)
{
	bResultPresentationActive = true;
	PendingResultScore = FMath::Clamp(Score, 0.0f, 100.0f);
	PendingResultText = InResultText;
	DebugPreviewState = NAME_None;
	RefreshPresentationState();
}

void UHeistForgeryViewModel::ClearResultPresentation()
{
	bResultPresentationActive = false;
	PendingResultScore = 0.0f;
	PendingResultText = FText::GetEmpty();
	RefreshPresentationState();
}

bool UHeistForgeryViewModel::SetDebugPreviewState(const FName StateName)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const bool bSupported = StateName.IsNone()
		|| StateName == PreviewObservation
		|| StateName == PreviewDrawing
		|| StateName == PreviewValidation
		|| StateName == PreviewResult;
	if (!bSupported)
	{
		return false;
	}

	DebugPreviewState = StateName;
	RefreshPresentationState();
	return true;
#endif
}

FName UHeistForgeryViewModel::GetDebugPreviewState() const
{
	return DebugPreviewState;
}

void UHeistForgeryViewModel::HandleActionStateChanged()
{
	if (IsValid(ActionComponent) && ActionComponent->IsObservationCastActive())
	{
		bResultPresentationActive = false;
	}
	RefreshPresentationState();
}

void UHeistForgeryViewModel::HandleForgerySessionStateChanged()
{
	if (IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive())
	{
		bResultPresentationActive = false;
	}
	RefreshPresentationState();
}

void UHeistForgeryViewModel::HandleObjectiveStateChanged(
	const FName,
	const FName,
	const EHeistObjectiveState,
	AHeistPlayerState*)
{
	RefreshPresentationState();
}

bool UHeistForgeryViewModel::IsPresentationVisible() const
{
	return bPresentationVisible;
}

bool UHeistForgeryViewModel::IsObservationVisible() const
{
	return bObservationVisible;
}

bool UHeistForgeryViewModel::IsDrawingVisible() const
{
	return bDrawingVisible;
}

bool UHeistForgeryViewModel::IsValidationVisible() const
{
	return bValidationVisible;
}

bool UHeistForgeryViewModel::IsResultVisible() const
{
	return bResultVisible;
}

float UHeistForgeryViewModel::GetStateEndServerTime() const
{
	return StateEndServerTime;
}

float UHeistForgeryViewModel::GetResultScore() const
{
	return ResultScore;
}

FName UHeistForgeryViewModel::GetReferenceArtifactId() const
{
	return ReferenceArtifactId;
}

FName UHeistForgeryViewModel::GetReferenceTemplateId() const
{
	return ReferenceTemplateId;
}

UTexture2D* UHeistForgeryViewModel::GetReferenceImage() const
{
	return ReferenceImage.Get();
}

UTexture2D* UHeistForgeryViewModel::GetReferenceMask() const
{
	return ReferenceMask.Get();
}

const TArray<FLinearColor>& UHeistForgeryViewModel::GetAllowedPalette() const
{
	return AllowedPalette;
}

float UHeistForgeryViewModel::GetObservationDuration() const
{
	return ObservationDuration;
}

float UHeistForgeryViewModel::GetForgeryDuration() const
{
	return ForgeryDuration;
}

int32 UHeistForgeryViewModel::GetStrokeLimit() const
{
	return StrokeLimit;
}

float UHeistForgeryViewModel::GetBrushSize() const
{
	return BrushSize;
}

bool UHeistForgeryViewModel::CalculatePreviewScore(
	const TArray<FVector2D>& NormalizedPoints,
	const TArray<int32>& StrokePointCounts,
	const TArray<uint8>& StrokePaletteIndices,
	const float InBrushSize,
	FHeistForgeryResult& OutResult,
	int32& OutReferenceMaskPixels,
	int32& OutSubmittedMaskPixels) const
{
	return IsValid(ForgeryComponent)
		&& ForgeryComponent->CalculateLocalForgeryPreview(
			NormalizedPoints,
			StrokePointCounts,
			StrokePaletteIndices,
			InBrushSize,
			OutResult,
			OutReferenceMaskPixels,
			OutSubmittedMaskPixels);
}

FName UHeistForgeryViewModel::GetActiveDisplayCaseName() const
{
	return ActiveDisplayCaseName;
}

const FText& UHeistForgeryViewModel::GetStateText() const
{
	return StateText;
}

const FText& UHeistForgeryViewModel::GetReferenceText() const
{
	return ReferenceText;
}

const FText& UHeistForgeryViewModel::GetResultText() const
{
	return ResultText;
}

int32 UHeistForgeryViewModel::GetVisibleStateCount() const
{
	return static_cast<int32>(bObservationVisible)
		+ static_cast<int32>(bDrawingVisible)
		+ static_cast<int32>(bValidationVisible)
		+ static_cast<int32>(bResultVisible);
}
