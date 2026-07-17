#include "UI/Pool/HeistSoundPingWidgetPool.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/Widgets/HeistSoundPingMarkerWidget.h"

#pragma region Construction

UHeistSoundPingWidgetPool::UHeistSoundPingWidgetPool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistSoundPingWidgetPool::SetupPool(
	APlayerController* InOwningPlayerController,
	AHeistGameState* InGameState,
	UPanelWidget* InMarkerLayer,
	TSubclassOf<UHeistSoundPingMarkerWidget> InMarkerWidgetClass,
	const float InScreenMarginPixels)
{
	if (bInitialized
		&& OwningPlayerController == InOwningPlayerController
		&& GameState == InGameState
		&& MarkerLayer == InMarkerLayer
		&& MarkerWidgetClass == InMarkerWidgetClass
		&& FMath::IsNearlyEqual(ScreenMarginPixels, InScreenMarginPixels))
	{
		return;
	}

	ShutdownPool();

	if (!IsValid(InOwningPlayerController)
		|| !InOwningPlayerController->IsLocalController()
		|| !IsValid(InGameState)
		|| !IsValid(InMarkerLayer)
		|| !InMarkerWidgetClass)
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("Sound Ping pool setup rejected: Controller=%s GameState=%s MarkerLayer=%s MarkerClass=%s"),
			*GetNameSafe(InOwningPlayerController),
			*GetNameSafe(InGameState),
			*GetNameSafe(InMarkerLayer),
			*GetNameSafe(InMarkerWidgetClass.Get()));
		return;
	}

	OwningPlayerController = InOwningPlayerController;
	GameState = InGameState;
	MarkerLayer = InMarkerLayer;
	MarkerWidgetClass = InMarkerWidgetClass;
	ScreenMarginPixels = FMath::Max(0.0f, InScreenMarginPixels);

	for (int32 MarkerIndex = 0; MarkerIndex < MaxActiveMarkerCount; ++MarkerIndex)
	{
		UHeistSoundPingMarkerWidget* MarkerWidget = CreateWidget<UHeistSoundPingMarkerWidget>(
			OwningPlayerController,
			MarkerWidgetClass);
		if (!IsValid(MarkerWidget))
		{
			continue;
		}

		MarkerLayer->AddChild(MarkerWidget);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f));
			CanvasSlot->SetAlignment(FVector2D(0.5f));
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
			CanvasSlot->SetAutoSize(true);
		}

		MarkerWidget->ReleaseSoundPingMarker();
		AllMarkerWidgets.Add(MarkerWidget);
		AvailableMarkerWidgets.Add(MarkerWidget);
	}

	if (AllMarkerWidgets.Num() == 0)
	{
		ShutdownPool();
		UE_LOG(LogHeistUI, Warning, TEXT("Sound Ping pool setup rejected: Reason=NoMarkerWidgetsCreated"));
		return;
	}

	GameState->GetSoundPingEventReportedDelegate().RemoveAll(this);
	GameState->GetSoundPingEventReportedDelegate().AddUObject(
		this,
		&UHeistSoundPingWidgetPool::HandleSoundPingReported);
	bInitialized = true;

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("Sound Ping pool setup: Controller=%s MarkerLayer=%s MarkerClass=%s Capacity=%d MergeAngle=%.1f MergeWindow=%.2f ScreenMargin=%.1f"),
		*GetNameSafe(OwningPlayerController),
		*GetNameSafe(MarkerLayer),
		*GetNameSafe(MarkerWidgetClass.Get()),
		AllMarkerWidgets.Num(),
		MergeAngleDegrees,
		MergeWindowSeconds,
		ScreenMarginPixels);

	const FHeistSoundPingEvent& ExistingEvent = GameState->GetLastSoundPingEvent();
	if (ExistingEvent.SequenceId > 0
		&& ExistingEvent.ServerTimeSeconds + ExistingEvent.Duration > GetServerTimeSeconds())
	{
		HandleSoundPingReported(ExistingEvent);
	}
}

