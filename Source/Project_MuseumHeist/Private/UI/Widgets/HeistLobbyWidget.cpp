#include "UI/Widgets/HeistLobbyWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/ViewModels/HeistLobbyViewModel.h"
#include "View/MVVMView.h"

#pragma region Construction

UHeistLobbyWidget::UHeistLobbyWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(LeaveSessionButton))
	{
		LeaveSessionButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyWidget::HandleLeaveSessionClicked);
	}
	if (IsValid(MapM01Button))
	{
		MapM01Button->OnClicked.AddUniqueDynamic(this, &UHeistLobbyWidget::HandleMapM01Clicked);
	}
	if (IsValid(MapM02Button))
	{
		MapM02Button->OnClicked.AddUniqueDynamic(this, &UHeistLobbyWidget::HandleMapM02Clicked);
	}
	if (IsValid(MapM03Button))
	{
		MapM03Button->OnClicked.AddUniqueDynamic(this, &UHeistLobbyWidget::HandleMapM03Clicked);
	}
	if (IsValid(MapRandomButton))
	{
		MapRandomButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyWidget::HandleMapRandomClicked);
	}
}

void UHeistLobbyWidget::NativeDestruct()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	if (IsValid(LeaveSessionButton))
	{
		LeaveSessionButton->OnClicked.RemoveDynamic(this, &UHeistLobbyWidget::HandleLeaveSessionClicked);
	}
	if (IsValid(MapM01Button))
	{
		MapM01Button->OnClicked.RemoveDynamic(this, &UHeistLobbyWidget::HandleMapM01Clicked);
	}
	if (IsValid(MapM02Button))
	{
		MapM02Button->OnClicked.RemoveDynamic(this, &UHeistLobbyWidget::HandleMapM02Clicked);
	}
	if (IsValid(MapM03Button))
	{
		MapM03Button->OnClicked.RemoveDynamic(this, &UHeistLobbyWidget::HandleMapM03Clicked);
	}
	if (IsValid(MapRandomButton))
	{
		MapRandomButton->OnClicked.RemoveDynamic(this, &UHeistLobbyWidget::HandleMapRandomClicked);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModel

void UHeistLobbyWidget::SetupLobbyWidget(UHeistLobbyViewModel* InLobbyViewModel)
{
	checkf(IsValid(InLobbyViewModel), TEXT("HeistLobbyWidget requires a valid HeistLobbyViewModel"));

	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	LobbyViewModel = InLobbyViewModel;
	LobbyViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	LobbyViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistLobbyWidget::RefreshLobbyPresentation);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(LobbyViewModel);
	ViewModelInterface.SetInterface(LobbyViewModel);
	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}

	RefreshLobbyPresentation();
}

UHeistLobbyViewModel* UHeistLobbyWidget::GetLobbyViewModel() const
{
	return LobbyViewModel;
}

#pragma endregion

#pragma region Presentation

void UHeistLobbyWidget::HandleLeaveSessionClicked()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->RequestLeaveSession();
	}
}

void UHeistLobbyWidget::HandleMapM01Clicked()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->RequestSelectMap(FName(TEXT("M01")));
	}
}

void UHeistLobbyWidget::HandleMapM02Clicked()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->RequestSelectMap(FName(TEXT("M02")));
	}
}

void UHeistLobbyWidget::HandleMapM03Clicked()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->RequestSelectMap(FName(TEXT("M03")));
	}
}

void UHeistLobbyWidget::HandleMapRandomClicked()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->RequestSelectMap(FName(TEXT("Random")));
	}
}

void UHeistLobbyWidget::RefreshLobbyPresentation()
{
	if (!IsValid(LobbyViewModel))
	{
		return;
	}

	if (IsValid(LeaveSessionButton))
	{
		LeaveSessionButton->SetIsEnabled(LobbyViewModel->CanRequestLeaveSession());
	}
	const bool bCanSelectMap = LobbyViewModel->CanSelectMap();
	if (IsValid(MapM01Button))
	{
		MapM01Button->SetIsEnabled(bCanSelectMap);
	}
	if (IsValid(MapM02Button))
	{
		MapM02Button->SetIsEnabled(bCanSelectMap);
	}
	if (IsValid(MapM03Button))
	{
		MapM03Button->SetIsEnabled(bCanSelectMap);
	}
	if (IsValid(MapRandomButton))
	{
		MapRandomButton->SetIsEnabled(bCanSelectMap);
	}
	if (IsValid(JoinCodeText))
	{
		JoinCodeText->SetText(LobbyViewModel->GetJoinCodeText());
		JoinCodeText->SetVisibility(LobbyViewModel->GetJoinCodeVisibility());
	}
	if (IsValid(SessionStatusText))
	{
		SessionStatusText->SetText(LobbyViewModel->GetSessionStatusText());
	}
	if (IsValid(SessionErrorText))
	{
		SessionErrorText->SetText(LobbyViewModel->GetSessionErrorText());
		SessionErrorText->SetVisibility(LobbyViewModel->GetSessionErrorVisibility());
	}
	if (IsValid(SelectedMapText))
	{
		SelectedMapText->SetText(LobbyViewModel->GetSelectedMapText());
	}
	if (IsValid(MapSelectionStatusText))
	{
		MapSelectionStatusText->SetText(LobbyViewModel->GetMapSelectionStatusText());
	}
	if (IsValid(PhaseText))
	{
		PhaseText->SetText(LobbyViewModel->GetPhaseText());
	}
	if (IsValid(PlayerCountText))
	{
		PlayerCountText->SetText(LobbyViewModel->GetPlayerCountText());
	}
	if (IsValid(LocalPlayerText))
	{
		LocalPlayerText->SetText(LobbyViewModel->GetLocalPlayerText());
	}
	if (IsValid(ReadyCountdownText))
	{
		ReadyCountdownText->SetText(LobbyViewModel->GetReadyCountdownText());
	}
	if (IsValid(DefaultLoadoutText))
	{
		DefaultLoadoutText->SetText(LobbyViewModel->GetDefaultLoadoutText());
	}
	if (IsValid(AuthorityBlockerContainer))
	{
		AuthorityBlockerContainer->SetVisibility(LobbyViewModel->GetAuthorityBlockerVisibility());
	}
	if (IsValid(AuthorityBlockerText))
	{
		AuthorityBlockerText->SetText(LobbyViewModel->GetAuthorityBlockerText());
	}
	if (IsValid(PlayerSlot1Text))
	{
		PlayerSlot1Text->SetText(LobbyViewModel->GetPlayerSlot1Text());
	}
	if (IsValid(PlayerSlot2Text))
	{
		PlayerSlot2Text->SetText(LobbyViewModel->GetPlayerSlot2Text());
	}
	if (IsValid(PlayerSlot3Text))
	{
		PlayerSlot3Text->SetText(LobbyViewModel->GetPlayerSlot3Text());
	}
	if (IsValid(PlayerSlot4Text))
	{
		PlayerSlot4Text->SetText(LobbyViewModel->GetPlayerSlot4Text());
	}
}

#pragma endregion
