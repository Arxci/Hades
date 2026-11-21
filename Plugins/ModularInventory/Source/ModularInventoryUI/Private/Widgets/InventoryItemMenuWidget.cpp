#include "Widgets/InventoryItemMenuWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "Sound/SoundBase.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "ModularInventory.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemActionButton.h"
#include "Widgets/InventoryItemWidget.h"
#include "Widgets/InventoryScreenWidget.h"

namespace
{
	FBox2D GetAbsoluteBox(const FGeometry& Geometry)
	{
		return FBox2D(Geometry.GetAbsolutePosition(), Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
	}

	/**
	 * The widget of type T a widget is drawn in, such as its inventory screen or grid: up through its parents, and from the top of each user
	 * widget to the user widget.
	 */
	template <typename T>
	T* FindEnclosing(UWidget* Widget)
	{
		while (Widget)
		{
			if (T* Found = Cast<T>(Widget))
			{
				return Found;
			}
			UWidget* Parent = Widget->GetParent();
			Widget = Parent ? Parent : Widget->GetTypedOuter<UUserWidget>();
		}
		return nullptr;
	}

	/** Top Left moved in as far as it takes for something of Size to fit inside Bounds, or onto their top or left edge where it's bigger. */
	FVector2D KeepIn(const FVector2D& TopLeft, const FVector2D& Size, const FBox2D& Bounds)
	{
		const FVector2D LastTopLeft = (Bounds.Max - Size).ComponentMax(Bounds.Min);
		return FVector2D(FMath::Clamp(TopLeft.X, Bounds.Min.X, LastTopLeft.X), FMath::Clamp(TopLeft.Y, Bounds.Min.Y, LastTopLeft.Y));
	}

	/** The first button that can take focus in Root, skipping whatever is hidden, such as a prompt that isn't asking. */
	UCommonButtonBase* FindFirstButtonIn(UWidget* Root)
	{
		if (!Root || !Root->IsVisible())
		{
			return nullptr;
		}
		if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(Root))
		{
			return Button->GetIsEnabled() ? Button : nullptr;
		}
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (UCommonButtonBase* Found = FindFirstButtonIn(Panel->GetChildAt(Index)))
				{
					return Found;
				}
			}
		}
		return nullptr;
	}

	/** Whether the widget is on the path, such as the path to the widget taking focus. */
	bool IsOnPath(const FWidgetPath& Path, const SWidget& Widget)
	{
		for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
		{
			if (&Path.Widgets[Index].Widget.Get() == &Widget)
			{
				return true;
			}
		}
		return false;
	}

	/** Presses and lets go of the left mouse button where the cursor is, as Common UI does for a gamepad's Accept while its cursor shows. */
	void ClickAtCursor(ULocalPlayer* LocalPlayer)
	{
		const TSharedPtr<FSlateUser> SlateUser = LocalPlayer && FSlateApplication::IsInitialized() ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
		if (!SlateUser)
		{
			return;
		}

		FSlateApplication& SlateApp = FSlateApplication::Get();
		const FVector2D Position = SlateUser->GetCursorPosition();
		const FModifierKeysState Modifiers = SlateApp.GetPlatformApplication()->GetModifierKeys();
		const uint32 UserIndex = static_cast<uint32>(SlateUser->GetUserIndex());

		const FPointerEvent Press(UserIndex, FSlateApplication::CursorPointerIndex, Position, Position, SlateApp.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Modifiers);
		SlateApp.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(), Press);

		const FPointerEvent Release(UserIndex, FSlateApplication::CursorPointerIndex, Position, Position, SlateApp.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Modifiers);
		SlateApp.ProcessMouseButtonUpEvent(Release);
	}

	/**
	 * Moves the cursor nowhere, as Slate does itself when what's under a still cursor changes: widgets no longer under it get Mouse Leave
	 * and stop looking hovered, and the one under it now gets Mouse Enter. Synthetic, so it changes neither the input method nor tooltips.
	 */
	void MoveCursorInPlace(ULocalPlayer* LocalPlayer)
	{
		const TSharedPtr<FSlateUser> SlateUser = LocalPlayer && FSlateApplication::IsInitialized() ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
		if (!SlateUser)
		{
			return;
		}

		FSlateApplication& SlateApp = FSlateApplication::Get();
		const FVector2D Position = SlateUser->GetCursorPosition();
		const FModifierKeysState Modifiers = SlateApp.GetPlatformApplication()->GetModifierKeys();
		const uint32 UserIndex = static_cast<uint32>(SlateUser->GetUserIndex());
		const FPointerEvent Move(UserIndex, FSlateApplication::CursorPointerIndex, Position, Position, SlateApp.GetPressedMouseButtons(), EKeys::Invalid, 0.0f, Modifiers);
		SlateApp.ProcessMouseMoveEvent(Move, /*bIsSynthetic*/ true);
	}
}

UInventoryItemMenuWidget::UInventoryItemMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;

	// The screen's actions stay out of the way while the menu is open, even in a stack inside the screen.
	bIsModal = true;
}

TOptional<FUIInputConfig> UInventoryItemMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(bGameKeepsInput ? ECommonInputMode::All : ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UInventoryItemMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The whole menu fades, a full-screen backdrop included, and Menu Panel moves, as it alone is laid out around the item.
	TransitionPlayer.Init(this, MenuPanel);

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &ThisClass::HandlePromptConfirmed);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &ThisClass::HandlePromptCancelled);
	}
	if (QuantitySlider)
	{
		QuantitySlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleQuantityChanged);
	}

	// The panels show again as the designer left them, or plainly if the prompt was left collapsed.
	if (ActionPanel)
	{
		ActionPanelVisibility = ActionPanel->GetVisibility();
	}
	if (PromptPanel)
	{
		const ESlateVisibility Designed = PromptPanel->GetVisibility();
		const bool bDesignedHidden = Designed == ESlateVisibility::Collapsed || Designed == ESlateVisibility::Hidden;
		PromptPanelVisibility = bDesignedHidden ? ESlateVisibility::SelfHitTestInvisible : Designed;
	}
	HidePrompt();
}

