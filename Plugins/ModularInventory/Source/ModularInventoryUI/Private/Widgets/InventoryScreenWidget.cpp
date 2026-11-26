#include "Widgets/InventoryScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "Sound/SoundBase.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include "Actions/InventoryItemAction.h"
#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "Layers/ModularUISubsystem.h"
#include "ModularInventory.h"
#include "ModularUIFocus.h"
#include "Widgets/InventoryCellWidget.h"
#include "Widgets/InventoryGridWidget.h"
#include "Widgets/InventoryItemMenuWidget.h"
#include "Widgets/InventoryItemWidget.h"
#include "Widgets/InventoryListWidget.h"
#include "Widgets/InventorySlotWidget.h"
#include "Widgets/ModularPageSwitcher.h"
#include "Widgets/ModularTabListWidget.h"

#define LOCTEXT_NAMESPACE "InventoryScreenWidget"

namespace
{
	UInventoryItemWidget* FindItemWidgetIn(const UUserWidget& Parent, FInventoryItemHandle Item)
	{
		UInventoryItemWidget* Found = nullptr;
		if (!Parent.WidgetTree)
		{
			return nullptr;
		}

		Parent.WidgetTree->ForEachWidget([&Found, Item](UWidget* Widget)
		{
			if (Found)
			{
				return;
			}
			if (const UInventoryGridWidget* Grid = Cast<UInventoryGridWidget>(Widget))
			{
				Found = Grid->GetItemWidget(Item);
			}
			else if (const UInventoryListWidget* List = Cast<UInventoryListWidget>(Widget))
			{
				Found = List->GetItemWidget(Item);
			}
			else if (const UInventorySlotWidget* SlotWidget = Cast<UInventorySlotWidget>(Widget))
			{
				Found = SlotWidget->GetItemWidget(Item);
			}
			else if (const UUserWidget* Group = Cast<UUserWidget>(Widget))
			{
				Found = FindItemWidgetIn(*Group, Item);
			}
		});
		return Found;
	}

	/** Every item widget among Parent's widgets and inside the user widgets among them, such as the items in its grids, lists and slots. */
	void ForEachItemWidgetIn(const UUserWidget& Parent, TFunctionRef<void(UInventoryItemWidget&)> Callback)
	{
		if (!Parent.WidgetTree)
		{
			return;
		}

		Parent.WidgetTree->ForEachWidget([&Callback](UWidget* Widget)
		{
			if (UInventoryItemWidget* ItemWidget = Cast<UInventoryItemWidget>(Widget))
			{
				Callback(*ItemWidget);
			}
			else if (const UUserWidget* Group = Cast<UUserWidget>(Widget))
			{
				ForEachItemWidgetIn(*Group, Callback);
			}
		});
	}

	/**
	 * Whether the item widget shows an item under the point, drawn now, where a press would reach it. Cached geometry is only a first
	 * check, as it's from the last time the widget was drawn: one on a page that's no longer shown keeps it. The path from the window
	 * only finds widgets drawn now, with where they are now, and through it, whether anything on the way lets presses through.
	 */
	bool IsPressableAt(const UInventoryItemWidget& ItemWidget, const FVector2D& ScreenPosition)
	{
		const TSharedPtr<SWidget> SlateWidget = ItemWidget.GetCachedWidget();
		if (!SlateWidget || ItemWidget.IsDragVisual() || !ItemWidget.GetItem().IsSet() || !ItemWidget.GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return false;
		}

		FWidgetPath Path;
		if (!FSlateApplication::Get().FindPathToWidget(SlateWidget.ToSharedRef(), Path) || Path.Widgets.Num() == 0)
		{
			return false;
		}
		const int32 Last = Path.Widgets.Num() - 1;
		if (!Path.Widgets[Last].Geometry.IsUnderLocation(ScreenPosition))
		{
			return false;
		}
		for (int32 Index = 0; Index < Last; ++Index)
		{
			if (!Path.Widgets[Index].Widget->GetVisibility().AreChildrenHitTestVisible())
			{
				return false;
			}
		}
		const EVisibility Visibility = SlateWidget->GetVisibility();
		return Visibility.IsHitTestVisible() || Visibility.AreChildrenHitTestVisible();
	}

