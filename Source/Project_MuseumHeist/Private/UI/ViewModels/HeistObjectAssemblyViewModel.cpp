#include "UI/ViewModels/HeistObjectAssemblyViewModel.h"

#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"

namespace
{
int32 WrapSelectionIndex(const int32 CurrentIndex, const int32 Delta, const int32 Count)
{
	if (Count <= 0)
	{
		return INDEX_NONE;
	}

	const int32 SafeCurrentIndex = FMath::Clamp(CurrentIndex, 0, Count - 1);
	return (SafeCurrentIndex + Delta % Count + Count) % Count;
}

FLinearColor ResolveAssemblyAlertColor(const EHeistAlertLevel AlertLevel)
{
	switch (AlertLevel)
	{
	case EHeistAlertLevel::Suspicious:
		return FLinearColor(1.0f, 0.68f, 0.12f);
	case EHeistAlertLevel::Searching:
		return FLinearColor(1.0f, 0.30f, 0.05f);
	case EHeistAlertLevel::Alarmed:
		return FLinearColor(1.0f, 0.04f, 0.02f);
	case EHeistAlertLevel::Lockdown:
		return FLinearColor(0.72f, 0.0f, 0.0f);
	case EHeistAlertLevel::Quiet:
	default:
		return FLinearColor::White;
	}
}
}

