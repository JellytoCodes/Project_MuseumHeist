#include "UI/Pool/HeistPopupWidgetPool.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "TimerManager.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#pragma region Construction

UHeistPopupWidgetPool::UHeistPopupWidgetPool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistPopupWidgetPool::SetupPool(
	AHeistPlayerController* InPlayerController,
	UPanelWidget* InPopupLayer,
	TSubclassOf<UHeistUserWidgetBase> InPopupWidgetClass,
	const int32 InCapacity)
{
	const int32 SafeCapacity = FMath::Clamp(InCapacity, 1, 5);
	if (bInitialized
		&& PlayerController == InPlayerController
		&& PopupLayer == InPopupLayer
		&& PopupWidgetClass == InPopupWidgetClass
		&& Capacity == SafeCapacity)
	{
		return;
	}

	ShutdownPool();
	if (!IsValid(InPlayerController)
		|| !InPlayerController->IsLocalController()
		|| !IsValid(InPopupLayer)
		|| !InPopupWidgetClass)
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("Popup feedback pool setup rejected: Controller=%s Layer=%s Class=%s"),
			*GetNameSafe(InPlayerController),
			*GetNameSafe(InPopupLayer),
			*GetNameSafe(InPopupWidgetClass.Get()));
		return;
	}

	PlayerController = InPlayerController;
	PopupLayer = InPopupLayer;
	PopupWidgetClass = InPopupWidgetClass;
	Capacity = SafeCapacity;

	for (int32 PopupIndex = 0; PopupIndex < Capacity; ++PopupIndex)
	{
		UHeistUserWidgetBase* PopupWidget = CreateWidget<UHeistUserWidgetBase>(
			PlayerController,
			PopupWidgetClass);
		if (!IsValid(PopupWidget))
		{
			continue;
		}

		PopupLayer->AddChild(PopupWidget);
		PopupWidget->SetVisibility(ESlateVisibility::Collapsed);
		AllPopupWidgets.Add(PopupWidget);
		AvailablePopupWidgets.Add(PopupWidget);
	}

	if (AllPopupWidgets.Num() == 0)
	{
		ShutdownPool();
		UE_LOG(LogHeistUI, Warning, TEXT("Popup feedback pool setup rejected: Reason=NoWidgetsCreated"));
		return;
	}

	PlayerController->GetPopupFeedbackRequestedDelegate().RemoveAll(this);
	PlayerController->GetPopupFeedbackRequestedDelegate().AddUObject(
		this,
		&UHeistPopupWidgetPool::HandlePopupFeedbackRequested);
	bInitialized = true;

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("Popup feedback pool setup: Controller=%s Layer=%s Class=%s Capacity=%d"),
		*GetNameSafe(PlayerController),
		*GetNameSafe(PopupLayer),
		*GetNameSafe(PopupWidgetClass.Get()),
		AllPopupWidgets.Num());
}

void UHeistPopupWidgetPool::ShutdownPool()
{
	if (IsValid(PlayerController))
	{
		PlayerController->GetPopupFeedbackRequestedDelegate().RemoveAll(this);
		if (UWorld* World = PlayerController->GetWorld())
		{
			World->GetTimerManager().ClearTimer(ExpirationTimerHandle);
		}
	}

	ReleaseAllPopups(TEXT("PoolShutdown"));
	for (UHeistUserWidgetBase* PopupWidget : AllPopupWidgets)
	{
		if (IsValid(PopupWidget))
		{
			PopupWidget->RemoveFromParent();
		}
	}

	AllPopupWidgets.Reset();
	AvailablePopupWidgets.Reset();
	ActivePopups.Reset();
	PlayerController = nullptr;
	PopupLayer = nullptr;
	PopupWidgetClass = nullptr;
	NextSequenceId = 1;
	bInitialized = false;
}

#pragma endregion

#pragma region Debug

void UHeistPopupWidgetPool::DebugDumpState() const
{
	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("Popup feedback pool dump: Initialized=%s Active=%d Available=%d Capacity=%d"),
		bInitialized ? TEXT("true") : TEXT("false"),
		ActivePopups.Num(),
		AvailablePopupWidgets.Num(),
		AllPopupWidgets.Num());

	for (int32 PopupIndex = 0; PopupIndex < ActivePopups.Num(); ++PopupIndex)
	{
		const FActivePopup& Popup = ActivePopups[PopupIndex];
		UE_LOG(
			LogHeistUI,
			Log,
			TEXT("Popup feedback pool entry: Slot=%d SequenceId=%d Message=%s EndWorldTime=%.2f Widget=%s"),
			PopupIndex,
			Popup.SequenceId,
			*Popup.Message.ToString(),
			Popup.EndWorldTime,
			*GetNameSafe(Popup.Widget));
	}
}

#pragma endregion

#pragma region Pool

