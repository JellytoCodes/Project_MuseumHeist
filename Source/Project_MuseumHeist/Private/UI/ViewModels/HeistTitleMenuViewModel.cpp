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
		UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsUnavailable", "설정을 사용할 수 없습니다."));
		SnapshotChangedDelegate.Broadcast();
		return false;
	}
	if (ResolutionWidth <= 0 || ResolutionHeight <= 0 || WindowModeValue < static_cast<int32>(EWindowMode::Fullscreen)
		|| WindowModeValue > static_cast<int32>(EWindowMode::Windowed))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsInvalidDisplay", "올바른 해상도와 창 모드를 선택하세요."));
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
	UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsApplied", "설정을 저장하고 적용했습니다."));
	SnapshotChangedDelegate.Broadcast();
	return true;
}

bool UHeistTitleMenuViewModel::RequestRestoreDefaultSettings()
{
	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!IsValid(Settings))
	{
		UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsDefaultsUnavailable", "설정을 사용할 수 없습니다."));
		SnapshotChangedDelegate.Broadcast();
		return false;
	}

	Settings->RestoreHeistDefaults();
	RefreshSettingsData();
	UE_MVVM_SET_PROPERTY_VALUE(SettingsStatusText, NSLOCTEXT("HeistTitleMenu", "SettingsDefaultsApplied", "기본 설정으로 복원했습니다."));
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
		return NSLOCTEXT("HeistTitleMenu", "OnlineUnavailable", "온라인 서비스를 사용할 수 없습니다.");
	}

	if (GameInstance->IsOnlineSessionCancellationPending())
	{
		return NSLOCTEXT("HeistTitleMenu", "CancellingSessionRequest", "세션 요청을 취소하는 중...");
	}
	if (GameInstance->IsSessionTravelPending())
	{
		const FName Destination = GameInstance->GetPendingTravelDestination();
		return Destination == FName(TEXT("Gameplay"))
				   ? NSLOCTEXT("HeistTitleMenu", "TravellingToMuseum", "박물관으로 이동하는 중...")
				   : NSLOCTEXT("HeistTitleMenu", "TravellingToLobby", "온라인 로비를 여는 중...");
	}

	const FName State = GameInstance->GetOnlineSessionState();
	if (State == FName(TEXT("Creating")))
	{
		return NSLOCTEXT("HeistTitleMenu", "CreatingSession", "비공개 로비를 만드는 중...");
	}
	if (State == FName(TEXT("Searching")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SearchingSession", "참가 코드로 로비를 찾는 중...");
	}
	if (State == FName(TEXT("Joining")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoiningSession", "로비에 참가하는 중...");
	}
	if (State == FName(TEXT("Leaving")))
	{
		return NSLOCTEXT("HeistTitleMenu", "LeavingSession", "현재 세션을 닫는 중...");
	}
	if (State == FName(TEXT("Failed")))
	{
		return NSLOCTEXT("HeistTitleMenu", "SessionFailed", "세션 요청에 실패했습니다.");
	}
	if (State == FName(TEXT("Hosting")))
	{
		return NSLOCTEXT("HeistTitleMenu", "HostingBeforeTravel", "로비가 생성되었습니다. 로비 연결을 다시 시도하세요.");
	}
	if (State == FName(TEXT("Joined")))
	{
		return NSLOCTEXT("HeistTitleMenu", "JoinedBeforeTravel", "호스트에 연결되었습니다. 로비 이동을 기다리는 중입니다.");
	}
	if (!GameInstance->GetLastOnlineSessionFailure().IsNone())
	{
		return NSLOCTEXT("HeistTitleMenu", "SessionRequestEnded", "마지막 세션 요청이 종료되었습니다.");
	}
	return NSLOCTEXT("HeistTitleMenu", "SessionIdle", "로비를 만들거나 참가 코드를 입력하세요.");
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

FText UHeistTitleMenuViewModel::ResolveSessionActionHintText() const
{
	if (!IsValid(GameInstance))
	{
		return FText::GetEmpty();
	}
	if (GameInstance->IsOnlineSessionCancellationPending())
	{
		return NSLOCTEXT("HeistTitleMenu", "CancellationPendingHint", "온라인 서비스가 정리를 마칠 때까지 기다리는 중입니다.");
	}
	if (GameInstance->CanCancelOnlineSessionOperation())
	{
		return NSLOCTEXT("HeistTitleMenu", "CancelAvailableHint", "취소를 선택하면 현재 세션 요청을 중단합니다.");
	}
	if (GameInstance->CanRetryLastOnlineSessionOperation())
	{
		return NSLOCTEXT("HeistTitleMenu", "RetryAvailableHint", "다시 시도를 선택하면 마지막 세션 요청을 반복합니다.");
	}
	if (!GameInstance->GetLastOnlineSessionFailure().IsNone())
	{
		return NSLOCTEXT("HeistTitleMenu", "NewRequestHint", "참가 코드를 바꾸거나 새 로비 요청을 시작하세요.");
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
