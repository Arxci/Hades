#include "HadesEquippedItemWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "Widgets/InventoryItemWidget.h"

void UHadesEquippedItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The quick slot component also says when what's in hand changes.
	APawn* Pawn = GetOwningPlayerPawn();
	QuickSlots = Pawn ? Pawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
	if (UHadesQuickSlotComponent* Component = QuickSlots.Get())
	{
		Component->OnChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	}
	Refresh();
}

void UHadesEquippedItemWidget::NativeDestruct()
{
	if (UHadesQuickSlotComponent* Component = QuickSlots.Get())
	{
		Component->OnChanged.RemoveDynamic(this, &ThisClass::Refresh);
	}
	QuickSlots.Reset();

	Super::NativeDestruct();
}

void UHadesEquippedItemWidget::Refresh()
{
	const UHadesInventoryComponent* Inventory = Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn()));
	const FInventoryItemHandle Item = Inventory ? Inventory->GetItemInHand() : FInventoryItemHandle();

	if (ItemWidget)
	{
		// Hidden, not collapsed, so a Size Box around it keeps its size.
		ItemWidget->SetItem(Item);
		ItemWidget->SetVisibility(Item.IsSet() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (NameText)
	{
		const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
		const UInventoryItemDefinition* Definition = Subsystem ? Subsystem->GetItemDefinition(Item) : nullptr;
		NameText->SetText(Definition ? Definition->DisplayName : EmptyText);
	}
	if (EmptyMarker)
	{
		EmptyMarker->SetVisibility(Item.IsSet() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
