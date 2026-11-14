#include "HadesShortcutMenu.h"

#include "CommonActionWidget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/Inventory/HadesQuickSlotComponent.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "Widgets/InventoryItemWidget.h"

// Entry
void UHadesShortcutEntry::SetShortcut(const FDataTableRowHandle& Key, FInventoryItemHandle SlotItem)
{
	if (KeyIcon)
	{
		KeyIcon->SetInputAction(Key);
	}
	if (ItemWidget)
	{
		ItemWidget->SetItem(SlotItem);
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
void UHadesShortcutMenu::NativeOnItemChanged()
{
	Super::NativeOnItemChanged();

	// The stack reuses menus, so the entries are made again each time.
	EntryPanel->ClearChildren();
	Entries.Reset();

	const UHadesQuickSlotComponent* QuickSlots = GetQuickSlots();
	for (int32 SlotIndex = 0; QuickSlots && EntryClass && SlotIndex < QuickSlots->GetNumSlots(); ++SlotIndex)
	{
		UHadesShortcutEntry* Entry = CreateWidget<UHadesShortcutEntry>(this, EntryClass);
		EntryPanel->AddChild(Entry);
		Entry->SetShortcut(QuickSlots->GetSlotKey(SlotIndex), QuickSlots->GetSlotItem(SlotIndex));
		Entry->OnClicked().AddUObject(this, &ThisClass::HandleEntryClicked, SlotIndex);
		Entries.Add(Entry);
	}
}

UWidget* UHadesShortcutMenu::NativeGetDesiredFocusTarget() const
{
	const UHadesQuickSlotComponent* QuickSlots = GetQuickSlots();
	const int32 CurrentSlot = QuickSlots ? QuickSlots->FindSlot(GetItem()) : INDEX_NONE;
	if (Entries.IsValidIndex(CurrentSlot))
	{
		return Entries[CurrentSlot].Get();
	}
	return Entries.IsEmpty() ? Super::NativeGetDesiredFocusTarget() : Entries[0].Get();
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
