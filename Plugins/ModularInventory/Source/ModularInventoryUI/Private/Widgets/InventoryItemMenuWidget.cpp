#include "Widgets/InventoryItemMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

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
}

void UInventoryItemMenuWidget::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Menus saved before Right Click replaced Right Press Outside and Close On Right Press. Only what was changed from their defaults, which
	// Right Click's default matches off the menu, is carried over, so a menu saved since keeps its Right Click.
	if (RightPressOutside_DEPRECATED != EModularMenuOutsideClick::PassThrough || !bCloseOnRightPress_DEPRECATED)
	{
		if (RightPressOutside_DEPRECATED == EModularMenuOutsideClick::PassThrough)
		{
			RightClick = EModularMenuRightClick::PassThrough;
		}
		else if (bCloseOnRightPress_DEPRECATED || RightPressOutside_DEPRECATED == EModularMenuOutsideClick::Close)
		{
			RightClick = EModularMenuRightClick::Back;
		}
		else
		{
			RightClick = EModularMenuRightClick::Ignore;
		}
		RightPressOutside_DEPRECATED = EModularMenuOutsideClick::PassThrough;
		bCloseOnRightPress_DEPRECATED = true;
	}

	// Game Keeps Input became an input mode of its own.
	if (bGameKeepsInput_DEPRECATED)
	{
		InputConfig = EModularUIInputMode::GameAndMenu;
		GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
		bGameKeepsInput_DEPRECATED = false;
	}
#endif
}

void UInventoryItemMenuWidget::SetItem(FInventoryItemHandle InItem, UInventoryItemWidget* ItemWidget)
{
	Item = InItem;
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

	// A question asked about the last item goes unanswered.
	if (IsPrompting())
	{
		CancelPrompt();
	}
	RefreshActions();
	NativeOnItemChanged();
	OnItemChanged();

	// Last, so the menu is measured with the actions this item shows.
	SetAnchorWidget(ItemWidget);
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

	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	BindScreen(Screen.Get());
}

void UInventoryItemMenuWidget::NativeOnDeactivated()
{
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	RegistryListener.Unbind();

	// The screen says its own hint again. It stays the menu's screen, should the menu show again from under another, but no longer closes it.
	ShowHint(FText::GetEmpty());
	if (UInventoryScreenWidget* ClosedOn = Screen.Get())
	{
		ClosedOn->OnDeactivated().RemoveAll(this);
	}

	// Choosing keeps the item selected while another one is picked for it.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && Interaction->GetSelectedItem() == Item)
	{
		Interaction->SetSelectedItem(FInventoryItemHandle());
	}

	Super::NativeOnDeactivated();
}

void UInventoryItemMenuWidget::NativeOnCloseStarted()
{
	// The screen says its own hint again as the menu starts to go.
	ShowHint(FText::GetEmpty());
	Super::NativeOnCloseStarted();
}

UInventoryUISubsystem* UInventoryItemMenuWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
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
				MadeActionButtons.Add(Made);
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

		// The mouse and focus highlight it as they reach it, as they do any entry.
		const int32 Index = ShownActions.Num();
		Button->OnClicked().AddUObject(this, &ThisClass::HandleActionClicked, Index);

		FShownAction& Shown = ShownActions.AddDefaulted_GetRef();
		Shown.Action = Action;
		Shown.Button = Button;
		Shown.bAvailable = bAvailable;
		Shown.UnavailableReason = Reason;
	}

	// Listed again while open: the first entry on screen, not the first listed, is highlighted again. Opening does this itself.
	if (IsActivated() && !IsClosing())
	{
		HighlightEntry(Cast<UCommonButtonBase>(GetDesiredFocusTarget()), /*bFocus*/ false);
		FocusDesiredTarget();
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
		}
	}
	ShownActions.Reset();

	for (UInventoryItemActionButton* Made : MadeActionButtons)
	{
		if (Made)
		{
			Made->RemoveFromParent();
		}
	}
	MadeActionButtons.Reset();
}

const UInventoryItemMenuWidget::FShownAction* UInventoryItemMenuWidget::FindShownAction(const UCommonButtonBase* Button) const
{
	return Button ? ShownActions.FindByPredicate([Button](const FShownAction& Shown) { return Shown.Button.Get() == Button; }) : nullptr;
}

FText UInventoryItemMenuWidget::GetEntryDescription(UCommonButtonBase* Entry) const
{
	// What the action does, or why it can't run now.
	if (const FShownAction* Shown = FindShownAction(Entry))
	{
		const UInventoryItemAction* Action = Shown->Action.Get();
		const bool bShowReason = !Shown->bAvailable && !Shown->UnavailableReason.IsEmpty();
		return bShowReason ? Shown->UnavailableReason : Action ? Action->Description : FText::GetEmpty();
	}
	return Super::GetEntryDescription(Entry);
}

void UInventoryItemMenuWidget::NativeOnEntryHighlighted(UCommonButtonBase* Entry, const FText& Description)
{
	Super::NativeOnEntryHighlighted(Entry, Description);

	// Not while a prompt asks: the question is on screen.
	UInventoryScreenWidget* ShownOn = Screen.Get();
	if (ShownOn && bShowHintOnScreen && !IsPrompting())
	{
		ShownOn->ShowActionHint(Description);
	}
}

UWidget* UInventoryItemMenuWidget::GetEntriesWidget() const
{
	return ActionPanel ? ActionPanel.Get() : Super::GetEntriesWidget();
}

