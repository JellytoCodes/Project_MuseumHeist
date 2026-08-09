#include "UI/Widgets/HeistResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistPlayerController.h"
#include "Engine/Texture2D.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "View/MVVMView.h"

namespace
{
constexpr float ReplicaCardWidth = 320.0f;
constexpr float ReplicaCardHeight = 235.0f;
const FVector2D ReplicaVisualSize(290.0f, 180.0f);
constexpr float ContributionIdentityColumnWidth = 128.0f;
constexpr float ContributionEscapeStateColumnWidth = 96.0f;
constexpr float ContributionMetricColumnWidth = 119.0f;

FLinearColor ResolveReplicaPartColor(const FName PartId)
{
	static const FLinearColor Colors[] = {
		FLinearColor(0.21f, 0.50f, 0.78f), FLinearColor(0.72f, 0.38f, 0.18f), FLinearColor(0.34f, 0.64f, 0.42f),
		FLinearColor(0.62f, 0.35f, 0.70f), FLinearColor(0.74f, 0.62f, 0.20f), FLinearColor(0.28f, 0.62f, 0.66f)};
	return Colors[GetTypeHash(PartId) % UE_ARRAY_COUNT(Colors)];
}

void ApplyRecapTextStyle(UTextBlock* TextBlock, const FSlateFontInfo& BaseFont, const int32 Size, const FLinearColor& Color)
{
	if (!IsValid(TextBlock))
	{
		return;
	}

	FSlateFontInfo Font = BaseFont;
	Font.Size = Size;
	TextBlock->SetFont(Font);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
}
}

#pragma region Construction