void UHeistObjectAssemblyViewModel::BeginDestroy()
{
	if (IsValid(GameState))
	{
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
	}
	if (IsValid(ObjectAssemblyComponent))
	{
		ObjectAssemblyComponent->GetSessionStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

void UHeistObjectAssemblyViewModel::SetupViewModel(AHeistGameState* InGameState, UHeistObjectAssemblyComponent* InObjectAssemblyComponent,
												   AHeistPlayerController* InPlayerController)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
	}
	if (ObjectAssemblyComponent != InObjectAssemblyComponent && IsValid(ObjectAssemblyComponent))
	{
		ObjectAssemblyComponent->GetSessionStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	ObjectAssemblyComponent = InObjectAssemblyComponent;
	PlayerController = InPlayerController;

	if (IsValid(GameState))
	{
		GameState->GetAlertStateChangedDelegate().RemoveAll(this);
		GameState->GetAlertStateChangedDelegate().AddUObject(this, &UHeistObjectAssemblyViewModel::HandleAlertStateChanged);
	}
	if (IsValid(ObjectAssemblyComponent))
	{
		ObjectAssemblyComponent->GetSessionStateChangedDelegate().RemoveAll(this);
		ObjectAssemblyComponent->GetSessionStateChangedDelegate().AddUObject(this, &UHeistObjectAssemblyViewModel::HandleAssemblySessionStateChanged);
	}

	RefreshPresentationState();
}

void UHeistObjectAssemblyViewModel::RefreshPresentationState()
{
	const bool bSessionActive = IsValid(ObjectAssemblyComponent) && ObjectAssemblyComponent->IsSessionActive();
	UE_MVVM_SET_PROPERTY_VALUE(bPresentationVisible, bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(SessionEndServerTime, bSessionActive ? ObjectAssemblyComponent->GetSessionEndServerTime() : 0.0f);

	if (bSessionActive)
	{
		const bool bSessionChanged = LoadedSessionRevision != ObjectAssemblyComponent->GetSessionRevision() ||
			ActiveTemplate.TemplateId != ObjectAssemblyComponent->GetActiveTemplateId();
		if (bSessionChanged)
		{
			ResetLocalAssemblyState();
			ClearLoadedTemplateData();
			LoadedSessionRevision = ObjectAssemblyComponent->GetSessionRevision();
			ObservedPayloadValidationRevision = ObjectAssemblyComponent->GetPayloadValidationRevision();
			bSubmitPending = false;
			UE_MVVM_SET_PROPERTY_VALUE(bDataReady, LoadActiveTemplateData());
			SetStatusMessage(bDataReady ? NSLOCTEXT("HeistObjectAssembly", "SelectPartPrompt", "SELECT A PART AND BUILD THE REPLICA.")
										: NSLOCTEXT("HeistObjectAssembly", "DataUnavailable", "ASSEMBLY DATA IS UNAVAILABLE."));
		}

		const int32 PayloadValidationRevision = ObjectAssemblyComponent->GetPayloadValidationRevision();
		if (PayloadValidationRevision != ObservedPayloadValidationRevision)
		{
			ObservedPayloadValidationRevision = PayloadValidationRevision;
			bSubmitPending = false;
			SetStatusMessage(ObjectAssemblyComponent->WasLastPayloadAccepted()
								 ? NSLOCTEXT("HeistObjectAssembly", "PayloadAccepted", "ASSEMBLY ACCEPTED.")
								 : MakePayloadReasonText(ObjectAssemblyComponent->GetLastPayloadReason()));
		}
	}
	else
	{
		bSubmitPending = false;
		LoadedSessionRevision = INDEX_NONE;
		ObservedPayloadValidationRevision = 0;
		ResetLocalAssemblyState();
		ClearLoadedTemplateData();
		UE_MVVM_SET_PROPERTY_VALUE(bDataReady, false);
		SetStatusMessage(FText::GetEmpty());
	}

	RefreshSelectionPresentation();
	RefreshAlertPresentation();
	PresentationChangedDelegate.Broadcast();
}

FHeistObjectAssemblyPresentationChanged& UHeistObjectAssemblyViewModel::GetPresentationChangedDelegate()
{
	return PresentationChangedDelegate;
}

void UHeistObjectAssemblyViewModel::HandleAssemblySessionStateChanged()
{
	RefreshPresentationState();
}

void UHeistObjectAssemblyViewModel::HandleAlertStateChanged(const EHeistAlertLevel, const EHeistAlertLevel, const int32, const FName)
{
	RefreshAlertPresentation();
	PresentationChangedDelegate.Broadcast();
}

bool UHeistObjectAssemblyViewModel::LoadActiveTemplateData()
{
	if (!IsValid(ObjectAssemblyComponent) || ObjectAssemblyComponent->GetActiveTemplateId().IsNone())
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* TemplateDataTable = IsValid(BalanceData) ? BalanceData->ObjectAssemblyTemplateDataTable.LoadSynchronous() : nullptr;
	UDataTable* PartDataTable = IsValid(BalanceData) ? BalanceData->ObjectAssemblyPartDataTable.LoadSynchronous() : nullptr;
	if (!IsValid(TemplateDataTable) || !IsValid(PartDataTable) ||
		TemplateDataTable->GetRowStruct() != FHeistObjectAssemblyTemplateRow::StaticStruct() ||
		PartDataTable->GetRowStruct() != FHeistObjectAssemblyPartRow::StaticStruct())
	{
		return false;
	}

	const FName TemplateId = ObjectAssemblyComponent->GetActiveTemplateId();
	const FHeistObjectAssemblyTemplateRow* TemplateRow =
		TemplateDataTable->FindRow<FHeistObjectAssemblyTemplateRow>(TemplateId, TEXT("UHeistObjectAssemblyViewModel::LoadActiveTemplateData"), false);
	if (TemplateRow == nullptr || TemplateRow->TemplateId != TemplateId || TemplateRow->FamilyId != ObjectAssemblyComponent->GetActiveFamilyId())
	{
		return false;
	}

	ActiveTemplate = *TemplateRow;
	TArray<FName> ReferencedPartIds;
	ReferencedPartIds.Reserve(1 + ActiveTemplate.RequiredParts.Num() + ActiveTemplate.DecoyPartIds.Num());
	ReferencedPartIds.Add(ActiveTemplate.CorePartId);
	for (const FHeistObjectAssemblyEntry& RequiredPart : ActiveTemplate.RequiredParts)
	{
		ReferencedPartIds.AddUnique(RequiredPart.PartId);
		CandidatePartIds.AddUnique(RequiredPart.PartId);
	}
	for (const FName DecoyPartId : ActiveTemplate.DecoyPartIds)
	{
		ReferencedPartIds.AddUnique(DecoyPartId);
		CandidatePartIds.AddUnique(DecoyPartId);
	}

	for (const FName PartId : ReferencedPartIds)
	{
		const FHeistObjectAssemblyPartRow* PartRow =
			PartDataTable->FindRow<FHeistObjectAssemblyPartRow>(PartId, TEXT("UHeistObjectAssemblyViewModel::LoadActiveTemplateData"), false);
		if (PartRow == nullptr || PartRow->PartId != PartId || PartRow->FamilyId != ActiveTemplate.FamilyId)
		{
			ClearLoadedTemplateData();
			return false;
		}
		PartDefinitions.Add(PartId, *PartRow);
	}

	SelectedPartIndex = CandidatePartIds.IsEmpty() ? INDEX_NONE : 0;
	SyncSelectionFromSelectedPart();
	UE_MVVM_SET_PROPERTY_VALUE(TemplateDisplayText, ActiveTemplate.DisplayName.IsEmpty()
														 ? MakeIdentifierDisplayText(ActiveTemplate.TemplateId)
														 : ActiveTemplate.DisplayName);
	return !CandidatePartIds.IsEmpty() && PartDefinitions.Contains(ActiveTemplate.CorePartId);
}

void UHeistObjectAssemblyViewModel::ClearLoadedTemplateData()
{
	ActiveTemplate = FHeistObjectAssemblyTemplateRow();
	PartDefinitions.Reset();
	CandidatePartIds.Reset();
	SelectedPartIndex = INDEX_NONE;
	SelectedSocketIndex = INDEX_NONE;
	SelectedOrientationIndex = INDEX_NONE;
	SelectedMaterialIndex = INDEX_NONE;
	UE_MVVM_SET_PROPERTY_VALUE(TemplateDisplayText, FText::GetEmpty());
}

void UHeistObjectAssemblyViewModel::ResetLocalAssemblyState()
{
	if (!LocalAssemblyEntries.IsEmpty())
	{
		LocalAssemblyEntries.Reset();
		++LocalPreviewRevision;
	}
	SelectedPartIndex = INDEX_NONE;
	SelectedSocketIndex = INDEX_NONE;
	SelectedOrientationIndex = INDEX_NONE;
	SelectedMaterialIndex = INDEX_NONE;
}

void UHeistObjectAssemblyViewModel::SyncSelectionFromSelectedPart()
{
	const FName SelectedPartId = GetSelectedPartId();
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(SelectedPartId);
	if (PartDefinition == nullptr)
	{
		SelectedSocketIndex = INDEX_NONE;
		SelectedOrientationIndex = INDEX_NONE;
		SelectedMaterialIndex = INDEX_NONE;
		return;
	}

	const FHeistObjectAssemblyEntry* ExistingEntry = FindLocalEntry(SelectedPartId);
	SelectedSocketIndex = ExistingEntry != nullptr ? PartDefinition->CompatibleSocketIds.IndexOfByKey(ExistingEntry->SocketId) : 0;
	SelectedOrientationIndex = ExistingEntry != nullptr ? PartDefinition->AllowedOrientationSteps.IndexOfByKey(ExistingEntry->QuantizedOrientation) : 0;
	SelectedMaterialIndex = ExistingEntry != nullptr ? PartDefinition->AllowedMaterialIds.IndexOfByKey(ExistingEntry->MaterialId) : 0;
	SelectedSocketIndex = PartDefinition->CompatibleSocketIds.IsValidIndex(SelectedSocketIndex) ? SelectedSocketIndex : (PartDefinition->CompatibleSocketIds.IsEmpty() ? INDEX_NONE : 0);
	SelectedOrientationIndex =
		PartDefinition->AllowedOrientationSteps.IsValidIndex(SelectedOrientationIndex) ? SelectedOrientationIndex : (PartDefinition->AllowedOrientationSteps.IsEmpty() ? INDEX_NONE : 0);
	SelectedMaterialIndex =
		PartDefinition->AllowedMaterialIds.IsValidIndex(SelectedMaterialIndex) ? SelectedMaterialIndex : (PartDefinition->AllowedMaterialIds.IsEmpty() ? INDEX_NONE : 0);
}

void UHeistObjectAssemblyViewModel::RefreshSelectionPresentation()
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectedPartText,
							   GetSelectedPartId().IsNone()
								   ? NSLOCTEXT("HeistObjectAssembly", "NoPartSelected", "PART  --")
								   : FText::Format(NSLOCTEXT("HeistObjectAssembly", "PartFormat", "PART  {0}"), MakeIdentifierDisplayText(GetSelectedPartId())));
	UE_MVVM_SET_PROPERTY_VALUE(SelectedSocketText,
							   GetSelectedSocketId().IsNone()
								   ? NSLOCTEXT("HeistObjectAssembly", "NoSocketSelected", "SOCKET  --")
								   : FText::Format(NSLOCTEXT("HeistObjectAssembly", "SocketFormat", "SOCKET  {0}"), MakeIdentifierDisplayText(GetSelectedSocketId())));
	const int32 OrientationDegrees = FMath::RoundToInt(static_cast<float>(GetSelectedOrientation()) * 22.5f);
	UE_MVVM_SET_PROPERTY_VALUE(
		SelectedOrientationText,
		GetSelectedPartId().IsNone()
			? NSLOCTEXT("HeistObjectAssembly", "NoOrientationSelected", "ROTATION  --")
			: FText::Format(NSLOCTEXT("HeistObjectAssembly", "OrientationFormat", "ROTATION  {0}\u00b0"), FText::AsNumber(OrientationDegrees)));
	UE_MVVM_SET_PROPERTY_VALUE(
		PlacementProgressText,
		FText::Format(NSLOCTEXT("HeistObjectAssembly", "PlacementProgressFormat", "REQUIRED PARTS  {0}/{1}  |  PLACED  {2}"), FText::AsNumber(GetPlacedRequiredPartCount()),
					  FText::AsNumber(GetRequiredPartCount()), FText::AsNumber(GetPlacedPartCount())));
}