	/** The grid cell, list entry or slot for a spot in a container, among Parent's widgets. */
	UInventoryCellWidget* FindCellWidgetIn(const UUserWidget& Parent, FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
	{
		UInventoryCellWidget* Found = nullptr;
		if (!Parent.WidgetTree)
		{
			return nullptr;
		}

		Parent.WidgetTree->ForEachWidget([&Found, Container, &Placement](UWidget* Widget)
		{
			if (Found)
			{
				return;
			}
			if (const UInventoryGridWidget* Grid = Cast<UInventoryGridWidget>(Widget))
			{
				Found = Grid->GetContainer() == Container ? Grid->GetCellWidget(Placement.Cell) : nullptr;
			}
			else if (const UInventoryListWidget* List = Cast<UInventoryListWidget>(Widget))
			{
				Found = List->GetContainer() == Container ? List->GetEntryWidget(Placement.Index) : nullptr;
			}
			else if (UInventoryCellWidget* Cell = Cast<UInventoryCellWidget>(Widget))
			{
				Found = Cell->GetContainer() == Container && Cell->GetPlacement() == Placement ? Cell : nullptr;
			}
			else if (const UUserWidget* Group = Cast<UUserWidget>(Widget))
			{
				Found = FindCellWidgetIn(*Group, Container, Placement);
			}
		});
		return Found;
	}
}

UInventoryScreenWidget::UInventoryScreenWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	InputConfig = EModularUIInputMode::Menu;
	GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
}

void UInventoryScreenWidget::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Screens saved before Input Config replaced Game Keeps Input.
	if (bGameKeepsInput_DEPRECATED)
	{
		InputConfig = EModularUIInputMode::GameAndMenu;
		GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
		bGameKeepsInput_DEPRECATED = false;
	}

	// Screens saved before the description line replaced the action hint's Idle Hint.
	if (!IdleHint_DEPRECATED.IsEmpty())
	{
		if (DefaultDescription.IsEmpty())
		{
			DefaultDescription = IdleHint_DEPRECATED;
		}
		IdleHint_DEPRECATED = FText::GetEmpty();
	}
#endif
}

void UInventoryScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TabList)
	{
		TabList->OnTabSelected.AddUniqueDynamic(this, &ThisClass::HandleTabSelected);
	}
}

void UInventoryScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The tab list is built by now, so it makes its tabs as it's linked.
	if (TabList && Pages && TabList->GetLinkedSwitcher() != Pages)
	{
		TabList->SetLinkedSwitcher(Pages);
	}
}

void UInventoryScreenWidget::RegisterConfirmAction()
{
	// Confirm is bound as the input method changes, by Refresh Actions.
}

#if WITH_EDITOR
void UInventoryScreenWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	const bool bHasMainGrid = BlueprintWidgetTree.FindWidget(GET_MEMBER_NAME_CHECKED(UInventoryScreenWidget, MainGrid)) != nullptr;
	if (!bHasMainGrid && !GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UInventoryScreenWidget, BP_GetDesiredFocusTarget)))
	{
		const FText Message = LOCTEXT("NoFocusTarget", "Gamepad focus has nowhere to start: name the main grid MainGrid, or implement Get Desired Focus Target.");
		if (GetParentNativeClass(GetClass()) == UInventoryScreenWidget::StaticClass())
		{
			CompileLog.Warning(Message);
		}
		else
		{
			CompileLog.Note(Message);
		}
	}
}
#endif

UInventoryItemMenuWidget* UInventoryScreenWidget::OpenItemMenu(FInventoryItemHandle Item, TSubclassOf<UInventoryItemMenuWidget> MenuClass)
{
	return OpenItemMenuWith(Item, MenuClass, [](UInventoryItemMenuWidget&) {});
}

