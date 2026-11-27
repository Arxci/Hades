#include "Widgets/ModularBoundActionBar.h"

#include "Algo/Sort.h"
#include "Blueprint/UserWidget.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "CommonUITypes.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/PlatformTime.h"
#include "Input/CommonBoundActionButtonInterface.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionBinding.h"
#include "InputAction.h"

#include "ModularUI.h"

#define LOCTEXT_NAMESPACE "ModularBoundActionBar"

namespace
{
	/**
	 * Whether the player has a key bound to the Enhanced Input action for the device in use: a gamepad's own keys, or else the keyboard's and
	 * the mouse's. A touch screen always has, as Common UI doesn't ask it for keys.
	 */
	bool HasKeyFor(const ULocalPlayer* LocalPlayer, const UInputAction* InputAction, ECommonInputType InputType)
	{
		if (InputType == ECommonInputType::Touch)
		{
			return true;
		}

		UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		if (!EnhancedInput || !InputAction)
		{
			return false;
		}

		const bool bGamepad = InputType == ECommonInputType::Gamepad;
		for (const FKey& Key : EnhancedInput->QueryKeysMappedToAction(InputAction))
		{
			if (Key.IsValid() && !Key.IsTouch() && Key.IsGamepadKey() == bGamepad)
			{
				return true;
			}
		}
		return false;
	}
}

UModularBoundActionBar::UModularBoundActionBar(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#if WITH_EDITOR
const FText UModularBoundActionBar::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Modular UI");
}
#endif

void UModularBoundActionBar::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	if (IsDesignTime())
	{
		return;
	}

	// Built afresh, so its buttons are too, at once, so it's never drawn empty before its first refresh.
	ResetButtons();
	Listen();
	RefreshActions();
}

void UModularBoundActionBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

#if WITH_EDITOR
	if (IsDesignTime())
	{
		RefreshDesignerPreview();
	}
#endif
}

void UModularBoundActionBar::ReleaseSlateResources(bool bReleaseChildren)
{
	CancelQueuedRefresh();
	StopListening();
	Shown.Reset();
	ShownHandles.Reset();

	Super::ReleaseSlateResources(bReleaseChildren);
}

// Listening: the action router says when what's bound changes, Common Input when the device in use does, which changes which actions have a
// key, and Enhanced Input when the player's keys change, such as after rebinding one.
void UModularBoundActionBar::Listen()
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UCommonUIActionRouterBase* Router = LocalPlayer ? LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	if (Router && Router == ListenedRouter.Get() && BoundActionsHandle.IsValid())
	{
		return;
	}

	StopListening();
	if (!Router)
	{
		UE_LOG(LogModularUI, Verbose, TEXT("[Modular UI] %s has no local player's action router yet, so it shows no actions."), *GetPathNameSafe(this));
		return;
	}

	ListenedRouter = Router;
	BoundActionsHandle = Router->OnBoundActionsUpdated().AddUObject(this, &ThisClass::HandleBoundActionsUpdated);
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer))
	{
		InputMethodHandle = Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		EnhancedInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::HandleControlMappingsRebuilt);
	}
}

void UModularBoundActionBar::StopListening()
{
	if (UCommonUIActionRouterBase* Router = ListenedRouter.Get())
	{
		Router->OnBoundActionsUpdated().Remove(BoundActionsHandle);
	}
	ListenedRouter.Reset();
	BoundActionsHandle.Reset();

	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer))
	{
		Input->OnInputMethodChangedNative.Remove(InputMethodHandle);
	}
	InputMethodHandle.Reset();
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		EnhancedInput->ControlMappingsRebuiltDelegate.RemoveAll(this);
	}
}

void UModularBoundActionBar::HandleBoundActionsUpdated()
{
	QueueRefresh();
}

void UModularBoundActionBar::HandleInputMethodChanged(ECommonInputType InputType)
{
	QueueRefresh();
}

void UModularBoundActionBar::HandleControlMappingsRebuilt()
{
	QueueRefresh();
}

