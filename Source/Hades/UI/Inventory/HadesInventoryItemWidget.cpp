#include "HadesInventoryItemWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"

#include "Widgets/ModularInputKeyWidget.h"

void UHadesInventoryItemWidget::NativeConstruct()
{
	APawn* Pawn = GetOwningPlayerPawn();
	QuickSlots = Pawn ? Pawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
	if (UHadesQuickSlotComponent* Component = QuickSlots.Get())
	{
		Component->OnShortcutsChanged.AddUniqueDynamic(this, &ThisClass::RefreshMarks);
	}

	Super::NativeConstruct();
}

void UHadesInventoryItemWidget::NativeDestruct()
{
	if (UHadesQuickSlotComponent* Component = QuickSlots.Get())
	{
		Component->OnShortcutsChanged.RemoveDynamic(this, &ThisClass::RefreshMarks);
	}
	QuickSlots.Reset();

	Super::NativeDestruct();
}

void UHadesInventoryItemWidget::NativeOnItemChanged()
{
	Super::NativeOnItemChanged();

	RefreshMarks();
}

void UHadesInventoryItemWidget::RefreshMarks()
{
	// From the player's inventory, or from a designer preview's tag values, as the designer has no inventory.
	const UHadesQuickSlotComponent* Component = QuickSlots.Get();
	int32 SlotIndex = INDEX_NONE;
	bool bInHand = false;
	if (const FInventoryItemPreview* ShownPreview = GetPreview())
	{
		float Value = 0.0f;
		SlotIndex = ShownPreview->FindTagValue(HadesGameplayTags::UI_Preview_QuickSlot, Value) && Value >= 1.0f ? FMath::RoundToInt32(Value) - 1 : INDEX_NONE;
		bInHand = ShownPreview->FindTagValue(HadesGameplayTags::UI_Preview_InHand, Value) && Value > 0.0f;
	}
	else
	{
		const UHadesInventoryComponent* Inventory = Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn()));
		SlotIndex = Component ? Component->FindSlotOfItem(GetItem()) : INDEX_NONE;
		bInHand = Inventory && Inventory->IsInHand(GetItem());
	}

	if (QuickSlotKeyWidget)
	{
		// The slot's input action comes from the quick slot component, which a preview has none of.
		QuickSlotKeyWidget->SetInputAction(SlotIndex != INDEX_NONE && Component ? Component->GetSlotInputAction(SlotIndex) : nullptr);
	}
	if (QuickSlotNumberText)
	{
		QuickSlotNumberText->SetText(SlotIndex != INDEX_NONE ? FText::AsNumber(SlotIndex + 1) : FText::GetEmpty());
		QuickSlotNumberText->SetVisibility(SlotIndex != INDEX_NONE ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (QuickSlotMarker)
	{
		QuickSlotMarker->SetVisibility(SlotIndex != INDEX_NONE ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (EquippedMarker)
	{
		EquippedMarker->SetVisibility(bInHand ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