UInventoryItemMenuWidget* UInventoryScreenWidget::OpenItemMenuWith(FInventoryItemHandle Item, TSubclassOf<UInventoryItemMenuWidget> MenuClass, TFunctionRef<void(UInventoryItemMenuWidget&)> InitMenu)
{
	const UInventoryItemMenuWidget* MenuDefaults = MenuClass ? GetDefault<UInventoryItemMenuWidget>(MenuClass) : nullptr;
	if (!MenuDefaults || !Item.IsSet() || !IsInventoryShown() || !MenuDefaults->CanOpenFor(MakeActionContext(Item)))
	{
		return nullptr;
	}

	// The last menu, if it's still closing, such as the one whose action opens this one, goes at once. Covered by the new one in its stack,
	// it would only be put aside, and come back as the new one closes.
	UInventoryItemMenuWidget* Previous = OpenedMenu.Get();
	if (Previous && Previous->IsClosing())
	{
		Previous->CloseMenuNow();
	}

	UInventoryItemMenuWidget* Opened = PushItemMenu(MenuClass, [this, Item, &InitMenu](UInventoryItemMenuWidget& Menu)
	{
		InitMenu(Menu);
		Menu.SetScreen(this);
		Menu.SetItem(Item, FindItemWidget(Item));
	});
	OpenedMenu = Opened;
	return Opened;
}

UInventoryItemMenuWidget* UInventoryScreenWidget::PushItemMenu(TSubclassOf<UInventoryItemMenuWidget> MenuClass, TFunctionRef<void(UInventoryItemMenuWidget&)> InitMenu)
{
	if (ItemMenuLayer.IsValid())
	{
		UModularUISubsystem* UI = UModularUISubsystem::Get(GetOwningLocalPlayer());
		if (UInventoryItemMenuWidget* Pushed = UI ? UI->PushWidgetToLayer<UInventoryItemMenuWidget>(ItemMenuLayer, MenuClass.Get(), InitMenu) : nullptr)
		{
			return Pushed;
		}
	}
	if (!ItemMenuStack)
	{
		UE_LOG(LogInventory, Warning, TEXT("[Inventory UI] %s: nowhere to open %s. Set Item Menu Layer to a Modular UI layer, or add a Common Activatable Widget Stack named ItemMenuStack."),
			*GetPathNameSafe(this), *GetNameSafe(MenuClass));
		return nullptr;
	}
	return ItemMenuStack->AddWidget<UInventoryItemMenuWidget>(MenuClass.Get(), InitMenu);
}

void UInventoryScreenWidget::SetInventoryShown(bool bShown)
{
	if (bShown == bInventoryShown)
	{
		return;
	}

	bInventoryShown = bShown;
	if (!IsActivated())
	{
		return;
	}

	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !bShown)
	{
		// The item hovered is hovered again as the items show again. Nothing else carries over: what was held goes back, and the hint goes
		// with the items.
		LastHoveredItem = Interaction->GetHoveredItem();
		Interaction->ClearInteraction();
		ClearHighlightedDescription(this, /*bEvenIfKept*/ true);
	}
	RefreshActions();
	RefreshDescription();
	if (bShown)
	{
		ReturnToItems();
	}
}

void UInventoryScreenWidget::ReturnToItems()
{
	// Once the items are drawn: a page just switched to may not be yet, however long its switcher takes, and a cell can't take focus until
	// it is.
	UWidget* FocusTarget = GetDesiredFocusTarget();
	UWidget* DrawnCheck = FocusTarget ? FocusTarget : MainGrid.Get();
	if (!DrawnCheck)
	{
		RestoreHover();
		return;
	}

	CallWhenDrawn(DrawnCheck, [this](UWidget&)
	{
		if (!IsInventoryShown())
		{
			return;
		}

		// Focus goes back with any input, so keys reach the items again: with a gamepad it aims at the cell, which hovers its item. The mouse
		// aims where it points, so the item it left is hovered again instead.
		const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
		const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
		if (UWidget* Target = GetDesiredFocusTarget())
		{
			Target->SetFocus();
		}
		if (!bGamepad)
		{
			RestoreHover();
		}
	}, ModularUI::PageDrawnTimeout);
}

