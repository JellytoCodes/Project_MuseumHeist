#include "UI/Widgets/HeistObjectAssemblyWidget.h"

#include "Components/Button.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/Viewport.h"
#include "Components/Widget.h"
#include "Core/HeistPlayerController.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "UI/ViewModels/HeistObjectAssemblyViewModel.h"

namespace
{
void ApplyText(UTextBlock* TextBlock, const FText& Text)
{
	if (IsValid(TextBlock))
	{
		TextBlock->SetText(Text);
	}
}

void ApplyVisibility(UWidget* Widget, const bool bVisible)
{
	if (IsValid(Widget))
	{
		Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
}

UHeistObjectAssemblyWidget::UHeistObjectAssemblyWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UHeistObjectAssemblyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtons();
	RefreshObjectAssemblyPresentation();
}

void UHeistObjectAssemblyWidget::NativeDestruct()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}
	DestroyLocalPreview();
	Super::NativeDestruct();
}

void UHeistObjectAssemblyWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshCountdownPresentation();
}

void UHeistObjectAssemblyWidget::SetupObjectAssemblyWidget(UHeistObjectAssemblyViewModel* InObjectAssemblyViewModel, AHeistPlayerController* InPlayerController)
{
	if (ObjectAssemblyViewModel != InObjectAssemblyViewModel && IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	ObjectAssemblyViewModel = InObjectAssemblyViewModel;
	PlayerController = InPlayerController;
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->GetPresentationChangedDelegate().RemoveAll(this);
		ObjectAssemblyViewModel->GetPresentationChangedDelegate().AddUObject(this, &UHeistObjectAssemblyWidget::RefreshObjectAssemblyPresentation);
	}

	BP_OnObjectAssemblySourcesReady();
	RefreshObjectAssemblyPresentation();
}

bool UHeistObjectAssemblyWidget::IsOwnerOnlyContractSatisfied() const
{
	return IsValid(ObjectAssemblyViewModel) && IsValid(PlayerController) && PlayerController == GetOwningPlayer() && PlayerController->IsLocalController() &&
		   ObjectAssemblyViewModel->IsOwnerOnlyContractSatisfied();
}

bool UHeistObjectAssemblyWidget::IsWidgetPresentationVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

bool UHeistObjectAssemblyWidget::IsPreviewReady() const
{
	return IsValid(PreviewActor) && IsValid(PreviewCoreComponent) && IsValid(PreviewCoreComponent->GetStaticMesh());
}

int32 UHeistObjectAssemblyWidget::GetPreviewComponentCount() const
{
	return (IsValid(PreviewCoreComponent) ? 1 : 0) + PreviewPartComponents.Num();
}

int32 UHeistObjectAssemblyWidget::GetUnresolvedPreviewSocketCount() const
{
	return UnresolvedPreviewSocketCount;
}

void UHeistObjectAssemblyWidget::BindButtons()
{
	PreviousPartButton->OnClicked.RemoveAll(this);
	PreviousPartButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandlePreviousPartClicked);
	NextPartButton->OnClicked.RemoveAll(this);
	NextPartButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleNextPartClicked);
	PreviousSocketButton->OnClicked.RemoveAll(this);
	PreviousSocketButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandlePreviousSocketClicked);
	NextSocketButton->OnClicked.RemoveAll(this);
	NextSocketButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleNextSocketClicked);
	RotateLeftButton->OnClicked.RemoveAll(this);
	RotateLeftButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleRotateLeftClicked);
	RotateRightButton->OnClicked.RemoveAll(this);
	RotateRightButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleRotateRightClicked);
	PlacePartButton->OnClicked.RemoveAll(this);
	PlacePartButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandlePlacePartClicked);
	RemovePartButton->OnClicked.RemoveAll(this);
	RemovePartButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleRemovePartClicked);
	ResetAssemblyButton->OnClicked.RemoveAll(this);
	ResetAssemblyButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleResetAssemblyClicked);
	SubmitButton->OnClicked.RemoveAll(this);
	SubmitButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleSubmitClicked);
	CancelButton->OnClicked.RemoveAll(this);
	CancelButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleCancelClicked);
}

