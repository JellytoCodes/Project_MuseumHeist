#include "UI/Widgets/HeistResultWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistPlayerController.h"
#include "Engine/Texture2D.h"
#include "UI/Result/Widgets/HeistResultPlayerRowWidget.h"
#include "UI/Result/Widgets/HeistResultReplicaCardWidget.h"
#include "UI/Result/Widgets/HeistResultRewardDetailWidget.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "View/MVVMView.h"

#pragma region Lifecycle

void UHeistResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(ReturnToLobbyButton))
	{
		ReturnToLobbyButton->OnClicked.RemoveAll(this);
		ReturnToLobbyButton->OnClicked.AddDynamic(this, &UHeistResultWidget::HandleReturnToLobbyClicked);
		const APlayerController* OwningPlayerController = GetOwningPlayer();
		const UHeistGameInstance* HeistGameInstance = IsValid(OwningPlayerController) ? Cast<UHeistGameInstance>(OwningPlayerController->GetGameInstance()) : nullptr;
		const bool bCanHostReturn = IsValid(OwningPlayerController) && OwningPlayerController->HasAuthority() && IsValid(HeistGameInstance) && HeistGameInstance->IsHostingOnlineSession();
		ReturnToLobbyButton->SetVisibility(bCanHostReturn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (IsValid(RewardDetailsButton))
	{
		RewardDetailsButton->OnClicked.RemoveAll(this);
		RewardDetailsButton->OnClicked.AddDynamic(this, &UHeistResultWidget::HandleRewardDetailsClicked);
	}
	if (IsValid(RewardDetailWidget))
	{
		RewardDetailWidget->HideDetail();
	}
}

void UHeistResultWidget::NativeDestruct()
{
	if (IsValid(ReturnToLobbyButton))
	{
		ReturnToLobbyButton->OnClicked.RemoveAll(this);
	}
	if (IsValid(RewardDetailsButton))
	{
		RewardDetailsButton->OnClicked.RemoveAll(this);
	}
	if (IsValid(ResultViewModel))
	{
		ResultViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	ReplicaRecapTextures.Reset();

	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModel

void UHeistResultWidget::SetupResultWidget(UHeistResultViewModel* InResultViewModel)
{
	checkf(IsValid(InResultViewModel), TEXT("HeistResultWidget requires a valid HeistResultViewModel"));

	if (IsValid(ResultViewModel))
	{
		ResultViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	ResultViewModel = InResultViewModel;
	ResultViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	ResultViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistResultWidget::RefreshResultPresentation);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(ResultViewModel);
	ViewModelInterface.SetInterface(ResultViewModel);

	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}

	RefreshResultPresentation();
}

UHeistResultViewModel* UHeistResultWidget::GetResultViewModel() const
{
	return ResultViewModel;
}

void UHeistResultWidget::ResetHiddenPresentationState()
{
	if (IsValid(RewardDetailWidget))
	{
		RewardDetailWidget->HideDetail();
	}
	if (IsValid(ReplicaRecapVisualContainer))
	{
		ReplicaRecapVisualContainer->ClearChildren();
	}
	if (IsValid(ContributionTableContainer))
	{
		ContributionTableContainer->ClearChildren();
	}
	ReplicaRecapTextures.Reset();
}

bool UHeistResultWidget::IsHiddenPresentationStateReset() const
{
	const bool bDetailsReset = !IsValid(RewardDetailWidget) || !RewardDetailWidget->IsDetailVisible();
	const bool bReplicaReset = !IsValid(ReplicaRecapVisualContainer) || ReplicaRecapVisualContainer->GetChildrenCount() == 0;
	const bool bContributionReset = !IsValid(ContributionTableContainer) || ContributionTableContainer->GetChildrenCount() == 0;
	return bDetailsReset && bReplicaReset && bContributionReset && ReplicaRecapTextures.IsEmpty();
}

bool UHeistResultWidget::IsRewardDetailVisible() const
{
	return IsValid(RewardDetailWidget) && RewardDetailWidget->IsDetailVisible();
}

UTexture2D* UHeistResultWidget::CreatePaintingRecapTexture(const FHeistReplicaRecapEntry& ReplicaRecap, const FLinearColor BackgroundColor)
{
	TArray64<uint8> TextureBytes;
	if (!DecodePaintingRecapPixels(ReplicaRecap, BackgroundColor.ToFColorSRGB(), TextureBytes))
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(ReplicaRecap.PaintingResolution, ReplicaRecap.PaintingResolution, PF_B8G8R8A8, NAME_None, TextureBytes);
	if (!IsValid(Texture))
	{
		return nullptr;
	}

	Texture->SRGB = true;
	Texture->Filter = TF_Nearest;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;
	Texture->UpdateResource();
	ReplicaRecapTextures.Add(Texture);
	return Texture;
}

FText UHeistResultWidget::BuildReplicaCardTitle(const FHeistReplicaRecapEntry& ReplicaRecap)
{
	const FText DisplayName = ReplicaRecap.ArtifactDisplayName.IsEmpty() ? FText::FromName(ReplicaRecap.ArtifactId) : ReplicaRecap.ArtifactDisplayName;
	return FText::Format(NSLOCTEXT("HeistResult", "ReplicaCardTitle", "{0}{1}  |  품질 {2}"),
						 ReplicaRecap.bRequiredTarget ? NSLOCTEXT("HeistResult", "RequiredReplicaPrefix", "[필수 목표] ") : FText::GetEmpty(), DisplayName,
						 FText::AsNumber(FMath::RoundToInt(ReplicaRecap.QualityScore)));
}

FVector2D UHeistResultWidget::ResolveAssemblyRecapSocketAnchor(const FName SocketId)
{
	const FString SocketName = SocketId.ToString();
	if (SocketName.Equals(TEXT("Crest"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.50, 0.10);
	}
	if (SocketName.Equals(TEXT("Head"), ESearchCase::IgnoreCase) || SocketName.Equals(TEXT("Lid"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.50, 0.23);
	}
	if (SocketName.Contains(TEXT("_L"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.27, 0.48);
	}
	if (SocketName.Equals(TEXT("Spout"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.78, 0.40);
	}
	if (SocketName.Contains(TEXT("_R"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.73, 0.48);
	}
	if (SocketName.Equals(TEXT("Foot"), ESearchCase::IgnoreCase) || SocketName.Equals(TEXT("Pedestal"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.50, 0.80);
	}

	static const FVector2D FallbackAnchors[] = {FVector2D(0.34, 0.32), FVector2D(0.66, 0.32), FVector2D(0.34, 0.66), FVector2D(0.66, 0.66)};
	return FallbackAnchors[GetTypeHash(SocketName) % UE_ARRAY_COUNT(FallbackAnchors)];
}

FVector2D UHeistResultWidget::ResolveAssemblyRecapPartSize(const FName PartId)
{
	const FString PartName = PartId.ToString();
	if (PartName.Contains(TEXT("Arm"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Handle"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Spout"), ESearchCase::IgnoreCase))
	{
		return FVector2D(118.0, 54.0);
	}
	if (PartName.Contains(TEXT("Pedestal"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Foot"), ESearchCase::IgnoreCase))
	{
		return FVector2D(140.0, 60.0);
	}
	if (PartName.Contains(TEXT("Head"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Lid"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Crest"), ESearchCase::IgnoreCase))
	{
		return FVector2D(78.0, 78.0);
	}
	return FVector2D(96.0, 70.0);
}

float UHeistResultWidget::ResolveAssemblyRecapPartAngle(const uint8 QuantizedOrientation)
{
	return static_cast<float>(QuantizedOrientation % 16) * 22.5f;
}

bool UHeistResultWidget::DecodePaintingRecapPixels(const FHeistReplicaRecapEntry& ReplicaRecap, const FColor& BackgroundColor, TArray64<uint8>& OutTextureBytes)
{
	OutTextureBytes.Reset();
	if (ReplicaRecap.ForgeryType != EHeistForgeryType::Drawing || ReplicaRecap.PaintingResolution != FHeistReplicaRecapEntry::PaintingThumbnailResolution ||
		!FMath::IsWithinInclusive(ReplicaRecap.PaintingPalette.Num(), 2, FHeistReplicaRecapEntry::MaximumPaintingPaletteColors))
	{
		return false;
	}

	const int32 PixelCount = ReplicaRecap.PaintingResolution * ReplicaRecap.PaintingResolution;
	if (ReplicaRecap.PaintingPackedPaletteIndices.Num() != FMath::DivideAndRoundUp(PixelCount, 2))
	{
		return false;
	}

	OutTextureBytes.SetNumUninitialized(static_cast<int64>(PixelCount) * 4);
	for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
	{
		const uint8 PackedByte = ReplicaRecap.PaintingPackedPaletteIndices[PixelIndex / 2];
		const uint8 PaletteValue = (PixelIndex & 1) == 0 ? PackedByte & 0x0f : PackedByte >> 4;
		if (PaletteValue > ReplicaRecap.PaintingPalette.Num())
		{
			OutTextureBytes.Reset();
			return false;
		}
		FColor PixelColor = PaletteValue == 0 ? BackgroundColor : ReplicaRecap.PaintingPalette[PaletteValue - 1];
		PixelColor.A = 255;
		const int64 ByteOffset = static_cast<int64>(PixelIndex) * 4;
		OutTextureBytes[ByteOffset] = PixelColor.B;
		OutTextureBytes[ByteOffset + 1] = PixelColor.G;
		OutTextureBytes[ByteOffset + 2] = PixelColor.R;
		OutTextureBytes[ByteOffset + 3] = PixelColor.A;
	}
	return true;
}

#pragma endregion

#pragma region Presentation

void UHeistResultWidget::RefreshResultPresentation()
{
	if (!IsValid(ResultViewModel))
	{
		return;
	}

	if (IsValid(OutcomeTextBlock))
	{
		OutcomeTextBlock->SetText(ResultViewModel->GetOutcomeText());
	}
	if (IsValid(OutcomeReasonTextBlock))
	{
		const FText& OutcomeReason = ResultViewModel->GetOutcomeReasonText();
		OutcomeReasonTextBlock->SetText(OutcomeReason);
		OutcomeReasonTextBlock->SetVisibility(OutcomeReason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (IsValid(TeamRewardTextBlock))
	{
		TeamRewardTextBlock->SetText(ResultViewModel->GetTeamRewardText());
	}
	RefreshRewardDetailPresentation(ResultViewModel->GetTeamResult());
	ReplicaRecapTextures.Reset();
	RefreshReplicaRecapPresentation(ResultViewModel->GetReplicaRecap());
	RefreshContributionTablePresentation(ResultViewModel->GetPlayerResults());
	BP_RefreshReplicaRecap(ResultViewModel->GetReplicaRecap());
}

void UHeistResultWidget::RefreshRewardDetailPresentation(const FHeistTeamResult& TeamResult)
{
	if (IsValid(RewardDetailWidget))
	{
		RewardDetailWidget->ApplyTeamResult(TeamResult);
	}
}

void UHeistResultWidget::RefreshReplicaRecapPresentation(const TArray<FHeistReplicaRecapEntry>& ReplicaRecap)
{
	if (!IsValid(ReplicaRecapVisualContainer))
	{
		return;
	}

	ReplicaRecapVisualContainer->ClearChildren();
	if (IsValid(ReplicaRecapVisualPanel))
	{
		ReplicaRecapVisualPanel->SetVisibility(ESlateVisibility::Visible);
	}

	int32 AddedReplicaCount = 0;
	for (const FHeistReplicaRecapEntry& ReplicaEntry : ReplicaRecap)
	{
		if (ReplicaEntry.ForgeryType != EHeistForgeryType::Drawing || !ReplicaEntry.HasPaintingVisualPayload() || !ReplicaCardWidgetClass)
		{
			continue;
		}
		UTexture2D* ReplicaTexture = CreatePaintingRecapTexture(ReplicaEntry, FLinearColor(0.94f, 0.92f, 0.86f));
		UHeistResultReplicaCardWidget* ReplicaCard = CreateWidget<UHeistResultReplicaCardWidget>(GetWorld(), ReplicaCardWidgetClass);
		if (!IsValid(ReplicaTexture) || !IsValid(ReplicaCard))
		{
			continue;
		}
		ReplicaCard->ApplyReplicaEntry(ReplicaEntry, ReplicaTexture);
		UHorizontalBoxSlot* CardSlot = ReplicaRecapVisualContainer->AddChildToHorizontalBox(ReplicaCard);
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		CardSlot->SetVerticalAlignment(VAlign_Center);
		++AddedReplicaCount;
	}

	if (IsValid(ReplicaRecapEmptyTextBlock))
	{
		ReplicaRecapEmptyTextBlock->SetVisibility(AddedReplicaCount == 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHeistResultWidget::RefreshContributionTablePresentation(const TArray<FHeistPlayerResult>& PlayerResults)
{
	if (!IsValid(ContributionTableContainer))
	{
		return;
	}

	ContributionTableContainer->ClearChildren();
	const int32 VisiblePlayerCount = FMath::Min(PlayerResults.Num(), 4);
	for (int32 PlayerIndex = 0; PlayerIndex < VisiblePlayerCount; ++PlayerIndex)
	{
		if (!PlayerRowWidgetClass)
		{
			break;
		}
		UHeistResultPlayerRowWidget* PlayerRow = CreateWidget<UHeistResultPlayerRowWidget>(GetWorld(), PlayerRowWidgetClass);
		if (IsValid(PlayerRow))
		{
			PlayerRow->ApplyPlayerResult(PlayerResults[PlayerIndex]);
			ContributionTableContainer->AddChildToVerticalBox(PlayerRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}
	}

	if (IsValid(ContributionEmptyTextBlock))
	{
		ContributionEmptyTextBlock->SetVisibility(ContributionTableContainer->GetChildrenCount() == 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHeistResultWidget::HandleReturnToLobbyClicked()
{
	if (AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayer()))
	{
		HeistPlayerController->RequestReturnToLobby();
	}
}

void UHeistResultWidget::HandleRewardDetailsClicked()
{
	if (IsValid(RewardDetailWidget))
	{
		RewardDetailWidget->ShowDetail();
	}
}

#pragma endregion
