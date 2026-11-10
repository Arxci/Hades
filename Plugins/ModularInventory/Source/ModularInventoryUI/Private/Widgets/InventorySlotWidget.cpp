#include "Widgets/InventorySlotWidget.h"

#include "Components/TextBlock.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "ModularInventory.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemWidget.h"

void UInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetChanged);
	}

	if (ContainerTag.IsValid())
	{
		SetContainer(InventoryWidgets::FindPlayerContainer(*this, ContainerTag));
	}
	else
	{
		Refresh();
	}
}

void UInventorySlotWidget::NativeDestruct()
{
	RegistryListener.Unbind();
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleTargetChanged);
	}

	Super::NativeDestruct();
}

void UInventorySlotWidget::SetContainer(FInventoryContainerHandle InContainer)
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const UInventoryContainerLayout* Layout = Inventory ? Inventory->GetContainerLayout(InContainer) : nullptr;
	const int32 SlotIndex = Layout ? Layout->FindSlot(SlotTag) : INDEX_NONE;

	if (SlotIndex == INDEX_NONE && InContainer.IsSet())
	{
		UE_LOG(LogInventory, Warning, TEXT("[Inventory UI] %s: the container has no slot tagged %s."), *GetPathNameSafe(this), *SlotTag.ToString());
	}

	SetContainerPlacement(SlotIndex != INDEX_NONE ? InContainer : FInventoryContainerHandle(), FInventoryPlacement::AtIndex(FMath::Max(SlotIndex, 0)));
}

void UInventorySlotWidget::SetContainerPlacement(FInventoryContainerHandle InContainer, const FInventoryPlacement& InPlacement)
{
	SetPlacement(InContainer, InPlacement);
	Refresh();
}

void UInventorySlotWidget::Refresh()
{
	const FInventoryItemHandle Item = GetItem();
	if (ItemWidget)
	{
		// Hidden, not collapsed: a Size Box whose only child is collapsed shrinks to nothing, overrides and all.
		ItemWidget->SetItem(Item);
		ItemWidget->SetVisibility(Item.IsSet() ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	if (NameText)
	{
		const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
		const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Item) : nullptr;
		NameText->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
	}

	if (Item != ShownItem)
	{
		ShownItem = Item;
		OnSlotItemChanged(Item);
	}
}

void UInventorySlotWidget::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	if (Changes.Containers.Contains(GetContainer()))
	{
		Refresh();
	}
}

void UInventorySlotWidget::HandleTargetChanged()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const EInventoryDropAction Action = IsTargeted() && Interaction->IsHolding() ? Interaction->GetTargetAction() : EInventoryDropAction::None;
	if (Action != PreviewAction)
	{
		PreviewAction = Action;
		OnPlacementPreviewChanged(Action);
	}
}

// Mouse drops
bool UInventorySlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!Cast<UInventoryDragDropOperation>(InOperation) || !Interaction || !GetContainer().IsSet())
	{
		return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
	}

	Interaction->SetTarget(GetContainer(), GetPlacement());
	return true;
}

void UInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	if (IsTargeted() && !GetInteraction()->bSnapHeldItem)
	{
		GetInteraction()->ClearTarget();
	}
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!Cast<UInventoryDragDropOperation>(InOperation) || !Interaction || !GetContainer().IsSet())
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}
	return Interaction->PlaceHeld(GetContainer(), GetPlacement());
}
