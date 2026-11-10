#include "HadesInventoryItemWidget.h"

#include "CommonActionWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"

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

	if (Background)
	{
		Background->SetVisibility(IsDragVisual() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
	RefreshMarks();
}

void UHadesInventoryItemWidget::RefreshMarks()
{
	const UHadesQuickSlotComponent* Component = QuickSlots.Get();
	if (ShortcutKey)
	{
		const int32 SlotIndex = Component ? Component->FindSlot(GetItem()) : INDEX_NONE;
		if (SlotIndex != INDEX_NONE)
		{
			ShortcutKey->SetInputAction(Component->GetSlotKey(SlotIndex));
		}
		else
		{
			ShortcutKey->SetInputActions(TArray<FDataTableRowHandle>());
		}
	}

	const UHadesInventoryComponent* Inventory = Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn()));
	if (EquippedMarker)
	{
		const bool bInHand = Inventory && Inventory->IsInHand(GetItem());
		EquippedMarker->SetVisibility(bInHand ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (AmmoText)
	{
		const int32 Capacity = Inventory ? Inventory->GetCapacity(GetItem()) : 0;
		AmmoText->SetVisibility(Capacity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Capacity > 0)
		{
			const int32 Rounds = Inventory->GetLoadedRounds(GetItem());
			AmmoText->SetText(FText::AsNumber(Rounds));
			AmmoText->SetColorAndOpacity(AmmoColors.Pick(Rounds, Capacity));
		}
	}
}
