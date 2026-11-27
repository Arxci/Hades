#pragma once

#include "CoreMinimal.h"

#include "Widgets/InventoryEquippedItemWidget.h"

#include "HadesEquippedItemWidget.generated.h"

class UModularSegmentedMeter;
class UTextBlock;


/**
 * The weapon in hand or the light carried, such as in the equipment strip under the inventory's details: the plugin's Equipped Item widget
 * with the game's numbers. Slots starts as the right hand, then the left, for whatever is in hand. For the light, make another with Slots
 * Equipment.Slot.LightSource and Active Tag Item.State.LightOn, so Active Marker shows while it's on.
 * Rounds Text shows a gun's loaded rounds out of what it holds, Spare Rounds Text the rounds for it carried anywhere else, and Charge Meter
 * an item's Item.Stat.Charge, such as a light's battery. Each is collapsed for items it doesn't apply to.
 */
UCLASS(Abstract)
class HADES_API UHadesEquippedItemWidget : public UInventoryEquippedItemWidget
{
	GENERATED_BODY()

public:
	UHadesEquippedItemWidget(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UInventoryEquippedItemWidget Interface
	virtual void NativeOnRefresh(FInventoryItemHandle Item) override;
	virtual bool ShouldRefreshFor(const FInventoryChangeSet& Changes) const override;
	//~ End UInventoryEquippedItemWidget Interface

	/** A gun's loaded rounds, as Rounds Format says. Collapsed for anything that isn't a gun. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipped Item|Hades", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RoundsText;

	/** The rounds for the gun carried anywhere else in the inventory, as Spare Rounds Format says. Collapsed for anything that isn't a gun. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipped Item|Hades", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpareRoundsText;

	/** Shown only for a gun, such as the box Rounds Text and Spare Rounds Text sit in. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipped Item|Hades", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RoundsPanel;

	/** The charge left, such as a light's battery: its Item.Stat.Charge out of what it starts with. Collapsed for items without charge. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipped Item|Hades", meta = (BindWidgetOptional))
	TObjectPtr<UModularSegmentedMeter> ChargeMeter;

	/** {Rounds} is the rounds loaded, {Capacity} what the gun holds when full. */
	UPROPERTY(EditAnywhere, Category = "Equipped Item|Hades")
	FText RoundsFormat;

	/** {Spare} is the rounds for the gun carried elsewhere. */
	UPROPERTY(EditAnywhere, Category = "Equipped Item|Hades")
	FText SpareRoundsFormat;

	/** Charge Meter turns Low Charge Color at or below this much of its charge, from 0 to 1, and Rounds Text at or below this much of the gun's capacity. */
	UPROPERTY(EditAnywhere, Category = "Equipped Item|Hades", meta = (ClampMin = "0", ClampMax = "1"))
	float LowFraction = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Equipped Item|Hades")
	FSlateColor LowColor = FSlateColor(FLinearColor(0.72f, 0.16f, 0.12f));

private:
	FSlateColor DefaultRoundsColor;
	FLinearColor DefaultChargeColor = FLinearColor::White;

	/** The item Charge Meter last showed, so a new item's charge shows at once rather than draining to it. */
	FInventoryItemHandle ChargeItem;
};
