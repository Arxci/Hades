#include "HadesQuickSlotMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/Inventory/HadesQuickSlotComponent.h"
#include "Hades/UI/Foundation/HadesInputKeyWidget.h"
#include "Hades/UI/Inventory/HadesItemMenu.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "Widgets/InventoryItemWidget.h"

// Entry
void UHadesQuickSlotMenuEntry::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Shown in the designer too, so entries placed by hand show which slot they are.
	if (NumberText)
	{
		NumberText->SetText(FText::AsNumber(SlotIndex + 1));
	}
}

void UHadesQuickSlotMenuEntry::SetQuickSlot(int32 InSlotIndex, const UInputAction* SlotInput, FInventoryItemHandle SlotItem)
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
UHadesQuickSlotMenu::UHadesQuickSlotMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OpenTransition = UHadesItemMenu::MakeTransition();
	bCloseReversesOpen = true;
}

void UHadesQuickSlotMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Entries placed in the designer keep their slot, so they're found and bound once. Those in Entry Panel are made on each opening.
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			UHadesQuickSlotMenuEntry* Entry = Cast<UHadesQuickSlotMenuEntry>(Widget);
			if (Entry && !(EntryPanel && Entry->IsChildOf(EntryPanel.Get())))
			{
				Entry->OnClicked().AddUObject(this, &ThisClass::HandleEntryClicked, Entry->GetSlotIndex());
				PlacedEntries.Add(Entry);
			}
		});
	}
}

void UHadesQuickSlotMenu::NativeOnItemChanged()
{
	Super::NativeOnItemChanged();

	const UHadesQuickSlotComponent* QuickSlots = GetQuickSlots();
	const int32 NumSlots = QuickSlots ? QuickSlots->GetNumSlots() : 0;
	Entries = PlacedEntries;

	// Placed entries for slots the player doesn't have are hidden, so a layout such as a cross keeps its shape.
	for (UHadesQuickSlotMenuEntry* Entry : PlacedEntries)
	{
		const int32 EntrySlot = Entry->GetSlotIndex();
		const bool bHasSlot = EntrySlot < NumSlots;
		Entry->SetVisibility(bHasSlot ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		if (bHasSlot)
		{
			Entry->SetQuickSlot(EntrySlot, QuickSlots->GetSlotInputAction(EntrySlot), QuickSlots->GetSlotItem(EntrySlot));
		}
	}

	// The stack reuses menus, so the entries in Entry Panel are made again each time.
	if (EntryPanel)
	{
		EntryPanel->ClearChildren();
		for (int32 NewSlot = 0; EntryClass && NewSlot < NumSlots; ++NewSlot)
		{
			UHadesQuickSlotMenuEntry* Entry = CreateWidget<UHadesQuickSlotMenuEntry>(this, EntryClass);
			EntryPanel->AddChild(Entry);
			Entry->SetQuickSlot(NewSlot, QuickSlots->GetSlotInputAction(NewSlot), QuickSlots->GetSlotItem(NewSlot));
			Entry->OnClicked().AddUObject(this, &ThisClass::HandleEntryClicked, NewSlot);
			Entries.Add(Entry);
		}
	}
}

UWidget* UHadesQuickSlotMenu::NativeGetDesiredFocusTarget() const
{
	// The slot the item is on, or else the first one.
	const UHadesQuickSlotComponent* QuickSlots = GetQuickSlots();
	const int32 CurrentSlot = QuickSlots ? QuickSlots->FindSlot(GetItem()) : INDEX_NONE;
	if (UHadesQuickSlotMenuEntry* Current = FindEntry(CurrentSlot))
	{
		return Current;
	}
	if (UHadesQuickSlotMenuEntry* First = FindEntry(0))
	{
		return First;
	}
	return Super::NativeGetDesiredFocusTarget();
}

UHadesQuickSlotMenuEntry* UHadesQuickSlotMenu::FindEntry(int32 SlotIndex) const
{
	for (UHadesQuickSlotMenuEntry* Entry : Entries)
	{
		if (Entry && Entry->GetSlotIndex() == SlotIndex && Entry->IsVisible())
		{
			return Entry;
		}
	}
	return nullptr;
}

void UHadesQuickSlotMenu::HandleEntryClicked(int32 SlotIndex)
{
	if (IsClosing())
	{
		return;
	}

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
	CloseMenu();
}

UHadesQuickSlotComponent* UHadesQuickSlotMenu::GetQuickSlots() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
}
