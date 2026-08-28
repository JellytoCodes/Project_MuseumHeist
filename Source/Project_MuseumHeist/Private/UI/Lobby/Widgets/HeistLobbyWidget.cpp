#include "UI/Lobby/Widgets/HeistLobbyWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UI/Lobby/ViewModels/HeistLobbyViewModel.h"
#include "UI/Lobby/Widgets/HeistLobbyMapCardWidget.h"
#include "UI/Lobby/Widgets/HeistLobbyPlayerCardWidget.h"
#include "View/MVVMView.h"

void UHeistLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(CopyJoinCodeButton))
	{
		CopyJoinCodeButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyWidget::HandleCopyJoinCodeClicked);
	}
	if (IsValid(LeaveSessionButton))
	{
		LeaveSessionButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyWidget::HandleLeaveSessionClicked);
	}
	if (IsValid(StartGameButton))
	{
		StartGameButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyWidget::HandleStartGameClicked);
	}

	ConfigureChildWidgets();
	BindChildWidgetDelegates();
}

void UHeistLobbyWidget::NativeDestruct()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	if (IsValid(CopyJoinCodeButton))
	{
		CopyJoinCodeButton->OnClicked.RemoveDynamic(this, &UHeistLobbyWidget::HandleCopyJoinCodeClicked);
	}
	if (IsValid(LeaveSessionButton))
	{
		LeaveSessionButton->OnClicked.RemoveDynamic(this, &UHeistLobbyWidget::HandleLeaveSessionClicked);
	}
	if (IsValid(StartGameButton))
	{
		StartGameButton->OnClicked.RemoveDynamic(this, &UHeistLobbyWidget::HandleStartGameClicked);
	}
	UnbindChildWidgetDelegates();

	Super::NativeDestruct();
}

void UHeistLobbyWidget::SetupLobbyWidget(UHeistLobbyViewModel* InLobbyViewModel)
{
	checkf(IsValid(InLobbyViewModel), TEXT("HeistLobbyWidget requires a valid HeistLobbyViewModel"));

	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	LobbyViewModel = InLobbyViewModel;
	LobbyViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistLobbyWidget::RefreshLobbyPresentation);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(LobbyViewModel);
	ViewModelInterface.SetInterface(LobbyViewModel);
	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}

	ConfigureChildWidgets();
	BindChildWidgetDelegates();
	RefreshLobbyPresentation();
}

UHeistLobbyViewModel* UHeistLobbyWidget::GetLobbyViewModel() const
{
	return LobbyViewModel;
}

void UHeistLobbyWidget::HandleCopyJoinCodeClicked()
{
	if (IsValid(LobbyViewModel))
	{
		const FString JoinCode = LobbyViewModel->GetJoinCodeText().ToString();
		if (!JoinCode.IsEmpty())
		{
			FPlatformApplicationMisc::ClipboardCopy(*JoinCode);
		}
	}
}

void UHeistLobbyWidget::HandleLeaveSessionClicked()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->RequestLeaveSession();
	}
}

void UHeistLobbyWidget::HandleStartGameClicked()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->RequestStartGame();
	}
}

void UHeistLobbyWidget::HandlePlayerReadyRequested(const int32 PlayerSlot)
{
	if (IsValid(LobbyViewModel) && PlayerSlot == LobbyViewModel->GetLocalPlayerId())
	{
		LobbyViewModel->RequestToggleLocalReady();
	}
}

void UHeistLobbyWidget::HandleMapSelected(const FName MapId)
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->RequestSelectMap(MapId);
	}
}

void UHeistLobbyWidget::ConfigureChildWidgets()
{
	if (IsValid(PlayerCard1))
	{
		PlayerCard1->ConfigurePlayerSlot(1);
	}
	if (IsValid(PlayerCard2))
	{
		PlayerCard2->ConfigurePlayerSlot(2);
	}
	if (IsValid(PlayerCard3))
	{
		PlayerCard3->ConfigurePlayerSlot(3);
	}
	if (IsValid(PlayerCard4))
	{
		PlayerCard4->ConfigurePlayerSlot(4);
	}
	if (IsValid(MapRandomCard))
	{
		MapRandomCard->ConfigureMapCard(FName(TEXT("Random")), NSLOCTEXT("HeistLobby", "RandomMapName", "무작위"));
	}
	if (IsValid(MapM01Card))
	{
		MapM01Card->ConfigureMapCard(FName(TEXT("M01")), NSLOCTEXT("HeistLobby", "MapM01Name", "M01"));
	}
	if (IsValid(MapM02Card))
	{
		MapM02Card->ConfigureMapCard(FName(TEXT("M02")), NSLOCTEXT("HeistLobby", "MapM02Name", "M02"));
	}
	if (IsValid(MapM03Card))
	{
		MapM03Card->ConfigureMapCard(FName(TEXT("M03")), NSLOCTEXT("HeistLobby", "MapM03Name", "M03"));
	}
}