void UInventoryScreenWidget::RestoreHover()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	if (!Interaction || !Inventory || Interaction->IsHolding() || !IsInventoryShown())
	{
		return;
	}

	// The item hovered as the items were left, while it's still on the screen, as the mouse would have left it with Keep Last Hovered Item.
	const bool bKeepsHover = Interaction->GetStyle().bKeepLastHoveredItem || bAimOnOpen;
	const bool bOnScreen = Inventory->IsValidItem(LastHoveredItem) && FindItemWidget(LastHoveredItem) != nullptr;
	if (bKeepsHover && bOnScreen)
	{
		TGuardValue<bool> Aiming(bAimingAtStart, true);
		Interaction->SetTarget(Inventory->GetItemContainer(LastHoveredItem), Inventory->GetItemPlacement(LastHoveredItem));
		Interaction->SetHoveredItem(LastHoveredItem);
	}
	else if (bAimOnOpen)
	{
		AimAtStart();
	}
}

// Pages
void UInventoryScreenWidget::OpenPage(int32 PageIndex)
{
	const bool bOpenedByTab = TabList && TabList->OpenTabAtIndex(PageIndex, /*bSuppressClickFeedback*/ true);
	if (!bOpenedByTab && Pages)
	{
		Pages->SetActiveWidgetIndex(PageIndex);
	}
	SetInventoryShown(IsItemsPageOpen());
}

int32 UInventoryScreenWidget::GetOpenPage() const
{
	const int32 OpenTab = TabList ? TabList->GetOpenTabIndex() : INDEX_NONE;
	if (OpenTab != INDEX_NONE)
	{
		return OpenTab;
	}
	return Pages ? Pages->GetActiveWidgetIndex() : INDEX_NONE;
}

void UInventoryScreenWidget::SetStartPage(int32 PageIndex)
{
	if (IsActivated())
	{
		OpenPage(PageIndex == INDEX_NONE ? ItemsPage : PageIndex);
		return;
	}
	StartPage = PageIndex;
}

void UInventoryScreenWidget::SetStartTab(FName TabId)
{
	const int32 PageIndex = TabList ? TabList->GetTabIndexOf(TabId) : INDEX_NONE;
	if (PageIndex == INDEX_NONE)
	{
		UE_LOG(LogInventory, Warning, TEXT("[Inventory UI] %s: no tab %s in Tab List to open on."), *GetPathNameSafe(this), *TabId.ToString());
		return;
	}
	SetStartPage(PageIndex);
}

bool UInventoryScreenWidget::IsItemsPageOpen() const
{
	const int32 OpenPage = GetOpenPage();
	return OpenPage == INDEX_NONE || OpenPage == ItemsPage;
}

void UInventoryScreenWidget::HandleTabSelected(FName TabId)
{
	// Left at once, so what was held is put back. Shown again once the page is drawn, as Return To Items waits for it.
	SetInventoryShown(TabList && TabList->GetTabIndexOf(TabId) == ItemsPage);
}

void UInventoryScreenWidget::SetTransferContainer(FInventoryContainerHandle Container)
{
	TransferContainer = Container;

	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && IsActivated())
	{
		Interaction->SetTransferContainer(Container);
		RefreshActions();
	}
}

UInventoryItemWidget* UInventoryScreenWidget::FindItemWidget(FInventoryItemHandle Item) const
{
	return Item.IsSet() ? FindItemWidgetIn(*this, Item) : nullptr;
}

UInventoryItemWidget* UInventoryScreenWidget::FindItemWidgetAt(const FVector2D& ScreenPosition) const
{
	UInventoryItemWidget* Found = nullptr;
	ForEachItemWidgetIn(*this, [&Found, &ScreenPosition](UInventoryItemWidget& ItemWidget)
	{
		if (!Found && IsPressableAt(ItemWidget, ScreenPosition))
		{
			Found = &ItemWidget;
		}
	});
	return Found;
}