void UInventoryItemMenuWidget::SetItem(FInventoryItemHandle InItem, UInventoryItemWidget* ItemWidget)
{
	Item = InItem;
	AnchorWidget = ItemWidget;
	Anchor = FindAnchor(ItemWidget);
	OpeningCursor = GetCursorPosition();
	if (UInventoryScreenWidget* FoundScreen = FindEnclosing<UInventoryScreenWidget>(ItemWidget))
	{
		BindScreen(FoundScreen);
	}

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->CancelHold();
		Interaction->SetSelectedItem(Item);
	}

	if (ItemNameText)
	{
		const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
		const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Item) : nullptr;
		ItemNameText->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
	}

	HidePrompt();
	RefreshActions();
	NativeOnItemChanged();
	OnItemChanged();

	// Last, so the menu is measured with the buttons this item shows.
	PlaceBeside(Anchor);
	PlaceNextFrame();
}

void UInventoryItemMenuWidget::SetScreen(UInventoryScreenWidget* InScreen)
{
	BindScreen(InScreen);
}

UInventoryScreenWidget* UInventoryItemMenuWidget::GetScreen() const
{
	return Screen.Get();
}

void UInventoryItemMenuWidget::BindScreen(UInventoryScreenWidget* NewScreen)
{
	if (UInventoryScreenWidget* Previous = Screen.Get())
	{
		Previous->OnDeactivated().RemoveAll(this);
	}
	Screen = NewScreen;
	if (NewScreen && IsActivated())
	{
		NewScreen->OnDeactivated().AddUObject(this, &ThisClass::HandleScreenDeactivated);
	}
}

void UInventoryItemMenuWidget::HandleScreenDeactivated()
{
	// The game closed the screen under the menu, such as for a cutscene.
	CloseMenuNow();
}

void UInventoryItemMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	bClosing = false;
	bWaitingForCloseAnimation = false;
	AfterClosing = TFunction<void()>();
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	BindScreen(Screen.Get());
	RefreshConfirm();

	if (OpenSound)
	{
		UGameplayStatics::PlaySound2D(this, OpenSound);
	}

	// Set Item has placed it, so it comes from where it opened. It starts afresh, as a menu is reused from however it last closed.
	TransitionPlayer.PlayIn(OpenTransition);
	StartTransitionTicker();
	if (OpenAnimation)
	{
		PlayAnimation(OpenAnimation);
	}

	// The first entry is highlighted at once, and takes focus once the menu is laid out: Common UI focuses it as the menu activates, but
	// that can be before the menu was ever drawn, when it can't take focus yet. A menu shown again from under another keeps its entry,
	// highlighted afresh so its hint shows again.
	UCommonButtonBase* Entry = Cast<UCommonButtonBase>(GetDesiredFocusTarget());
	ClearHighlight();
	HighlightEntry(Entry, /*bFocus*/ false);
	FocusNextFrame();

	// The menu is reused, and a button it last closed under, or hid under the cursor for a prompt, can still think it's hovered.
	RefreshHover();
}

void UInventoryItemMenuWidget::NativeOnDeactivated()
{
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	RegistryListener.Unbind();
	FTSTicker::GetCoreTicker().RemoveTicker(FocusHandle);
	FocusHandle = FTSTicker::FDelegateHandle();
	FTSTicker::GetCoreTicker().RemoveTicker(PlaceHandle);
	PlaceHandle = FTSTicker::FDelegateHandle();
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.RemoveAll(this);
	}

	// The transition stops where it got to: hidden after a close, and cut short by anything else. The next opening starts it afresh.
	FTSTicker::GetCoreTicker().RemoveTicker(TransitionHandle);
	TransitionHandle = FTSTicker::FDelegateHandle();
	TransitionPlayer.Stop();

	// Closed some other way while closing, such as with its screen: an action waiting for the close doesn't run. Cleared first, so the
	// animation stopping doesn't finish the close again.
	const bool bWasClosing = bClosing;
	const bool bCloseAnimationPlaying = bWaitingForCloseAnimation;
	bClosing = false;
	bWaitingForCloseAnimation = false;
	AfterClosing = TFunction<void()>();
	if (bCloseAnimationPlaying && CloseAnimation)
	{
		StopAnimation(CloseAnimation);
	}

	// The screen says its own hint again. The buttons stay as they are while a layer's transition fades the menu out; Set Item replaces them.
	ShowHint(FText::GetEmpty());
	if (UInventoryScreenWidget* ClosedOn = Screen.Get())
	{
		ClosedOn->OnDeactivated().RemoveAll(this);
	}
	Screen.Reset();

	// Choosing keeps the item selected while another one is picked for it.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && Interaction->GetSelectedItem() == Item)
	{
		Interaction->SetSelectedItem(FInventoryItemHandle());
	}

	// A close that played made its sound as it started.
	if (!bWasClosing)
	{
		PlayCloseSound();
	}

	// The buttons it goes from under stop looking hovered, so they don't come back lit the next time it opens.
	RefreshHover();

	Super::NativeOnDeactivated();
}

UWidget* UInventoryItemMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}

	// While a prompt asks: its button the mouse or focus reached last, or else where it starts: the count to choose, then the safe answer
	// to a confirmation, or the prompt's first button.
	UCommonButtonBase* Highlighted = HighlightedEntry.Get();
	const bool bHighlightUsable = Highlighted && Highlighted->IsVisible() && Highlighted->GetIsEnabled();
	if (IsPrompting())
	{
		if (bHighlightUsable && IsPromptButton(Highlighted))
		{
			return Highlighted;
		}
		if (bPromptQuantity && QuantitySlider && QuantitySlider->IsVisible())
		{
			return QuantitySlider.Get();
		}
		if (bConfirmStartsOnCancel && CancelButton && CancelButton->IsVisible())
		{
			return CancelButton.Get();
		}
		if (UCommonButtonBase* First = FindFirstButtonIn(PromptPanel.Get()))
		{
			return First;
		}
		return ConfirmButton.Get();
	}

	// The highlighted entry, such as the one a prompt was answered for, or else the first.
	if (bHighlightUsable)
	{
		return Highlighted;
	}
	return FindFirstButtonIn(MenuPanel.Get());
}

bool UInventoryItemMenuWidget::NativeOnHandleBackAction()
{
	if (!bIsBackHandler)
	{
		return Super::NativeOnHandleBackAction();
	}

	// Back answers a prompt first, then closes the menu, as the base class does, but after its close transition and Close Animation.
	if (!bClosing && IsPrompting())
	{
		HandlePromptCancelled();
	}
	else if (!bClosing && !BP_OnHandleBackAction())
	{
		CloseMenu();
	}
	return true;
}

