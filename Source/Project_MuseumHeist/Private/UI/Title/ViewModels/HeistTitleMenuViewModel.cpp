#include "UI/Title/ViewModels/HeistTitleMenuViewModel.h"

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
	const bool bSessionActive = IsValid(GameInstance)
		&& (GameInstance->IsHostingOnlineSession() || GameInstance->IsJoinedOnlineSession() || GameInstance->HasActiveNamedOnlineSession());
	const bool bInTitleMenu = IsValid(GameInstance) && GameInstance->IsCurrentWorldTitleMenu();
	const FText NewSessionErrorText = ResolveOnlineSessionFailureText();

	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorText, NewSessionErrorText);
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorVisibility, NewSessionErrorText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestHostSession, bInTitleMenu && !bOperationPending && !bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestJoinSession, bInTitleMenu && !bOperationPending && !bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanCancelSessionOperation, bInTitleMenu && IsValid(GameInstance) && GameInstance->CanCancelOnlineSessionOperation());
	UE_MVVM_SET_PROPERTY_VALUE(bCanRetrySessionOperation, bInTitleMenu && IsValid(GameInstance) && GameInstance->CanRetryLastOnlineSessionOperation());
	UE_MVVM_SET_PROPERTY_VALUE(bSessionOperationPending, bOperationPending);
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

void UHeistTitleMenuViewModel::HandleOnlineSessionStateChanged()
{
	RefreshTitleMenuData();
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
		return NSLOCTEXT("HeistTitleMenu", "InvalidJoinCode", "올바른 6자리 참가 코드를 입력하세요.");
	}
	if (Failure == FName(TEXT("SessionNotFound")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SessionNotFound", "해당 코드의 로비를 찾을 수 없습니다.");
	}
	if (Failure == FName(TEXT("SessionFull")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SessionFull", "해당 로비는 이미 4명으로 가득 찼습니다.");
	}
	if (Failure == FName(TEXT("VersionMismatch")))
	{
		return NSLOCTEXT("HeistTitleMenu", "VersionMismatch", "호스트가 다른 게임 버전을 사용하고 있습니다.");
	}
	if (Failure == FName(TEXT("ConnectStringNotResolved")))
	{
		return NSLOCTEXT("HeistTitleMenu", "AddressFailed", "호스트 주소를 확인할 수 없습니다.");
	}
	if (Failure == FName(TEXT("CreateTimedOut")))
	{
		return NSLOCTEXT("HeistTitleMenu", "CreateTimedOut", "로비 생성 시간이 초과되었습니다. 다시 시도하세요.");
	}
	if (Failure == FName(TEXT("FindTimedOut")))
	{
		return NSLOCTEXT("HeistTitleMenu", "FindTimedOut", "참가 코드 검색 시간이 초과되었습니다. 다시 검색하세요.");
	}
	if (Failure == FName(TEXT("JoinTimedOut")) || Failure == FName(TEXT("TravelTimedOut")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoinTimedOut", "로비 연결 시간이 초과되었습니다. 다시 연결하세요.");
	}
	if (Failure == FName(TEXT("OperationCancelled")))
	{
		return NSLOCTEXT("HeistTitleMenu", "OperationCancelled", "세션 요청이 취소되었습니다. 다시 시도하거나 새 요청을 시작할 수 있습니다.");
	}
	if (Failure == FName(TEXT("NetworkFailure")) || Failure == FName(TEXT("ConnectionLost")))
	{
		return NSLOCTEXT("HeistTitleMenu", "NetworkFailure", "온라인 연결이 끊어졌습니다. 네트워크를 확인한 뒤 다시 시도하세요.");
	}
	if (Failure == FName(TEXT("TravelFailure")) || Failure == FName(TEXT("LobbyTravelRejected")))
	{
		return NSLOCTEXT("HeistTitleMenu", "LobbyTravelFailure", "온라인 로비를 열 수 없습니다. 다시 시도하세요.");
	}
	if (Failure == FName(TEXT("OnlineSessionUnavailable")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SubsystemUnavailable", "온라인 서비스를 사용할 수 없습니다.");
	}
	if (Failure == FName(TEXT("SessionAlreadyExists")))
	{
		return NSLOCTEXT("HeistTitleMenu", "ExistingSession", "이미 세션이 진행 중입니다.");
	}
	if (Failure == FName(TEXT("TitleMenuOnly")))
	{
		return NSLOCTEXT("HeistTitleMenu", "TitleMenuOnly", "세션 입장은 타이틀 메뉴에서만 가능합니다.");
	}
	if (Failure == FName(TEXT("ClientCannotHost")))
	{
		return NSLOCTEXT("HeistTitleMenu", "ClientCannotHost", "접속 중인 클라이언트는 다른 로비를 만들 수 없습니다.");
	}
	if (Failure == FName(TEXT("HostQuit")))
	{
		return NSLOCTEXT("HeistTitleMenu", "HostQuit", "호스트가 세션을 닫았습니다. 새 로비를 만들거나 다른 참가 코드를 입력하세요.");
	}
	if (Failure == FName(TEXT("CreateFailed")) || Failure == FName(TEXT("CreateRequestRejected")))
	{
		return NSLOCTEXT("HeistTitleMenu", "CreateFailed", "로비를 만들 수 없습니다. 다시 시도하세요.");
	}
	if (Failure == FName(TEXT("FindFailed")) || Failure == FName(TEXT("JoinRequestRejected")) || Failure == FName(TEXT("SessionNotJoinable"))
		|| Failure == FName(TEXT("JoinFailed")) || Failure == FName(TEXT("JoinUnknownError")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoinFailed", "로비에 참가할 수 없습니다. 코드를 확인한 뒤 다시 시도하세요.");
	}
	return NSLOCTEXT("HeistTitleMenu", "GenericSessionFailure", "온라인 세션을 계속할 수 없습니다. 다시 시도하거나 새 요청을 시작하세요.");
}

#pragma endregion

#pragma region TitleMenuData

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

bool UHeistTitleMenuViewModel::CanCancelSessionOperation() const
{
	return bCanCancelSessionOperation;
}

bool UHeistTitleMenuViewModel::CanRetrySessionOperation() const
{
	return bCanRetrySessionOperation;
}

bool UHeistTitleMenuViewModel::IsSessionOperationPending() const
{
	return bSessionOperationPending;
}

#pragma endregion