// Refreshing: once nothing has changed for Settle Time, from the core ticker, so it works while the game is paused behind a menu, and before
// Slate lays the frame out, so the buttons it adds are laid out with the frame they first show in.
void UModularBoundActionBar::QueueRefresh()
{
	LastChangeTime = FPlatformTime::Seconds();
	if (RefreshHandle.IsValid())
	{
		return;
	}

	RefreshHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		if (FPlatformTime::Seconds() - LastChangeTime < SettleTime)
		{
			return true;
		}

		// Cleared first, as showing the actions can run Blueprint that changes what's bound again, queuing a refresh of its own.
		RefreshHandle = FTSTicker::FDelegateHandle();
		ShowActions(GatherWantedActions());
		return false;
	}));
}

void UModularBoundActionBar::CancelQueuedRefresh()
{
	if (RefreshHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RefreshHandle);
		RefreshHandle = FTSTicker::FDelegateHandle();
	}
}

void UModularBoundActionBar::RefreshActions()
{
	CancelQueuedRefresh();
	if (!IsDesignTime())
	{
		ShowActions(GatherWantedActions());
	}
}

TArray<UModularBoundActionBar::FWantedAction> UModularBoundActionBar::GatherWantedActions() const
{
	TArray<FWantedAction> Wanted;
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UCommonUIActionRouterBase* Router = LocalPlayer ? LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
	if (!Router || !Input)
	{
		return Wanted;
	}

	const ECommonInputType InputType = Input->GetCurrentInputType();
	const FName GamepadName = Input->GetCurrentGamepadName();
	const bool bEnhancedInput = CommonUI::IsEnhancedInputSupportEnabled();

	TSet<FName> Accepted;
	for (const FUIActionBindingHandle& Handle : Router->GatherActiveBindings())
	{
		const TSharedPtr<FUIActionBinding> Binding = FUIActionBinding::FindBinding(Handle);
		if (!Binding || !Binding->bDisplayInActionBar)
		{
			continue;
		}

		// As Common UI's own bar shows them: an Enhanced Input action the player has a key for on the device in use, or a row of an input
		// action table that shows for it. Actions bound only by a UI action tag never show in action bars.
		int32 Priority = 0;
		const UInputAction* InputAction = Binding->InputAction.Get();
		if (bEnhancedInput && InputAction)
		{
			if (!HasKeyFor(LocalPlayer, InputAction, InputType))
			{
				continue;
			}
			// Its Nav Bar Priority is in its Common Input Metadata, as Common UI's own bar reads it.
			const UCommonInputMetadata* Metadata = CommonUI::GetEnhancedInputActionMetadata(InputAction);
			Priority = Metadata ? Metadata->NavBarPriority : 0;
		}
		else if (const FCommonInputActionDataBase* Data = Binding->GetLegacyInputActionData())
		{
			if (!Data->CanDisplayInReflector(InputType, GamepadName))
			{
				continue;
			}
			Priority = Data->NavBarPriority;
		}
		else
		{
			continue;
		}

		bool bAlreadyAccepted = false;
		Accepted.Add(Binding->ActionName, &bAlreadyAccepted);
		if (bAlreadyAccepted && bIgnoreDuplicateActions)
		{
			continue;
		}

		FWantedAction& Action = Wanted.AddDefaulted_GetRef();
		Action.Handle = Handle;
		Action.Key = Binding->ActionName;
		Action.Priority = Priority;
		Action.ShownIndex = Shown.IndexOfByPredicate([Key = Action.Key](const FShownAction& Other) { return Other.Key == Key; });
	}

	// A lower Nav Bar Priority first, as Common UI sorts them. Then the actions shown already, in the order they're shown, so they keep their
	// places, and the rest after them, in the order they were bound.
	Algo::Sort(Wanted, [](const FWantedAction& A, const FWantedAction& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority < B.Priority;
		}
		const bool bAShown = A.ShownIndex != INDEX_NONE;
		const bool bBShown = B.ShownIndex != INDEX_NONE;
		if (bAShown != bBShown)
		{
			return bAShown;
		}
		return bAShown ? A.ShownIndex < B.ShownIndex : GetTypeHash(A.Handle) < GetTypeHash(B.Handle);
	});
	return Wanted;
}