void UHeistSoundPingWidgetPool::ShutdownPool()
{
	if (IsValid(GameState))
	{
		GameState->GetSoundPingEventReportedDelegate().RemoveAll(this);
		if (UWorld* World = GameState->GetWorld())
		{
			World->GetTimerManager().ClearTimer(ExpirationTimerHandle);
		}
	}

	ReleaseAllMarkers(TEXT("PoolShutdown"));
	for (UHeistSoundPingMarkerWidget* MarkerWidget : AllMarkerWidgets)
	{
		if (IsValid(MarkerWidget))
		{
			MarkerWidget->RemoveFromParent();
		}
	}

	AllMarkerWidgets.Reset();
	AvailableMarkerWidgets.Reset();
	ActiveMarkers.Reset();
	OwningPlayerController = nullptr;
	GameState = nullptr;
	MarkerLayer = nullptr;
	MarkerWidgetClass = nullptr;
	bInitialized = false;
}

#pragma endregion

#pragma region Debug

void UHeistSoundPingWidgetPool::DebugDumpState() const
{
	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("Sound Ping pool dump: Initialized=%s Active=%d Available=%d Capacity=%d"),
		bInitialized ? TEXT("true") : TEXT("false"),
		ActiveMarkers.Num(),
		AvailableMarkerWidgets.Num(),
		AllMarkerWidgets.Num());

	for (int32 MarkerIndex = 0; MarkerIndex < ActiveMarkers.Num(); ++MarkerIndex)
	{
		const FActiveSoundPingMarker& Marker = ActiveMarkers[MarkerIndex];
		UE_LOG(
			LogHeistUI,
			Log,
			TEXT("Sound Ping pool entry: Slot=%d SequenceId=%d Type=%d Priority=%d Angle=%.1f EndServerTime=%.2f Widget=%s"),
			MarkerIndex,
			Marker.Event.SequenceId,
			static_cast<int32>(Marker.Event.PingType),
			Marker.Priority,
			ResolveDirectionAngleDegrees(Marker.ScreenDirection),
			Marker.EndServerTime,
			*GetNameSafe(Marker.Widget));
	}
}

void UHeistSoundPingWidgetPool::DebugRunPresentationTest()
{
	if (!bInitialized || !IsValid(OwningPlayerController) || !IsValid(OwningPlayerController->GetPawn()))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("Sound Ping pool test rejected: Reason=PoolNotReady"));
		return;
	}

	ReleaseAllMarkers(TEXT("DebugTestReset"));
	const FVector Origin = OwningPlayerController->GetPawn()->GetActorLocation();
	const float BaseServerTime = GetServerTimeSeconds();

	const auto InjectEvent = [this, Origin, BaseServerTime](
		const int32 SequenceId,
		const EHeistSoundPingType PingType,
		const float WorldAngleDegrees,
		const float TimeOffset)
	{
		const float Radians = FMath::DegreesToRadians(WorldAngleDegrees);
		FHeistSoundPingEvent Event;
		Event.SequenceId = SequenceId;
		Event.PingType = PingType;
		Event.WorldLocation = Origin + FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0f) * 1000.0f;
		Event.Radius = 2000.0f;
		Event.Duration = 5.0f;
		Event.bAffectsPlayers = true;
		Event.ServerTimeSeconds = BaseServerTime + TimeOffset;
		HandleSoundPingReported(Event);
	};

	InjectEvent(9001, EHeistSoundPingType::Footstep, -135.0f, 0.00f);
	InjectEvent(9002, EHeistSoundPingType::CoinImpact, -45.0f, 0.05f);
	InjectEvent(9003, EHeistSoundPingType::GlassBreak, 45.0f, 0.10f);
	InjectEvent(9004, EHeistSoundPingType::Footstep, 135.0f, 0.15f);
	InjectEvent(9005, EHeistSoundPingType::StunHit, -42.0f, 0.20f);
	InjectEvent(9006, EHeistSoundPingType::Footstep, 180.0f, 0.25f);

	UE_LOG(LogHeistUI, Log, TEXT("Sound Ping pool deterministic presentation test completed."));
	DebugDumpState();
}

#pragma endregion

#pragma region Pool

