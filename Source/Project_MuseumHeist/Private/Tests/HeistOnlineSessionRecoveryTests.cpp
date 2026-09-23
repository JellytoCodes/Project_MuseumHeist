#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Core/HeistGameInstance.h"
#include "Misc/AutomationTest.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistOnlineSessionRecoveryTest, "ProjectMuseumHeist.Session.FailureRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistOnlineSessionRecoveryTest::RunTest(const FString& Parameters)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
	if (!TestNotNull(TEXT("Null subsystem available"), Subsystem)) return false;
	const IOnlineSessionPtr Sessions = Subsystem->GetSessionInterface();
	if (!TestTrue(TEXT("Session interface available"), Sessions.IsValid())) return false;
	UHeistGameInstance* Instance = NewObject<UHeistGameInstance>();
	Instance->OnlineSessionInterface = Sessions;
	Instance->TitleMenuMapPath.Reset(); // No world travel in this isolated backend fault fixture.
	Instance->LocalOnlineSessionName = FName(*FString::Printf(TEXT("HeistRecoveryTest_%s"), *FGuid::NewGuid().ToString()));
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = true;
	Settings.NumPublicConnections = 4;
	auto Begin = [&](const TCHAR* Operation, EOnlineSessionState::Type State)
	{
		TestTrue(TEXT("Create isolated backend fixture"), Sessions->CreateSession(0, Instance->LocalOnlineSessionName, Settings));
		FNamedOnlineSession* Session = Sessions->GetNamedSession(Instance->LocalOnlineSessionName);
		if (!TestNotNull(TEXT("Fixture session exists"), Session)) return;
		Session->SessionState = State;
		Session->SessionInfo.Reset(); // Inject a backend that has not produced a usable address.
		Instance->BeginOnlineSessionOperation(FName(Operation), 20.0f);
		Instance->SetOnlineSessionState(FName(Operation));
	};
	auto Timeout = [&]()
	{
		Instance->ClearOnlineSessionOperationTimeout();
		Instance->HandleOnlineSessionOperationTimeout(0.0f);
	};
	for (const TCHAR* Operation : {TEXT("Create"), TEXT("Join")})
	{
		Begin(Operation, EOnlineSessionState::Creating);
		const FName OldName = Instance->LocalOnlineSessionName;
		Timeout();
		TestFalse(TEXT("Missing completion no longer blocks retry"), Instance->IsOnlineSessionOperationPending());
		TestFalse(TEXT("Old backend session does not occupy retry name"), Instance->HasActiveNamedOnlineSession());
		TestNotEqual(TEXT("Retry isolates old backend finalization"), Instance->LocalOnlineSessionName, OldName);
		Begin(TEXT("Create"), EOnlineSessionState::Creating);
		const FName RetryName = Instance->LocalOnlineSessionName;
		// Simulate the old operation finalizing while a new request is already pending.
		Sessions->GetNamedSession(OldName)->SessionState = EOnlineSessionState::Pending;
		if (FName(Operation) == FName(TEXT("Create"))) Sessions->TriggerOnCreateSessionCompleteDelegates(OldName, true);
		else Sessions->TriggerOnJoinSessionCompleteDelegates(OldName, EOnJoinSessionCompleteResult::Success);
		Instance->HandleCreateSessionComplete(OldName, true);
		Instance->HandleJoinSessionComplete(OldName, EOnJoinSessionCompleteResult::Success);
		TestNull(TEXT("Late completion cleans only retired session"), Sessions->GetNamedSession(OldName));
		TestNotNull(TEXT("Retry backend session survives"), Sessions->GetNamedSession(RetryName));
		TestEqual(TEXT("Retry operation survives stale callbacks"), Instance->GetActiveOnlineSessionOperation(), FName(TEXT("Create")));
		Sessions->RemoveNamedSession(RetryName);
		Instance->ResetOnlineSessionRuntimeState();
	}
	Begin(TEXT("Join"), EOnlineSessionState::Creating);
	TestTrue(TEXT("Cancel accepted"), Instance->RequestCancelOnlineSessionOperation());
	TestTrue(TEXT("Cancelled request keeps bounded recovery watchdog"), Instance->GetOnlineSessionOperationTimeoutRemaining() > 0.0f);
	const FName CancelledName = Instance->LocalOnlineSessionName;
	Timeout();
	TestFalse(TEXT("Missing cancel completion releases UI"), Instance->IsOnlineSessionOperationPending());
	TestEqual(TEXT("Cancellation reason preserved"), Instance->GetLastOnlineSessionFailure(), FName(TEXT("OperationCancelled")));
	Sessions->RemoveNamedSession(CancelledName);
	Sessions->TriggerOnJoinSessionCompleteDelegates(CancelledName, EOnJoinSessionCompleteResult::UnknownError);

	Begin(TEXT("Leave"), EOnlineSessionState::Destroying);
	const FName LeavingName = Instance->LocalOnlineSessionName;
	Timeout();
	TestFalse(TEXT("Missing destroy completion releases UI"), Instance->IsOnlineSessionOperationPending());
	Sessions->RemoveNamedSession(LeavingName);
	Sessions->TriggerOnDestroySessionCompleteDelegates(LeavingName, true);

	Begin(TEXT("MapUpdate"), EOnlineSessionState::Pending);
	Instance->bMapSelectionUpdatePending = true;
	Timeout();
	TestFalse(TEXT("Map update timeout clears pending state"), Instance->IsOnlineSessionOperationPending());
	TestFalse(TEXT("Map update timeout releases active named session"), Instance->HasActiveNamedOnlineSession());

	Begin(TEXT("Join"), EOnlineSessionState::Pending);
	Instance->HandleJoinSessionComplete(Instance->LocalOnlineSessionName, EOnJoinSessionCompleteResult::Success);
	TestFalse(TEXT("Unresolved address cleans successful backend join"), Instance->HasActiveNamedOnlineSession());
	TestFalse(TEXT("Join preparation failure permits another operation"), Instance->IsOnlineSessionOperationPending());
	TestEqual(TEXT("Original preparation failure retained"), Instance->GetLastOnlineSessionFailure(), FName(TEXT("ConnectStringNotResolved")));

	Settings.NumPublicConnections = 4;
	TestTrue(TEXT("Backend can create another session after recovery"), Sessions->CreateSession(0, Instance->LocalOnlineSessionName, Settings));
	FString ResolvedAddress;
	TestTrue(TEXT("Second fixture has a valid backend address"), Sessions->GetResolvedConnectString(Instance->LocalOnlineSessionName, ResolvedAddress));
	Instance->BeginOnlineSessionOperation(FName(TEXT("Join")), 20.0f);
	Instance->HandleJoinSessionComplete(Instance->LocalOnlineSessionName, EOnJoinSessionCompleteResult::Success);
	TestFalse(TEXT("Missing local controller also cleans joined session"), Instance->HasActiveNamedOnlineSession());
	TestEqual(TEXT("Controller failure remains actionable"), Instance->GetLastOnlineSessionFailure(), FName(TEXT("MissingLocalPlayerController")));

	Begin(TEXT("Leave"), EOnlineSessionState::Pending);
	Instance->PendingFailureAfterDestroy = FName(TEXT("ConnectStringNotResolved"));
	const FName FailedDestroyName = Instance->LocalOnlineSessionName;
	Instance->HandleDestroySessionComplete(FailedDestroyName, false);
	TestNull(TEXT("Failed cleanup does not leave a named session"), Sessions->GetNamedSession(FailedDestroyName));
	TestFalse(TEXT("Failed cleanup does not leave an operation pending"), Instance->IsOnlineSessionOperationPending());
	TestEqual(TEXT("Cleanup failure preserves root failure"), Instance->GetLastOnlineSessionFailure(), FName(TEXT("ConnectStringNotResolved")));
	TestEqual(TEXT("All completed retired sessions released"), Instance->RetiredOnlineSessionNames.Num(), 0);
	Instance->Shutdown();
	return true;
}

#endif