UInventoryUISubsystem* UInventoryItemMenuWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

// Closing
void UInventoryItemMenuWidget::CloseMenu()
{
	CloseThen(TFunction<void()>());
}

void UInventoryItemMenuWidget::CloseThen(TFunction<void()>&& AfterClose)
{
	if (bClosing || !IsActivated())
	{
		return;
	}

	// From here the menu takes no more input, and the screen says its own hint again, as the menu starts to go.
	bClosing = true;
	AfterClosing = MoveTemp(AfterClose);
	ShowHint(FText::GetEmpty());
	PlayCloseSound();

	if (bCloseReversesOpen)
	{
		TransitionPlayer.ReverseIn();
	}
	else
	{
		TransitionPlayer.PlayOut(CloseTransition);
	}
	StartTransitionTicker();

	if (CloseAnimation)
	{
		bWaitingForCloseAnimation = true;
		PlayAnimation(CloseAnimation);
	}

	// At once, when there's nothing to play.
	FinishClosingWhenDone();
}

void UInventoryItemMenuWidget::CloseMenuNow()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
}

void UInventoryItemMenuWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (bWaitingForCloseAnimation && Animation && Animation == CloseAnimation.Get())
	{
		bWaitingForCloseAnimation = false;
		FinishClosingWhenDone();
	}
}

void UInventoryItemMenuWidget::FinishClosingWhenDone()
{
	if (bClosing && !TransitionPlayer.IsPlaying() && !bWaitingForCloseAnimation)
	{
		FinishClosing();
	}
}

void UInventoryItemMenuWidget::PlayCloseSound()
{
	if (CloseSound)
	{
		UGameplayStatics::PlaySound2D(this, CloseSound);
	}
}

void UInventoryItemMenuWidget::StartTransitionTicker()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TransitionHandle);
	TransitionHandle = FTSTicker::FDelegateHandle();
	if (!TransitionPlayer.IsPlaying())
	{
		return;
	}

	TransitionHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
	{
		if (TransitionPlayer.Advance(DeltaTime))
		{
			return true;
		}

		// Cleared before the close finishes, as finishing it can open the menu again, with a ticker of its own.
		TransitionHandle = FTSTicker::FDelegateHandle();
		FinishClosingWhenDone();
		return false;
	}));
}

void UInventoryItemMenuWidget::FinishClosing()
{
	// Deactivating clears what was to happen after, so it's taken first.
	TFunction<void()> AfterClose = MoveTemp(AfterClosing);
	AfterClosing = TFunction<void()>();
	DeactivateWidget();
	if (AfterClose)
	{
		AfterClose();
	}
}

void UInventoryItemMenuWidget::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	// The item is gone, such as used up by the game behind the menu, so the menu goes as Back would close it. One already closing, such as
	// for the action that used the item up, carries on.
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	if (Item.IsSet() && Inventory && !Inventory->IsValidItem(Item))
	{
		CloseMenu();
	}
}

// Actions
FInventoryItemActionContext UInventoryItemMenuWidget::GetActionContext() const
{
	FInventoryItemActionContext Context;
	Context.Item = Item;
	Context.Player = GetOwningPlayer();
	Context.Screen = Screen.Get();
	Context.Menu = const_cast<UInventoryItemMenuWidget*>(this);
	return Context;
}

void UInventoryItemMenuWidget::GatherActions(TArray<UInventoryItemAction*>& OutActions) const
{
	if (ActionSet)
	{
		for (UInventoryItemAction* Action : ActionSet->Actions)
		{
			if (Action)
			{
				OutActions.Add(Action);
			}
		}
	}
	for (UInventoryItemAction* Action : Actions)
	{
		if (Action)
		{
			OutActions.Add(Action);
		}
	}
}

