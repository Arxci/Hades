#include "HadesQuickSlotWidget.h"

#include "Components/TextBlock.h"

#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Player/HadesPlayerController.h"

#include "Shortcuts/InventoryShortcutComponent.h"

#define LOCTEXT_NAMESPACE "HadesQuickSlotWidget"

bool UHadesQuickSlotWidget::ShouldRefreshFor(const FInventoryChangeSet& Changes) const
{
	// A gun's spare rounds are in other items, so any change can change them.
	return true;
}

void UHadesQuickSlotWidget::NativeOnRefresh(FInventoryItemHandle Item)
{
	Super::NativeOnRefresh(Item);

	const UInventoryShortcutComponent* Component = GetShortcuts();
	const UHadesInventoryComponent* Inventory = Component ? Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(Component->GetOwner())) : nullptr;
	const int32 Capacity = Inventory ? Inventory->GetCapacity(Item) : 0;
	const ESlateVisibility AmmoVisibility = Capacity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (AmmoText)
	{
		AmmoText->SetVisibility(AmmoVisibility);
		if (Capacity > 0)
		{
			const int32 Rounds = Inventory->GetLoadedRounds(Item);
			AmmoText->SetText(FText::AsNumber(Rounds));
			AmmoText->SetColorAndOpacity(FSlateColor(AmmoColors.Pick(static_cast<float>(Rounds) / Capacity)));
		}
	}
	if (SpareAmmoText)
	{
		SpareAmmoText->SetVisibility(AmmoVisibility);
		if (Capacity > 0)
		{
			SpareAmmoText->SetText(FText::Format(LOCTEXT("SpareRounds", "/{0}"), Inventory->GetSpareRounds(Item)));
		}
	}

	const bool bNowInHand = Inventory && Inventory->IsInHand(Item);
	if (bNowInHand != bInHand)
	{
		bInHand = bNowInHand;
		OnInHandChanged(bInHand);
	}
}

void UHadesQuickSlotWidget::NativeOnAssignRefused(FInventoryItemHandle Item)
{
	Super::NativeOnAssignRefused(Item);

	AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("CantAssign", "Only things to hold or use go in quick slots."));
}

#undef LOCTEXT_NAMESPACE