void UHeistLobbyWidget::BindChildWidgetDelegates()
{
	UnbindChildWidgetDelegates();
	for (UHeistLobbyPlayerCardWidget* PlayerCard : {PlayerCard1.Get(), PlayerCard2.Get(), PlayerCard3.Get(), PlayerCard4.Get()})
	{
		if (IsValid(PlayerCard))
		{
			PlayerCard->GetReadyRequestedDelegate().AddUObject(this, &UHeistLobbyWidget::HandlePlayerReadyRequested);
		}
	}
	for (UHeistLobbyMapCardWidget* MapCard : {MapRandomCard.Get(), MapM01Card.Get(), MapM02Card.Get(), MapM03Card.Get()})
	{
		if (IsValid(MapCard))
		{
			MapCard->GetMapSelectedDelegate().AddUObject(this, &UHeistLobbyWidget::HandleMapSelected);
		}
	}
}

void UHeistLobbyWidget::UnbindChildWidgetDelegates()
{
	for (UHeistLobbyPlayerCardWidget* PlayerCard : {PlayerCard1.Get(), PlayerCard2.Get(), PlayerCard3.Get(), PlayerCard4.Get()})
	{
		if (IsValid(PlayerCard))
		{
			PlayerCard->GetReadyRequestedDelegate().RemoveAll(this);
		}
	}
	for (UHeistLobbyMapCardWidget* MapCard : {MapRandomCard.Get(), MapM01Card.Get(), MapM02Card.Get(), MapM03Card.Get()})
	{
		if (IsValid(MapCard))
		{
			MapCard->GetMapSelectedDelegate().RemoveAll(this);
		}
	}
}

void UHeistLobbyWidget::RefreshLobbyPresentation()
{
	if (!IsValid(LobbyViewModel))
	{
		return;
	}

	if (IsValid(JoinCodeText))
	{
		JoinCodeText->SetText(LobbyViewModel->GetJoinCodeText());
	}
	if (IsValid(PlayerCountText))
	{
		PlayerCountText->SetText(LobbyViewModel->GetPlayerCountText());
	}
	if (IsValid(CopyJoinCodeButton))
	{
		CopyJoinCodeButton->SetIsEnabled(!LobbyViewModel->GetJoinCodeText().IsEmpty());
	}
	if (IsValid(LeaveSessionButton))
	{
		LeaveSessionButton->SetIsEnabled(LobbyViewModel->CanRequestLeaveSession());
	}
	if (IsValid(StartGameButton))
	{
		StartGameButton->SetIsEnabled(LobbyViewModel->CanStartGame());
	}

	const bool bCanToggleReady = LobbyViewModel->CanToggleLocalReady();
	int32 PlayerSlot = 1;
	for (UHeistLobbyPlayerCardWidget* PlayerCard : {PlayerCard1.Get(), PlayerCard2.Get(), PlayerCard3.Get(), PlayerCard4.Get()})
	{
		FHeistLobbyPlayerCardData PlayerCardData;
		if (IsValid(PlayerCard) && LobbyViewModel->TryGetPlayerCardData(PlayerSlot, PlayerCardData))
		{
			PlayerCard->ApplyPlayerData(PlayerCardData, bCanToggleReady);
		}
		++PlayerSlot;
	}

	const FName SelectedMapId = LobbyViewModel->GetSelectedMapId();
	const bool bRandomSelection = LobbyViewModel->IsRandomMapSelection();
	const bool bCanSelectMap = LobbyViewModel->CanSelectMap();
	if (IsValid(MapRandomCard))
	{
		MapRandomCard->ApplySelectionState(bRandomSelection, bCanSelectMap);
	}
	for (UHeistLobbyMapCardWidget* MapCard : {MapM01Card.Get(), MapM02Card.Get(), MapM03Card.Get()})
	{
		if (IsValid(MapCard))
		{
			MapCard->ApplySelectionState(!bRandomSelection && MapCard->GetMapId() == SelectedMapId, bCanSelectMap);
		}
	}
}
