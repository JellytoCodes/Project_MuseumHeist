#include "UI/ViewModels/HeistTitleMenuViewModel.h"

#include "Core/HeistGameInstance.h"
#include "Core/HeistGameUserSettings.h"

#pragma region Construction

UHeistTitleMenuViewModel::UHeistTitleMenuViewModel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistTitleMenuViewModel::BeginDestroy()
{
	if (IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistTitleMenuViewModel::SetupViewModel(UHeistGameInstance* InGameInstance)
{
	if (GameInstance != InGameInstance && IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
	}

	GameInstance = InGameInstance;
	if (IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
		GameInstance->GetOnlineSessionStateChangedDelegate().AddUObject(this, &UHeistTitleMenuViewModel::HandleOnlineSessionStateChanged);
	}

	RefreshTitleMenuData();
}

void UHeistTitleMenuViewModel::RefreshTitleMenuData()
{
	const bool bOperationPending = IsValid(GameInstance) && GameInstance->IsOnlineSessionOperationPending();
	const bool bSessionActive = IsValid(GameInstance)
		&& (GameInstance->IsHostingOnlineSession() || GameInstance->IsJoinedOnlineSession() || GameInstance->HasActiveNamedOnlineSession());
	const bool bInTitleMenu = IsValid(GameInstance) && GameInstance->IsCurrentWorldTitleMenu();
	const FText NewSessionErrorText = ResolveOnlineSessionFailureText();
	const FText NewSessionActionHintText = ResolveSessionActionHintText();

	UE_MVVM_SET_PROPERTY_VALUE(SessionStatusText, ResolveOnlineSessionStatusText());
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorText, NewSessionErrorText);
	UE_MVVM_SET_PROPERTY_VALUE(SessionActionHintText, NewSessionActionHintText);
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorVisibility, NewSessionErrorText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(SessionActionHintVisibility, NewSessionActionHintText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestHostSession, bInTitleMenu && !bOperationPending && !bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestJoinSession, bInTitleMenu && !bOperationPending && !bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanCancelSessionOperation, bInTitleMenu && IsValid(GameInstance) && GameInstance->CanCancelOnlineSessionOperation());
	UE_MVVM_SET_PROPERTY_VALUE(bCanRetrySessionOperation, bInTitleMenu && IsValid(GameInstance) && GameInstance->CanRetryLastOnlineSessionOperation());
	RefreshSettingsData();
	SnapshotChangedDelegate.Broadcast();
}

FHeistTitleMenuSnapshotChanged& UHeistTitleMenuViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}

bool UHeistTitleMenuViewModel::RequestHostSession()
{
	return IsValid(GameInstance) && bCanRequestHostSession && GameInstance->RequestHostSession();
}

bool UHeistTitleMenuViewModel::RequestJoinSessionByCode(const FString& JoinCode)
{
	return IsValid(GameInstance) && bCanRequestJoinSession && GameInstance->RequestJoinSessionByCode(JoinCode);
}

bool UHeistTitleMenuViewModel::RequestCancelSessionOperation()
{
	return IsValid(GameInstance) && bCanCancelSessionOperation && GameInstance->RequestCancelOnlineSessionOperation();
}

bool UHeistTitleMenuViewModel::RequestRetrySessionOperation()
{
	return IsValid(GameInstance) && bCanRetrySessionOperation && GameInstance->RequestRetryLastOnlineSessionOperation();
}

bool UHeistTitleMenuViewModel::RequestApplySettings(const float FieldOfView, const float MouseSensitivity, const float MasterVolume, const int32 ResolutionWidth,
													const int32 ResolutionHeight, const int32 WindowModeValue)
{
	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!IsValid(Settings))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsUnavailable", "SETTINGS ARE UNAVAILABLE."));
		SnapshotChangedDelegate.Broadcast();
		return false;
	}
	if (ResolutionWidth <= 0 || ResolutionHeight <= 0 || WindowModeValue < static_cast<int32>(EWindowMode::Fullscreen)
		|| WindowModeValue > static_cast<int32>(EWindowMode::Windowed))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsInvalidDisplay", "SELECT A VALID RESOLUTION AND WINDOW MODE."));
		SnapshotChangedDelegate.Broadcast();
		return false;
	}

	Settings->SetFieldOfView(FieldOfView);
	Settings->SetMouseSensitivity(MouseSensitivity);
	Settings->SetMasterVolume(MasterVolume);
	Settings->SetScreenResolution(FIntPoint(ResolutionWidth, ResolutionHeight));
	Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(WindowModeValue));
	Settings->ApplyHeistSettings(false);

	RefreshSettingsData();
	UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsApplied", "SETTINGS SAVED AND APPLIED."));
	SnapshotChangedDelegate.Broadcast();
	return true;
}