void UInventoryItemMenuWidget::RefreshActions()
{
	ClearHighlight();
	ClearActionButtons();

	TArray<UInventoryItemAction*> Gathered;
	GatherActions(Gathered);
	const FInventoryItemActionContext Context = GetActionContext();
	for (UInventoryItemAction* Action : Gathered)
	{
		// An action whose Item Filter leaves the item out isn't for it at all, so it isn't shown even disabled.
		const bool bApplies = Action->AppliesTo(Context);
		const bool bAvailable = bApplies && Action->CanRunFor(Context);
		const bool bShown = bAvailable || (bApplies && Action->WhenUnavailable == EInventoryActionUnavailable::ShowDisabled);

		UCommonButtonBase* Button = nullptr;
		if (!Action->ButtonName.IsNone())
		{
			Button = WidgetTree ? Cast<UCommonButtonBase>(WidgetTree->FindWidget(Action->ButtonName)) : nullptr;
			if (Button)
			{
				Button->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
			else if (bShown)
			{
				UE_LOG(LogInventory, Warning, TEXT("[Inventory UI] %s: no button named %s for the action %s."),
					*GetPathNameSafe(this), *Action->ButtonName.ToString(), *GetNameSafe(Action));
			}
		}
		if (!Button && bShown)
		{
			if (UInventoryItemActionButton* Made = ActionButtonClass && ActionPanel ? CreateWidget<UInventoryItemActionButton>(this, ActionButtonClass) : nullptr)
			{
				ActionPanel->AddChild(Made);
				MadeButtons.Add(Made);
				Button = Made;
			}
			else
			{
				UE_LOG(LogInventory, Warning, TEXT("[Inventory UI] %s: nowhere to list the action %s. Give the menu an Action Panel and an Action Button Class."),
					*GetPathNameSafe(this), *GetNameSafe(Action));
			}
		}
		if (!Button || !bShown)
		{
			continue;
		}

		const FText Reason = bAvailable ? FText::GetEmpty() : Action->GetUnavailableReasonFor(Context);
		if (UInventoryItemActionButton* ActionButton = Cast<UInventoryItemActionButton>(Button))
		{
			ActionButton->SetAction(Action, Action->DisplayName, bAvailable, Reason);
		}
		if (!Action->InputAction.IsNull())
		{
			Button->SetTriggeringInputAction(Action->InputAction);
		}

		// Gamepad focus moves Common UI's hidden cursor onto the button, so hovering covers both.
		const int32 Index = ShownActions.Num();
		Button->OnClicked().AddUObject(this, &ThisClass::HandleActionClicked, Index);
		Button->OnHovered().AddUObject(this, &ThisClass::HandleActionHovered, Index);

		FShownAction& Shown = ShownActions.AddDefaulted_GetRef();
		Shown.Action = Action;
		Shown.Button = Button;
		Shown.bAvailable = bAvailable;
		Shown.UnavailableReason = Reason;
	}

	// Listed again while open: the first entry on screen, not the first listed, is highlighted again. Opening does this itself.
	if (IsActivated())
	{
		HighlightEntry(Cast<UCommonButtonBase>(GetDesiredFocusTarget()), /*bFocus*/ false);
		FocusNextFrame();
	}
}

bool UInventoryItemMenuWidget::CanOpenFor(const FInventoryItemActionContext& Context) const
{
	TArray<UInventoryItemAction*> Gathered;
	GatherActions(Gathered);
	return Gathered.IsEmpty() || Gathered.ContainsByPredicate([&Context](const UInventoryItemAction* Action)
	{
		return Action->AppliesTo(Context) && (Action->WhenUnavailable == EInventoryActionUnavailable::ShowDisabled || Action->CanRunFor(Context));
	});
}

void UInventoryItemMenuWidget::ClearActionButtons()
{
	// The stack reuses menus: buttons made for the last item go, and placed ones forget it.
	for (const FShownAction& Shown : ShownActions)
	{
		if (UCommonButtonBase* Button = Shown.Button.Get())
		{
			Button->OnClicked().RemoveAll(this);
			Button->OnHovered().RemoveAll(this);
		}
	}
	ShownActions.Reset();

	for (UInventoryItemActionButton* Made : MadeButtons)
	{
		if (Made)
		{
			Made->RemoveFromParent();
		}
	}
	MadeButtons.Reset();
}

void UInventoryItemMenuWidget::HandleActionHovered(int32 Index)
{
	// The mouse highlights what it's over, and takes focus there so Accept runs it. On a gamepad, focus alone decides, through Native On
	// Focus Changing: Common UI's hidden cursor follows focus, but where it's left over another button, such as after the menu moved, its
	// hover would highlight that one while focus stays on another.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	if (!bGamepad && ShownActions.IsValidIndex(Index))
	{
		const bool bMouse = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
		HighlightEntry(ShownActions[Index].Button.Get(), bMouse);
	}
}

void UInventoryItemMenuWidget::ShowHintFor(int32 Index)
{
	// Not while prompting or closing: the prompt asks, and a closing menu gives the screen its own hint back.
	const FShownAction* Shown = ShownActions.IsValidIndex(Index) ? &ShownActions[Index] : nullptr;
	const UInventoryItemAction* Action = Shown ? Shown->Action.Get() : nullptr;
	if (Action && !IsPrompting() && !bClosing)
	{
		const bool bShowReason = !Shown->bAvailable && !Shown->UnavailableReason.IsEmpty();
		ShowHint(bShowReason ? Shown->UnavailableReason : Action->Description);
	}
}

// Highlight
void UInventoryItemMenuWidget::HighlightEntry(UCommonButtonBase* Entry, bool bFocus)
{
	if (bClosing || !IsEntry(Entry))
	{
		return;
	}

	UCommonButtonBase* Previous = HighlightedEntry.Get();
	if (Entry == Previous)
	{
		return;
	}
	HighlightedEntry = Entry;

	if (bKeepEntryHighlighted)
	{
		// Only one entry is ever selected: not only the one highlighted before, but any other left selected, such as a prompt's button
		// from the last time this reused menu was open, or one a button style selects as it takes focus.
		ForEachEntry([Entry](UCommonButtonBase& Other)
		{
			if (&Other != Entry && Other.GetSelected())
			{
				Other.ClearSelection();
			}
		});

		// Selected, so its style's Selected look shows it, yet still clickable, and only the menu unselects it.
		Entry->SetIsSelectable(true);
		Entry->SetIsToggleable(false);
		Entry->SetIsInteractableWhenSelected(true);
		Entry->SetIsSelected(true, /*bGiveClickFeedback*/ false);
	}

	const int32 Index = ShownActions.IndexOfByPredicate([Entry](const FShownAction& Shown) { return Shown.Button.Get() == Entry; });
	if (Index != INDEX_NONE)
	{
		ShowHintFor(Index);
	}
	if (bFocus)
	{
		Entry->SetFocus();
	}
}

void UInventoryItemMenuWidget::ClearHighlight()
{
	UCommonButtonBase* Previous = HighlightedEntry.Get();
	if (Previous && bKeepEntryHighlighted)
	{
		Previous->ClearSelection();
	}
	HighlightedEntry.Reset();
}

bool UInventoryItemMenuWidget::IsEntry(UCommonButtonBase* Button) const
{
	return Button && ((MenuPanel && Button->IsChildOf(MenuPanel.Get())) || IsPromptButton(Button));
}

bool UInventoryItemMenuWidget::IsPromptButton(UCommonButtonBase* Button) const
{
	return Button && (Button == ConfirmButton || Button == CancelButton || (PromptPanel && Button->IsChildOf(PromptPanel.Get())));
}

void UInventoryItemMenuWidget::ForEachEntry(TFunctionRef<void(UCommonButtonBase&)> Callback) const
{
	// The prompt's buttons are usually inside Menu Panel, but needn't be, so each button is visited once wherever it is.
	TSet<const UCommonButtonBase*> Visited;
	for (UWidget* Root : { static_cast<UWidget*>(MenuPanel.Get()), static_cast<UWidget*>(PromptPanel.Get()),
		static_cast<UWidget*>(ConfirmButton.Get()), static_cast<UWidget*>(CancelButton.Get()) })
	{
		if (!Root)
		{
			continue;
		}
		UWidgetTree::ForWidgetAndChildren(Root, [&Visited, &Callback](UWidget* Widget)
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget);
			bool bAlreadyVisited = false;
			if (Button)
			{
				Visited.Add(Button, &bAlreadyVisited);
			}
			if (Button && !bAlreadyVisited)
			{
				Callback(*Button);
			}
		});
	}
}

