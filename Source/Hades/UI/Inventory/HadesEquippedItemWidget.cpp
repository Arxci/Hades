#include "HadesEquippedItemWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesInventoryComponent.h"

#include "InventoryRegistry.h"
#include "InventorySubsystem.h"
#include "Widgets/ModularSegmentedMeter.h"

#define LOCTEXT_NAMESPACE "HadesEquippedItem"

UHadesEquippedItemWidget::UHadesEquippedItemWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Slots = { FGameplayTag(HadesGameplayTags::Equipment_Slot_Hand_Right), FGameplayTag(HadesGameplayTags::Equipment_Slot_Hand_Left) };
	RoundsFormat = LOCTEXT("RoundsFormat", "{Rounds} / {Capacity}");
	SpareRoundsFormat = LOCTEXT("SpareRoundsFormat", "{Spare}");
}

void UHadesEquippedItemWidget::NativeOnInitialized()
{
	// The colors set in the designer, which low values replace for a while.
	if (RoundsText)
	{
		DefaultRoundsColor = RoundsText->GetColorAndOpacity();
	}
	if (ChargeMeter)
	{
		DefaultChargeColor = ChargeMeter->GetFillColor();
	}

	Super::NativeOnInitialized();
}

bool UHadesEquippedItemWidget::ShouldRefreshFor(const FInventoryChangeSet& Changes) const
{
	// A gun's spare rounds are ammo anywhere in the inventory, which changes without the gun changing.
	const UHadesInventoryComponent* Inventory = Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn()));
	return Super::ShouldRefreshFor(Changes) || (SpareRoundsText && Inventory && Inventory->IsGun(GetShownItem()));
}

void UHadesEquippedItemWidget::NativeOnRefresh(FInventoryItemHandle Item)
{
	Super::NativeOnRefresh(Item);

	const UHadesInventoryComponent* Inventory = Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn()));
	const bool bGun = Inventory && Inventory->IsGun(Item);
	const ESlateVisibility GunVisibility = bGun ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (RoundsText)
	{
		const int32 Rounds = bGun ? Inventory->GetLoadedRounds(Item) : 0;
		const int32 Capacity = bGun ? Inventory->GetCapacity(Item) : 0;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Rounds"), Rounds);
		Args.Add(TEXT("Capacity"), Capacity);
		RoundsText->SetText(bGun ? FText::Format(RoundsFormat, Args) : FText::GetEmpty());
		RoundsText->SetColorAndOpacity(bGun && Capacity > 0 && Rounds <= Capacity * LowFraction ? LowColor : DefaultRoundsColor);
		RoundsText->SetVisibility(GunVisibility);
	}
	if (SpareRoundsText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Spare"), bGun ? Inventory->GetSpareRounds(Item) : 0);
		SpareRoundsText->SetText(bGun ? FText::Format(SpareRoundsFormat, Args) : FText::GetEmpty());
		SpareRoundsText->SetVisibility(GunVisibility);
	}
	if (RoundsPanel)
	{
		RoundsPanel->SetVisibility(GunVisibility);
	}

	if (ChargeMeter)
	{
		const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
		float Charge = 0.0f;
		const bool bHasCharge = Subsystem && Item.IsSet() && Subsystem->GetTagValueFraction(Item, HadesGameplayTags::Item_Stat_Charge, FGameplayTag(), Charge);
		if (bHasCharge && Item == ChargeItem)
		{
			ChargeMeter->SetPercent(Charge);
		}
		else
		{
			ChargeMeter->SetPercentImmediately(Charge);
		}
		ChargeItem = bHasCharge ? Item : FInventoryItemHandle();
		ChargeMeter->SetFillColor(bHasCharge && Charge <= LowFraction ? LowColor.GetSpecifiedColor() : DefaultChargeColor);
		ChargeMeter->SetVisibility(bHasCharge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

#undef LOCTEXT_NAMESPACE