// Mouse: clicks and the wheel that nothing on the screen used come here.
FReply UInventoryScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Interaction && Interaction->IsHolding())
	{
		switch (Interaction->GetStyle().HeldRightClick)
		{
		case EInventoryHeldRightClick::PutBack:
			Interaction->CancelHold();
			return FReply::Handled();
		case EInventoryHeldRightClick::Rotate:
			Interaction->RotateHeld();
			return FReply::Handled();
		case EInventoryHeldRightClick::AsUsual:
			break;
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryScreenWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsHolding() && Interaction->GetStyle().bWheelRotatesHeld && InMouseEvent.GetWheelDelta() != 0.0f)
	{
		Interaction->RotateHeld();
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void UInventoryScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		PreviousStyle = Interaction->GetStyle();
		Interaction->SetStyle(InteractionStyle);
		Interaction->SetTransferContainer(TransferContainer);

		Interaction->OnItemMenuRequested.AddUniqueDynamic(this, &ThisClass::HandleItemMenuRequested);
		Interaction->OnDefaultActionRequested.AddUniqueDynamic(this, &ThisClass::HandleDefaultActionRequested);
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnSelectedItemChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectedItemChanged);
		Interaction->OnHoldEvent.AddUniqueDynamic(this, &ThisClass::HandleHoldEvent);
	}
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}

	// The screen opens on the items, whichever page it closed on, or on the page asked for. At once: the screen's own open brings it in.
	const int32 OpenOn = StartPage != INDEX_NONE ? StartPage : ItemsPage;
	StartPage = INDEX_NONE;
	if (Pages || TabList)
	{
		UModularPageSwitcher* PageSwitcher = Cast<UModularPageSwitcher>(Pages);
		const bool bPageTransitions = PageSwitcher && PageSwitcher->ArePageTransitionsEnabled();
		if (PageSwitcher)
		{
			PageSwitcher->SetPageTransitionsEnabled(false);
		}
		OpenPage(OpenOn);
		if (PageSwitcher)
		{
			PageSwitcher->SetPageTransitionsEnabled(bPageTransitions);
		}
	}
	if (bAimOnOpen && IsInventoryShown())
	{
		AimAtStart();
	}
	RefreshActions();
	RefreshDescription();
}

void UInventoryScreenWidget::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnItemMenuRequested.RemoveDynamic(this, &ThisClass::HandleItemMenuRequested);
		Interaction->OnDefaultActionRequested.RemoveDynamic(this, &ThisClass::HandleDefaultActionRequested);
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnSelectedItemChanged.RemoveDynamic(this, &ThisClass::HandleSelectedItemChanged);
		Interaction->OnHoldEvent.RemoveDynamic(this, &ThisClass::HandleHoldEvent);

		// Closed on another page, the items were left already, with the item hovered then.
		if (IsInventoryShown())
		{
			LastHoveredItem = Interaction->GetHoveredItem();
		}
		Interaction->ClearInteraction();
		Interaction->SetStyle(PreviousStyle);
		Interaction->SetTransferContainer(FInventoryContainerHandle());
	}
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.RemoveAll(this);
	}

	// Common UI reuses closed screens, so a chest's container and the last cell aimed at mustn't carry over to the next opening. Neither does
	// what Description Text said, which the base forgets.
	TransferContainer = FInventoryContainerHandle();
	LastAimedContainer = FInventoryContainerHandle();
	OpenedMenu.Reset();

	Super::NativeOnDeactivated();
}

bool UInventoryScreenWidget::NativeOnHandleBackAction()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsChoosingItem())
	{
		Interaction->CancelChoosingItem();
		return true;
	}
	if (Interaction && Interaction->IsHolding())
	{
		Interaction->CancelHold();
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

void UInventoryScreenWidget::NativeOnCloseStarted()
{
	Super::NativeOnCloseStarted();

	// Nothing on the screen works while it goes, its action bar included.
	RefreshActions();
}

UWidget* UInventoryScreenWidget::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}
	if (!IsInventoryShown())
	{
		return nullptr;
	}

	// Back where the player was, such as on the item whose menu just closed, rather than on the first cell.
	UInventoryCellWidget* LastAimed = LastAimedContainer.IsSet() ? FindCellWidgetIn(*this, LastAimedContainer, LastAimedPlacement) : nullptr;
	if (LastAimed && LastAimed->IsVisible() && LastAimed->GetIsEnabled())
	{
		return LastAimed;
	}

	// Then on the item hovered as the screen last closed, wherever it is now.
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	if (bStartOnLastItem && Inventory && LastHoveredItem.IsSet())
	{
		const FInventoryContainerHandle Container = Inventory->GetItemContainer(LastHoveredItem);
		UInventoryCellWidget* LastItemCell = Container.IsSet() ? FindCellWidgetIn(*this, Container, Inventory->GetItemPlacement(LastHoveredItem)) : nullptr;
		if (LastItemCell && LastItemCell->IsVisible() && LastItemCell->GetIsEnabled())
		{
			return LastItemCell;
		}
	}
	return MainGrid ? MainGrid->GetCellWidget(FIntPoint::ZeroValue) : nullptr;
}

