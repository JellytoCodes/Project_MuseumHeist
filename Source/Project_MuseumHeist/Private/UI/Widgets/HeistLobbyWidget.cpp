#include "UI/Widgets/HeistLobbyWidget.h"

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

void UHeistLobbyWidget::NativeDestruct()
{
	if (IsValid(LobbyViewModel))
	{
		LobbyViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
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

void UHeistLobbyWidget::RefreshLobbyPresentation()
{
	if (!IsValid(LobbyViewModel))
	{
		return;
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