void UInventoryItemMenuWidget::RefreshHover()
{
	// Slate only sends a widget Mouse Leave as the cursor moves, so one hidden or moved from under a still cursor stays hovered. A move in
	// place sends it, on the next frame, once the change has been drawn: what's under the cursor is found from what was drawn last.
	FTSTicker::GetCoreTicker().RemoveTicker(HoverHandle);
	HoverHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		HoverHandle = FTSTicker::FDelegateHandle();
		MoveCursorInPlace(GetOwningLocalPlayer());
		return false;
	}));
}

void UInventoryItemMenuWidget::NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	// Gamepad and keyboard focus highlight the entry they reach, the prompt's buttons included. Focus on anything else, such as Quantity
	// Slider, leaves the highlight where it was.
	UCommonButtonBase* Focused = nullptr;
	ForEachEntry([&Focused, &NewWidgetPath](UCommonButtonBase& Button)
	{
		const TSharedPtr<SWidget> SlateButton = Button.GetCachedWidget();
		if (!Focused && SlateButton && IsOnPath(NewWidgetPath, *SlateButton))
		{
			Focused = &Button;
		}
	});
	HighlightEntry(Focused, /*bFocus*/ false);
}

void UInventoryItemMenuWidget::HandleActionClicked(int32 Index)
{
	if (bClosing || IsPrompting() || !ShownActions.IsValidIndex(Index))
	{
		return;
	}

	// What can run may have changed since the menu opened.
	const FShownAction Shown = ShownActions[Index];
	UInventoryItemAction* Action = Shown.Action.Get();
	const FInventoryItemActionContext Context = GetActionContext();
	if (!Action || !Shown.bAvailable || !Action->CanRunFor(Context))
	{
		const FText Reason = Action ? Action->GetUnavailableReasonFor(Context) : FText::GetEmpty();
		if (!Reason.IsEmpty())
		{
			ShowHint(Reason);
		}
		return;
	}

	if (Action->Prompt != EInventoryActionPrompt::None && PromptPanel && ConfirmButton)
	{
		ShowPrompt(Index);
		return;
	}

	// Without a prompt to ask in, a count is the one the prompt would start at.
	int32 Min = 0;
	int32 Max = 0;
	int32 Count = 0;
	if (Action->Prompt == EInventoryActionPrompt::Quantity)
	{
		Action->GetQuantityRangeFor(Context, Min, Max, Count);
	}
	RunAction(Action, Count);
}

void UInventoryItemMenuWidget::RunAction(UInventoryItemAction* Action, int32 Count)
{
	FInventoryItemActionContext Context = GetActionContext();
	Context.Count = Count;
	const TWeakObjectPtr<UInventoryItemAction> WeakAction = Action;
	TFunction<void()> Run = [WeakAction, Context]()
	{
		// Asked again as it runs, as the close may have taken a while.
		UInventoryItemAction* ToRun = WeakAction.Get();
		if (ToRun && ToRun->CanRunFor(Context))
		{
			ToRun->RunFor(Context);
		}
	};

	if (Action->bKeepsMenuOpen)
	{
		// Listed again for the item as the action left it, with the action still highlighted, unless running it closed the menu, such as by
		// using the item up.
		HidePrompt();
		Run();
		if (IsActivated() && !bClosing)
		{
			RefreshActions();
			PlaceBeside(Anchor);
			if (const FShownAction* Ran = ShownActions.FindByPredicate([&WeakAction](const FShownAction& Shown) { return Shown.Action == WeakAction; }))
			{
				HighlightEntry(Ran->Button.Get(), /*bFocus*/ false);
			}
		}
		return;
	}

	if (ActionTiming == EInventoryMenuActionTiming::AfterMenuCloses)
	{
		// The menu closes first, so the screen has focus again for whatever the action starts, such as choosing another item.
		CloseThen(MoveTemp(Run));
		return;
	}

	// The menu starts to close first, so it takes no more input and the screen's hint is back, and a menu the action opens in the same
	// stack replaces it at once.
	CloseMenu();
	Run();
}

void UInventoryItemMenuWidget::ShowHint(const FText& Hint)
{
	if (DescriptionText)
	{
		DescriptionText->SetText(Hint);
	}
	UInventoryScreenWidget* ShownOn = Screen.Get();
	if (ShownOn && bShowHintOnScreen)
	{
		ShownOn->ShowActionHint(Hint);
	}
}