void UModularBoundActionBar::ShowActions(const TArray<FWantedAction>& Wanted)
{
	bool bChanged = false;

	// Actions no longer wanted go, and the others keep their buttons.
	for (int32 Index = Shown.Num() - 1; Index >= 0; --Index)
	{
		const FName Key = Shown[Index].Key;
		if (!Wanted.ContainsByPredicate([Key](const FWantedAction& Action) { return Action.Key == Key; }))
		{
			RemoveButton(Shown[Index]);
			Shown.RemoveAt(Index);
			bChanged = true;
		}
	}

	// From the first place the order differs, buttons are made again, in order. Buttons can only be added at the end, and as the actions
	// shown keep their order, that's usually past the last one shown, so only new actions are added.
	int32 FirstDifferent = 0;
	while (FirstDifferent < Shown.Num() && FirstDifferent < Wanted.Num() && Shown[FirstDifferent].Key == Wanted[FirstDifferent].Key)
	{
		++FirstDifferent;
	}
	for (int32 Index = Shown.Num() - 1; Index >= FirstDifferent; --Index)
	{
		RemoveButton(Shown[Index]);
		Shown.RemoveAt(Index);
		bChanged = true;
	}

	// Those kept show the binding wanted now, such as a menu's Back in place of the screen's under it, which looks the same.
	for (int32 Index = 0; Index < Shown.Num(); ++Index)
	{
		if (!(Shown[Index].Handle == Wanted[Index].Handle))
		{
			Shown[Index].Handle = Wanted[Index].Handle;
			if (ICommonBoundActionButtonInterface* Button = Cast<ICommonBoundActionButtonInterface>(Shown[Index].Button.Get()))
			{
				Button->SetRepresentedAction(Wanted[Index].Handle);
			}
		}
	}

	for (int32 Index = Shown.Num(); Index < Wanted.Num(); ++Index)
	{
		FShownAction& Added = Shown.AddDefaulted_GetRef();
		Added.Handle = Wanted[Index].Handle;
		Added.Key = Wanted[Index].Key;
		Added.Button = MakeButton(Wanted[Index].Handle);
		bChanged = true;
	}

	ShownHandles.Reset(Shown.Num());
	for (const FShownAction& Action : Shown)
	{
		ShownHandles.Add(Action.Handle);
	}
	if (bChanged)
	{
		OnActionsChanged(Shown.Num());
	}
}

UUserWidget* UModularBoundActionBar::MakeButton(const FUIActionBindingHandle& Handle)
{
	if (!ActionButtonClass)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s has no Action Button Class, so it shows no actions."), *GetPathNameSafe(this));
		return nullptr;
	}

	UUserWidget* Button = CreateEntryInternal(ActionButtonClass.Get());
	if (ICommonBoundActionButtonInterface* BoundButton = Cast<ICommonBoundActionButtonInterface>(Button))
	{
		BoundButton->SetRepresentedAction(Handle);
	}
	return Button;
}

void UModularBoundActionBar::RemoveButton(const FShownAction& Action)
{
	if (UUserWidget* Button = Action.Button.Get())
	{
		RemoveEntryInternal(Button);
	}
}

void UModularBoundActionBar::ResetButtons()
{
	ResetInternal();
	Shown.Reset();
	ShownHandles.Reset();
}

#if WITH_EDITOR
void UModularBoundActionBar::RefreshDesignerPreview()
{
	// Built afresh each time, as the designer runs this again whenever the widget changes.
	ResetInternal();
	for (int32 Index = 0; ActionButtonClass && Index < NumDesignerPreviewEntries; ++Index)
	{
		CreateEntryInternal(ActionButtonClass.Get());
	}
}
#endif

#undef LOCTEXT_NAMESPACE
