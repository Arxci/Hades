#include "Widgets/InventoryShortcutMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "Shortcuts/InventoryShortcutComponent.h"
#include "Widgets/InventoryItemWidget.h"
#include "Widgets/ModularInputKeyWidget.h"

// Entry
void UInventoryShortcutMenuEntry::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Shown in the designer too, so entries placed by hand show which slot they are.
	if (NumberText)
	{
		NumberText->SetText(FText::AsNumber(SlotIndex + 1));
	}
}

void UInventoryShortcutMenuEntry::SetShortcutSlot(const UInventoryShortcutComponent* Shortcuts, int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
	const FInventoryItemHandle SlotItem = Shortcuts ? Shortcuts->GetSlotItem(SlotIndex) : FInventoryItemHandle();

	if (NumberText)
	{
		NumberText->SetText(FText::AsNumber(SlotIndex + 1));
	}
	if (SlotNameText)
	{
		SlotNameText->SetText(Shortcuts ? Shortcuts->GetSlotDisplayName(SlotIndex) : FText::GetEmpty());
	}
	if (KeyWidget)
	{
		KeyWidget->SetInputAction(Shortcuts ? Shortcuts->GetSlotInputAction(SlotIndex) : nullptr);
	}
	if (ItemWidget)
	{
		ItemWidget->SetItem(SlotItem);
		if (ItemCellSize > 0.0f)
		{
			ItemWidget->SetCellLayout(ItemCellSize, ItemCellGap, /*bInRotated*/ false);
		}
		ItemWidget->SetVisibility(SlotItem.IsSet() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (NameText)
	{
		const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
		const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(SlotItem) : nullptr;
		NameText->SetText(Definition ? Definition->DisplayName : EmptyText);
	}
	OnShortcutSlotShown(SlotIndex, SlotItem);
}

// Menu
bool UInventoryShortcutMenuWidget::CanOpenFor(const FInventoryItemActionContext& Context) const
{
	// Asked of the class defaults, so the player comes from the context.
	const APawn* Pawn = Context.Player ? Context.Player->GetPawn() : nullptr;
	const UInventoryShortcutComponent* Shortcuts = UInventoryShortcutComponent::FindShortcutComponent(Pawn);
	if (Shortcuts && (Shortcuts->CanAssign(Context.Item) || Shortcuts->FindSlotOfItem(Context.Item) != INDEX_NONE))
	{
		return true;
	}

	TArray<UInventoryItemAction*> Gathered;
	GatherActions(Gathered);
	return !Gathered.IsEmpty() && Super::CanOpenFor(Context);
}

UInventoryShortcutComponent* UInventoryShortcutMenuWidget::GetShortcuts() const
{
	return UInventoryShortcutComponent::FindShortcutComponent(GetOwningPlayerPawn());
}

void UInventoryShortcutMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Entries placed in the designer keep their slot, so they're found and bound once. Those in Entry Panel are made on each opening.
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			UInventoryShortcutMenuEntry* Entry = Cast<UInventoryShortcutMenuEntry>(Widget);
			if (Entry && !(EntryPanel && Entry->IsChildOf(EntryPanel.Get())))
			{
				const TWeakObjectPtr<UInventoryShortcutMenuEntry> WeakEntry = Entry;
				Entry->OnClicked().AddWeakLambda(this, [this, WeakEntry]()
				{
					HandleEntryClicked(WeakEntry.Get());
				});
				PlacedEntries.Add(Entry);
			}
		});
	}
}

void UInventoryShortcutMenuWidget::NativeOnItemChanged()
{
	Super::NativeOnItemChanged();

	RefreshEntries();
}