// Prompt
void UInventoryItemMenuWidget::ShowPrompt(int32 Index)
{
	const UInventoryItemAction* Action = ShownActions.IsValidIndex(Index) ? ShownActions[Index].Action.Get() : nullptr;
	if (!Action || !PromptPanel)
	{
		return;
	}

	PromptIndex = Index;
	PromptEntry = ShownActions[Index].Button;
	bPromptQuantity = Action->Prompt == EInventoryActionPrompt::Quantity;
	int32 Min = 1;
	int32 Max = 1;
	int32 Default = 1;
	if (bPromptQuantity)
	{
		Action->GetQuantityRangeFor(GetActionContext(), Min, Max, Default);
	}
	PromptDefaultCount = Default;
	PromptMaxCount = Max;

	const bool bSlider = bPromptQuantity && QuantitySlider;
	if (QuantitySlider)
	{
		QuantitySlider->SetMinValue(static_cast<float>(Min));
		QuantitySlider->SetMaxValue(static_cast<float>(Max));
		QuantitySlider->SetStepSize(1.0f);
		QuantitySlider->SetValue(static_cast<float>(Default));
		QuantitySlider->SetVisibility(bSlider ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (QuantityText)
	{
		QuantityText->SetVisibility(bPromptQuantity ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshPrompt();

	if (ActionPanel)
	{
		ActionPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	PromptPanel->SetVisibility(PromptPanelVisibility);
	PlaceBeside(Anchor);

	// Where the prompt starts is highlighted at once, as the menu's first entry is as it opens, and takes focus once it's laid out. The
	// action just picked was under the cursor as its panel went, so it's told it no longer is.
	HighlightEntry(Cast<UCommonButtonBase>(GetDesiredFocusTarget()), /*bFocus*/ false);
	FocusNextFrame();
	RefreshHover();
}

void UInventoryItemMenuWidget::HidePrompt()
{
	const bool bWasPrompting = IsPrompting();
	PromptIndex = INDEX_NONE;
	bPromptQuantity = false;
	if (PromptPanel)
	{
		PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ActionPanel)
	{
		ActionPanel->SetVisibility(ActionPanelVisibility);
	}
	if (bWasPrompting && IsActivated())
	{
		PlaceBeside(Anchor);

		// Back on the entry that asked, which takes focus again as the prompt's caller says. The prompt's buttons went from under the
		// cursor, so they're told they no longer are under it.
		HighlightEntry(PromptEntry.Get(), /*bFocus*/ false);
		RefreshHover();
	}
	PromptEntry.Reset();
}

void UInventoryItemMenuWidget::RefreshPrompt()
{
	const UInventoryItemAction* Action = ShownActions.IsValidIndex(PromptIndex) ? ShownActions[PromptIndex].Action.Get() : nullptr;
	if (!Action)
	{
		return;
	}

	FInventoryItemActionContext Context = GetActionContext();
	Context.Count = GetChosenCount();
	if (PromptText)
	{
		PromptText->SetText(Action->FormatText(Action->PromptText, Context));
	}
	if (QuantityText)
	{
		QuantityText->SetText(FText::FormatNamed(QuantityFormat, TEXT("Count"), FText::AsNumber(Context.Count), TEXT("Max"), FText::AsNumber(PromptMaxCount)));
	}
}

int32 UInventoryItemMenuWidget::GetChosenCount() const
{
	if (!bPromptQuantity)
	{
		return 0;
	}
	return QuantitySlider ? FMath::RoundToInt32(QuantitySlider->GetValue()) : PromptDefaultCount;
}

void UInventoryItemMenuWidget::HandleQuantityChanged(float Value)
{
	RefreshPrompt();
}

void UInventoryItemMenuWidget::HandlePromptConfirmed()
{
	UInventoryItemAction* Action = ShownActions.IsValidIndex(PromptIndex) ? ShownActions[PromptIndex].Action.Get() : nullptr;
	if (bClosing)
	{
		return;
	}
	if (!Action)
	{
		HidePrompt();
		return;
	}

	// The prompt stays as the menu closes, rather than the actions coming back as it goes. Set Item hides it for the next item.
	RunAction(Action, GetChosenCount());
}

void UInventoryItemMenuWidget::HandlePromptCancelled()
{
	HidePrompt();
	FocusNextFrame();
}

void UInventoryItemMenuWidget::FocusNextFrame()
{
	FTSTicker::GetCoreTicker().RemoveTicker(FocusHandle);
	FocusHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		FocusHandle = FTSTicker::FDelegateHandle();
		if (IsActivated() && !bClosing)
		{
			if (UWidget* Target = GetDesiredFocusTarget())
			{
				Target->SetFocus();
			}
		}
		return false;
	}));
}

// Placing
FBox2D UInventoryItemMenuWidget::FindAnchor(const UInventoryItemWidget* ItemWidget) const
{
	if (ItemWidget)
	{
		return GetAbsoluteBox(ItemWidget->GetCachedGeometry());
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	if (!SlateUser)
	{
		return FBox2D(ForceInit);
	}

	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
	const TSharedPtr<SWidget> Focused = SlateUser->GetFocusedWidget();
	if (Focused && Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		return GetAbsoluteBox(Focused->GetTickSpaceGeometry());
	}

	const FVector2D CursorPosition = SlateUser->GetCursorPosition();
	return FBox2D(CursorPosition, CursorPosition);
}

void UInventoryItemMenuWidget::PlaceBeside(const FBox2D& Box)
{
	UCanvasPanelSlot* PanelSlot = MenuPanel ? Cast<UCanvasPanelSlot>(MenuPanel->Slot) : nullptr;
	if (!PanelSlot || Placement == EInventoryMenuPlacement::AsDesigned)
	{
		// Not laid out around the item, so transitions From Origin come up from below, and grow from the middle.
		TransitionPlayer.SetOrigin(FVector2D(0.0, -1.0), FVector2D(0.5, 0.5));
		return;
	}

	// In the canvas's own space, where its slots are positioned.
	const FVector2D Min = AbsoluteToCanvas(Box.Min);
	const FVector2D Max = AbsoluteToCanvas(Box.Max);
	const FVector2D ScreenSize = GetCanvasSize();
	const FBox2D ScreenBox(FVector2D::ZeroVector, ScreenSize);
	const FBox2D Inside(FVector2D(ScreenPadding.Left, ScreenPadding.Top), ScreenSize - FVector2D(ScreenPadding.Right, ScreenPadding.Bottom));

	PanelSlot->SetAutoSize(true);
	const FVector2D MenuSize = MeasureMenuPanel();

	// Where the menu goes, by the corner the canvas slot's alignment picks: the top left, unless the menu opens toward the middle.
	FVector2D Alignment = FVector2D::ZeroVector;
	FVector2D Point;

	// For transitions that come From Origin: the way from the item to the menu, and the menu's corner nearest the item.
	FVector2D Away;
	FVector2D Corner;

	const bool bAtCursor = Placement == EInventoryMenuPlacement::AtCursor && OpeningCursor.IsSet();
	const TOptional<FVector2D> CursorPoint = bAtCursor ? TOptional<FVector2D>(AbsoluteToCanvas(OpeningCursor.GetValue())) : TOptional<FVector2D>();
	if (CursorPoint.IsSet())
	{
		// At the cursor while there was one as the menu opened: down and to the right of it, unless only the other side has room.
		const FVector2D At = CursorPoint.GetValue();
		const bool bOpenLeft = At.X + MenuGap + MenuSize.X > Inside.Max.X && At.X - MenuGap - MenuSize.X >= Inside.Min.X;
		const bool bOpenUp = At.Y + MenuGap + MenuSize.Y > Inside.Max.Y && At.Y - MenuGap - MenuSize.Y >= Inside.Min.Y;
		Point = FVector2D(bOpenLeft ? At.X - MenuGap - MenuSize.X : At.X + MenuGap, bOpenUp ? At.Y - MenuGap - MenuSize.Y : At.Y + MenuGap);

		// Dropping down from the cursor, or up where it opened above it, as desktop menus do, from the corner at the cursor.
		Away = FVector2D(0.0, bOpenUp ? -1.0 : 1.0);
		Corner = FVector2D(bOpenLeft ? 1.0 : 0.0, bOpenUp ? 1.0 : 0.0);
	}
	else if (Placement == EInventoryMenuPlacement::TowardCenter || Placement == EInventoryMenuPlacement::AtCursor)
	{
		// Toward the middle, as At Cursor does without the mouse too, such as on a gamepad. Placed by the corner nearest the item, so the menu
		// keeps its edge level with the item's as it changes size.
		const FVector2D Middle = Inside.GetCenter();
		const bool bOpenLeft = Max.X > Middle.X;
		const bool bOpenUp = Max.Y > Middle.Y;
		Alignment = FVector2D(bOpenLeft ? 1.0 : 0.0, bOpenUp ? 1.0 : 0.0);
		Point = FVector2D(bOpenLeft ? Min.X - MenuGap : Max.X + MenuGap, bOpenUp ? Max.Y : Min.Y);

		// Out of the item's side, from the corner level with its edge.
		Away = FVector2D(bOpenLeft ? -1.0 : 1.0, 0.0);
		Corner = Alignment;
	}
	else
	{
		// On the side asked for, unless only the other side has room. Level with the item's top.
		const bool bRoomRight = Max.X + MenuGap + MenuSize.X <= Inside.Max.X;
		const bool bRoomLeft = Min.X - MenuGap - MenuSize.X >= Inside.Min.X;
		const bool bOpenLeft = Placement == EInventoryMenuPlacement::Left ? bRoomLeft || !bRoomRight : !bRoomRight && bRoomLeft;
		Point = FVector2D(bOpenLeft ? Min.X - MenuGap - MenuSize.X : Max.X + MenuGap, Min.Y);

		// Out of the item's side, from the top corner nearest it.
		Away = FVector2D(bOpenLeft ? -1.0 : 1.0, 0.0);
		Corner = FVector2D(bOpenLeft ? 1.0 : 0.0, 0.0);
	}

	// A negative padding can't push the menu off screen.
	FVector2D TopLeft = Point - Alignment * MenuSize;
	TopLeft = KeepIn(TopLeft, MenuSize, Inside);
	TopLeft = KeepIn(TopLeft, MenuSize, ScreenBox);

	PanelSlot->SetAlignment(Alignment);
	PanelSlot->SetPosition(TopLeft + Alignment * MenuSize);
	TransitionPlayer.SetOrigin(Away, Corner);
}

void UInventoryItemMenuWidget::PlaceNextFrame()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PlaceHandle);
	PlaceHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		PlaceHandle = FTSTicker::FDelegateHandle();
		if (IsActivated() && !bClosing)
		{
			PlaceBeside(Anchor);
		}
		return false;
	}));
}

