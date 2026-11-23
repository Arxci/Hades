#include "Widgets/InventoryListWidget.h"

#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventorySlotWidget.h"

void UInventoryListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	if (ContainerTag.IsValid())
	{
		if (APlayerController* Player = GetOwningPlayer())
		{
			Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
		}
		Container = InventoryWidgets::FindPlayerContainer(*this, ContainerTag);
	}
	Refresh();
}

void UInventoryListWidget::NativeDestruct()
{
	RegistryListener.Unbind();
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}

	Super::NativeDestruct();
}

void UInventoryListWidget::SetContainer(FInventoryContainerHandle InContainer)
{
	Container = InContainer;
	Refresh();
}

void UInventoryListWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	SetContainer(InventoryWidgets::FindPawnContainer(NewPawn, ContainerTag));
}

UInventorySlotWidget* UInventoryListWidget::GetEntryWidget(int32 Index) const
{
	return Entries.IsValidIndex(Index) ? Entries[Index].Get() : nullptr;
}

UInventoryItemWidget* UInventoryListWidget::GetItemWidget(FInventoryItemHandle Item) const
{
	for (const UInventorySlotWidget* Entry : Entries)
	{
		if (UInventoryItemWidget* Found = Entry ? Entry->GetItemWidget(Item) : nullptr)
		{
			return Found;
		}
	}
	return nullptr;
}

void UInventoryListWidget::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	if (Changes.Containers.Contains(Container))
	{
		Refresh();
	}
}

void UInventoryListWidget::SetDisplayFilter(const FInventoryItemQuery& InDisplayFilter)
{
	DisplayFilter = InDisplayFilter;
	Refresh();
}

void UInventoryListWidget::SetSort(EInventoryListSort InSortBy, FGameplayTag InSortTag, bool bInSortDescending)
{
	SortBy = InSortBy;
	SortTag = InSortTag;
	bSortDescending = bInSortDescending;
	Refresh();
}

void UInventoryListWidget::Refresh()
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const FInventoryContainerRecord* Record = Inventory ? Inventory->GetRegistry().FindContainer(Container) : nullptr;

	// The positions in the container of the items shown, in the order shown, then the end of the list for the empty entry. Worked out
	// before any entry is filled in, as entries run Blueprint events, which could change the inventory.
	TArray<int32> Positions;
	int32 EndPosition = 0;
	if (Record && Record->Layout->Type == EInventoryContainerType::List && EntryClass && EntryPanel)
	{
		const TArray<FInventoryItemHandle> Items = Record->Items;
		for (int32 Position = 0; Position < Items.Num(); ++Position)
		{
			if (Inventory->MatchesQuery(Items[Position], DisplayFilter))
			{
				Positions.Add(Position);
			}
		}
		SortPositions(*Inventory, Items, Positions);

		// A list without a limit always has room. The empty entry is where an item put on it goes: the end.
		const int32 Capacity = Inventory->GetCapacity(Container);
		const bool bHasRoom = Capacity == INDEX_NONE || Items.Num() < Capacity;
		EndPosition = Items.Num();
		if (bShowEmptyEntry && bHasRoom && Record->Layout->CanPlayerPutIn())
		{
			Positions.Add(EndPosition);
		}
	}

	while (Entries.Num() < Positions.Num())
	{
		UInventorySlotWidget* Entry = CreateWidget<UInventorySlotWidget>(this, EntryClass);
		EntryPanel->AddChild(Entry);
		Entries.Add(Entry);
	}
	while (Entries.Num() > Positions.Num())
	{
		Entries.Pop()->RemoveFromParent();
	}

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index]->SetContainerPlacement(Container, FInventoryPlacement::AtIndex(Positions[Index]));
	}
}

void UInventoryListWidget::SortPositions(const UInventorySubsystem& Inventory, const TArray<FInventoryItemHandle>& Items, TArray<int32>& InOutPositions) const
{
	if (SortBy == EInventoryListSort::ContainerOrder)
	{
		return;
	}

	// Ties go by name, so the order never jumps about, and then by where they are in the container.
	InOutPositions.StableSort([this, &Inventory, &Items](int32 A, int32 B)
	{
		const FInventoryItemHandle ItemA = Items[A];
		const FInventoryItemHandle ItemB = Items[B];
		const UInventoryItemDefinition* DefinitionA = Inventory.GetItemDefinition(ItemA);
		const UInventoryItemDefinition* DefinitionB = Inventory.GetItemDefinition(ItemB);

		int32 Order = 0;
		switch (SortBy)
		{
		case EInventoryListSort::Category:
		{
			const FText CategoryA = DefinitionA ? DefinitionA->Category : FText::GetEmpty();
			const FText CategoryB = DefinitionB ? DefinitionB->Category : FText::GetEmpty();
			Order = CategoryA.CompareTo(CategoryB);
			break;
		}
		case EInventoryListSort::TagValue:
		{
			const float ValueA = Inventory.GetEffectiveTagValue(ItemA, SortTag);
			const float ValueB = Inventory.GetEffectiveTagValue(ItemB, SortTag);
			Order = ValueA < ValueB ? -1 : ValueA > ValueB ? 1 : 0;
			break;
		}
		case EInventoryListSort::Count:
		{
			const int32 CountA = Inventory.GetItemCount(ItemA);
			const int32 CountB = Inventory.GetItemCount(ItemB);
			Order = CountA < CountB ? -1 : CountA > CountB ? 1 : 0;
			break;
		}
		case EInventoryListSort::Name:
		case EInventoryListSort::ContainerOrder:
			break;
		}

		if (Order == 0)
		{
			const FText NameA = DefinitionA ? DefinitionA->DisplayName : FText::GetEmpty();
			const FText NameB = DefinitionB ? DefinitionB->DisplayName : FText::GetEmpty();
			Order = NameA.CompareTo(NameB);
		}
		return bSortDescending ? Order > 0 : Order < 0;
	});
}

// Mouse drops between and after the entries, which take drops on themselves
UInventoryUISubsystem* UInventoryListWidget::GetDropInteraction(const UDragDropOperation* Operation) const
{
	return Cast<UInventoryDragDropOperation>(Operation) && Container.IsSet() ? UInventoryUISubsystem::Get(GetOwningLocalPlayer()) : nullptr;
}

FInventoryPlacement UInventoryListWidget::GetEndPlacement() const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const FInventoryContainerRecord* Record = Inventory ? Inventory->GetRegistry().FindContainer(Container) : nullptr;
	return FInventoryPlacement::AtIndex(Record ? Record->Items.Num() : 0);
}

bool UInventoryListWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryUISubsystem* Interaction = GetDropInteraction(InOperation);
	if (!Interaction)
	{
		return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
	}

	Interaction->SetTarget(Container, GetEndPlacement());
	return true;
}

void UInventoryListWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	UInventoryUISubsystem* Interaction = GetDropInteraction(InOperation);
	if (Interaction && !Interaction->GetStyle().bSnapHeldItem && Interaction->GetTargetContainer() == Container)
	{
		Interaction->ClearTarget();
	}
}

bool UInventoryListWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryUISubsystem* Interaction = GetDropInteraction(InOperation);
	if (!Interaction)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}
	return Interaction->PlaceHeld(Container, GetEndPlacement());
}
