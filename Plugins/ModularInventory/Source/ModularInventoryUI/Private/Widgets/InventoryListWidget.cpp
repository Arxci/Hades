#include "Widgets/InventoryListWidget.h"

#include "Components/PanelWidget.h"

#include "Data/InventoryContainerLayout.h"
#include "InventorySubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "Widgets/InventorySlotWidget.h"

void UInventoryListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	if (ContainerTag.IsValid())
	{
		Container = InventoryWidgets::FindPlayerContainer(*this, ContainerTag);
	}
	Refresh();
}

void UInventoryListWidget::NativeDestruct()
{
	RegistryListener.Unbind();

	Super::NativeDestruct();
}

void UInventoryListWidget::SetContainer(FInventoryContainerHandle InContainer)
{
	Container = InContainer;
	Refresh();
}

UInventorySlotWidget* UInventoryListWidget::GetEntryWidget(int32 Index) const
{
	return Entries.IsValidIndex(Index) ? Entries[Index].Get() : nullptr;
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
		NumEntries = Record->Items.Num() + (bHasRoom ? 1 : 0);
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