void UHeistObjectAssemblyViewModel::RefreshAlertPresentation()
{
	const EHeistAlertLevel NewAlertLevel = IsValid(GameState) ? GameState->GetAlertLevel() : EHeistAlertLevel::Quiet;
	const bool bShowWarning = bPresentationVisible && NewAlertLevel != EHeistAlertLevel::Quiet;
	FText NewWarningText;
	if (bShowWarning)
	{
		const int32 SecurityLevel = FMath::Clamp(static_cast<int32>(NewAlertLevel), 0, 4);
		FString SecurityLevelStars;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (Index > 0)
			{
				SecurityLevelStars += TEXT(" ");
			}
			SecurityLevelStars += Index < SecurityLevel ? TEXT("\u2605") : TEXT("\u2606");
		}
		NewWarningText = FText::Format(NSLOCTEXT("HeistObjectAssembly", "SecurityLevelFormat", "SECURITY LEVEL {0}/4  {1}"), FText::AsNumber(SecurityLevel),
									 FText::FromString(SecurityLevelStars));
	}

	const bool bLockdownActive = bPresentationVisible && IsValid(GameState) && GameState->IsLockdownCountdownActive();
	UE_MVVM_SET_PROPERTY_VALUE(AlertLevel, NewAlertLevel);
	UE_MVVM_SET_PROPERTY_VALUE(bDangerWarningVisible, bShowWarning);
	UE_MVVM_SET_PROPERTY_VALUE(DangerWarningText, NewWarningText);
	UE_MVVM_SET_PROPERTY_VALUE(DangerWarningColor, ResolveAssemblyAlertColor(NewAlertLevel));
	UE_MVVM_SET_PROPERTY_VALUE(bLockdownCountdownVisible, bLockdownActive);
	UE_MVVM_SET_PROPERTY_VALUE(LockdownCountdownEndServerTime, bLockdownActive ? GameState->GetAlertNextTransitionServerTime() : 0.0f);
}

