#include "Widgets/InventoryListWidget.h"

#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"

#include "Data/InventoryContainerLayout.h"
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

void UInventoryListWidget::Refresh()
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const FInventoryContainerRecord* Record = Inventory ? Inventory->GetRegistry().FindContainer(Container) : nullptr;

	int32 NumEntries = 0;
	if (Record && Record->Layout->Type == EInventoryContainerType::List && EntryClass && EntryPanel)
	{
		const int32 Capacity = Record->Layout->Capacity;
		const bool bHasRoom = Capacity == 0 || Record->Items.Num() < Capacity;
		const bool bEmptyEntry = bShowEmptyEntry && bHasRoom && !Record->Layout->bReadOnlyInUI;
		NumEntries = Record->Items.Num() + (bEmptyEntry ? 1 : 0);
	}

	while (Entries.Num() < NumEntries)
	{
		UInventorySlotWidget* Entry = CreateWidget<UInventorySlotWidget>(this, EntryClass);
		EntryPanel->AddChild(Entry);
		Entries.Add(Entry);
	}
	while (Entries.Num() > NumEntries)
	{
		Entries.Pop()->RemoveFromParent();
	}

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index]->SetContainerPlacement(Container, FInventoryPlacement::AtIndex(Index));
	}
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