void UInventoryScreenWidget::AimAtStart()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	const UInventoryCellWidget* Start = Interaction && IsInventoryShown() && !Interaction->IsHolding() ? Cast<UInventoryCellWidget>(GetDesiredFocusTarget()) : nullptr;
	if (Start)
	{
		TGuardValue<bool> Aiming(bAimingAtStart, true);
		Interaction->SetTarget(Start->GetContainer(), Start->GetPlacement());
		Interaction->SetHoveredItem(Start->GetItem());
	}
}

UInventoryUISubsystem* UInventoryScreenWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

// Item menus
void UInventoryScreenWidget::HandleItemMenuRequested(FInventoryItemHandle Item)
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && IsInventoryShown())
	{
		if (ItemMenuClass)
		{
			OpenItemMenu(Item, ItemMenuClass);
		}
		NativeOnItemMenuRequested(Item);
		OnItemMenuRequested(Item);
	}
}

// Actions
void UInventoryScreenWidget::RefreshActions()
{
	// By Mode, what's bound changes only as the player starts or stops holding or choosing an item, so action bars, which rebuild each time
	// it changes, stay still as the cursor moves. By Target, it follows what's aimed at.
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bUsable = Interaction && IsInventoryShown() && !IsClosing();
	const bool bHolding = bUsable && Interaction->IsHolding();
	const bool bBrowsing = bUsable && !bHolding && !Interaction->IsChoosingItem();
	const bool bByTarget = ActionAvailability == EInventoryActionAvailability::ByTarget;
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const FInventoryItemHandle Hovered = Interaction ? Interaction->GetHoveredItem() : FInventoryItemHandle();

	// With a gamepad, Accept goes to the focused cell, which aims, so Confirm is only shown. With the mouse, keyboard focus isn't on what
	// the mouse is, so Confirm's key acts on the hovered item itself. Which of the two is bound changes only with the input method.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bMouse = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
	SetConfirmShown(!bMouse && !IsClosing());
	const bool bCanConfirm = bUsable && bMouse && (!bByTarget || Interaction->CanActivateAimed());
	SetActionBound(ConfirmHandle, ConfirmAction, bCanConfirm, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfirmAction));

	const bool bCanMove = bByTarget ? bUsable && Interaction->CanPickUpOrPlace() : bBrowsing || bHolding;
	SetActionBound(MoveHandle, MoveAction, bCanMove, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleMoveAction));

	const bool bCanOpenItemMenu = bBrowsing && (!bByTarget || (Inventory && Inventory->IsValidItem(Hovered)));
	SetActionBound(ItemMenuHandle, ItemMenuAction, bCanOpenItemMenu, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleItemMenuAction));

	const bool bCanRotate = bHolding && (bByTarget ? Interaction->CanRotateHeld() : Interaction->CanEverRotateHeld());
	SetActionBound(RotateHandle, RotateAction, bCanRotate, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRotateAction));
	SetActionBound(SplitHandle, SplitAction, bHolding && Interaction->CanSplitHeld(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSplitAction));

	const bool bCanTransfer = bBrowsing && (bByTarget ? Interaction->CanTransferItem(Hovered) : TransferContainer.IsSet());
	SetActionBound(TransferHandle, TransferAction, bCanTransfer, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTransferAction));
	SetActionBound(TakeAllHandle, TakeAllAction, bUsable && Interaction->CanTakeAll(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTakeAllAction));
	SetActionBound(SortHandle, SortAction, bUsable && !Interaction->IsChoosingItem() && GetSortContainer().IsSet(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSortAction));

	// While an item is held, the tab keys can turn it instead of changing pages. Neither works while the screen closes.
	if (TabList)
	{
		const bool bTurnHeld = bTabKeysTurnHeldItem && bUsable && Interaction->IsHolding();
		const FSimpleDelegate Rotate = FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRotateAction);
		TabList->SetListeningForInput(!bTurnHeld && !IsClosing());
		SetActionBound(PreviousTabRotateHandle, TabList->GetPreviousTabAction(), bTurnHeld, Rotate, /*bShowInActionBar*/ false);
		SetActionBound(NextTabRotateHandle, TabList->GetNextTabAction(), bTurnHeld, Rotate, /*bShowInActionBar*/ false);
	}
}