bool UHeistTitleMenuViewModel::RequestRestoreDefaultSettings()
{
	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!IsValid(Settings))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsDefaultsUnavailable", "SETTINGS ARE UNAVAILABLE."));
		SnapshotChangedDelegate.Broadcast();
		return false;
	}

	Settings->RestoreHeistDefaults();
	RefreshSettingsData();
	UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsDefaultsApplied", "DEFAULT SETTINGS RESTORED."));
	SnapshotChangedDelegate.Broadcast();
	return true;
}

void UHeistTitleMenuViewModel::GetSupportedSettingsResolutions(TArray<FIntPoint>& OutResolutions) const
{
	UHeistGameUserSettings::GetSupportedScreenResolutions(OutResolutions);
}

void UHeistTitleMenuViewModel::RefreshSettingsData()
{
	const UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(SettingsFieldOfView, Settings->GetFieldOfView());
	UE_MVVM_SET_PROPERTY_VALUE(SettingsMouseSensitivity, Settings->GetMouseSensitivity());
	UE_MVVM_SET_PROPERTY_VALUE(SettingsMasterVolume, Settings->GetMasterVolume());
	UE_MVVM_SET_PROPERTY_VALUE(SettingsScreenResolution, Settings->GetScreenResolution());
	UE_MVVM_SET_PROPERTY_VALUE(SettingsWindowModeValue, static_cast<int32>(Settings->GetFullscreenMode()));
}

void UHeistTitleMenuViewModel::HandleOnlineSessionStateChanged()
{
	RefreshTitleMenuData();
}

