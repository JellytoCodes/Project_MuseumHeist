#include "World/Actors/Security/HeistDetentionDoorActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistPlayerState.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AHeistDetentionDoorActor::AHeistDetentionDoorActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBoxComponent>(TEXT("InteractionCollision")))
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(20.0f);
	CastChecked<UBoxComponent>(InteractionCollision)->InitBoxExtent(FVector(100, 120, 130));
	DoorBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBlocker"));
	DoorBlocker->SetupAttachment(RootComponent);
	DoorBlocker->InitBoxExtent(FVector(90, 12, 145));
	DoorBlocker->SetCollisionProfileName(TEXT("BlockAll"));
	DoorBlocker->SetCanEverAffectNavigation(false);
	DoorHinge = CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge"));
	DoorHinge->SetupAttachment(RootComponent);
	DoorHinge->SetRelativeLocation(FVector(-90,0,0));
	VisualMeshComponent->SetupAttachment(DoorHinge);
	CellBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CellBounds"));
	CellBounds->SetupAttachment(RootComponent);
	CellBounds->InitBoxExtent(FVector(400,300,200));
	CellBounds->SetRelativeLocation(FVector(0,-300,0));
	CellBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CellBounds->SetCanEverAffectNavigation(false);
}

void AHeistDetentionDoorActor::BeginPlay()
{
	Super::BeginPlay();
	LatchPeriodSeconds = FMath::IsFinite(LatchPeriodSeconds) ? FMath::Max(3.0f, LatchPeriodSeconds) : 6.0f;
	RescueDurationSeconds = FMath::IsFinite(RescueDurationSeconds) ? FMath::Max(0.5f, RescueDurationSeconds) : 2.0f;
	ApplyPresentation();
	if (HasAuthority())
	{
		BoundGameState = GetWorld()->GetGameState<AHeistGameState>();
		if (BoundGameState.IsValid())
			BoundGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistDetentionDoorActor::HandlePhaseChanged);
	}
}

void AHeistDetentionDoorActor::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(ValidationTimer);
	if (BoundGameState.IsValid()) BoundGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
	Super::EndPlay(Reason);
}

float AHeistDetentionDoorActor::ServerTime() const
{
	const AHeistGameState* State = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return IsValid(State) ? State->GetServerWorldTimeSeconds() : 0.0f;
}

bool AHeistDetentionDoorActor::ContainsLocation(const FVector& Location) const
{
	return FBox(-CellBounds->GetUnscaledBoxExtent(), CellBounds->GetUnscaledBoxExtent()).IsInsideOrOn(CellBounds->GetComponentTransform().InverseTransformPosition(Location));
}

bool AHeistDetentionDoorActor::IsPlayerContained(const AHeistPlayerState* Player) const
{
	return !bOpen && IsValid(Player) && Player->GetDetentionDoor() == this && IsValid(Player->GetPawn()) && ContainsLocation(Player->GetPawn()->GetActorLocation());
}

bool AHeistDetentionDoorActor::CanSecureCell() const
{
	// Do not close a door through a crew member during a concurrent arrest.
	const FVector Extent = DoorBlocker->GetUnscaledBoxExtent();
	for (TActorIterator<AHeistPlayerCharacter> It(GetWorld()); It; ++It)
	{
		const FVector Local = DoorBlocker->GetComponentTransform().InverseTransformPosition(It->GetActorLocation());
		const float Radius = It->GetSimpleCollisionRadius();
		if (FMath::Abs(Local.X) < Extent.X + Radius && FMath::Abs(Local.Y) < Extent.Y + Radius && FMath::Abs(Local.Z) < Extent.Z + It->GetSimpleCollisionHalfHeight()) return false;
	}
	return true;
}

void AHeistDetentionDoorActor::SecureForArrest(AHeistPlayerState* Player)
{
	if (!HasAuthority() || !IsValid(Player)) return;
	CancelOperation();
	bOpen = false;
	CompletedLatches = 0;
	++Revision;
	Player->SetDetentionDoor(this);
	ApplyPresentation();
	ForceNetUpdate();
}