void UInventoryScreenWidget::SetActionBound(FUIActionBindingHandle& Handle, const FDataTableRowHandle& Row, bool bBound, const FSimpleDelegate& Handler, bool bShowInActionBar)
{
	if (bBound && !Handle.IsValid() && !Row.IsNull())
	{
		Handle = RegisterUIActionBinding(FBindUIActionArgs(Row, bShowInActionBar, Handler));
	}
	else if (!bBound && Handle.IsValid())
	{
		Handle.Unregister();
		Handle = FUIActionBindingHandle();
	}
}

void UInventoryScreenWidget::HandleInteractionChanged()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->GetTargetContainer().IsSet())
	{
		LastAimedContainer = Interaction->GetTargetContainer();
		LastAimedPlacement = Interaction->GetTargetPlacement();
	}
	RefreshActions();
	RefreshDescription();
}

void UInventoryScreenWidget::HandleSelectedItemChanged(FInventoryItemHandle Item)
{
	// Choosing an item for another selects the one it's for, so the hint follows the choosing too.
	RefreshDescription();
}

// Item actions
FInventoryItemActionContext UInventoryScreenWidget::MakeActionContext(FInventoryItemHandle Item) const
{
	FInventoryItemActionContext Context;
	Context.Item = Item;
	Context.Player = GetOwningPlayer();
	Context.Screen = const_cast<UInventoryScreenWidget*>(this);
	return Context;
}

bool UInventoryScreenWidget::RunAction(UInventoryItemAction* Action, FInventoryItemHandle Item, int32 Count)
{
	FInventoryItemActionContext Context = MakeActionContext(Item);
	Context.Count = Count;
	if (!Action || !Item.IsSet() || !Action->CanRunFor(Context))
	{
		return false;
	}
	Action->RunFor(Context);
	return true;
}

bool UInventoryScreenWidget::RunDefaultAction(FInventoryItemHandle Item)
{
	if (!DefaultActions || !Item.IsSet())
	{
		return false;
	}

	FInventoryItemActionContext Context = MakeActionContext(Item);
	for (UInventoryItemAction* Action : DefaultActions->Actions)
	{
		if (!Action || !Action->CanRunFor(Context))
		{
			continue;
		}

		// No prompt here, so a count is the one it would start at.
		int32 Min = 0;
		int32 Max = 0;
		int32 Count = 0;
		if (Action->Prompt == EInventoryActionPrompt::Quantity)
		{
			Action->GetQuantityRangeFor(Context, Min, Max, Count);
		}
		Context.Count = Count;
		Action->RunFor(Context);
		return true;
	}
	return false;
}

void UInventoryScreenWidget::HandleDefaultActionRequested(FInventoryItemHandle Item)
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && IsInventoryShown())
	{
		RunDefaultAction(Item);
	}
}

// Description: what the highlighted option of an item menu open over the screen does comes first, as the menu says it here. Then what to do
// with the held item, or what to choose, then what a highlighted button does. Browsing the items says nothing: details panels show them.
FText UInventoryScreenWidget::GetHint() const
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (!Interaction || !IsInventoryShown())
	{
		return FText::GetEmpty();
	}

	if (Interaction->IsHolding())
	{
		if (Interaction->GetTargetAction() == EInventoryDropAction::Invalid)
		{
			if (const FText* Reason = InvalidHints.Find(Interaction->GetTargetError()))
			{
				return *Reason;
			}
			if (!InvalidHint.IsEmpty())
			{
				return InvalidHint;
			}
		}
		return MoveHint;
	}
	return Interaction->IsChoosingItem() ? ChooseHint : FText::GetEmpty();
}

void UInventoryScreenWidget::HandleHoveredItemChanged(FInventoryItemHandle Item)
{
	// Not while an item is held, as items aren't drawn hovered then, nor for the item the screen opens on.
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Item.IsSet() && Interaction && !Interaction->IsHolding() && !bAimingAtStart)
	{
		PlayInventorySound(Sounds.HoverItem);
	}

	// An item takes the highlight from any button, such as a tab, and says nothing, so the button's description isn't kept for it.
	if (Item.IsSet())
	{
		SetHighlightedDescription(FText::GetEmpty(), this);
	}
	RefreshActions();
}