void UHeistSoundPingWidgetPool::HandleSoundPingReported(const FHeistSoundPingEvent& SoundPingEvent)
{
	if (!bInitialized
		|| !SoundPingEvent.bAffectsPlayers
		|| SoundPingEvent.PingType == EHeistSoundPingType::None
		|| SoundPingEvent.Duration <= 0.0f)
	{
		return;
	}

	FVector2D ScreenDirection;
	if (!ComputeScreenDirection(SoundPingEvent.WorldLocation, ScreenDirection))
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("Sound Ping marker rejected: SequenceId=%d Reason=DirectionProjectionFailed"),
			SoundPingEvent.SequenceId);
		return;
	}

	const float CurrentServerTime = GetServerTimeSeconds();
	const float EndServerTime = SoundPingEvent.ServerTimeSeconds + SoundPingEvent.Duration;
	if (EndServerTime <= CurrentServerTime)
	{
		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("Sound Ping marker rejected: SequenceId=%d Reason=Expired EndServerTime=%.2f CurrentServerTime=%.2f"),
			SoundPingEvent.SequenceId,
			EndServerTime,
			CurrentServerTime);
		return;
	}

	const int32 Priority = ResolvePriority(SoundPingEvent.PingType);
	const float DirectionAngle = ResolveDirectionAngleDegrees(ScreenDirection);
	for (FActiveSoundPingMarker& ActiveMarker : ActiveMarkers)
	{
		const float ActiveAngle = ResolveDirectionAngleDegrees(ActiveMarker.ScreenDirection);
		const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(ActiveAngle, DirectionAngle));
		const float TimeDelta = FMath::Abs(SoundPingEvent.ServerTimeSeconds - ActiveMarker.LastEventServerTime);
		if (AngleDelta > MergeAngleDegrees || TimeDelta > MergeWindowSeconds)
		{
			continue;
		}

		const bool bIncomingWins = Priority < ActiveMarker.Priority;
		const int32 PreviousSequenceId = ActiveMarker.Event.SequenceId;
		const int32 PreviousPriority = ActiveMarker.Priority;
		if (bIncomingWins || Priority == ActiveMarker.Priority)
		{
			ActiveMarker.Event = SoundPingEvent;
			ActiveMarker.ScreenDirection = ScreenDirection;
			ActiveMarker.Priority = Priority;
			ActiveMarker.EndServerTime = EndServerTime;
			ActiveMarker.LastEventServerTime = SoundPingEvent.ServerTimeSeconds;
			ActiveMarker.Widget->ShowSoundPingMarker(
				ActiveMarker.Event,
				ActiveMarker.ScreenDirection,
				ComputeScreenEdgeTranslation(ActiveMarker.ScreenDirection));
		}

		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("Sound Ping marker merged: IncomingSequenceId=%d ActiveSequenceId=%d IncomingPriority=%d ActivePriority=%d AngleDelta=%.1f TimeDelta=%.2f IncomingWins=%s"),
			SoundPingEvent.SequenceId,
			PreviousSequenceId,
			Priority,
			PreviousPriority,
			AngleDelta,
			TimeDelta,
			bIncomingWins ? TEXT("true") : TEXT("false"));
		RefreshExpirationTimer();
		return;
	}

	ActivateMarker(SoundPingEvent, ScreenDirection, Priority);
}