void UHeistObjectAssemblyWidget::RefreshObjectAssemblyPresentation()
{
	const bool bVisible = IsValid(ObjectAssemblyViewModel) && ObjectAssemblyViewModel->IsPresentationVisible();
	const bool bDataReady = bVisible && ObjectAssemblyViewModel->IsDataReady();
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (!bVisible)
	{
		DestroyLocalPreview();
		LastDisplayedAssemblyTimeSeconds = INDEX_NONE;
		LastDisplayedLockdownSeconds = INDEX_NONE;
		BP_RefreshObjectAssemblyPresentation(false, false, 0, 0);
		return;
	}

	ApplyText(TemplateNameText, ObjectAssemblyViewModel->GetTemplateDisplayText());
	ApplyText(SelectedPartText, ObjectAssemblyViewModel->GetSelectedPartText());
	ApplyText(SelectedSocketText, ObjectAssemblyViewModel->GetSelectedSocketText());
	ApplyText(SelectedOrientationText, ObjectAssemblyViewModel->GetSelectedOrientationText());
	ApplyText(PlacementProgressText, ObjectAssemblyViewModel->GetPlacementProgressText());
	ApplyText(AssemblyStatusText, ObjectAssemblyViewModel->GetStatusText());
	ApplyText(AssemblyAlertWarningText, ObjectAssemblyViewModel->GetDangerWarningText());
	if (IsValid(AssemblyAlertWarningText))
	{
		AssemblyAlertWarningText->SetColorAndOpacity(ObjectAssemblyViewModel->GetDangerWarningColor());
	}
	ApplyVisibility(AssemblyAlertWarningText, ObjectAssemblyViewModel->IsDangerWarningVisible());

	const int32 CandidatePartCount = ObjectAssemblyViewModel->GetCandidatePartCount();
	const bool bHasSelection = bDataReady && CandidatePartCount > 0;
	PreviousPartButton->SetIsEnabled(bHasSelection && CandidatePartCount > 1);
	NextPartButton->SetIsEnabled(bHasSelection && CandidatePartCount > 1);
	PreviousSocketButton->SetIsEnabled(bHasSelection);
	NextSocketButton->SetIsEnabled(bHasSelection);
	RotateLeftButton->SetIsEnabled(bHasSelection);
	RotateRightButton->SetIsEnabled(bHasSelection);
	PlacePartButton->SetIsEnabled(bHasSelection);
	RemovePartButton->SetIsEnabled(bHasSelection && ObjectAssemblyViewModel->GetPlacedPartCount() > 0);
	ResetAssemblyButton->SetIsEnabled(bDataReady && ObjectAssemblyViewModel->GetPlacedPartCount() > 0);
	SubmitButton->SetIsEnabled(bDataReady && ObjectAssemblyViewModel->GetPlacedPartCount() > 0 && !ObjectAssemblyViewModel->IsSubmitPending());
	CancelButton->SetIsEnabled(true);

	RefreshLocalPreview();
	RefreshCountdownPresentation();
	BP_RefreshObjectAssemblyPresentation(true, bDataReady, ObjectAssemblyViewModel->GetPlacedPartCount(), ObjectAssemblyViewModel->GetRequiredPartCount());
}