// Sounds
void UInventoryScreenWidget::HandleHoldEvent(EInventoryHoldEvent Event)
{
	switch (Event)
	{
	case EInventoryHoldEvent::PickedUp: PlayInventorySound(Sounds.PickUp); break;
	case EInventoryHoldEvent::Placed:   PlayInventorySound(Sounds.PutDown); break;
	case EInventoryHoldEvent::Merged:   PlayInventorySound(Sounds.Merge ? Sounds.Merge : Sounds.PutDown); break;
	case EInventoryHoldEvent::Swapped:  PlayInventorySound(Sounds.Swap ? Sounds.Swap : Sounds.PutDown); break;
	case EInventoryHoldEvent::Refused:  PlayInventorySound(Sounds.CantPlace); break;
	case EInventoryHoldEvent::Rotated:  PlayInventorySound(Sounds.Rotate); break;
	case EInventoryHoldEvent::Split:    PlayInventorySound(Sounds.Split); break;
	case EInventoryHoldEvent::PutBack:  PlayInventorySound(Sounds.PutBack); break;
	}
}

void UInventoryScreenWidget::PlayInventorySound(USoundBase* Sound) const
{
	// A UI sound, so it plays while the game is paused behind the screen.
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

void UInventoryScreenWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	RefreshActions();
}

void UInventoryScreenWidget::SetConfirmShown(bool bShown)
{
	if (bShown && !ConfirmShownHandle.IsValid() && !ConfirmAction.IsNull())
	{
		// Not consuming it lets Accept reach the focused cell.
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = false;
		ConfirmShownHandle = RegisterUIActionBinding(ConfirmArgs);
	}
	else if (!bShown && ConfirmShownHandle.IsValid())
	{
		ConfirmShownHandle.Unregister();
		ConfirmShownHandle = FUIActionBindingHandle();
	}
}

void UInventoryScreenWidget::HandleConfirmAction()
{
	// Bound By Mode whatever is aimed at, so it does nothing where it can't.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->CanActivateAimed())
	{
		Interaction->ActivateAimed();
	}
}

void UInventoryScreenWidget::HandleMoveAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->PickUpOrPlace();
	}
}

void UInventoryScreenWidget::HandleItemMenuAction()
{
	// Bound By Mode over empty cells too, so only an item asks for its menu.
	UInventoryUISubsystem* Interaction = GetInteraction();
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	if (Interaction && Inventory && Inventory->IsValidItem(Interaction->GetHoveredItem()))
	{
		Interaction->RequestItemMenu(Interaction->GetHoveredItem());
	}
}

void UInventoryScreenWidget::HandleRotateAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->RotateHeld();
	}
}

void UInventoryScreenWidget::HandleSplitAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SplitHeld();
	}
}

void UInventoryScreenWidget::HandleTransferAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->TransferItem(Interaction->GetHoveredItem());
		RefreshActions();
	}
}

void UInventoryScreenWidget::HandleSortAction()
{
	SortItems();
	RefreshActions();
}

// Sorting
FInventoryContainerHandle UInventoryScreenWidget::GetSortContainer() const
{
	return MainGrid ? MainGrid->GetContainer() : FInventoryContainerHandle();
}

bool UInventoryScreenWidget::SortItems()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const FInventoryContainerHandle Container = GetSortContainer();
	if (!Inventory || !Container.IsSet() || (Interaction && Interaction->IsChoosingItem()))
	{
		return false;
	}

	// The held item goes back first, so it's sorted with the rest.
	if (Interaction)
	{
		Interaction->CancelHold();
	}
	EInventoryError Error = EInventoryError::None;
	const bool bSorted = Inventory->SortContainerBy(Container, SortSettings, Error);
	NativeOnSorted(bSorted);
	OnSorted(bSorted);
	return bSorted;
}

void UInventoryScreenWidget::HandleTakeAllAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->TakeAll();
		RefreshActions();
	}
}

#undef LOCTEXT_NAMESPACE