bool AHeistDetentionDoorActor::CanInteract(const AActor* Interactor) const
{
	const AHeistPlayerCharacter* Character = Cast<AHeistPlayerCharacter>(Interactor);
	const AHeistPlayerState* Player = IsValid(Character) ? Character->GetPlayerState<AHeistPlayerState>() : nullptr;
	const AHeistGameState* State = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return Super::CanInteract(Interactor) && !bOpen && IsValid(Player) && IsValid(State) && State->GetMatchPhase() == EHeistMatchPhase::InGame &&
		State->PlayerArray.Contains(Player) && Character->CanPerformGameplayActions() && !Character->GetInventoryComponent()->IsInventoryOpen() &&
		(!IsValid(Operator) || Operator == Player) &&
		(!ContainsLocation(Character->GetActorLocation()) || Player->GetDetentionDoor() == this);
}

void AHeistDetentionDoorActor::Interact(AActor* Interactor)
{
	TryUse(Cast<AHeistPlayerCharacter>(Interactor), Revision);
}

bool AHeistDetentionDoorActor::TryUse(AHeistPlayerCharacter* Character, int32 ExpectedRevision)
{
	if (!HasAuthority() || ExpectedRevision != Revision || !CanInteract(Character) ||
		!Character->GetInteractionComponent()->IsActorOverlappingInteractionArea(this)) return false;
	const float Now = ServerTime();
	if (Now - LastAttemptServerTime < 0.15f) return false;
	LastAttemptServerTime = Now;
	if (!IsValid(Operator))
	{
		Operator = Character->GetPlayerState<AHeistPlayerState>();
		bOutsideRescue = !ContainsLocation(Character->GetActorLocation());
		OperationOrigin = Character->GetActorLocation();
		RoundStartServerTime = Now + (bOutsideRescue ? 0.0f : 0.5f);
		++Revision;
		GetWorldTimerManager().SetTimer(ValidationTimer, this, &AHeistDetentionDoorActor::ValidateOperation, 0.1f, true);
		ForceNetUpdate();
		MulticastSound(0);
		return true;
	}
	if (!IsOperatorValid() || bOutsideRescue || Now < RoundStartServerTime) return false;
	const float Progress = GetTimingProgress();
	const bool bSuccess = FMath::Abs(Progress - GetSuccessWindowCenter()) <= GetSuccessWindowWidth() * 0.5f;
	if (bSuccess)
	{
		++CompletedLatches;
		MulticastSound(0);
		if (CompletedLatches == 3) { OpenCell(); return true; }
	}
	else
	{
		MulticastSound(1);
		FHeistSoundPingEvent Noise;
		Noise.PingType = EHeistSoundPingType::DetentionLock;
		Noise.SoundPingTag = FHeistGameplayTags::Get().Event_SoundPing_DetentionLock;
		// Investigate from the corridor side, never route a guard through the closed door.
		Noise.WorldLocation = GetActorLocation() + GetActorRightVector() * 100.0f;
		Noise.Radius = FailureNoiseRadius;
		Noise.Duration = 3.0f;
		Noise.bAffectsGuards = true;
		Noise.ServerTimeSeconds = Now;
		GetWorld()->GetGameState<AHeistGameState>()->ReportSoundPing(Noise);
	}
	RoundStartServerTime = Now + 0.5f;
	++Revision;
	ForceNetUpdate();
	return true;
}

bool AHeistDetentionDoorActor::IsOperatorValid() const
{
	const AHeistPlayerCharacter* Character = IsValid(Operator) ? Cast<AHeistPlayerCharacter>(Operator->GetPawn()) : nullptr;
	return IsValid(Character) && CanInteract(Character) && Character->GetInteractionComponent()->IsActorOverlappingInteractionArea(this) &&
		FVector::DistSquared(OperationOrigin, Character->GetActorLocation()) <= FMath::Square(48.0f) &&
		bOutsideRescue != ContainsLocation(Character->GetActorLocation());
}

void AHeistDetentionDoorActor::ValidateOperation()
{
	if (!IsOperatorValid()) { CancelOperation(); return; }
	if (bOutsideRescue && ServerTime() >= RoundStartServerTime + RescueDurationSeconds) OpenCell();
}