void UInventoryItemMenuWidget::HandleActionClicked(int32 Index)
{
	if (IsClosing() || IsPrompting() || !ShownActions.IsValidIndex(Index))
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

	if (Action->Prompt == EInventoryActionPrompt::None)
	{
		RunAction(Action, 0);
		return;
	}

	// Asked as Prompt Presentation says, or, without anywhere to ask, answered yes at once with the count the prompt would start at.
	const TWeakObjectPtr<ThisClass> WeakThis = this;
	const TWeakObjectPtr<UInventoryItemAction> WeakAction = Action;
	ShowPrompt(MakePromptRequest(*Action, Context), [WeakThis, WeakAction](bool bConfirmed, int32 Count)
	{
		ThisClass* Menu = WeakThis.Get();
		UInventoryItemAction* Asked = WeakAction.Get();
		if (Menu && Asked && bConfirmed && Menu->IsActivated() && !Menu->IsClosing())
		{
			Menu->RunAction(Asked, Count);
		}
	});
}

FModularPromptRequest UInventoryItemMenuWidget::MakePromptRequest(const UInventoryItemAction& Action, const FInventoryItemActionContext& Context) const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Context.Item) : nullptr;
	const FText ItemName = Definition ? Definition->DisplayName : FText::GetEmpty();

	FModularPromptRequest Request;
	Request.Kind = Action.Prompt == EInventoryActionPrompt::Quantity ? EModularPromptKind::Quantity : EModularPromptKind::Confirm;
	Request.Title = ItemName;
	Request.ConfirmLabel = Action.PromptConfirmLabel;

	// {Count} and {Max} stay as they are, for the prompt to fill in as the count changes.
	Request.Message = FText::FormatNamed(Action.PromptText, TEXT("Item"), ItemName);

	if (Request.Kind == EModularPromptKind::Quantity)
	{
		Action.GetQuantityRangeFor(Context, Request.MinCount, Request.MaxCount, Request.DefaultCount);
	}
	return Request;
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
		// using the item up. A prompt it was asked in goes once this returns.
		Run();
		if (IsActivated() && !IsClosing())
		{
			RefreshActions();
			PlaceMenu();
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
		CloseWidgetThen(MoveTemp(Run));
		return;
	}

	// The menu starts to close first, so it takes no more input and the screen's hint is back, and a menu the action opens in the same
	// stack replaces it at once.
	CloseMenu();
	Run();
}

void UInventoryItemMenuWidget::ShowHint(const FText& Hint)
{
	ShowDescription(Hint);
	UInventoryScreenWidget* ShownOn = Screen.Get();
	if (ShownOn && bShowHintOnScreen)
	{
		ShownOn->ShowActionHint(Hint);
	}
}

// Mouse: Outside Click and Right Click decide what a press off the menu does, as for any context menu. A press passed through goes on to
// the item under it, as if the menu weren't open: a left press then waits, as the item would, to see whether it ends in a click or a drag,
// and a right press reaches the item at once.
// The screen may open another menu as the press reaches an item, perhaps this same one again, so nothing here is touched after that.
FReply UInventoryItemMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// A new press forgets the last one's item, whatever it became.
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

bool UInventoryItemMenuWidget::PassPressThrough(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, FReply& OutReply)
{
	// Only the left and right buttons reach items. The others, such as the middle one, only close the menu.
	const FKey Button = InMouseEvent.GetEffectingButton();
	const bool bLeft = Button == EKeys::LeftMouseButton;
	const bool bRight = Button == EKeys::RightMouseButton;
	UInventoryItemWidget* ItemWidget = bLeft || bRight ? FindItemWidgetUnder(InMouseEvent.GetScreenSpacePosition()) : nullptr;
	if (!ItemWidget)
	{
		return false;
	}

	if (bLeft)
	{
		PressedItemWidget = ItemWidget;
		PressedItem = ItemWidget->GetItem();
		OutReply = FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		return true;
	}

	// A right-click that would only open a menu for this menu's item again only closes this one. The menu covered the item, so the item
	// takes the hover first.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!Interaction || WouldReopen(ItemWidget, Interaction->GetStyle().RightClick))
	{
		return false;
	}
	CloseMenuNow();
	Interaction->SetHoveredItem(ItemWidget->GetItem());
	ItemWidget->RightClick(InMouseEvent);
	OutReply = FReply::Handled();
	return true;
}

FReply UInventoryItemMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryItemWidget* ItemWidget = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? TakePressedItemWidget() : nullptr;
	if (!ItemWidget || IsClosing())
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
	if (IsClosing())
	{
		return FReply::Handled();
	}

	// The second click of a double-click on the menu's own item, whose first click opened the menu: the double-click is the item's.
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bDoubleClicks = Interaction && Interaction->GetStyle().DoubleClick != EInventoryItemPress::None;
	const bool bLeft = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	UInventoryItemWidget* ItemWidget = bLeft && bDoubleClicks && !IsPrompting() && !IsOnMenu(Position) ? FindItemWidgetUnder(Position) : nullptr;
	if (ItemWidget && ItemWidget == GetAnchorWidget())
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
	// closes either way, even when the item can't be picked up, as any press passed through does.
	UInventoryItemWidget* ItemWidget = TakePressedItemWidget();
	if (!ItemWidget || IsClosing())
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
	UInventoryItemWidget* AnchorItemWidget = Cast<UInventoryItemWidget>(GetAnchorWidget());
	if (AnchorItemWidget && AnchorItemWidget->GetItem() == Item && GetAnchorBox().IsInside(ScreenPosition))
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

UInventoryItemWidget* UInventoryItemMenuWidget::TakePressedItemWidget()
{
	UInventoryItemWidget* ItemWidget = PressedItemWidget.Get();
	const bool bSameItem = ItemWidget && PressedItem.IsSet() && ItemWidget->GetItem() == PressedItem;
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	return bSameItem ? ItemWidget : nullptr;
}
