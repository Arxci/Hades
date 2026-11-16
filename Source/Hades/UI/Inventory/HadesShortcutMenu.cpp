#include "HadesShortcutMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/Inventory/HadesQuickSlotComponent.h"
#include "Hades/UI/Foundation/HadesInputKeyWidget.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "Widgets/InventoryItemWidget.h"

// Entry
void UHadesShortcutEntry::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Shown in the designer too, so entries placed by hand show which slot they are.
	if (NumberText)
	{
		NumberText->SetText(FText::AsNumber(SlotIndex + 1));
	}
}

void UHadesShortcutEntry::SetShortcut(int32 InSlotIndex, const UInputAction* SlotInput, FInventoryItemHandle SlotItem)
{
	SlotIndex = InSlotIndex;
	if (NumberText)
	{
		NumberText->SetText(FText::AsNumber(SlotIndex + 1));
	}
	if (KeyWidget)
	{
		KeyWidget->SetInputAction(SlotInput);
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
		const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
		const UInventoryItemDefinition* Definition = Subsystem ? Subsystem->GetItemDefinition(SlotItem) : nullptr;
		NameText->SetText(Definition ? Definition->DisplayName : EmptyText);
	}
}

// Menu
UHadesShortcutMenu::UHadesShortcutMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// As the item menu it opens from: a click outside only closes it, and a right-click on another item opens that item's menu instead.
	LeftPressOutside = EInventoryMenuOutsidePress::Close;
	RightPressOutside = EInventoryMenuOutsidePress::PassThrough;
}

void UHadesShortcutMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Entries placed in the designer keep their slot, so they're found and bound once. Those in Entry Panel are made on each opening.
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			UHadesShortcutEntry* Entry = Cast<UHadesShortcutEntry>(Widget);
			if (Entry && !(EntryPanel && Entry->IsChildOf(EntryPanel.Get())))
			{
				Entry->OnClicked().AddUObject(this, &ThisClass::HandleEntryClicked, Entry->GetSlotIndex());
				PlacedEntries.Add(Entry);
			}
		});
	}
}

void UHadesShortcutMenu::NativeOnItemChanged()
{
	Super::NativeOnItemChanged();

	const UHadesQuickSlotComponent* QuickSlots = GetQuickSlots();
	const int32 NumSlots = QuickSlots ? QuickSlots->GetNumSlots() : 0;
	Entries = PlacedEntries;

	// Placed entries for slots the player doesn't have are hidden, so a layout such as a cross keeps its shape.
	for (UHadesShortcutEntry* Entry : PlacedEntries)
	{
		const int32 EntrySlot = Entry->GetSlotIndex();
		const bool bHasSlot = EntrySlot < NumSlots;
		Entry->SetVisibility(bHasSlot ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		if (bHasSlot)
		{
			Entry->SetShortcut(EntrySlot, QuickSlots->GetSlotInputAction(EntrySlot), QuickSlots->GetSlotItem(EntrySlot));
		}
	}

	// The stack reuses menus, so the entries in Entry Panel are made again each time.
	if (EntryPanel)
	{
		EntryPanel->ClearChildren();
		for (int32 NewSlot = 0; EntryClass && NewSlot < NumSlots; ++NewSlot)
		{
			UHadesShortcutEntry* Entry = CreateWidget<UHadesShortcutEntry>(this, EntryClass);
			EntryPanel->AddChild(Entry);
			Entry->SetShortcut(NewSlot, QuickSlots->GetSlotInputAction(NewSlot), QuickSlots->GetSlotItem(NewSlot));
			Entry->OnClicked().AddUObject(this, &ThisClass::HandleEntryClicked, NewSlot);
			Entries.Add(Entry);
		}
	}
}

UWidget* UHadesShortcutMenu::NativeGetDesiredFocusTarget() const
{
	// The shortcut the item is on, or else the first one.
	const UHadesQuickSlotComponent* QuickSlots = GetQuickSlots();
	const int32 CurrentSlot = QuickSlots ? QuickSlots->FindSlot(GetItem()) : INDEX_NONE;
	if (UHadesShortcutEntry* Current = FindEntry(CurrentSlot))
	{
		return Current;
	}
	if (UHadesShortcutEntry* First = FindEntry(0))
	{
		return First;
	}
	return Super::NativeGetDesiredFocusTarget();
}

UHadesShortcutEntry* UHadesShortcutMenu::FindEntry(int32 SlotIndex) const
{
	for (UHadesShortcutEntry* Entry : Entries)
	{
		if (Entry && Entry->GetSlotIndex() == SlotIndex && Entry->IsVisible())
		{
			return Entry;
		}
	}
	return nullptr;
}

void UHadesShortcutMenu::HandleEntryClicked(int32 SlotIndex)
{
	if (UHadesQuickSlotComponent* QuickSlots = GetQuickSlots())
	{
		if (QuickSlots->GetSlotItem(SlotIndex) == GetItem())
		{
			QuickSlots->ClearSlot(SlotIndex);
		}
		else
		{
			QuickSlots->AssignItem(SlotIndex, GetItem());
		}
	}
	DeactivateWidget();
}

UHadesQuickSlotComponent* UHadesShortcutMenu::GetQuickSlots() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
}
