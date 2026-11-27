#include "Widgets/InventoryEquippedItemWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Data/InventoryItemDefinition.h"
#include "Equipment/InventoryEquipmentComponent.h"
#include "InventoryRegistry.h"
#include "InventorySubsystem.h"
#include "Widgets/InventoryItemWidget.h"

void UInventoryEquippedItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// A HUD is often made before the pawn exists, so the component is found again as the pawn changes.
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	}
	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateWeakLambda(this, [this](const FInventoryChangeSet& Changes)
	{
		if (ShouldRefreshFor(Changes))
		{
			Refresh();
		}
	}));
	BindEquipment(UInventoryEquipmentComponent::FindEquipmentComponent(GetOwningPlayerPawn()));
}

void UInventoryEquippedItemWidget::NativeDestruct()
{
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}
	RegistryListener.Unbind();

	// Unbound without refreshing, so no Blueprint event fires as the widget goes.
	if (UInventoryEquipmentComponent* Previous = Equipment.Get())
	{
		Previous->OnItemEquipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
		Previous->OnItemUnequipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Equipment.Reset();

	Super::NativeDestruct();
}

void UInventoryEquippedItemWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindEquipment(UInventoryEquipmentComponent::FindEquipmentComponent(NewPawn));
}

void UInventoryEquippedItemWidget::BindEquipment(UInventoryEquipmentComponent* NewEquipment)
{
	if (UInventoryEquipmentComponent* Previous = Equipment.Get())
	{
		Previous->OnItemEquipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
		Previous->OnItemUnequipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}

	Equipment = NewEquipment;
	if (NewEquipment)
	{
		NewEquipment->OnItemEquipped.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
		NewEquipment->OnItemUnequipped.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Refresh();
}

void UInventoryEquippedItemWidget::HandleEquipmentChanged(FInventoryItemHandle Item, FGameplayTag EquipSlot)
{
	Refresh();
}

void UInventoryEquippedItemWidget::SetSlots(const TArray<FGameplayTag>& InSlots)
{
	Slots = InSlots;
	Refresh();
}

bool UInventoryEquippedItemWidget::ShouldRefreshFor(const FInventoryChangeSet& Changes) const
{
	if (ShownItem.IsSet() && Changes.Items.Contains(ShownItem))
	{
		return true;
	}

	const UInventoryEquipmentComponent* Component = Equipment.Get();
	return Component && Slots.ContainsByPredicate([&](const FGameplayTag& EquipSlot)
	{
		const FInventoryContainerHandle Container = Component->FindEquipmentContainer(EquipSlot);
		return Container.IsSet() && Changes.Containers.Contains(Container);
	});
}

bool UInventoryEquippedItemWidget::IsItemActive(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	return Subsystem && Item.IsSet() && ActiveTag.IsValid() && (Subsystem->HasItemTag(Item, ActiveTag) || Subsystem->GetTagValue(Item, ActiveTag) > 0.0f);
}

void UInventoryEquippedItemWidget::Refresh()
{
	const UInventoryEquipmentComponent* Component = Equipment.Get();
	FInventoryItemHandle Item;
	FGameplayTag ItemSlot;
	for (const FGameplayTag& EquipSlot : Slots)
	{
		const FInventoryItemHandle Equipped = Component && EquipSlot.IsValid() ? Component->GetEquippedItem(EquipSlot) : FInventoryItemHandle();
		if (Equipped.IsSet())
		{
			Item = Equipped;
			ItemSlot = EquipSlot;
			break;
		}
	}
	const bool bActive = IsItemActive(Item);

	// Kept before anything shows, so a subclass's Native On Refresh reads the new item.
	const bool bFirstShow = !bShownOnce;
	const bool bItemChanged = Item != ShownItem || ItemSlot != ShownSlot || bFirstShow;
	const bool bActiveChanged = bActive != bShownActive || bFirstShow;
	ShownItem = Item;
	ShownSlot = ItemSlot;
	bShownActive = bActive;
	bShownOnce = true;

	if (ItemWidget)
	{
		// Hidden, not collapsed, so a Size Box around it keeps its size. It never takes clicks itself: an item widget would start dragging
		// the item out of its slot.
		ItemWidget->SetItem(Item);
		ItemWidget->SetVisibility(Item.IsSet() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (NameText)
	{
		const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
		const UInventoryItemDefinition* Definition = Subsystem && Item.IsSet() ? Subsystem->GetItemDefinition(Item) : nullptr;
		NameText->SetText(Definition ? Definition->DisplayName : EmptyText);
	}
	if (EmptyMarker)
	{
		EmptyMarker->SetVisibility(Item.IsSet() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (ActiveMarker)
	{
		ActiveMarker->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	NativeOnRefresh(Item);

	if (bItemChanged)
	{
		OnEquippedItemChanged(Item, ItemSlot);
	}
	if (bActiveChanged)
	{
		OnActiveChanged(bActive);
	}
}