void UHeistSoundPingWidgetPool::ActivateMarker(
	const FHeistSoundPingEvent& SoundPingEvent,
	const FVector2D& ScreenDirection,
	const int32 Priority)
{
	int32 ReplacementIndex = INDEX_NONE;
	if (ActiveMarkers.Num() >= MaxActiveMarkerCount)
	{
		int32 LowestPriority = MIN_int32;
		float EarliestEndServerTime = MAX_flt;
		for (int32 MarkerIndex = 0; MarkerIndex < ActiveMarkers.Num(); ++MarkerIndex)
		{
			const FActiveSoundPingMarker& ActiveMarker = ActiveMarkers[MarkerIndex];
			if (ActiveMarker.Priority > LowestPriority
				|| (ActiveMarker.Priority == LowestPriority && ActiveMarker.EndServerTime < EarliestEndServerTime))
			{
				LowestPriority = ActiveMarker.Priority;
				EarliestEndServerTime = ActiveMarker.EndServerTime;
				ReplacementIndex = MarkerIndex;
			}
		}

		if (ReplacementIndex == INDEX_NONE || Priority >= ActiveMarkers[ReplacementIndex].Priority)
		{
			UE_LOG(
				LogHeistUI,
				Verbose,
				TEXT("Sound Ping marker suppressed: SequenceId=%d Priority=%d Reason=PoolFull Active=%d"),
				SoundPingEvent.SequenceId,
				Priority,
				ActiveMarkers.Num());
			return;
		}
	}

	UHeistSoundPingMarkerWidget* MarkerWidget = nullptr;
	if (ReplacementIndex != INDEX_NONE)
	{
		MarkerWidget = ActiveMarkers[ReplacementIndex].Widget;
		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("Sound Ping marker replaced: RemovedSequenceId=%d RemovedPriority=%d IncomingSequenceId=%d IncomingPriority=%d"),
			ActiveMarkers[ReplacementIndex].Event.SequenceId,
			ActiveMarkers[ReplacementIndex].Priority,
			SoundPingEvent.SequenceId,
			Priority);
		ActiveMarkers.RemoveAt(ReplacementIndex);
	}
	else
	{
		MarkerWidget = AcquireMarkerWidget();
	}

	if (!IsValid(MarkerWidget))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("Sound Ping marker rejected: SequenceId=%d Reason=NoPooledWidget"), SoundPingEvent.SequenceId);
		return;
	}

	FActiveSoundPingMarker& NewMarker = ActiveMarkers.AddDefaulted_GetRef();
	NewMarker.Widget = MarkerWidget;
	NewMarker.Event = SoundPingEvent;
	NewMarker.ScreenDirection = ScreenDirection;
	NewMarker.Priority = Priority;
	NewMarker.EndServerTime = SoundPingEvent.ServerTimeSeconds + SoundPingEvent.Duration;
	NewMarker.LastEventServerTime = SoundPingEvent.ServerTimeSeconds;
	MarkerWidget->ShowSoundPingMarker(
		SoundPingEvent,
		ScreenDirection,
		ComputeScreenEdgeTranslation(ScreenDirection));

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("Sound Ping marker activated: SequenceId=%d Type=%d Priority=%d Angle=%.1f Duration=%.2f Active=%d"),
		SoundPingEvent.SequenceId,
		static_cast<int32>(SoundPingEvent.PingType),
		Priority,
		ResolveDirectionAngleDegrees(ScreenDirection),
		SoundPingEvent.Duration,
		ActiveMarkers.Num());
	RefreshExpirationTimer();
}

void UHeistSoundPingWidgetPool::ReleaseMarkerAt(const int32 ActiveMarkerIndex, const TCHAR* Reason)
{
	if (!ActiveMarkers.IsValidIndex(ActiveMarkerIndex))
	{
		return;
	}

	const FActiveSoundPingMarker Marker = ActiveMarkers[ActiveMarkerIndex];
	if (IsValid(Marker.Widget))
	{
		Marker.Widget->ReleaseSoundPingMarker();
		AvailableMarkerWidgets.AddUnique(Marker.Widget);
	}

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("Sound Ping marker released: SequenceId=%d Reason=%s ActiveAfter=%d"),
		Marker.Event.SequenceId,
		Reason,
		ActiveMarkers.Num() - 1);
	ActiveMarkers.RemoveAt(ActiveMarkerIndex);
}

void UHeistSoundPingWidgetPool::ReleaseAllMarkers(const TCHAR* Reason)
{
	for (int32 MarkerIndex = ActiveMarkers.Num() - 1; MarkerIndex >= 0; --MarkerIndex)
	{
		ReleaseMarkerAt(MarkerIndex, Reason);
	}
}

UHeistSoundPingMarkerWidget* UHeistSoundPingWidgetPool::AcquireMarkerWidget()
{
	while (AvailableMarkerWidgets.Num() > 0)
	{
		UHeistSoundPingMarkerWidget* MarkerWidget = AvailableMarkerWidgets.Pop();
		if (IsValid(MarkerWidget))
		{
			return MarkerWidget;
		}
	}
	return nullptr;
}

