#include "Widgets/InventoryItemDetailsWidget.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

#include "Equipment/InventoryEquipmentComponent.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "Widgets/InventoryStatWidget.h"

void UInventoryItemDetailsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (NameText)
	{
		DefaultNameColor = NameText->GetColorAndOpacity();
	}
}

void UInventoryItemDetailsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));

	UInventoryUISubsystem* Interaction = UInventoryUISubsystem::Get(GetOwningLocalPlayer());
	if (bFollowHoveredItem && Interaction)
	{
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::FollowInteraction);
		FollowInteraction();
	}

	Equipment = bCompareWithEquipped ? UInventoryEquipmentComponent::FindEquipmentComponent(GetOwningPlayerPawn()) : nullptr;
	if (UInventoryEquipmentComponent* EquipmentComponent = Equipment.Get())
	{
		EquipmentComponent->OnItemEquipped.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
		EquipmentComponent->OnItemUnequipped.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Refresh();
}

void UInventoryItemDetailsWidget::NativeDestruct()
{
	RegistryListener.Unbind();
	if (UInventoryUISubsystem* Interaction = UInventoryUISubsystem::Get(GetOwningLocalPlayer()))
	{
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::FollowInteraction);
	}
	if (UInventoryEquipmentComponent* EquipmentComponent = Equipment.Get())
	{
		EquipmentComponent->OnItemEquipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
		EquipmentComponent->OnItemUnequipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Equipment.Reset();
	InventoryWidgets::CancelLoad(IconHandle);

	Super::NativeDestruct();
}

void UInventoryItemDetailsWidget::SetItem(FInventoryItemHandle InItem)
{
	Item = InItem;
	Refresh();
}

void UInventoryItemDetailsWidget::HandleHoveredItemChanged(FInventoryItemHandle HoveredItem)
{
	FollowInteraction();
}

void UInventoryItemDetailsWidget::FollowInteraction()
{
	const UInventoryUISubsystem* Interaction = UInventoryUISubsystem::Get(GetOwningLocalPlayer());
	const FInventoryItemHandle Followed = !Interaction ? FInventoryItemHandle() : Interaction->IsHolding() ? Interaction->GetHeldItem() : Interaction->GetHoveredItem();
	if (Followed.IsSet() || !bKeepLastItem)
	{
		SetItem(Followed);
	}
}

void UInventoryItemDetailsWidget::HandleEquipmentChanged(FInventoryItemHandle EquippedItem, FGameplayTag EquipSlot)
{
	Refresh();
}

void UInventoryItemDetailsWidget::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	if (Changes.Items.Contains(Item) || (ComparedItem.IsSet() && Changes.Items.Contains(ComparedItem)))
	{
		Refresh();
	}
}

void UInventoryItemDetailsWidget::Refresh()
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Item) : nullptr;
	const int32 Count = Inventory ? Inventory->GetItemCount(Item) : 0;

	if (Definition != ShownDefinition.Get())
	{
		ShownDefinition = Definition;
		InventoryWidgets::ShowIcon(Icon, Definition, IconHandle);
	}
	if (NameText)
	{
		NameText->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
	}
	if (NameText && !TagColors.IsEmpty())
	{
		const FLinearColor* TagColor = Inventory ? InventoryWidgets::FindTagColor(TagColors, Inventory->GetEffectiveItemTags(Item)) : nullptr;
		NameText->SetColorAndOpacity(TagColor ? FSlateColor(*TagColor) : DefaultNameColor);
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(Definition ? Definition->Description : FText::GetEmpty());
	}
	if (CountText)
	{
		CountText->SetText(FText::AsNumber(Count));
		CountText->SetVisibility(Count > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshStats(Inventory);
	if (bHideWithoutItem)
	{
		SetVisibility(Definition ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	OnItemChanged();
}

void UInventoryItemDetailsWidget::RefreshStats(const UInventorySubsystem* Inventory)
{
	const UInventoryEquipmentComponent* EquipmentComponent = Equipment.Get();
	ComparedItem = EquipmentComponent ? EquipmentComponent->FindEquippedItemFor(Item) : FInventoryItemHandle();
	if (!StatPanel || !StatClass)
	{
		return;
	}

	// Rows are kept and reused, and the ones left over are collapsed.
	int32 NumShown = 0;
	for (const FInventoryStatDisplay& Stat : Stats)
	{
		if (!Inventory || !Inventory->HasTagValue(Item, Stat.Tag))
		{
			continue;
		}
		if (!StatRows.IsValidIndex(NumShown))
		{
			UInventoryStatWidget* NewRow = CreateWidget<UInventoryStatWidget>(this, StatClass);
			if (!NewRow)
			{
				break;
			}
			StatPanel->AddChild(NewRow);
			StatRows.Add(NewRow);
		}

		const float Max = Stat.bShowMax ? Inventory->GetTagValueMax(Item, Stat.Tag, Stat.MaxTag) : 0.0f;
		const float ComparedValue = ComparedItem.IsSet() ? Inventory->GetEffectiveTagValue(ComparedItem, Stat.Tag) : 0.0f;
		UInventoryStatWidget* Row = StatRows[NumShown++];
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Row->SetStat(Stat, Inventory->GetEffectiveTagValue(Item, Stat.Tag), Max, ComparedItem.IsSet(), ComparedValue);
	}
	for (int32 Index = NumShown; Index < StatRows.Num(); ++Index)
	{
		StatRows[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}