void UInventoryShortcutMenuWidget::RefreshEntries()
{
	const UInventoryShortcutComponent* Shortcuts = GetShortcuts();
	const int32 NumSlots = Shortcuts ? Shortcuts->GetNumSlots() : 0;
	SlotEntries = PlacedEntries;

	// Placed entries for slots the player doesn't have are hidden, not collapsed, so a layout such as a cross keeps its shape.
	for (UInventoryShortcutMenuEntry* Entry : PlacedEntries)
	{
		const bool bHasSlot = Entry->GetSlotIndex() < NumSlots;
		Entry->SetVisibility(bHasSlot ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		if (bHasSlot)
		{
			Entry->SetShortcutSlot(Shortcuts, Entry->GetSlotIndex());
		}
	}

	// The stack reuses menus, so the entries in Entry Panel are made again each time.
	if (EntryPanel)
	{
		EntryPanel->ClearChildren();
		for (int32 NewSlot = 0; SlotEntryClass && NewSlot < NumSlots; ++NewSlot)
		{
			UInventoryShortcutMenuEntry* Entry = CreateWidget<UInventoryShortcutMenuEntry>(this, SlotEntryClass);
			EntryPanel->AddChild(Entry);
			Entry->SetShortcutSlot(Shortcuts, NewSlot);
			const TWeakObjectPtr<UInventoryShortcutMenuEntry> WeakEntry = Entry;
			Entry->OnClicked().AddWeakLambda(this, [this, WeakEntry]()
			{
				HandleEntryClicked(WeakEntry.Get());
			});
			SlotEntries.Add(Entry);
		}
	}
}

UWidget* UInventoryShortcutMenuWidget::NativeGetDesiredFocusTarget() const
{
	// Until the player moves the highlight, and unless a prompt asks or Blueprint says otherwise: the slot the item is on, or the first.
	UCommonButtonBase* Highlighted = GetHighlightedEntry();
	const bool bHighlightUsable = Highlighted && IsEntry(Highlighted) && Highlighted->IsVisible() && Highlighted->GetIsEnabled();
	const bool bBlueprintTarget = GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UInventoryShortcutMenuWidget, BP_GetDesiredFocusTarget));
	if (!bHighlightUsable && !bBlueprintTarget && !IsPrompting())
	{
		const UInventoryShortcutComponent* Shortcuts = GetShortcuts();
		const int32 CurrentSlot = Shortcuts ? Shortcuts->FindSlotOfItem(GetItem()) : INDEX_NONE;
		if (UInventoryShortcutMenuEntry* Current = FindEntry(CurrentSlot))
		{
			return Current;
		}
		for (int32 SlotIndex = 0; Shortcuts && SlotIndex < Shortcuts->GetNumSlots(); ++SlotIndex)
		{
			if (UInventoryShortcutMenuEntry* First = FindEntry(SlotIndex))
			{
				return First;
			}
		}
	}
	return Super::NativeGetDesiredFocusTarget();
}

UInventoryShortcutMenuEntry* UInventoryShortcutMenuWidget::FindEntry(int32 SlotIndex) const
{
	if (SlotIndex == INDEX_NONE)
	{
		return nullptr;
	}
	for (UInventoryShortcutMenuEntry* Entry : SlotEntries)
	{
		if (Entry && Entry->GetSlotIndex() == SlotIndex && Entry->IsVisible())
		{
			return Entry;
		}
	}
	return nullptr;
}

FText UInventoryShortcutMenuWidget::GetEntryDescription(UCommonButtonBase* Entry) const
{
	const UInventoryShortcutMenuEntry* SlotEntry = Cast<UInventoryShortcutMenuEntry>(Entry);
	if (!SlotEntry)
	{
		return Super::GetEntryDescription(Entry);
	}

	const UInventoryShortcutComponent* Shortcuts = GetShortcuts();
	const bool bOnThisSlot = Shortcuts && GetItem().IsSet() && Shortcuts->GetSlotItem(SlotEntry->GetSlotIndex()) == GetItem();
	return bOnThisSlot ? ClearHint : AssignHint;
}

void UInventoryShortcutMenuWidget::HandleEntryClicked(UInventoryShortcutMenuEntry* Entry)
{
	UInventoryShortcutComponent* Shortcuts = GetShortcuts();
	if (!Entry || !Shortcuts || IsClosing() || IsPrompting())
	{
		return;
	}

	// The slot it's on already takes it off. Any other puts it there, moving it from wherever it was.
	const int32 SlotIndex = Entry->GetSlotIndex();
	const FInventoryItemHandle MenuItem = GetItem();
	if (Shortcuts->GetSlotItem(SlotIndex) == MenuItem)
	{
		Shortcuts->ClearSlot(SlotIndex);
	}
	else if (!Shortcuts->AssignItem(SlotIndex, MenuItem))
	{
		if (!RefusedHint.IsEmpty())
		{
			ShowHint(RefusedHint);
		}
		OnAssignRefused(SlotIndex, MenuItem);
		return;
	}

	if (bCloseOnPick)
	{
		CloseMenu();
		return;
	}

	// Open still: the slots as they are now, with the one picked still highlighted.
	RefreshEntries();
	if (UInventoryShortcutMenuEntry* Picked = FindEntry(SlotIndex))
	{
		ClearHighlight();
		HighlightEntry(Picked, /*bFocus*/ false);
	}
	FocusDesiredTarget();
}