bool UHeistSoundPingWidgetPool::ComputeScreenDirection(
	const FVector& WorldLocation,
	FVector2D& OutScreenDirection) const
{
	OutScreenDirection = FVector2D::ZeroVector;
	if (!IsValid(OwningPlayerController) || !IsValid(OwningPlayerController->GetPawn()))
	{
		return false;
	}

	FVector2D OriginScreenPosition;
	FVector2D TargetScreenPosition;
	const FVector OriginWorldLocation = OwningPlayerController->GetPawn()->GetActorLocation();
	if (!OwningPlayerController->ProjectWorldLocationToScreen(OriginWorldLocation, OriginScreenPosition, true)
		|| !OwningPlayerController->ProjectWorldLocationToScreen(WorldLocation, TargetScreenPosition, true))
	{
		return false;
	}

	OutScreenDirection = (TargetScreenPosition - OriginScreenPosition).GetSafeNormal();
	return !OutScreenDirection.IsNearlyZero();
}

FVector2D UHeistSoundPingWidgetPool::ComputeScreenEdgeTranslation(const FVector2D& ScreenDirection) const
{
	if (!IsValid(OwningPlayerController))
	{
		return FVector2D::ZeroVector;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	OwningPlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return FVector2D::ZeroVector;
	}

	const float ViewportScale = FMath::Max(
		KINDA_SMALL_NUMBER,
		UWidgetLayoutLibrary::GetViewportScale(OwningPlayerController));
	const FVector2D Direction = ScreenDirection.GetSafeNormal();
	const float HorizontalExtent = FMath::Max(
		0.0f,
		ViewportWidth / ViewportScale * 0.5f - ScreenMarginPixels);
	const float VerticalExtent = FMath::Max(
		0.0f,
		ViewportHeight / ViewportScale * 0.5f - ScreenMarginPixels);
	const float HorizontalScale = FMath::Abs(Direction.X) > KINDA_SMALL_NUMBER
		? HorizontalExtent / FMath::Abs(Direction.X)
		: MAX_flt;
	const float VerticalScale = FMath::Abs(Direction.Y) > KINDA_SMALL_NUMBER
		? VerticalExtent / FMath::Abs(Direction.Y)
		: MAX_flt;
	return Direction * FMath::Min(HorizontalScale, VerticalScale);
}

void UHeistSoundPingWidgetPool::RefreshExpirationTimer()
{
	if (!IsValid(GameState) || !GameState->GetWorld())
	{
		return;
	}

	FTimerManager& TimerManager = GameState->GetWorld()->GetTimerManager();
	TimerManager.ClearTimer(ExpirationTimerHandle);
	if (ActiveMarkers.Num() == 0)
	{
		return;
	}

	float EarliestEndServerTime = MAX_flt;
	for (const FActiveSoundPingMarker& Marker : ActiveMarkers)
	{
		EarliestEndServerTime = FMath::Min(EarliestEndServerTime, Marker.EndServerTime);
	}

	TimerManager.SetTimer(
		ExpirationTimerHandle,
		this,
		&UHeistSoundPingWidgetPool::HandleExpirationTimer,
		FMath::Max(0.01f, EarliestEndServerTime - GetServerTimeSeconds()),
		false);
}

void UHeistSoundPingWidgetPool::HandleExpirationTimer()
{
	const float CurrentServerTime = GetServerTimeSeconds();
	for (int32 MarkerIndex = ActiveMarkers.Num() - 1; MarkerIndex >= 0; --MarkerIndex)
	{
		if (ActiveMarkers[MarkerIndex].EndServerTime <= CurrentServerTime + KINDA_SMALL_NUMBER)
		{
			ReleaseMarkerAt(MarkerIndex, TEXT("DurationExpired"));
		}
	}
	RefreshExpirationTimer();
}

float UHeistSoundPingWidgetPool::GetServerTimeSeconds() const
{
	return IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : 0.0f;
}

int32 UHeistSoundPingWidgetPool::ResolvePriority(const EHeistSoundPingType PingType)
{
	switch (PingType)
	{
	case EHeistSoundPingType::StunHit:
		return 2;
	case EHeistSoundPingType::GlassBreak:
	case EHeistSoundPingType::NoiseTrap:
		return 3;
	case EHeistSoundPingType::CoinImpact:
		return 4;
	case EHeistSoundPingType::Footstep:
		return 5;
	default:
		return MAX_int32;
	}
}

float UHeistSoundPingWidgetPool::ResolveDirectionAngleDegrees(const FVector2D& ScreenDirection)
{
	return FMath::RadiansToDegrees(FMath::Atan2(ScreenDirection.Y, ScreenDirection.X));
}

#pragma endregion
