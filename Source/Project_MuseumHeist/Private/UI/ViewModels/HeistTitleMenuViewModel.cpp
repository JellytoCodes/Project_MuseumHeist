#include "UI/ViewModels/HeistTitleMenuViewModel.h"

#include "Core/HeistGameInstance.h"

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
	const bool bSessionActive = IsValid(GameInstance) && (GameInstance->IsHostingOnlineSession() || GameInstance->IsJoinedOnlineSession());
	const bool bInTitleMenu = IsValid(GameInstance) && GameInstance->IsCurrentWorldTitleMenu();
	const FText NewSessionErrorText = ResolveOnlineSessionFailureText();

	UE_MVVM_SET_PROPERTY_VALUE(SessionStatusText, ResolveOnlineSessionStatusText());
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorText, NewSessionErrorText);
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorVisibility, NewSessionErrorText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestHostSession, bInTitleMenu && !bOperationPending && !bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestJoinSession, bInTitleMenu && !bOperationPending && !bSessionActive);
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
	if (Failure == FName(TEXT("CreateFailed")) || Failure == FName(TEXT("CreateRequestRejected")))
	{
		return NSLOCTEXT("HeistTitleMenu", "CreateFailed", "THE LOBBY COULD NOT BE CREATED.");
	}
	if (Failure == FName(TEXT("FindFailed")) || Failure == FName(TEXT("JoinRequestRejected")) || Failure == FName(TEXT("SessionNotJoinable")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoinFailed", "THE LOBBY COULD NOT BE JOINED.");
	}
	return FText::Format(NSLOCTEXT("HeistTitleMenu", "SessionErrorFormat", "SESSION ERROR: {0}"), FText::FromName(Failure));
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

ESlateVisibility UHeistTitleMenuViewModel::GetSessionErrorVisibility() const
{
	return SessionErrorVisibility;
}

bool UHeistTitleMenuViewModel::CanRequestHostSession() const
{
	return bCanRequestHostSession;
}

bool UHeistTitleMenuViewModel::CanRequestJoinSession() const
{
	return bCanRequestJoinSession;
}

#pragma endregion
