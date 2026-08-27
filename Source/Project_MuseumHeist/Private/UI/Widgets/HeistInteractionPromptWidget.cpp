#include "UI/Widgets/HeistInteractionPromptWidget.h"

#include "Character/Components/HeistInteractionComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistDroppedOriginalActor.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

namespace
{
FText ResolveArtifactDisplayName(const FName ArtifactId)
{
	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* ArtifactDataTable = IsValid(BalanceData) ? BalanceData->ArtifactDataTable.LoadSynchronous() : nullptr;
	const FHeistArtifactDataRow* ArtifactDefinition = IsValid(ArtifactDataTable) && ArtifactDataTable->GetRowStruct() == FHeistArtifactDataRow::StaticStruct()
		? ArtifactDataTable->FindRow<FHeistArtifactDataRow>(ArtifactId, TEXT("ResolveInteractionArtifactDisplayName"), false)
		: nullptr;
	if (ArtifactDefinition != nullptr && ArtifactDefinition->ArtifactId == ArtifactId && !ArtifactDefinition->DisplayName.IsEmpty())
	{
		return ArtifactDefinition->DisplayName;
	}

	FString FallbackName = ArtifactId.ToString();
	FallbackName.ReplaceInline(TEXT("_"), TEXT(" "));
	return FText::FromString(FallbackName);
}

FText ResolveGradeText(const EHeistLootGrade ItemGrade)
{
	switch (ItemGrade)
	{
	case EHeistLootGrade::OneStar:
		return NSLOCTEXT("HeistInteraction", "OneStarGrade", "★");
	case EHeistLootGrade::TwoStar:
		return NSLOCTEXT("HeistInteraction", "TwoStarGrade", "★★");
	case EHeistLootGrade::ThreeStar:
		return NSLOCTEXT("HeistInteraction", "ThreeStarGrade", "★★★");
	case EHeistLootGrade::FourStar:
		return NSLOCTEXT("HeistInteraction", "FourStarGrade", "★★★★");
	default:
		return FText::GetEmpty();
	}
}
}

#pragma region Construction

UHeistInteractionPromptWidget::UHeistInteractionPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), InteractionKeyLabel(NSLOCTEXT("HeistInteraction", "DefaultInteractionKey", "E"))
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistInteractionPromptWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (IsValid(HUDViewModel) && (HUDViewModel->IsObservationCastActive() || HUDViewModel->IsEscapeCastActive()))
	{
		RefreshActionProgress();
	}
}