void UHeistObjectAssemblyViewModel::SetStatusMessage(const FText& NewStatusText)
{
	UE_MVVM_SET_PROPERTY_VALUE(StatusText, NewStatusText);
}

const FHeistObjectAssemblyPartRow* UHeistObjectAssemblyViewModel::FindPartDefinition(const FName PartId) const
{
	return PartDefinitions.Find(PartId);
}

const FHeistObjectAssemblyEntry* UHeistObjectAssemblyViewModel::FindLocalEntry(const FName PartId) const
{
	return LocalAssemblyEntries.FindByPredicate([PartId](const FHeistObjectAssemblyEntry& Entry) { return Entry.PartId == PartId; });
}

FHeistObjectAssemblyEntry* UHeistObjectAssemblyViewModel::FindMutableLocalEntry(const FName PartId)
{
	return LocalAssemblyEntries.FindByPredicate([PartId](const FHeistObjectAssemblyEntry& Entry) { return Entry.PartId == PartId; });
}

FText UHeistObjectAssemblyViewModel::MakeIdentifierDisplayText(const FName Identifier)
{
	if (Identifier.IsNone())
	{
		return FText::GetEmpty();
	}

	FString DisplayString = Identifier.ToString();
	if (DisplayString.StartsWith(TEXT("Part_")))
	{
		int32 LastSeparatorIndex = INDEX_NONE;
		if (DisplayString.FindLastChar(TEXT('_'), LastSeparatorIndex) && LastSeparatorIndex + 1 < DisplayString.Len())
		{
			DisplayString = DisplayString.Mid(LastSeparatorIndex + 1);
		}
	}
	else if (DisplayString.StartsWith(TEXT("Socket_")))
	{
		DisplayString.RightChopInline(7);
	}

	for (int32 Index = DisplayString.Len() - 1; Index > 0; --Index)
	{
		if (FChar::IsUpper(DisplayString[Index]) && FChar::IsLower(DisplayString[Index - 1]))
		{
			DisplayString.InsertAt(Index, TCHAR(' '));
		}
	}
	DisplayString.ReplaceInline(TEXT("_"), TEXT(" "));
	return FText::FromString(DisplayString.ToUpper());
}