void UHeistObjectAssemblyWidget::RefreshCountdownPresentation()
{
	if (!IsValid(ObjectAssemblyViewModel) || !ObjectAssemblyViewModel->IsPresentationVisible())
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	const bool bHasAuthoritativeAssemblyTime = IsValid(GameState) && ObjectAssemblyViewModel->GetSessionEndServerTime() > 0.0f;
	const float ServerWorldTime = bHasAuthoritativeAssemblyTime ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.0f;
	const int32 AssemblyTimeSeconds =
		bHasAuthoritativeAssemblyTime ? FMath::Max(0, FMath::CeilToInt(ObjectAssemblyViewModel->GetSessionEndServerTime() - ServerWorldTime)) : INDEX_NONE;
	if (AssemblyTimeSeconds != LastDisplayedAssemblyTimeSeconds)
	{
		LastDisplayedAssemblyTimeSeconds = AssemblyTimeSeconds;
		const FText TimeText = AssemblyTimeSeconds == INDEX_NONE
								   ? FText::FromString(TEXT("--:--"))
								   : FText::FromString(FString::Printf(TEXT("%02d:%02d"), AssemblyTimeSeconds / 60, AssemblyTimeSeconds % 60));
		ApplyText(AssemblyTimeRemainingText, FText::Format(NSLOCTEXT("HeistObjectAssembly", "AssemblyTimeRemaining", "ASSEMBLY TIME  {0}"), TimeText));
	}

	const bool bShowLockdown = ObjectAssemblyViewModel->IsLockdownCountdownVisible();
	const int32 LockdownTimeSeconds =
		bShowLockdown && IsValid(GameState) && ObjectAssemblyViewModel->GetLockdownCountdownEndServerTime() > 0.0f
			? FMath::Max(0, FMath::CeilToInt(ObjectAssemblyViewModel->GetLockdownCountdownEndServerTime() - static_cast<float>(GameState->GetServerWorldTimeSeconds())))
			: INDEX_NONE;
	if (LockdownTimeSeconds != LastDisplayedLockdownSeconds)
	{
		LastDisplayedLockdownSeconds = LockdownTimeSeconds;
		const FText TimeText = LockdownTimeSeconds == INDEX_NONE
								   ? FText::FromString(TEXT("--:--"))
								   : FText::FromString(FString::Printf(TEXT("%02d:%02d"), LockdownTimeSeconds / 60, LockdownTimeSeconds % 60));
		ApplyText(AssemblyLockdownCountdownText,
				  bShowLockdown ? FText::Format(NSLOCTEXT("HeistObjectAssembly", "LockdownCountdown", "THE MUSEUM WILL ENTER LOCKDOWN IN {0}."), TimeText)
								: FText::GetEmpty());
	}
	ApplyVisibility(AssemblyLockdownCountdownText, bShowLockdown);
}

void UHeistObjectAssemblyWidget::RefreshLocalPreview()
{
	if (!IsValid(ObjectAssemblyViewModel) || !ObjectAssemblyViewModel->IsDataReady() || !IsValid(AssemblyViewport))
	{
		DestroyLocalPreview();
		return;
	}

	const int32 PreviewRevision = ObjectAssemblyViewModel->GetLocalPreviewRevision();
	if (DisplayedPreviewRevision == PreviewRevision && IsValid(PreviewActor))
	{
		return;
	}

	DestroyLocalPreview();
	PreviewActor = AssemblyViewport->Spawn(AActor::StaticClass());
	if (!IsValid(PreviewActor))
	{
		return;
	}

	USceneComponent* PreviewRoot = NewObject<USceneComponent>(PreviewActor, TEXT("ObjectAssemblyPreviewRoot"));
	PreviewActor->SetRootComponent(PreviewRoot);
	PreviewActor->AddInstanceComponent(PreviewRoot);
	PreviewRoot->RegisterComponent();

	PreviewCoreComponent = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("ObjectAssemblyPreviewCore"));
	PreviewCoreComponent->SetupAttachment(PreviewRoot);
	PreviewCoreComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewCoreComponent->SetStaticMesh(ObjectAssemblyViewModel->LoadCoreStaticMesh());
	PreviewActor->AddInstanceComponent(PreviewCoreComponent);
	PreviewCoreComponent->RegisterComponent();

	const TArray<FHeistObjectAssemblyEntry>& Entries = ObjectAssemblyViewModel->GetLocalAssemblyEntries();
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FHeistObjectAssemblyEntry& Entry = Entries[EntryIndex];
		UStaticMeshComponent* PartComponent = NewObject<UStaticMeshComponent>(PreviewActor);
		PartComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartComponent->SetStaticMesh(ObjectAssemblyViewModel->LoadPartStaticMesh(Entry.PartId));

		if (IsValid(PreviewCoreComponent) && PreviewCoreComponent->DoesSocketExist(Entry.SocketId))
		{
			PartComponent->SetupAttachment(PreviewCoreComponent, Entry.SocketId);
			PartComponent->SetRelativeRotation(FRotator(0.0f, static_cast<float>(Entry.QuantizedOrientation) * 22.5f, 0.0f));
		}
		else
		{
			++UnresolvedPreviewSocketCount;
			PartComponent->SetupAttachment(PreviewRoot);
			PartComponent->SetRelativeTransform(ResolveFallbackPartTransform(Entry.SocketId, EntryIndex, Entry.QuantizedOrientation));
		}

		PreviewActor->AddInstanceComponent(PartComponent);
		PartComponent->RegisterComponent();
		PreviewPartComponents.Add(PartComponent);
	}

	const FVector CameraLocation(360.0f, 0.0f, 120.0f);
	const FVector PreviewLookAt(0.0f, 0.0f, 35.0f);
	AssemblyViewport->SetViewLocation(CameraLocation);
	AssemblyViewport->SetViewRotation((PreviewLookAt - CameraLocation).Rotation());
	AssemblyViewport->SetBackgroundColor(FLinearColor(0.018f, 0.024f, 0.035f, 1.0f));
	AssemblyViewport->SetLightIntensity(4.0f);
	AssemblyViewport->SetSkyIntensity(0.75f);
	DisplayedPreviewRevision = PreviewRevision;
}