bool UInventoryItemMenuWidget::GetCanvasGeometry(FGeometry& OutGeometry) const
{
	const UWidget* Canvas = MenuPanel ? MenuPanel->GetParent() : nullptr;
	if (!Canvas)
	{
		return false;
	}

	OutGeometry = Canvas->GetCachedGeometry();
	const FVector2D Size = OutGeometry.GetLocalSize();
	return Size.X > 0.0 && Size.Y > 0.0;
}

FVector2D UInventoryItemMenuWidget::AbsoluteToCanvas(const FVector2D& AbsolutePoint)
{
	FGeometry CanvasGeometry;
	if (GetCanvasGeometry(CanvasGeometry))
	{
		return CanvasGeometry.AbsoluteToLocal(AbsolutePoint);
	}

	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, AbsolutePoint, PixelPosition, ViewportPosition);
	return ViewportPosition;
}

FVector2D UInventoryItemMenuWidget::GetCanvasSize()
{
	FGeometry CanvasGeometry;
	if (GetCanvasGeometry(CanvasGeometry))
	{
		return CanvasGeometry.GetLocalSize();
	}
	return UWidgetLayoutLibrary::GetViewportSize(this) / UWidgetLayoutLibrary::GetViewportScale(this);
}

TOptional<FVector2D> UInventoryItemMenuWidget::GetCursorPosition() const
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
	if (!SlateUser || !Input || Input->GetCurrentInputType() != ECommonInputType::MouseAndKeyboard)
	{
		return TOptional<FVector2D>();
	}
	return TOptional<FVector2D>(FVector2D(SlateUser->GetCursorPosition()));
}

FVector2D UInventoryItemMenuWidget::MeasureMenuPanel()
{
	// Pushed menus are placed before they're first drawn, so their size isn't known yet without a layout pass.
	TakeWidget();
	ForceLayoutPrepass();
	return MenuPanel ? MenuPanel->GetDesiredSize() : FVector2D::ZeroVector;
}

