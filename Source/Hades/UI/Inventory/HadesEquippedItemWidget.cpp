#include "HadesEquippedItemWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/Inventory/HadesInventoryComponent.h"

#include "Data/InventoryItemDefinition.h"
#include "Equipment/InventoryEquipmentComponent.h"
#include "InventorySubsystem.h"
#include "Widgets/InventoryItemWidget.h"

void UHadesEquippedItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(GetOwningPlayerPawn());
	if (UInventoryEquipmentComponent* Component = Equipment.Get())
	{
		Component->OnItemEquipped.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
		Component->OnItemUnequipped.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateWeakLambda(this, [this](const FInventoryChangeSet&) { Refresh(); }));
	Refresh();
}

void UHadesEquippedItemWidget::NativeDestruct()
{
	if (UInventoryEquipmentComponent* Component = Equipment.Get())
	{
		Component->OnItemEquipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
		Component->OnItemUnequipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Equipment.Reset();
	RegistryListener.Unbind();

	Super::NativeDestruct();
}

void UHadesEquippedItemWidget::HandleEquipmentChanged(FInventoryItemHandle Item, FGameplayTag EquipSlot)
{
	Refresh();
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