FText UHeistObjectAssemblyViewModel::MakePayloadReasonText(const FName Reason)
{
	if (Reason == FName(TEXT("MissingEntries")))
	{
		return NSLOCTEXT("HeistObjectAssembly", "MissingEntries", "PLACE AT LEAST ONE PART BEFORE SUBMITTING.");
	}
	if (Reason == FName(TEXT("DuplicatePart")) || Reason == FName(TEXT("DuplicateSocket")))
	{
		return NSLOCTEXT("HeistObjectAssembly", "DuplicatePlacement", "EACH PART AND SOCKET CAN ONLY BE USED ONCE.");
	}
	if (Reason == FName(TEXT("SessionRevisionMismatch")) || Reason == FName(TEXT("SessionInactive")))
	{
		return NSLOCTEXT("HeistObjectAssembly", "SessionChanged", "THE ASSEMBLY SESSION CHANGED. TRY AGAIN.");
	}
	if (Reason == FName(TEXT("SubmissionTimeout")))
	{
		return NSLOCTEXT("HeistObjectAssembly", "SubmissionTimeout", "ASSEMBLY TIME EXPIRED.");
	}
	return NSLOCTEXT("HeistObjectAssembly", "PayloadRejected", "THE ASSEMBLY WAS REJECTED. CHECK EACH CONNECTION.");
}

