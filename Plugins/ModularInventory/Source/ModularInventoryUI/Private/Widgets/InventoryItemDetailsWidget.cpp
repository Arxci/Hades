#include "Widgets/InventoryItemDetailsWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "InventoryWidgetHelpers.h"

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

void UInventoryItemDetailsWidget::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	if (Changes.Items.Contains(Item))
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
	if (DescriptionText)
	{
		DescriptionText->SetText(Definition ? Definition->Description : FText::GetEmpty());
	}
	if (CountText)
	{
		CountText->SetText(FText::AsNumber(Count));
		CountText->SetVisibility(Count > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bHideWithoutItem)
	{
		SetVisibility(Definition ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	OnItemChanged();
}