FText UHeistTitleMenuViewModel::ResolveOnlineSessionStatusText() const
{
	if (!IsValid(GameInstance))
	{
		return NSLOCTEXT("HeistTitleMenu", "OnlineUnavailable", "ONLINE SERVICE IS UNAVAILABLE.");
	}

	if (GameInstance->IsOnlineSessionCancellationPending())
	{
		return NSLOCTEXT("HeistTitleMenu", "CancellingSessionRequest", "CANCELLING THE SESSION REQUEST...");
	}
	if (GameInstance->IsSessionTravelPending())
	{
		const FName Destination = GameInstance->GetPendingTravelDestination();
		return Destination == FName(TEXT("Gameplay"))
				   ? NSLOCTEXT("HeistTitleMenu", "TravellingToMuseum", "TRAVELLING TO THE MUSEUM...")
				   : NSLOCTEXT("HeistTitleMenu", "TravellingToLobby", "OPENING THE ONLINE LOBBY...");
	}

	const FName State = GameInstance->GetOnlineSessionState();
	if (State == FName(TEXT("Creating")))
	{
		return NSLOCTEXT("HeistTitleMenu", "CreatingSession", "CREATING A PRIVATE LOBBY...");
	}
	if (State == FName(TEXT("Searching")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SearchingSession", "SEARCHING FOR THAT JOIN CODE...");
	}
	if (State == FName(TEXT("Joining")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoiningSession", "JOINING THE LOBBY...");
	}
	if (State == FName(TEXT("Leaving")))
	{
		return NSLOCTEXT("HeistTitleMenu", "LeavingSession", "CLOSING THE CURRENT SESSION...");
	}
	if (State == FName(TEXT("Failed")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SessionFailed", "THE SESSION REQUEST FAILED.");
	}
	if (State == FName(TEXT("Hosting")))
	{
		return NSLOCTEXT("HeistTitleMenu", "HostingBeforeTravel", "THE LOBBY IS CREATED. RETRY THE LOBBY CONNECTION.");
	}
	if (State == FName(TEXT("Joined")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoinedBeforeTravel", "CONNECTED TO THE HOST. WAITING FOR LOBBY TRAVEL.");
	}
	if (!GameInstance->GetLastOnlineSessionFailure().IsNone())
	{
		return NSLOCTEXT("HeistTitleMenu", "SessionRequestEnded", "THE LAST SESSION REQUEST ENDED.");
	}
	return NSLOCTEXT("HeistTitleMenu", "SessionIdle", "HOST A LOBBY OR ENTER A JOIN CODE.");
}

FText UHeistTitleMenuViewModel::ResolveOnlineSessionFailureText() const
{
	if (!IsValid(GameInstance) || GameInstance->GetLastOnlineSessionFailure().IsNone())
	{
		return FText::GetEmpty();
	}

	const FName Failure = GameInstance->GetLastOnlineSessionFailure();
	if (Failure == FName(TEXT("InvalidJoinCode")))
	{
		return NSLOCTEXT("HeistTitleMenu", "InvalidJoinCode", "ENTER A VALID 6-CHARACTER JOIN CODE.");
	}
	if (Failure == FName(TEXT("SessionNotFound")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SessionNotFound", "NO LOBBY WAS FOUND FOR THAT CODE.");
	}
	if (Failure == FName(TEXT("SessionFull")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SessionFull", "THAT LOBBY ALREADY HAS 4 PLAYERS.");
	}
	if (Failure == FName(TEXT("VersionMismatch")))
	{
		return NSLOCTEXT("HeistTitleMenu", "VersionMismatch", "THE HOST IS USING A DIFFERENT GAME BUILD.");
	}
	if (Failure == FName(TEXT("ConnectStringNotResolved")))
	{
		return NSLOCTEXT("HeistTitleMenu", "AddressFailed", "THE HOST ADDRESS COULD NOT BE RESOLVED.");
	}
	if (Failure == FName(TEXT("CreateTimedOut")))
	{
		return NSLOCTEXT("HeistTitleMenu", "CreateTimedOut", "LOBBY CREATION TIMED OUT. SELECT RETRY TO TRY AGAIN.");
	}
	if (Failure == FName(TEXT("FindTimedOut")))
	{
		return NSLOCTEXT("HeistTitleMenu", "FindTimedOut", "THE JOIN CODE SEARCH TIMED OUT. SELECT RETRY TO SEARCH AGAIN.");
	}
	if (Failure == FName(TEXT("JoinTimedOut")) || Failure == FName(TEXT("TravelTimedOut")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoinTimedOut", "CONNECTION TO THE LOBBY TIMED OUT. SELECT RETRY TO CONNECT AGAIN.");
	}
	if (Failure == FName(TEXT("OperationCancelled")))
	{
		return NSLOCTEXT("HeistTitleMenu", "OperationCancelled", "THE SESSION REQUEST WAS CANCELLED. YOU CAN RETRY IT OR START A NEW REQUEST.");
	}
	if (Failure == FName(TEXT("NetworkFailure")) || Failure == FName(TEXT("ConnectionLost")))
	{
		return NSLOCTEXT("HeistTitleMenu", "NetworkFailure", "THE ONLINE CONNECTION WAS LOST. CHECK YOUR NETWORK, THEN SELECT RETRY.");
	}
	if (Failure == FName(TEXT("TravelFailure")) || Failure == FName(TEXT("LobbyTravelRejected")))
	{
		return NSLOCTEXT("HeistTitleMenu", "LobbyTravelFailure", "THE ONLINE LOBBY COULD NOT BE OPENED. SELECT RETRY TO TRY AGAIN.");
	}
	if (Failure == FName(TEXT("OnlineSessionUnavailable")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SubsystemUnavailable", "THE ONLINE SERVICE IS NOT AVAILABLE.");
	}
	if (Failure == FName(TEXT("SessionAlreadyExists")))
	{
		return NSLOCTEXT("HeistTitleMenu", "ExistingSession", "A SESSION IS ALREADY ACTIVE.");
	}
	if (Failure == FName(TEXT("TitleMenuOnly")))
	{
		return NSLOCTEXT("HeistTitleMenu", "TitleMenuOnly", "SESSION ENTRY IS ONLY AVAILABLE FROM THE TITLE MENU.");
	}
	if (Failure == FName(TEXT("ClientCannotHost")))
	{
		return NSLOCTEXT("HeistTitleMenu", "ClientCannotHost", "A CONNECTED CLIENT CANNOT CREATE ANOTHER LOBBY.");
	}
	if (Failure == FName(TEXT("HostQuit")))
	{
		return NSLOCTEXT("HeistTitleMenu", "HostQuit", "THE HOST CLOSED THE SESSION. HOST A NEW LOBBY OR ENTER ANOTHER JOIN CODE.");
	}
	if (Failure == FName(TEXT("CreateFailed")) || Failure == FName(TEXT("CreateRequestRejected")))
	{
		return NSLOCTEXT("HeistTitleMenu", "CreateFailed", "THE LOBBY COULD NOT BE CREATED. SELECT RETRY TO TRY AGAIN.");
	}
	if (Failure == FName(TEXT("FindFailed")) || Failure == FName(TEXT("JoinRequestRejected")) || Failure == FName(TEXT("SessionNotJoinable"))
		|| Failure == FName(TEXT("JoinFailed")) || Failure == FName(TEXT("JoinUnknownError")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoinFailed", "THE LOBBY COULD NOT BE JOINED. VERIFY THE CODE, THEN SELECT RETRY.");
	}
	return NSLOCTEXT("HeistTitleMenu", "GenericSessionFailure", "THE ONLINE SESSION COULD NOT CONTINUE. RETRY THE REQUEST OR START A NEW ONE.");
}

FText UHeistTitleMenuViewModel::ResolveSessionActionHintText() const
{
	if (!IsValid(GameInstance))
	{
		return FText::GetEmpty();
	}
	if (GameInstance->IsOnlineSessionCancellationPending())
	{
		return NSLOCTEXT("HeistTitleMenu", "CancellationPendingHint", "WAITING FOR THE ONLINE SERVICE TO FINISH CLEANUP.");
	}
	if (GameInstance->CanCancelOnlineSessionOperation())
	{
		return NSLOCTEXT("HeistTitleMenu", "CancelAvailableHint", "SELECT CANCEL TO STOP THIS SESSION REQUEST.");
	}
	if (GameInstance->CanRetryLastOnlineSessionOperation())
	{
		return NSLOCTEXT("HeistTitleMenu", "RetryAvailableHint", "SELECT RETRY TO REPEAT THE LAST SESSION REQUEST.");
	}
	if (!GameInstance->GetLastOnlineSessionFailure().IsNone())
	{
		return NSLOCTEXT("HeistTitleMenu", "NewRequestHint", "CHANGE THE JOIN CODE OR START A NEW LOBBY REQUEST.");
	}
	return FText::GetEmpty();
}

#pragma endregion

#pragma region TitleMenuData

const FText& UHeistTitleMenuViewModel::GetSessionStatusText() const
{
	return SessionStatusText;
}

const FText& UHeistTitleMenuViewModel::GetSessionErrorText() const
{
	return SessionErrorText;
}

const FText& UHeistTitleMenuViewModel::GetSessionActionHintText() const
{
	return SessionActionHintText;
}

ESlateVisibility UHeistTitleMenuViewModel::GetSessionErrorVisibility() const
{
	return SessionErrorVisibility;
}

ESlateVisibility UHeistTitleMenuViewModel::GetSessionActionHintVisibility() const
{
	return SessionActionHintVisibility;
}

bool UHeistTitleMenuViewModel::CanRequestHostSession() const
{
	return bCanRequestHostSession;
}

bool UHeistTitleMenuViewModel::CanRequestJoinSession() const
{
	return bCanRequestJoinSession;
}

bool UHeistTitleMenuViewModel::CanCancelSessionOperation() const
{
	return bCanCancelSessionOperation;
}

bool UHeistTitleMenuViewModel::CanRetrySessionOperation() const
{
	return bCanRetrySessionOperation;
}

float UHeistTitleMenuViewModel::GetSettingsFieldOfView() const
{
	return SettingsFieldOfView;
}

float UHeistTitleMenuViewModel::GetSettingsMouseSensitivity() const
{
	return SettingsMouseSensitivity;
}

float UHeistTitleMenuViewModel::GetSettingsMasterVolume() const
{
	return SettingsMasterVolume;
}

FIntPoint UHeistTitleMenuViewModel::GetSettingsScreenResolution() const
{
	return SettingsScreenResolution;
}

int32 UHeistTitleMenuViewModel::GetSettingsWindowModeValue() const
{
	return SettingsWindowModeValue;
}

const FText& UHeistTitleMenuViewModel::GetSettingsStatusText() const
{
	return SettingsStatusText;
}

#pragma endregion
