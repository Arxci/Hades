#include "HadesInventoryItemWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"
#include "Hades/UI/Foundation/HadesInputKeyWidget.h"

#include "Data/InventoryItemDefinition.h"

namespace
{
	bool IsShown(const UWidget* Widget)
	{
		const ESlateVisibility Visibility = Widget ? Widget->GetVisibility() : ESlateVisibility::Collapsed;
		return Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden;
	}
}

void UHadesInventoryItemWidget::NativeConstruct()
{
	APawn* Pawn = GetOwningPlayerPawn();
	QuickSlots = Pawn ? Pawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
	if (UHadesQuickSlotComponent* Component = QuickSlots.Get())
	{
		Component->OnChanged.AddUniqueDynamic(this, &ThisClass::RefreshMarks);
	}

	Super::NativeConstruct();
}

void UHadesInventoryItemWidget::NativeDestruct()
{
	if (UHadesQuickSlotComponent* Component = QuickSlots.Get())
	{
		Component->OnChanged.RemoveDynamic(this, &ThisClass::RefreshMarks);
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
	const UHadesInventoryComponent* Inventory = Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn()));
	int32 SlotIndex = INDEX_NONE;
	bool bInHand = false;
	int32 Capacity = 0;
	int32 Rounds = 0;
	if (const FInventoryItemPreview* ShownPreview = GetPreview())
	{
		float Value = 0.0f;
		SlotIndex = ShownPreview->FindTagValue(HadesGameplayTags::UI_Preview_Shortcut, Value) && Value >= 1.0f ? FMath::RoundToInt32(Value) - 1 : INDEX_NONE;
		bInHand = ShownPreview->FindTagValue(HadesGameplayTags::UI_Preview_InHand, Value) && Value > 0.0f;

		const FHadesGun* Gun = ShownPreview->Definition ? ShownPreview->Definition->FindFragment<FHadesGun>() : nullptr;
		Capacity = Gun ? Gun->Capacity : 0;
		if (Gun)
		{
			Rounds = ShownPreview->FindTagValue(HadesGameplayTags::Item_Stat_Rounds, Value) ? FMath::RoundToInt32(Value) : FMath::Min(Gun->StartingRounds, Gun->Capacity);
		}
	}
	else
	{
		SlotIndex = Component ? Component->FindSlot(GetItem()) : INDEX_NONE;
		bInHand = Inventory && Inventory->IsInHand(GetItem());
		Capacity = Inventory ? Inventory->GetCapacity(GetItem()) : 0;
		Rounds = Capacity > 0 ? Inventory->GetLoadedRounds(GetItem()) : 0;
	}

	if (ShortcutKeyWidget)
	{
		// The slot's input action comes from the quick slot component, which a preview has none of.
		ShortcutKeyWidget->SetInputAction(SlotIndex != INDEX_NONE && Component ? Component->GetSlotInputAction(SlotIndex) : nullptr);
	}
	if (ShortcutNumberText)
	{
		ShortcutNumberText->SetText(SlotIndex != INDEX_NONE ? FText::AsNumber(SlotIndex + 1) : FText::GetEmpty());
		ShortcutNumberText->SetVisibility(SlotIndex != INDEX_NONE ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ShortcutMarker)
	{
		ShortcutMarker->SetVisibility(SlotIndex != INDEX_NONE ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (EquippedMarker)
	{
		EquippedMarker->SetVisibility(bInHand ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (AmmoText)
	{
		AmmoText->SetVisibility(Capacity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Capacity > 0)
		{
			AmmoText->SetText(FText::AsNumber(Rounds));
			AmmoText->SetColorAndOpacity(AmmoColors.Pick(Rounds, Capacity));
		}
	}

	// Count Text's visibility is set by the base before this runs, for stacks only.
	if (CountPatch)
	{
		CountPatch->SetVisibility(IsShown(CountText) || IsShown(AmmoText) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