UHeistResultWidget::UHeistResultWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

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
	if (IsValid(RewardDetailsCloseButton))
	{
		RewardDetailsCloseButton->OnClicked.RemoveAll(this);
		RewardDetailsCloseButton->OnClicked.AddDynamic(this, &UHeistResultWidget::HandleRewardDetailsCloseClicked);
	}
	if (IsValid(RewardDetailPanel))
	{
		RewardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
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
	if (IsValid(RewardDetailsCloseButton))
	{
		RewardDetailsCloseButton->OnClicked.RemoveAll(this);
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
	if (IsValid(RewardDetailPanel))
	{
		RewardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
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
	const bool bDetailsReset = !IsValid(RewardDetailPanel) || RewardDetailPanel->GetVisibility() == ESlateVisibility::Collapsed;
	const bool bReplicaReset = !IsValid(ReplicaRecapVisualContainer) || ReplicaRecapVisualContainer->GetChildrenCount() == 0;
	const bool bContributionReset = !IsValid(ContributionTableContainer) || ContributionTableContainer->GetChildrenCount() == 0;
	return bDetailsReset && bReplicaReset && bContributionReset && ReplicaRecapTextures.IsEmpty();
}

bool UHeistResultWidget::IsRewardDetailVisible() const
{
	return IsValid(RewardDetailPanel) && RewardDetailPanel->GetVisibility() == ESlateVisibility::Visible;
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
	const FText TypeText = ReplicaRecap.ForgeryType == EHeistForgeryType::Assembly ? NSLOCTEXT("HeistResult", "ReplicaAssembly", "조립")
		: NSLOCTEXT("HeistResult", "ReplicaPainting", "그림");
	return FText::Format(NSLOCTEXT("HeistResult", "ReplicaCardTitle", "{0}{1}  |  {2}  |  품질 {3}"),
		ReplicaRecap.bRequiredTarget ? NSLOCTEXT("HeistResult", "RequiredReplicaPrefix", "[필수 목표] ") : FText::GetEmpty(), DisplayName, TypeText,
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
	if (PartName.Contains(TEXT("Arm"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Handle"), ESearchCase::IgnoreCase) ||
		PartName.Contains(TEXT("Spout"), ESearchCase::IgnoreCase))
	{
		return FVector2D(118.0, 54.0);
	}
	if (PartName.Contains(TEXT("Pedestal"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Foot"), ESearchCase::IgnoreCase))
	{
		return FVector2D(140.0, 60.0);
	}
	if (PartName.Contains(TEXT("Head"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Lid"), ESearchCase::IgnoreCase) ||
		PartName.Contains(TEXT("Crest"), ESearchCase::IgnoreCase))
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
	if (ReplicaRecap.ForgeryType != EHeistForgeryType::Drawing ||
		ReplicaRecap.PaintingResolution != FHeistReplicaRecapEntry::PaintingThumbnailResolution ||
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

	if (IsValid(MyFinalScoreText))
	{
		MyFinalScoreText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(EscapedBadge))
	{
		EscapedBadge->SetVisibility(ESlateVisibility::Collapsed);
	}

	const bool bUsesContributionTable = IsValid(ContributionTableContainer);
	if (IsValid(ResultRow1Container))
	{
		ResultRow1Container->SetVisibility(bUsesContributionTable ? ESlateVisibility::Collapsed : ResultViewModel->GetResultRow1Visibility());
	}
	if (IsValid(ResultRow2Container))
	{
		ResultRow2Container->SetVisibility(bUsesContributionTable ? ESlateVisibility::Collapsed : ResultViewModel->GetResultRow2Visibility());
	}
	if (IsValid(ResultRow3Container))
	{
		ResultRow3Container->SetVisibility(bUsesContributionTable ? ESlateVisibility::Collapsed : ResultViewModel->GetResultRow3Visibility());
	}
	if (IsValid(ResultRow4Container))
	{
		ResultRow4Container->SetVisibility(bUsesContributionTable ? ESlateVisibility::Collapsed : ResultViewModel->GetResultRow4Visibility());
	}

	if (!bUsesContributionTable && IsValid(ResultRow1TextBlock))
	{
		ResultRow1TextBlock->SetText(ResultViewModel->GetResultRow1Text());
	}
	if (!bUsesContributionTable && IsValid(ResultRow2TextBlock))
	{
		ResultRow2TextBlock->SetText(ResultViewModel->GetResultRow2Text());
	}
	if (!bUsesContributionTable && IsValid(ResultRow3TextBlock))
	{
		ResultRow3TextBlock->SetText(ResultViewModel->GetResultRow3Text());
	}
	if (!bUsesContributionTable && IsValid(ResultRow4TextBlock))
	{
		ResultRow4TextBlock->SetText(ResultViewModel->GetResultRow4Text());
	}
}

void UHeistResultWidget::RefreshRewardDetailPresentation(const FHeistTeamResult& TeamResult)
{
	const FText RequiredTargetName = TeamResult.RequiredTargetDisplayName.IsEmpty()
		? FText::FromName(TeamResult.RequiredTargetArtifactId)
		: TeamResult.RequiredTargetDisplayName;

	if (IsValid(DetailRequiredTargetValueTextBlock))
	{
		DetailRequiredTargetValueTextBlock->SetText(RequiredTargetName);
	}
	if (IsValid(DetailTargetStatusValueTextBlock))
	{
		DetailTargetStatusValueTextBlock->SetText(TeamResult.bRequiredTargetSecured
			? NSLOCTEXT("HeistResult", "DetailTargetSecured", "확보 완료")
			: NSLOCTEXT("HeistResult", "DetailTargetMissing", "미확보"));
	}
	if (IsValid(DetailQuotaValueTextBlock))
	{
		DetailQuotaValueTextBlock->SetText(FText::AsNumber(TeamResult.LootValueQuota));
	}
	if (IsValid(DetailSecuredValueTextBlock))
	{
		DetailSecuredValueTextBlock->SetText(FText::AsNumber(TeamResult.SecuredValue));
	}
	if (IsValid(DetailExtraValueTextBlock))
	{
		DetailExtraValueTextBlock->SetText(FText::AsNumber(TeamResult.ExtraValue));
	}
	if (IsValid(DetailRequiredTargetRewardValueTextBlock))
	{
		DetailRequiredTargetRewardValueTextBlock->SetText(FText::AsNumber(TeamResult.RequiredTargetValue));
	}
	if (IsValid(DetailLooseLootValueTextBlock))
	{
		DetailLooseLootValueTextBlock->SetText(FText::AsNumber(TeamResult.SecuredLooseLootValue));
	}

	FNumberFormattingOptions PercentFormatting;
	PercentFormatting.MinimumFractionalDigits = 0;
	PercentFormatting.MaximumFractionalDigits = 1;
	if (IsValid(DetailForgeryMultiplierValueTextBlock))
	{
		DetailForgeryMultiplierValueTextBlock->SetText(FText::AsPercent(TeamResult.ForgeryRewardMultiplier, &PercentFormatting));
	}
	if (IsValid(DetailStealthMultiplierValueTextBlock))
	{
		DetailStealthMultiplierValueTextBlock->SetText(FText::AsPercent(TeamResult.StealthRewardMultiplier, &PercentFormatting));
	}
	if (IsValid(DetailArrestPenaltyValueTextBlock))
	{
		const FText ArrestPenaltyText = TeamResult.ArrestPenalty > 0
			? FText::Format(NSLOCTEXT("HeistResult", "DetailArrestPenaltyFormat", "-{0}"), FText::AsNumber(TeamResult.ArrestPenalty))
			: FText::AsNumber(0);
		DetailArrestPenaltyValueTextBlock->SetText(ArrestPenaltyText);
	}
}

void UHeistResultWidget::RefreshReplicaRecapPresentation(const TArray<FHeistReplicaRecapEntry>& ReplicaRecap)
{
	if (!IsValid(ReplicaRecapVisualContainer) || !IsValid(WidgetTree))
	{
		return;
	}

	ReplicaRecapVisualContainer->ClearChildren();
	if (IsValid(ReplicaRecapVisualPanel))
	{
		ReplicaRecapVisualPanel->SetVisibility(ESlateVisibility::Visible);
	}

	if (ReplicaRecap.IsEmpty())
	{
		UTextBlock* EmptyStateText = WidgetTree->ConstructWidget<UTextBlock>();
		if (IsValid(EmptyStateText))
		{
			EmptyStateText->SetText(NSLOCTEXT("HeistResult", "NoReplicaVisualRecap", "남기고 온 위조품이 없습니다."));
			EmptyStateText->SetJustification(ETextJustify::Center);
			const FSlateFontInfo EmptyStateFont = IsValid(ReplicaRecapTextBlock) ? ReplicaRecapTextBlock->GetFont() : EmptyStateText->GetFont();
			ApplyRecapTextStyle(EmptyStateText, EmptyStateFont, 18, FLinearColor(0.58f, 0.63f, 0.70f));
			UHorizontalBoxSlot* EmptyStateSlot = ReplicaRecapVisualContainer->AddChildToHorizontalBox(EmptyStateText);
			EmptyStateSlot->SetPadding(FMargin(24.0f));
			EmptyStateSlot->SetHorizontalAlignment(HAlign_Center);
			EmptyStateSlot->SetVerticalAlignment(VAlign_Center);
		}
		return;
	}

	for (const FHeistReplicaRecapEntry& ReplicaEntry : ReplicaRecap)
	{
		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>();
		UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>();
		UVerticalBox* CardLayout = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* CardTitle = WidgetTree->ConstructWidget<UTextBlock>();
		USizeBox* VisualSizeBox = WidgetTree->ConstructWidget<USizeBox>();
		UWidget* VisualWidget = CreateReplicaVisualWidget(ReplicaEntry);
		if (!IsValid(CardSizeBox) || !IsValid(CardBorder) || !IsValid(CardLayout) || !IsValid(CardTitle) || !IsValid(VisualSizeBox) || !IsValid(VisualWidget))
		{
			continue;
		}

		CardSizeBox->SetWidthOverride(ReplicaCardWidth);
		CardSizeBox->SetHeightOverride(ReplicaCardHeight);
		CardBorder->SetBrushColor(ReplicaEntry.bRequiredTarget ? FLinearColor(0.16f, 0.24f, 0.12f, 1.0f) : FLinearColor(0.055f, 0.075f, 0.11f, 1.0f));
		CardBorder->SetPadding(FMargin(8.0f, 6.0f));
		CardTitle->SetText(BuildReplicaCardTitle(ReplicaEntry));
		CardTitle->SetAutoWrapText(true);
		CardTitle->SetJustification(ETextJustify::Center);
		const FSlateFontInfo CardFont = IsValid(ReplicaRecapTextBlock) ? ReplicaRecapTextBlock->GetFont() : CardTitle->GetFont();
		ApplyRecapTextStyle(CardTitle, CardFont, 14, ReplicaEntry.bRequiredTarget ? FLinearColor(0.78f, 0.95f, 0.55f) : FLinearColor(0.88f, 0.91f, 0.96f));
		VisualSizeBox->SetWidthOverride(ReplicaVisualSize.X);
		VisualSizeBox->SetHeightOverride(ReplicaVisualSize.Y);
		VisualSizeBox->AddChild(VisualWidget);

		CardSizeBox->AddChild(CardBorder);
		CardBorder->AddChild(CardLayout);
		UVerticalBoxSlot* TitleSlot = CardLayout->AddChildToVerticalBox(CardTitle);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		UVerticalBoxSlot* VisualSlot = CardLayout->AddChildToVerticalBox(VisualSizeBox);
		VisualSlot->SetHorizontalAlignment(HAlign_Center);
		VisualSlot->SetVerticalAlignment(VAlign_Center);

		UHorizontalBoxSlot* CardSlot = ReplicaRecapVisualContainer->AddChildToHorizontalBox(CardSizeBox);
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		CardSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UHeistResultWidget::RefreshContributionTablePresentation(const TArray<FHeistPlayerResult>& PlayerResults)
{
	if (!IsValid(ContributionTableContainer) || !IsValid(WidgetTree))
	{
		return;
	}

	ContributionTableContainer->ClearChildren();
	if (UWidget* HeaderRow = CreateContributionTableRow(nullptr, true, INDEX_NONE))
	{
		ContributionTableContainer->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	const int32 VisiblePlayerCount = FMath::Min(PlayerResults.Num(), 4);
	for (int32 PlayerIndex = 0; PlayerIndex < VisiblePlayerCount; ++PlayerIndex)
	{
		if (UWidget* PlayerRow = CreateContributionTableRow(&PlayerResults[PlayerIndex], false, PlayerIndex))
		{
			ContributionTableContainer->AddChildToVerticalBox(PlayerRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}
	}
}

UWidget* UHeistResultWidget::CreateContributionTableRow(const FHeistPlayerResult* PlayerResult, const bool bHeaderRow, const int32 RowIndex)
{
	UHorizontalBox* RowContainer = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (!IsValid(RowContainer))
	{
		return nullptr;
	}

	TArray<FText> Cells;
	if (bHeaderRow)
	{
		Cells = {
			NSLOCTEXT("HeistResult", "ContributionPlayer", "플레이어"),
			NSLOCTEXT("HeistResult", "ContributionEscapeState", "탈출 여부"),
			NSLOCTEXT("HeistResult", "ContributionSurfaceCount", "그림"),
			NSLOCTEXT("HeistResult", "ContributionSurfaceBest", "그림 최고"),
			NSLOCTEXT("HeistResult", "ContributionAssemblyCount", "조립"),
			NSLOCTEXT("HeistResult", "ContributionAssemblyBest", "조립 최고"),
			NSLOCTEXT("HeistResult", "ContributionRecovered", "원본 회수"),
			NSLOCTEXT("HeistResult", "ContributionCarryTime", "운반 시간"),
			NSLOCTEXT("HeistResult", "ContributionSecuredValue", "확보 가치"),
			NSLOCTEXT("HeistResult", "ContributionDistraction", "경비 유인"),
			NSLOCTEXT("HeistResult", "ContributionRescue", "팀원 구조"),
			NSLOCTEXT("HeistResult", "ContributionAlarms", "경보 유발")};
	}
	else if (PlayerResult)
	{
		const FText PlayerStateText = PlayerResult->bEscaped ? NSLOCTEXT("HeistResult", "ContributionEscaped", "탈출")
			: NSLOCTEXT("HeistResult", "ContributionUnresolved", "미탈출");
		const FHeistPlayerContribution& Contribution = PlayerResult->Contribution;
		Cells = {
			FText::Format(NSLOCTEXT("HeistResult", "ContributionPlayerIdentity", "PLAYER {0}"), FText::AsNumber(PlayerResult->PlayerId)), PlayerStateText,
			FText::AsNumber(Contribution.SurfaceForgeries), FText::AsNumber(FMath::RoundToInt(Contribution.BestSurfaceQuality)),
			FText::AsNumber(Contribution.Assemblies), FText::AsNumber(FMath::RoundToInt(Contribution.BestAssemblyQuality)),
			FText::AsNumber(Contribution.ArtifactsRecovered),
			FText::Format(NSLOCTEXT("HeistResult", "ContributionCarrySeconds", "{0}초"), FText::AsNumber(FMath::RoundToInt(Contribution.CarryTimeSeconds))),
			FText::AsNumber(Contribution.SecuredLootValue), FText::AsNumber(Contribution.GuardsDistracted),
			FText::AsNumber(Contribution.TeammatesRescued), FText::AsNumber(Contribution.AlarmsTriggered)};
	}

	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		const float ColumnWidth = CellIndex == 0 ? ContributionIdentityColumnWidth : CellIndex == 1 ? ContributionEscapeStateColumnWidth : ContributionMetricColumnWidth;
		AddContributionTableCell(RowContainer, Cells[CellIndex], ColumnWidth, bHeaderRow, RowIndex);
	}
	return RowContainer;
}

void UHeistResultWidget::AddContributionTableCell(UHorizontalBox* RowContainer, const FText& CellText, const float Width, const bool bHeaderCell, const int32 RowIndex)
{
	if (!IsValid(RowContainer))
	{
		return;
	}

	USizeBox* CellSizeBox = WidgetTree->ConstructWidget<USizeBox>();
	UBorder* CellBorder = WidgetTree->ConstructWidget<UBorder>();
	UTextBlock* CellTextBlock = WidgetTree->ConstructWidget<UTextBlock>();
	if (!IsValid(CellSizeBox) || !IsValid(CellBorder) || !IsValid(CellTextBlock))
	{
		return;
	}

	CellSizeBox->SetWidthOverride(Width);
	CellSizeBox->SetHeightOverride(bHeaderCell ? 34.0f : 42.0f);
	CellBorder->SetPadding(FMargin(5.0f, 3.0f));
	CellBorder->SetBrushColor(bHeaderCell ? FLinearColor(0.06f, 0.16f, 0.24f, 0.98f)
		: (RowIndex % 2 == 0 ? FLinearColor(0.035f, 0.075f, 0.115f, 0.96f) : FLinearColor(0.025f, 0.055f, 0.09f, 0.96f)));
	CellTextBlock->SetText(CellText);
	CellTextBlock->SetJustification(ETextJustify::Center);
	CellTextBlock->SetAutoWrapText(false);
	const FSlateFontInfo TableFont = IsValid(ReplicaRecapTextBlock) ? ReplicaRecapTextBlock->GetFont() : CellTextBlock->GetFont();
	ApplyRecapTextStyle(CellTextBlock, TableFont, bHeaderCell ? 11 : 13, bHeaderCell ? FLinearColor(0.64f, 0.82f, 0.92f) : FLinearColor(0.90f, 0.93f, 0.96f));

	CellSizeBox->AddChild(CellBorder);
	if (UBorderSlot* TextSlot = Cast<UBorderSlot>(CellBorder->AddChild(CellTextBlock)))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Fill);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
	UHorizontalBoxSlot* CellSlot = RowContainer->AddChildToHorizontalBox(CellSizeBox);
	CellSlot->SetPadding(FMargin(0.0f, 0.0f, 2.0f, 0.0f));
	CellSlot->SetVerticalAlignment(VAlign_Center);
}

UWidget* UHeistResultWidget::CreateReplicaVisualWidget(const FHeistReplicaRecapEntry& ReplicaRecap)
{
	UBorder* VisualBackdrop = WidgetTree->ConstructWidget<UBorder>();
	if (!IsValid(VisualBackdrop))
	{
		return nullptr;
	}
	VisualBackdrop->SetBrushColor(FLinearColor(0.012f, 0.018f, 0.030f, 1.0f));
	VisualBackdrop->SetPadding(FMargin(4.0f));

	if (ReplicaRecap.HasPaintingVisualPayload())
	{
		if (UTexture2D* PaintingTexture = CreatePaintingRecapTexture(ReplicaRecap, FLinearColor(0.94f, 0.92f, 0.86f)))
		{
			UImage* PaintingImage = WidgetTree->ConstructWidget<UImage>();
			PaintingImage->SetBrushFromTexture(PaintingTexture, true);
			VisualBackdrop->AddChild(PaintingImage);
			return VisualBackdrop;
		}
	}
	else if (ReplicaRecap.HasAssemblyVisualPayload())
	{
		UCanvasPanel* AssemblyCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		if (IsValid(AssemblyCanvas))
		{
			VisualBackdrop->AddChild(AssemblyCanvas);
			for (const FHeistObjectAssemblyEntry& AssemblyEntry : ReplicaRecap.AssemblyEntries)
			{
				UBorder* PartBorder = WidgetTree->ConstructWidget<UBorder>();
				if (!IsValid(PartBorder))
				{
					continue;
				}

				PartBorder->SetBrushColor(ResolveReplicaPartColor(AssemblyEntry.PartId));

				const FVector2D PartSize = ResolveAssemblyRecapPartSize(AssemblyEntry.PartId) * 0.55f;
				const FVector2D PartAnchor = ResolveAssemblyRecapSocketAnchor(AssemblyEntry.SocketId) * ReplicaVisualSize;
				UCanvasPanelSlot* PartSlot = AssemblyCanvas->AddChildToCanvas(PartBorder);
				PartSlot->SetAutoSize(false);
				PartSlot->SetSize(PartSize);
				PartSlot->SetPosition(PartAnchor - PartSize * 0.5f);

				FWidgetTransform PartTransform;
				PartTransform.Angle = ResolveAssemblyRecapPartAngle(AssemblyEntry.QuantizedOrientation);
				PartBorder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				PartBorder->SetRenderTransform(PartTransform);
			}
			return VisualBackdrop;
		}
	}

	UTextBlock* MissingDataText = WidgetTree->ConstructWidget<UTextBlock>();
	if (!IsValid(MissingDataText))
	{
		return VisualBackdrop;
	}
	MissingDataText->SetText(NSLOCTEXT("HeistResult", "ReplicaVisualUnavailable", "시각 데이터 없음"));
	MissingDataText->SetJustification(ETextJustify::Center);
	const FSlateFontInfo FallbackFont = IsValid(ReplicaRecapTextBlock) ? ReplicaRecapTextBlock->GetFont() : MissingDataText->GetFont();
	ApplyRecapTextStyle(MissingDataText, FallbackFont, 12, FLinearColor(0.65f, 0.68f, 0.73f));
	VisualBackdrop->AddChild(MissingDataText);
	return VisualBackdrop;
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
	if (IsValid(RewardDetailPanel))
	{
		RewardDetailPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UHeistResultWidget::HandleRewardDetailsCloseClicked()
{
	if (IsValid(RewardDetailPanel))
	{
		RewardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

#pragma endregion
