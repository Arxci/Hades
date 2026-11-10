#include "Widgets/InventoryCellWidget.h"

#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"

void UInventoryCellWidget::SetPlacement(FInventoryContainerHandle InContainer, const FInventoryPlacement& InPlacement)
{
	Container = InContainer;
	Placement = InPlacement;
}

FInventoryItemHandle UInventoryCellWidget::GetItem() const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	return Inventory ? Inventory->GetItemAt(Container, Placement) : FInventoryItemHandle();
}

UInventoryUISubsystem* UInventoryCellWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

bool UInventoryCellWidget::IsTargeted() const
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	return Interaction && Container.IsSet() && Interaction->GetTargetContainer() == Container && Interaction->GetTargetPlacement() == Placement;
}

void UInventoryCellWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnHovered().AddUObject(this, &ThisClass::Aim);
	OnUnhovered().AddUObject(this, &ThisClass::StopAiming);
	OnClicked().AddUObject(this, &ThisClass::HandleClicked);
	IdleStyle = GetStyle() ? GetStyle()->GetClass() : nullptr;
}

void UInventoryCellWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
	}
	HandleHeldItemChanged();
}

void UInventoryCellWidget::NativeDestruct()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
	}

	Super::NativeDestruct();
}

void UInventoryCellWidget::HandleHeldItemChanged()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (HoldingStyle && IdleStyle)
	{
		SetStyle(Interaction && Interaction->IsHolding() ? HoldingStyle : IdleStyle);
	}
}

// Gamepads aim by focus. Hover alone isn't enough: an item drawn over a grid cell takes the cell's hover.
void UInventoryCellWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	Aim();
}

void UInventoryCellWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	StopAiming();
}

void UInventoryCellWidget::Aim()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SetTarget(Container, Placement);
		Interaction->SetHoveredItem(GetItem());
	}
}

void UInventoryCellWidget::StopAiming()
{
	// A snapped held item stays on the last cells aimed at.
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (IsTargeted() && !(Interaction->bSnapHeldItem && Interaction->IsHolding()))
	{
		GetInteraction()->ClearTarget();
	}
}

void UInventoryCellWidget::HandleClicked()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->ActivatePlacement(Container, Placement);
	}
}
