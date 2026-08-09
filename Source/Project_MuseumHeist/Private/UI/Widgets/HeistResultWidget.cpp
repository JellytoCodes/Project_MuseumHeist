#include "UI/Widgets/HeistResultWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistPlayerController.h"
#include "Engine/Texture2D.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "View/MVVMView.h"

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
}

void UHeistResultWidget::NativeDestruct()
{
	if (IsValid(ReturnToLobbyButton))
	{
		ReturnToLobbyButton->OnClicked.RemoveAll(this);
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

	if (IsValid(MyFinalScoreText))
	{
		MyFinalScoreText->SetText(ResultViewModel->GetMyFinalScoreText());
	}
	if (IsValid(OutcomeTextBlock))
	{
		OutcomeTextBlock->SetText(ResultViewModel->GetOutcomeText());
	}
	if (IsValid(OutcomeReasonTextBlock))
	{
		OutcomeReasonTextBlock->SetText(ResultViewModel->GetOutcomeReasonText());
	}
	if (IsValid(ContractProgressTextBlock))
	{
		ContractProgressTextBlock->SetText(ResultViewModel->GetContractProgressText());
	}
	if (IsValid(TeamRewardTextBlock))
	{
		TeamRewardTextBlock->SetText(ResultViewModel->GetTeamRewardText());
	}
	if (IsValid(RewardBreakdownTextBlock))
	{
		RewardBreakdownTextBlock->SetText(ResultViewModel->GetRewardBreakdownText());
	}
	if (IsValid(ReplicaRecapTextBlock))
	{
		ReplicaRecapTextBlock->SetText(ResultViewModel->GetReplicaRecapText());
	}
	ReplicaRecapTextures.Reset();
	BP_RefreshReplicaRecap(ResultViewModel->GetReplicaRecap());

	if (IsValid(EscapedBadge))
	{
		EscapedBadge->SetVisibility(ResultViewModel->GetEscapedVisibility());
	}

	if (IsValid(ResultRow1Container))
	{
		ResultRow1Container->SetVisibility(ResultViewModel->GetResultRow1Visibility());
	}
	if (IsValid(ResultRow2Container))
	{
		ResultRow2Container->SetVisibility(ResultViewModel->GetResultRow2Visibility());
	}
	if (IsValid(ResultRow3Container))
	{
		ResultRow3Container->SetVisibility(ResultViewModel->GetResultRow3Visibility());
	}
	if (IsValid(ResultRow4Container))
	{
		ResultRow4Container->SetVisibility(ResultViewModel->GetResultRow4Visibility());
	}

	if (IsValid(ResultRow1TextBlock))
	{
		ResultRow1TextBlock->SetText(ResultViewModel->GetResultRow1Text());
	}
	if (IsValid(ResultRow2TextBlock))
	{
		ResultRow2TextBlock->SetText(ResultViewModel->GetResultRow2Text());
	}
	if (IsValid(ResultRow3TextBlock))
	{
		ResultRow3TextBlock->SetText(ResultViewModel->GetResultRow3Text());
	}
	if (IsValid(ResultRow4TextBlock))
	{
		ResultRow4TextBlock->SetText(ResultViewModel->GetResultRow4Text());
	}
}

void UHeistResultWidget::HandleReturnToLobbyClicked()
{
	if (AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayer()))
	{
		HeistPlayerController->RequestReturnToLobby();
	}
}

#pragma endregion