void AHeistDetentionDoorActor::ReleaseRescue(AHeistPlayerCharacter* Character)
{
	if (HasAuthority() && bOutsideRescue && IsValid(Character) && Character->GetPlayerState() == Operator) CancelOperation();
}

void AHeistDetentionDoorActor::CancelForPlayer(AHeistPlayerCharacter* Character)
{
	if (HasAuthority() && IsValid(Character) && Character->GetPlayerState() == Operator) CancelOperation();
}

void AHeistDetentionDoorActor::CancelOperation()
{
	GetWorldTimerManager().ClearTimer(ValidationTimer);
	Operator = nullptr;
	bOutsideRescue = false;
	RoundStartServerTime = 0.0f;
	++Revision;
	ForceNetUpdate();
}

void AHeistDetentionDoorActor::OpenCell()
{
	AHeistPlayerState* Rescuer = bOutsideRescue ? Operator.Get() : nullptr;
	bOpen = true;
	CancelOperation();
	ApplyPresentation();
	for (TActorIterator<AHeistPlayerState> It(GetWorld()); It; ++It)
	{
		if (It->GetDetentionDoor() != this) continue;
		if (It->IsArrested()) It->ClearArrested();
		It->SetDetentionDoor(nullptr);
		if (IsValid(Rescuer) && Rescuer != *It) Rescuer->RecordTeammateRescueContribution();
	}
	MulticastSound(2);
	ForceNetUpdate();
}

float AHeistDetentionDoorActor::GetTimingProgress() const
{
	if (!IsValid(Operator)) return 0.0f;
	const float Elapsed = FMath::Max(0.0f, ServerTime() - RoundStartServerTime);
	return bOutsideRescue ? FMath::Clamp(Elapsed / RescueDurationSeconds, 0.0f, 1.0f) : FMath::Fmod(Elapsed / LatchPeriodSeconds, 1.0f);
}

float AHeistDetentionDoorActor::GetSuccessWindowWidth() const
{
	return CompletedLatches == 0 ? 0.28f : CompletedLatches == 1 ? 0.20f : 0.14f;
}

FText AHeistDetentionDoorActor::GetPrompt(const AHeistPlayerCharacter* Character) const
{
	if (IsValid(Operator) && bOutsideRescue) return NSLOCTEXT("HeistDetention", "Rescuing", "[E] 유지 · 철창문 여는 중");
	if (IsValid(Operator)) return FText::Format(NSLOCTEXT("HeistDetention", "Latches", "걸쇠 {0}/3 · 초록 구간에서 [E]"), FText::AsNumber(CompletedLatches));
	return IsValid(Character) && ContainsLocation(Character->GetActorLocation()) ?
		NSLOCTEXT("HeistDetention", "Begin", "[E] 잠금 해제 시작 · 이동하면 중단") : NSLOCTEXT("HeistDetention", "Rescue", "[E] 2초 유지 · 철창문 열기");
}

void AHeistDetentionDoorActor::ApplyPresentation()
{
	DoorBlocker->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	DoorHinge->SetRelativeRotation(FRotator(0,bOpen ? -100.0f : 0.0f,0));
}

void AHeistDetentionDoorActor::OnRep_State()
{
	ApplyPresentation();
}

void AHeistDetentionDoorActor::HandlePhaseChanged(EHeistMatchPhase Previous, EHeistMatchPhase Current)
{
	if (Current != EHeistMatchPhase::InGame) CancelOperation();
}

void AHeistDetentionDoorActor::MulticastSound_Implementation(uint8 Event)
{
	USoundBase* Sound = Event == 1 ? FailureSound.Get() : Event == 2 ? OpenSound.Get() : LatchSound.Get();
	if (IsValid(Sound)) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), 1.0f, 1.0f, 0.0f, SoundAttenuation);
}

void AHeistDetentionDoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHeistDetentionDoorActor,bOpen);
	DOREPLIFETIME(AHeistDetentionDoorActor,CompletedLatches);
	DOREPLIFETIME(AHeistDetentionDoorActor,Revision);
	DOREPLIFETIME(AHeistDetentionDoorActor,Operator);
	DOREPLIFETIME(AHeistDetentionDoorActor,bOutsideRescue);
	DOREPLIFETIME(AHeistDetentionDoorActor,RoundStartServerTime);
}