void UHeistPopupWidgetPool::HandlePopupFeedbackRequested(
	const FText& Message,
	const float DurationSeconds)
{
	if (!bInitialized || Message.IsEmpty() || !IsValid(PlayerController) || !PlayerController->GetWorld())
	{
		return;
	}

	UHeistUserWidgetBase* PopupWidget = AcquirePopupWidget();
	if (!IsValid(PopupWidget) && ActivePopups.Num() > 0)
	{
		int32 OldestPopupIndex = 0;
		for (int32 PopupIndex = 1; PopupIndex < ActivePopups.Num(); ++PopupIndex)
		{
			if (ActivePopups[PopupIndex].EndWorldTime < ActivePopups[OldestPopupIndex].EndWorldTime)
			{
				OldestPopupIndex = PopupIndex;
			}
		}

		PopupWidget = ActivePopups[OldestPopupIndex].Widget;
		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("Popup feedback replaced: RemovedSequenceId=%d IncomingSequenceId=%d"),
			ActivePopups[OldestPopupIndex].SequenceId,
			NextSequenceId);
		ActivePopups.RemoveAt(OldestPopupIndex);
	}

	if (!IsValid(PopupWidget))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("Popup feedback rejected: Reason=NoPooledWidget"));
		return;
	}

	FActivePopup& NewPopup = ActivePopups.AddDefaulted_GetRef();
	NewPopup.Widget = PopupWidget;
	NewPopup.Message = Message;
	NewPopup.EndWorldTime = PlayerController->GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, DurationSeconds);
	NewPopup.SequenceId = NextSequenceId++;
	ShowPopupWidget(PopupWidget, Message);

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("Popup feedback activated: SequenceId=%d Message=%s Duration=%.2f Active=%d"),
		NewPopup.SequenceId,
		*Message.ToString(),
		FMath::Max(0.1f, DurationSeconds),
		ActivePopups.Num());
	RefreshExpirationTimer();
}

void UHeistPopupWidgetPool::ShowPopupWidget(
	UHeistUserWidgetBase* PopupWidget,
	const FText& Message) const
{
	if (!IsValid(PopupWidget))
	{
		return;
	}

	PopupWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWidget* PopupContainer = PopupWidget->GetWidgetFromName(TEXT("PopupContainer")))
	{
		PopupContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (UTextBlock* PopupText = Cast<UTextBlock>(PopupWidget->GetWidgetFromName(TEXT("PopupText"))))
	{
		PopupText->SetText(Message);
	}
}

void UHeistPopupWidgetPool::ReleasePopupAt(const int32 ActivePopupIndex, const TCHAR* Reason)
{
	if (!ActivePopups.IsValidIndex(ActivePopupIndex))
	{
		return;
	}

	const FActivePopup Popup = ActivePopups[ActivePopupIndex];
	if (IsValid(Popup.Widget))
	{
		Popup.Widget->SetVisibility(ESlateVisibility::Collapsed);
		AvailablePopupWidgets.AddUnique(Popup.Widget);
	}

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("Popup feedback released: SequenceId=%d Reason=%s ActiveAfter=%d"),
		Popup.SequenceId,
		Reason,
		ActivePopups.Num() - 1);
	ActivePopups.RemoveAt(ActivePopupIndex);
}

void UHeistPopupWidgetPool::ReleaseAllPopups(const TCHAR* Reason)
{
	for (int32 PopupIndex = ActivePopups.Num() - 1; PopupIndex >= 0; --PopupIndex)
	{
		ReleasePopupAt(PopupIndex, Reason);
	}
}

UHeistUserWidgetBase* UHeistPopupWidgetPool::AcquirePopupWidget()
{
	while (AvailablePopupWidgets.Num() > 0)
	{
		UHeistUserWidgetBase* PopupWidget = AvailablePopupWidgets.Pop();
		if (IsValid(PopupWidget))
		{
			return PopupWidget;
		}
	}
	return nullptr;
}

void UHeistPopupWidgetPool::RefreshExpirationTimer()
{
	if (!IsValid(PlayerController) || !PlayerController->GetWorld())
	{
		return;
	}

	FTimerManager& TimerManager = PlayerController->GetWorld()->GetTimerManager();
	TimerManager.ClearTimer(ExpirationTimerHandle);
	if (ActivePopups.Num() == 0)
	{
		return;
	}

	float EarliestEndWorldTime = MAX_flt;
	for (const FActivePopup& Popup : ActivePopups)
	{
		EarliestEndWorldTime = FMath::Min(EarliestEndWorldTime, Popup.EndWorldTime);
	}

	TimerManager.SetTimer(
		ExpirationTimerHandle,
		this,
		&UHeistPopupWidgetPool::HandleExpirationTimer,
		FMath::Max(0.01f, EarliestEndWorldTime - PlayerController->GetWorld()->GetTimeSeconds()),
		false);
}

void UHeistPopupWidgetPool::HandleExpirationTimer()
{
	if (!IsValid(PlayerController) || !PlayerController->GetWorld())
	{
		return;
	}

	const float CurrentWorldTime = PlayerController->GetWorld()->GetTimeSeconds();
	for (int32 PopupIndex = ActivePopups.Num() - 1; PopupIndex >= 0; --PopupIndex)
	{
		if (ActivePopups[PopupIndex].EndWorldTime <= CurrentWorldTime + KINDA_SMALL_NUMBER)
		{
			ReleasePopupAt(PopupIndex, TEXT("DurationExpired"));
		}
	}
	RefreshExpirationTimer();
}

#pragma endregion