void UHeistObjectAssemblyWidget::DestroyLocalPreview()
{
	PreviewPartComponents.Reset();
	PreviewCoreComponent = nullptr;
	UnresolvedPreviewSocketCount = 0;
	DisplayedPreviewRevision = INDEX_NONE;
	if (IsValid(PreviewActor))
	{
		PreviewActor->Destroy();
	}
	PreviewActor = nullptr;
}

FTransform UHeistObjectAssemblyWidget::ResolveFallbackPartTransform(const FName SocketId, const int32 PlacementIndex, const uint8 QuantizedOrientation) const
{
	const FString SocketName = SocketId.ToString();
	FVector Location;
	FVector Scale(0.55f);
	if (SocketName.Contains(TEXT("Head"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, 0.0f, 105.0f);
		Scale = FVector(0.65f);
	}
	else if (SocketName.Contains(TEXT("Shoulder_L"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, -75.0f, 40.0f);
		Scale = FVector(0.32f, 0.32f, 0.85f);
	}
	else if (SocketName.Contains(TEXT("Shoulder_R"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, 75.0f, 40.0f);
		Scale = FVector(0.32f, 0.32f, 0.85f);
	}
	else if (SocketName.Contains(TEXT("Pedestal"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, 0.0f, -80.0f);
		Scale = FVector(0.65f);
	}
	else if (SocketName.Contains(TEXT("Handle"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, 75.0f, 25.0f);
		Scale = FVector(0.45f);
	}
	else
	{
		const float AngleRadians = FMath::DegreesToRadians(static_cast<float>(PlacementIndex) * 72.0f);
		Location = FVector(0.0f, FMath::Cos(AngleRadians) * 85.0f, 25.0f + FMath::Sin(AngleRadians) * 60.0f);
	}

	const FRotator Rotation(0.0f, static_cast<float>(QuantizedOrientation) * 22.5f, 0.0f);
	return FTransform(Rotation, Location, Scale);
}

void UHeistObjectAssemblyWidget::HandlePreviousPartClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->SelectPreviousPart();
	}
}

void UHeistObjectAssemblyWidget::HandleNextPartClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->SelectNextPart();
	}
}

void UHeistObjectAssemblyWidget::HandlePreviousSocketClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->SelectPreviousSocket();
	}
}

void UHeistObjectAssemblyWidget::HandleNextSocketClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->SelectNextSocket();
	}
}

void UHeistObjectAssemblyWidget::HandleRotateLeftClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->RotatePrevious();
	}
}

void UHeistObjectAssemblyWidget::HandleRotateRightClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->RotateNext();
	}
}

void UHeistObjectAssemblyWidget::HandlePlacePartClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->PlaceOrUpdateSelectedPart();
	}
}

void UHeistObjectAssemblyWidget::HandleRemovePartClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->RemoveSelectedPart();
	}
}

void UHeistObjectAssemblyWidget::HandleResetAssemblyClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->ResetLocalAssembly();
	}
}

void UHeistObjectAssemblyWidget::HandleSubmitClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->RequestSubmitAssembly();
	}
}

void UHeistObjectAssemblyWidget::HandleCancelClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->RequestCancelAssembly();
	}
}