void UHeistInteractionPromptWidget::NativeDestruct()
{
	if (IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationRefreshTimerHandle);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region Presentation

void UHeistInteractionPromptWidget::SetupInteractionPresentation(UHeistInteractionComponent* InInteractionComponent, UHeistHUDViewModel* InHUDViewModel)
{
	if (HUDViewModel != InHUDViewModel && IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	InteractionComponent = InInteractionComponent;
	HUDViewModel = InHUDViewModel;
	if (IsValid(AvailabilityText))
	{
		AvailabilityText->SetText(NSLOCTEXT("HeistInteraction", "InteractionPrompt", "[E] 상호작용"));
	}

	UE_LOG(LogHeistUI, Verbose,
		   TEXT("[%s] Interaction presentation setup: InteractionComponent=%s HUDViewModel=%s PromptContainer=%s ActionProgressContainer=%s SelfPromptFallback=%s SelfActionFallback=%s"), *GetName(),
		   *GetNameSafe(InteractionComponent.Get()), *GetNameSafe(HUDViewModel.Get()), IsValid(InteractionPromptContainer) ? TEXT("true") : TEXT("false"),
		   IsValid(ActionProgressContainer) ? TEXT("true") : TEXT("false"),
		   (!IsValid(InteractionPromptContainer) && (IsValid(TargetText) || IsValid(KeyText) || IsValid(AvailabilityText)) &&
			!(IsValid(ActionTypeText) || IsValid(ActionProgressBar) || IsValid(ActionRemainingText) || IsValid(CancelHintText)))
			   ? TEXT("true")
			   : TEXT("false"),
		   (!IsValid(ActionProgressContainer) && (IsValid(ActionTypeText) || IsValid(ActionProgressBar) || IsValid(ActionRemainingText) || IsValid(CancelHintText)) &&
			!(IsValid(TargetText) || IsValid(KeyText) || IsValid(AvailabilityText)))
			   ? TEXT("true")
			   : TEXT("false"));

	if (IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
		HUDViewModel->GetPresentationChangedDelegate().AddUObject(this, &UHeistInteractionPromptWidget::RefreshPresentation);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationRefreshTimerHandle);
		World->GetTimerManager().SetTimer(PresentationRefreshTimerHandle, this, &UHeistInteractionPromptWidget::RefreshPresentation, 0.1f, true);
	}
	if (IsValid(InteractionComponent))
	{
		InteractionComponent->RefreshInteractionTarget();
	}

	RefreshPresentation();
}

void UHeistInteractionPromptWidget::RefreshPresentation()
{
	const bool bActionActive = IsValid(HUDViewModel) && (HUDViewModel->IsObservationCastActive() || HUDViewModel->IsEscapeCastActive());

	RefreshInteractionPrompt(bActionActive);
	RefreshActionProgress();
}

void UHeistInteractionPromptWidget::RefreshInteractionPrompt(const bool bActionActive)
{
	AActor* TargetActor = nullptr;
	bool bAvailable = false;
	if (IsValid(InteractionComponent))
	{
		TargetActor = InteractionComponent->GetCurrentInteractionTarget();
		bAvailable = IsValid(TargetActor) && InteractionComponent->HasValidInteractionTarget();
	}

	const bool bVisible = bAvailable && !bActionActive;
	if (IsValid(InteractionPromptContainer))
	{
		InteractionPromptContainer->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (!IsValid(InteractionPromptContainer) && (IsValid(TargetText) || IsValid(KeyText) || IsValid(AvailabilityText)) &&
			 !(IsValid(ActionTypeText) || IsValid(ActionProgressBar) || IsValid(ActionRemainingText) || IsValid(CancelHintText)))
	{
		const ESlateVisibility FallbackVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		SetVisibility(FallbackVisibility);
		if (UWidget* RootWidget = GetRootWidget())
		{
			RootWidget->SetVisibility(FallbackVisibility);
		}
	}
	if (IsValid(TargetText))
	{
		TargetText->SetText(ResolveTargetLabel(TargetActor));
	}
	if (IsValid(KeyText))
	{
		const AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(TargetActor);
		const AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(TargetActor);
		const bool bPaintingReviewReady = IsValid(PaintingCase) && PaintingCase->IsReplicaReviewReadyFor(GetOwningPlayerPawn());
		const bool bObjectReviewReady = IsValid(ObjectCase) && ObjectCase->IsReplicaReviewReadyFor(GetOwningPlayerPawn());
		KeyText->SetText(bPaintingReviewReady ? NSLOCTEXT("HeistInteraction", "PaintingReplicaReviewKeys", "E 교체·회수  |  R 다시 그리기")
										: bObjectReviewReady ? NSLOCTEXT("HeistInteraction", "ObjectReplicaReviewKeys", "E 교체·회수  |  R 다시 조립") : InteractionKeyLabel);
	}
}

void UHeistInteractionPromptWidget::RefreshActionProgress()
{
	const bool bObservationActive = IsValid(HUDViewModel) && HUDViewModel->IsObservationCastActive();
	const bool bEscapeActive = IsValid(HUDViewModel) && HUDViewModel->IsEscapeCastActive();
	const bool bVentSettlement = IsValid(HUDViewModel) && HUDViewModel->GetLocalLootScore() > 0;
	const bool bActionActive = bObservationActive || bEscapeActive;
	const FName ActionType = bObservationActive ? FName(TEXT("Observation")) : (bEscapeActive ? FName(TEXT("Escape")) : NAME_None);
	const float EndServerTime = bObservationActive ? HUDViewModel->GetObservationCastEndServerTime() : (bEscapeActive ? HUDViewModel->GetEscapeCastEndServerTime() : 0.0f);
	const float ServerTime = GetServerWorldTimeSeconds();

	if (bActionActive && (TrackedActionType != ActionType || !FMath::IsNearlyEqual(TrackedActionEndServerTime, EndServerTime)))
	{
		TrackedActionType = ActionType;
		TrackedActionEndServerTime = EndServerTime;
		TrackedActionDuration = FMath::Max(EndServerTime - ServerTime, KINDA_SMALL_NUMBER);
	}
	else if (!bActionActive)
	{
		TrackedActionType = NAME_None;
		TrackedActionEndServerTime = 0.0f;
		TrackedActionDuration = 0.0f;
	}

	const float RemainingSeconds = bActionActive ? FMath::Max(EndServerTime - ServerTime, 0.0f) : 0.0f;
	const float CompletionRatio = bActionActive && TrackedActionDuration > KINDA_SMALL_NUMBER ? 1.0f - FMath::Clamp(RemainingSeconds / TrackedActionDuration, 0.0f, 1.0f) : 0.0f;

	if (IsValid(ActionProgressContainer))
	{
		ActionProgressContainer->SetVisibility(bActionActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (!IsValid(ActionProgressContainer) && (IsValid(ActionTypeText) || IsValid(ActionProgressBar) || IsValid(ActionRemainingText) || IsValid(CancelHintText)) &&
			 !(IsValid(TargetText) || IsValid(KeyText) || IsValid(AvailabilityText)))
	{
		const ESlateVisibility FallbackVisibility = bActionActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		SetVisibility(FallbackVisibility);
		if (UWidget* RootWidget = GetRootWidget())
		{
			RootWidget->SetVisibility(FallbackVisibility);
		}
	}
	if (IsValid(ActionTypeText))
	{
		ActionTypeText->SetText(bObservationActive ? NSLOCTEXT("HeistInteraction", "ObservationAction", "관찰 중")
												  : (bEscapeActive && bVentSettlement ? NSLOCTEXT("HeistInteraction", "VentSettlementAction", "전리품 정산 중")
																				 : (bEscapeActive ? NSLOCTEXT("HeistInteraction", "VentEscapeAction", "최종 탈출 중") : FText::GetEmpty())));
	}
	if (IsValid(ActionProgressBar))
	{
		ActionProgressBar->SetPercent(CompletionRatio);
	}
	if (IsValid(ActionRemainingText))
	{
		ActionRemainingText->SetText(FText::Format(NSLOCTEXT("HeistInteraction", "RemainingFormat", "{0}초"),
												  FText::AsNumber(FMath::CeilToInt(RemainingSeconds))));
	}
	if (IsValid(CancelHintText))
	{
		CancelHintText->SetText(
			bObservationActive ? NSLOCTEXT("HeistInteraction", "ObservationCancelHint", "E를 놓거나 이동, 피해 또는 체포 상태가 되면 관찰이 취소됩니다.")
							   : (bEscapeActive ? NSLOCTEXT("HeistInteraction", "VentCancelHint", "이동하거나 피해를 받으면 벤트 사용이 취소됩니다.") : FText::GetEmpty()));
	}

	const bool bReferenceVisible = bObservationActive && HUDViewModel->IsObservationReferenceVisible();
	if (IsValid(ObservationReferenceContainer))
	{
		ObservationReferenceContainer->SetVisibility(bReferenceVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(ObservationReferenceText))
	{
		ObservationReferenceText->SetText(bReferenceVisible ? HUDViewModel->GetObservationReferenceText() : FText::GetEmpty());
		ObservationReferenceText->SetVisibility(bReferenceVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

FText UHeistInteractionPromptWidget::ResolveTargetLabel(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FText::GetEmpty();
	}

	if (const AHeistLootActor* LootActor = Cast<AHeistLootActor>(TargetActor))
	{
		const FName LootRowId = LootActor->GetLootRowId();
		if (LootRowId.IsNone())
		{
			return NSLOCTEXT("HeistInteraction", "LootTarget", "획득");
		}

		FString LootDisplayName = LootRowId.ToString();
		LootDisplayName.ReplaceInline(TEXT("_"), TEXT(" "));
		return FText::Format(NSLOCTEXT("HeistInteraction", "NamedLootTarget", "{0} 획득"), FText::FromString(LootDisplayName));
	}

	if (const AHeistDroppedOriginalActor* DroppedOriginal = Cast<AHeistDroppedOriginalActor>(TargetActor))
	{
		const FText TargetPrefix = DroppedOriginal->IsRequiredTarget() ? NSLOCTEXT("HeistInteraction", "RequiredTargetPrefix", "[핵심 목표] ") : FText::GetEmpty();
		return FText::Format(NSLOCTEXT("HeistInteraction", "DroppedOriginalTarget", "{0}[{1}] {2} 원본 회수"), TargetPrefix,
							 ResolveGradeText(DroppedOriginal->GetItemGrade()), DroppedOriginal->GetArtifactDisplayName());
	}

	if (const AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(TargetActor))
	{
		const FText ArtifactName = ResolveArtifactDisplayName(PaintingCase->GetTargetArtifactId());
		if (PaintingCase->IsReplicaReviewReadyFor(GetOwningPlayerPawn()))
		{
			return FText::Format(NSLOCTEXT("HeistInteraction", "PaintingReplicaReviewTarget", "{0} 교체 준비 완료"), ArtifactName);
		}
		return FText::Format(NSLOCTEXT("HeistInteraction", "PaintingTarget", "{0} 그림 관찰"), ArtifactName);
	}

	if (const AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(TargetActor))
	{
		const FText ArtifactName = ResolveArtifactDisplayName(ObjectCase->GetTargetArtifactId());
		if (ObjectCase->IsReplicaReviewReadyFor(GetOwningPlayerPawn()))
		{
			return FText::Format(NSLOCTEXT("HeistInteraction", "ObjectReplicaReviewTarget", "{0} 교체 준비 완료"), ArtifactName);
		}
		return FText::Format(NSLOCTEXT("HeistInteraction", "ObjectTarget", "{0} 작품 관찰"), ArtifactName);
	}

	if (Cast<AHeistVentActor>(TargetActor) != nullptr)
	{
		return IsValid(HUDViewModel) && HUDViewModel->GetLocalLootScore() > 0 ? NSLOCTEXT("HeistInteraction", "VentSettlementTarget", "전리품 정산")
																					 : NSLOCTEXT("HeistInteraction", "VentEscapeTarget", "최종 탈출");
	}

	if (const AHeistPlayerCharacter* TargetPlayerCharacter = Cast<AHeistPlayerCharacter>(TargetActor))
	{
		const AHeistPlayerState* TargetPlayerState = TargetPlayerCharacter->GetPlayerState<AHeistPlayerState>();
		return IsValid(TargetPlayerState) && TargetPlayerState->HeistPlayerId > 0
			? FText::Format(NSLOCTEXT("HeistInteraction", "RescueNamedPlayer", "플레이어 {0} 구조"), FText::AsNumber(TargetPlayerState->HeistPlayerId))
			: NSLOCTEXT("HeistInteraction", "RescuePlayer", "동료 구조");
	}

	FString TargetDisplayName = TargetActor->GetClass()->GetName();
	TargetDisplayName.RemoveFromStart(TEXT("BP_"));
	TargetDisplayName.RemoveFromEnd(TEXT("_C"));
	TargetDisplayName.ReplaceInline(TEXT("_"), TEXT(" "));
	return FText::Format(NSLOCTEXT("HeistInteraction", "GenericTarget", "{0} 상호작용"), FText::FromString(TargetDisplayName));
}

float UHeistInteractionPromptWidget::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}

#pragma endregion