bool UHeistObjectAssemblyViewModel::SelectPreviousPart()
{
	if (!bPresentationVisible || CandidatePartIds.IsEmpty())
	{
		return false;
	}
	SelectedPartIndex = WrapSelectionIndex(SelectedPartIndex, -1, CandidatePartIds.Num());
	SyncSelectionFromSelectedPart();
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::SelectNextPart()
{
	if (!bPresentationVisible || CandidatePartIds.IsEmpty())
	{
		return false;
	}
	SelectedPartIndex = WrapSelectionIndex(SelectedPartIndex, 1, CandidatePartIds.Num());
	SyncSelectionFromSelectedPart();
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::SelectPreviousSocket()
{
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(GetSelectedPartId());
	if (!bPresentationVisible || PartDefinition == nullptr || PartDefinition->CompatibleSocketIds.IsEmpty())
	{
		return false;
	}
	SelectedSocketIndex = WrapSelectionIndex(SelectedSocketIndex, -1, PartDefinition->CompatibleSocketIds.Num());
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::SelectNextSocket()
{
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(GetSelectedPartId());
	if (!bPresentationVisible || PartDefinition == nullptr || PartDefinition->CompatibleSocketIds.IsEmpty())
	{
		return false;
	}
	SelectedSocketIndex = WrapSelectionIndex(SelectedSocketIndex, 1, PartDefinition->CompatibleSocketIds.Num());
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::RotatePrevious()
{
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(GetSelectedPartId());
	if (!bPresentationVisible || PartDefinition == nullptr || PartDefinition->AllowedOrientationSteps.IsEmpty())
	{
		return false;
	}
	SelectedOrientationIndex = WrapSelectionIndex(SelectedOrientationIndex, -1, PartDefinition->AllowedOrientationSteps.Num());
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::RotateNext()
{
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(GetSelectedPartId());
	if (!bPresentationVisible || PartDefinition == nullptr || PartDefinition->AllowedOrientationSteps.IsEmpty())
	{
		return false;
	}
	SelectedOrientationIndex = WrapSelectionIndex(SelectedOrientationIndex, 1, PartDefinition->AllowedOrientationSteps.Num());
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::PlaceOrUpdateSelectedPart()
{
	const FName PartId = GetSelectedPartId();
	const FName SocketId = GetSelectedSocketId();
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(PartId);
	if (!bPresentationVisible || !bDataReady || PartDefinition == nullptr || PartId.IsNone() || SocketId.IsNone())
	{
		SetStatusMessage(NSLOCTEXT("HeistObjectAssembly", "InvalidSelection", "SELECT A VALID PART AND SOCKET."));
		PresentationChangedDelegate.Broadcast();
		return false;
	}

	const bool bSocketOccupied = LocalAssemblyEntries.ContainsByPredicate(
		[PartId, SocketId](const FHeistObjectAssemblyEntry& Entry) { return Entry.PartId != PartId && Entry.SocketId == SocketId; });
	if (bSocketOccupied)
	{
		SetStatusMessage(NSLOCTEXT("HeistObjectAssembly", "SocketOccupied", "THAT SOCKET IS ALREADY OCCUPIED."));
		PresentationChangedDelegate.Broadcast();
		return false;
	}

	FHeistObjectAssemblyEntry NewEntry;
	NewEntry.PartId = PartId;
	NewEntry.SocketId = SocketId;
	NewEntry.QuantizedOrientation = GetSelectedOrientation();
	NewEntry.MaterialId = GetSelectedMaterialId();
	if (FHeistObjectAssemblyEntry* ExistingEntry = FindMutableLocalEntry(PartId))
	{
		*ExistingEntry = NewEntry;
	}
	else
	{
		LocalAssemblyEntries.Add(NewEntry);
	}

	++LocalPreviewRevision;
	SetStatusMessage(FText::Format(NSLOCTEXT("HeistObjectAssembly", "PartSnapped", "{0} SNAPPED TO {1}."), MakeIdentifierDisplayText(PartId),
								   MakeIdentifierDisplayText(SocketId)));
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::RemoveSelectedPart()
{
	const FName PartId = GetSelectedPartId();
	const int32 RemovedCount = LocalAssemblyEntries.RemoveAll([PartId](const FHeistObjectAssemblyEntry& Entry) { return Entry.PartId == PartId; });
	if (RemovedCount <= 0)
	{
		SetStatusMessage(NSLOCTEXT("HeistObjectAssembly", "PartNotPlaced", "THE SELECTED PART IS NOT PLACED."));
		PresentationChangedDelegate.Broadcast();
		return false;
	}

	++LocalPreviewRevision;
	SetStatusMessage(FText::Format(NSLOCTEXT("HeistObjectAssembly", "PartRemoved", "{0} REMOVED."), MakeIdentifierDisplayText(PartId)));
	SyncSelectionFromSelectedPart();
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::ResetLocalAssembly()
{
	if (!bPresentationVisible)
	{
		return false;
	}

	LocalAssemblyEntries.Reset();
	++LocalPreviewRevision;
	SyncSelectionFromSelectedPart();
	SetStatusMessage(NSLOCTEXT("HeistObjectAssembly", "AssemblyReset", "LOCAL ASSEMBLY RESET."));
	RefreshSelectionPresentation();
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::RequestSubmitAssembly()
{
	if (!bPresentationVisible || !bDataReady || bSubmitPending || LocalAssemblyEntries.IsEmpty() || !IsValid(PlayerController))
	{
		if (LocalAssemblyEntries.IsEmpty())
		{
			SetStatusMessage(NSLOCTEXT("HeistObjectAssembly", "PlaceBeforeSubmit", "PLACE AT LEAST ONE PART BEFORE SUBMITTING."));
			PresentationChangedDelegate.Broadcast();
		}
		return false;
	}

	bSubmitPending = true;
	SetStatusMessage(NSLOCTEXT("HeistObjectAssembly", "Submitting", "SUBMITTING ASSEMBLY..."));
	PlayerController->RequestSubmitObjectAssembly(LocalAssemblyEntries, GetSessionRevision());
	PresentationChangedDelegate.Broadcast();
	return true;
}

bool UHeistObjectAssemblyViewModel::RequestCancelAssembly()
{
	if (!bPresentationVisible || !IsValid(PlayerController))
	{
		return false;
	}

	PlayerController->RequestCancelObjectAssembly();
	return true;
}

bool UHeistObjectAssemblyViewModel::IsPresentationVisible() const
{
	return bPresentationVisible;
}

bool UHeistObjectAssemblyViewModel::IsDataReady() const
{
	return bDataReady;
}

bool UHeistObjectAssemblyViewModel::IsOwnerOnlyContractSatisfied() const
{
	const AHeistPlayerCharacter* OwnerCharacter = IsValid(ObjectAssemblyComponent) ? Cast<AHeistPlayerCharacter>(ObjectAssemblyComponent->GetOwner()) : nullptr;
	return IsValid(PlayerController) && PlayerController->IsLocalController() && IsValid(OwnerCharacter) && PlayerController->GetPawn() == OwnerCharacter;
}

bool UHeistObjectAssemblyViewModel::IsSubmitPending() const
{
	return bSubmitPending;
}

float UHeistObjectAssemblyViewModel::GetSessionEndServerTime() const
{
	return SessionEndServerTime;
}

int32 UHeistObjectAssemblyViewModel::GetSessionRevision() const
{
	return IsValid(ObjectAssemblyComponent) ? ObjectAssemblyComponent->GetSessionRevision() : INDEX_NONE;
}

FName UHeistObjectAssemblyViewModel::GetActiveArtifactId() const
{
	return IsValid(ObjectAssemblyComponent) ? ObjectAssemblyComponent->GetActiveArtifactId() : NAME_None;
}

FName UHeistObjectAssemblyViewModel::GetActiveTemplateId() const
{
	return IsValid(ObjectAssemblyComponent) ? ObjectAssemblyComponent->GetActiveTemplateId() : NAME_None;
}

FName UHeistObjectAssemblyViewModel::GetActiveFamilyId() const
{
	return IsValid(ObjectAssemblyComponent) ? ObjectAssemblyComponent->GetActiveFamilyId() : NAME_None;
}

const FHeistObjectAssemblyTemplateRow& UHeistObjectAssemblyViewModel::GetActiveTemplate() const
{
	return ActiveTemplate;
}

const TArray<FName>& UHeistObjectAssemblyViewModel::GetCandidatePartIds() const
{
	return CandidatePartIds;
}

const TArray<FHeistObjectAssemblyEntry>& UHeistObjectAssemblyViewModel::GetLocalAssemblyEntries() const
{
	return LocalAssemblyEntries;
}

FName UHeistObjectAssemblyViewModel::GetSelectedPartId() const
{
	return CandidatePartIds.IsValidIndex(SelectedPartIndex) ? CandidatePartIds[SelectedPartIndex] : NAME_None;
}

FName UHeistObjectAssemblyViewModel::GetSelectedSocketId() const
{
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(GetSelectedPartId());
	return PartDefinition != nullptr && PartDefinition->CompatibleSocketIds.IsValidIndex(SelectedSocketIndex)
			   ? PartDefinition->CompatibleSocketIds[SelectedSocketIndex]
			   : NAME_None;
}

uint8 UHeistObjectAssemblyViewModel::GetSelectedOrientation() const
{
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(GetSelectedPartId());
	return PartDefinition != nullptr && PartDefinition->AllowedOrientationSteps.IsValidIndex(SelectedOrientationIndex)
			   ? PartDefinition->AllowedOrientationSteps[SelectedOrientationIndex]
			   : 0;
}

FName UHeistObjectAssemblyViewModel::GetSelectedMaterialId() const
{
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(GetSelectedPartId());
	return PartDefinition != nullptr && PartDefinition->AllowedMaterialIds.IsValidIndex(SelectedMaterialIndex)
			   ? PartDefinition->AllowedMaterialIds[SelectedMaterialIndex]
			   : NAME_None;
}

int32 UHeistObjectAssemblyViewModel::GetCandidatePartCount() const
{
	return CandidatePartIds.Num();
}

int32 UHeistObjectAssemblyViewModel::GetPlacedPartCount() const
{
	return LocalAssemblyEntries.Num();
}

int32 UHeistObjectAssemblyViewModel::GetRequiredPartCount() const
{
	return ActiveTemplate.RequiredParts.Num();
}

int32 UHeistObjectAssemblyViewModel::GetPlacedRequiredPartCount() const
{
	int32 PlacedRequiredPartCount = 0;
	for (const FHeistObjectAssemblyEntry& RequiredPart : ActiveTemplate.RequiredParts)
	{
		PlacedRequiredPartCount += FindLocalEntry(RequiredPart.PartId) != nullptr ? 1 : 0;
	}
	return PlacedRequiredPartCount;
}

int32 UHeistObjectAssemblyViewModel::GetLocalPreviewRevision() const
{
	return LocalPreviewRevision;
}

UStaticMesh* UHeistObjectAssemblyViewModel::LoadCoreStaticMesh() const
{
	const FHeistObjectAssemblyPartRow* CoreDefinition = FindPartDefinition(ActiveTemplate.CorePartId);
	return CoreDefinition != nullptr ? CoreDefinition->StaticMesh.LoadSynchronous() : nullptr;
}

UStaticMesh* UHeistObjectAssemblyViewModel::LoadPartStaticMesh(const FName PartId) const
{
	const FHeistObjectAssemblyPartRow* PartDefinition = FindPartDefinition(PartId);
	return PartDefinition != nullptr ? PartDefinition->StaticMesh.LoadSynchronous() : nullptr;
}

const FText& UHeistObjectAssemblyViewModel::GetTemplateDisplayText() const
{
	return TemplateDisplayText;
}

const FText& UHeistObjectAssemblyViewModel::GetSelectedPartText() const
{
	return SelectedPartText;
}

const FText& UHeistObjectAssemblyViewModel::GetSelectedSocketText() const
{
	return SelectedSocketText;
}

const FText& UHeistObjectAssemblyViewModel::GetSelectedOrientationText() const
{
	return SelectedOrientationText;
}

const FText& UHeistObjectAssemblyViewModel::GetPlacementProgressText() const
{
	return PlacementProgressText;
}

const FText& UHeistObjectAssemblyViewModel::GetStatusText() const
{
	return StatusText;
}

EHeistAlertLevel UHeistObjectAssemblyViewModel::GetAlertLevel() const
{
	return AlertLevel;
}

bool UHeistObjectAssemblyViewModel::IsDangerWarningVisible() const
{
	return bDangerWarningVisible;
}

const FText& UHeistObjectAssemblyViewModel::GetDangerWarningText() const
{
	return DangerWarningText;
}

FLinearColor UHeistObjectAssemblyViewModel::GetDangerWarningColor() const
{
	return DangerWarningColor;
}

bool UHeistObjectAssemblyViewModel::IsLockdownCountdownVisible() const
{
	return bLockdownCountdownVisible;
}

float UHeistObjectAssemblyViewModel::GetLockdownCountdownEndServerTime() const
{
	return LockdownCountdownEndServerTime;
}