// Mouse: a right press on the menu closes it, as Back does, with Close On Right Press. A press outside Menu Panel closes it too, and with
// Pass Through it goes on to the item under it, as if the menu weren't open. A left press then waits, as the item would, to see whether
// it ends in a click or a drag.
// The screen may open another menu as the press reaches an item, perhaps this same one again, so nothing here is touched after that.
FReply UInventoryItemMenuWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The menu sees a press before its buttons do, so while it's closing, they can't be pressed or picked either.
	if (bClosing)
	{
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryItemMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	if (bClosing)
	{
		return FReply::Handled();
	}

	const FKey Button = InMouseEvent.GetEffectingButton();
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	if (MenuPanel && MenuPanel->GetCachedGeometry().IsUnderLocation(Position))
	{
		if (Button == EKeys::RightMouseButton && bCloseOnRightPress)
		{
			CloseMenu();
			return FReply::Handled();
		}
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const bool bLeft = Button == EKeys::LeftMouseButton;
	const bool bRight = Button == EKeys::RightMouseButton;
	const bool bPassThrough = (bLeft && LeftPressOutside == EInventoryMenuOutsidePress::PassThrough)
		|| (bRight && RightPressOutside == EInventoryMenuOutsidePress::PassThrough);
	UInventoryItemWidget* ItemWidget = bPassThrough ? FindItemWidgetUnder(Position) : nullptr;
	if (ItemWidget && bLeft)
	{
		PressedItemWidget = ItemWidget;
		PressedItem = ItemWidget->GetItem();
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	// A right press reaches the item at once, as it does without a menu. The menu covered the item, so the item takes the hover first.
	UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bRightClickItem = ItemWidget && Interaction && !WouldReopen(ItemWidget, Interaction->GetStyle().RightClick);
	if (!bRightClickItem)
	{
		CloseMenu();
		return FReply::Handled();
	}

	CloseMenuNow();
	Interaction->SetHoveredItem(ItemWidget->GetItem());
	ItemWidget->RightClick(InMouseEvent);
	return FReply::Handled();
}

FReply UInventoryItemMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryItemWidget* ItemWidget = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? TakePressedItemWidget() : nullptr;
	if (!ItemWidget || bClosing)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	// The press closes the menu, and is a click on the item if it's let go over it, as a click must start and end on the item. A click that
	// would only open a menu for this menu's item again leaves it closed. The menu covered the item, so the item takes the hover first.
	UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bClicked = Interaction && ItemWidget->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& !WouldReopen(ItemWidget, Interaction->GetStyle().LeftClick);
	CloseMenuNow();
	if (bClicked)
	{
		Interaction->SetHoveredItem(ItemWidget->GetItem());
		ItemWidget->Click(InMouseEvent);
	}
	return FReply::Handled();
}

FReply UInventoryItemMenuWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bClosing)
	{
		return FReply::Handled();
	}

	// The second click of a double-click on the menu's own item, whose first click opened the menu: the double-click is the item's.
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	const bool bOnMenu = MenuPanel && MenuPanel->GetCachedGeometry().IsUnderLocation(Position);
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bDoubleClicks = Interaction && Interaction->GetStyle().DoubleClick != EInventoryItemPress::None;
	UInventoryItemWidget* ItemWidget = !bOnMenu && bDoubleClicks && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FindItemWidgetUnder(Position) : nullptr;
	if (ItemWidget && ItemWidget == AnchorWidget.Get())
	{
		CloseMenuNow();
		ItemWidget->DoubleClick(InMouseEvent);
		return FReply::Handled();
	}

	// Otherwise it's a press like any other.
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventoryItemMenuWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	// The press became a drag of the item it was on, so it won't be a click: the drag picks the item up, as it does without a menu. The menu
	// closes either way, even when the item can't be picked up, as any press outside it does.
	UInventoryItemWidget* ItemWidget = TakePressedItemWidget();
	if (!ItemWidget || bClosing)
	{
		return;
	}

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SetHoveredItem(ItemWidget->GetItem());
	}
	OutOperation = ItemWidget->BeginMouseDrag(InMouseEvent);
	CloseMenuNow();
}

UInventoryItemWidget* UInventoryItemMenuWidget::FindItemWidgetUnder(const FVector2D& ScreenPosition) const
{
	// A pooled item widget may show another item by now.
	UInventoryItemWidget* AnchorItemWidget = AnchorWidget.Get();
	if (AnchorItemWidget && AnchorItemWidget->GetItem() == Item && Anchor.IsInside(ScreenPosition))
	{
		return AnchorItemWidget;
	}

	const UInventoryScreenWidget* ShownOn = Screen.Get();
	return ShownOn ? ShownOn->FindItemWidgetAt(ScreenPosition) : nullptr;
}

bool UInventoryItemMenuWidget::WouldReopen(const UInventoryItemWidget* ItemWidget, EInventoryItemPress Press) const
{
	return Press == EInventoryItemPress::OpenItemMenu && ItemWidget && Item.IsSet() && ItemWidget->GetItem() == Item;
}

FReply UInventoryItemMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	RefreshConfirm();

	// The mouse highlights any entry it's over, such as one a subclass placed, and focus goes with it so Accept runs what's highlighted.
	// Off the entries, the last one stays highlighted.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	if (Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard)
	{
		HighlightEntry(FindHoveredButton(), /*bFocus*/ true);
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// Accept goes to the focused button, before any action binding sees the key, but focus doesn't follow the mouse. So with the mouse on a
// button, Accept clicks that one. The menu sees the key before its buttons do.
FReply UInventoryItemMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// While closing, keys reach nothing on the menu either, so focus stays where it was and nothing more is picked.
	if (bClosing)
	{
		return FReply::Handled();
	}

	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bMouse = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
	if (bMouse && FindHoveredButton() && FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		if (!InKeyEvent.IsRepeat())
		{
			ClickAtCursor(GetOwningLocalPlayer());
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

// Confirm: shown for what Accept does. If focus has left the buttons, its key comes here and clicks the button under the mouse.
void UInventoryItemMenuWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	RefreshConfirm();
}

void UInventoryItemMenuWidget::RefreshConfirm()
{
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bClicks = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard && FindHoveredButton();
	if (ConfirmHandle.IsValid() && bConfirmClicks != bClicks)
	{
		ConfirmHandle.Unregister();
		ConfirmHandle = FUIActionBindingHandle();
	}
	if (!ConfirmHandle.IsValid() && !ConfirmAction.IsNull())
	{
		// Without the mouse on a button, it's only shown: not consuming it lets Accept reach the focused button.
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, bClicks ? FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfirmAction) : FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = bClicks;
		ConfirmHandle = RegisterUIActionBinding(ConfirmArgs);
		bConfirmClicks = bClicks;
	}
}

void UInventoryItemMenuWidget::HandleConfirmAction()
{
	// A real click, so the button does all it does for one, its sounds included.
	if (FindHoveredButton())
	{
		ClickAtCursor(GetOwningLocalPlayer());
	}
}

UCommonButtonBase* UInventoryItemMenuWidget::FindHoveredButton() const
{
	UCommonButtonBase* Hovered = nullptr;
	if (MenuPanel)
	{
		UWidgetTree::ForWidgetAndChildren(MenuPanel, [&Hovered](UWidget* Widget)
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget);
			if (!Hovered && Button && Button->IsHovered() && Button->IsVisible() && Button->GetIsEnabled())
			{
				Hovered = Button;
			}
		});
	}
	return Hovered;
}

UInventoryItemWidget* UInventoryItemMenuWidget::TakePressedItemWidget()
{
	UInventoryItemWidget* ItemWidget = PressedItemWidget.Get();
	const bool bSameItem = ItemWidget && PressedItem.IsSet() && ItemWidget->GetItem() == PressedItem;
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	return bSameItem ? ItemWidget : nullptr;
}
