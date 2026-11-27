#include "HadesHUDLayout.h"

#include "Animation/WidgetAnimation.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"

#include "InputAction.h"
#include "Layers/ModularUISubsystem.h"
#include "Widgets/InventoryScreenWidget.h"

UHadesHUDLayout::UHadesHUDLayout(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InputConfig = EModularUIInputMode::GameAndMenu;
}

void UHadesHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RegisterInputAction(PauseInputAction, /*bDisplayInActionBar*/ false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandlePauseAction));
	RegisterInputAction(InventoryInputAction, /*bDisplayInActionBar*/ false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleInventoryAction));
	RegisterInputAction(CraftingInputAction, /*bDisplayInActionBar*/ false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCraftingAction));
}

void UHadesHUDLayout::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	}
	HandlePawnChanged(nullptr, GetOwningPlayerPawn());
}

void UHadesHUDLayout::NativeDestruct()
{
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}
	HandlePawnChanged(nullptr, nullptr);

	Super::NativeDestruct();
}

void UHadesHUDLayout::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (UHadesQuickSlotComponent* Previous = QuickSlots.Get())
	{
		Previous->OnShortcutActivated.RemoveDynamic(this, &ThisClass::HandleQuickSlotActivated);
	}

	QuickSlots = NewPawn ? NewPawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
	if (UHadesQuickSlotComponent* Current = QuickSlots.Get())
	{
		Current->OnShortcutActivated.AddUniqueDynamic(this, &ThisClass::HandleQuickSlotActivated);
	}
}

void UHadesHUDLayout::HandleQuickSlotActivated(int32 SlotIndex, FInventoryItemHandle Item)
{
	if (ShowQuickSlotsAnimation)
	{
		PlayAnimation(ShowQuickSlotsAnimation);
	}
}

void UHadesHUDLayout::HandlePauseAction()
{
	UModularUISubsystem* UI = UModularUISubsystem::Get(GetOwningLocalPlayer());
	if (UI && ensure(!EscapeMenuClass.IsNull()))
	{
		UI->K2_PushStreamedWidgetToLayer(HadesGameplayTags::UI_Layer_Menu, EscapeMenuClass);
	}
}

void UHadesHUDLayout::HandleInventoryAction()
{
	OpenInventory(NAME_None);
}

void UHadesHUDLayout::HandleCraftingAction()
{
	OpenInventory(CraftingTab);
}

void UHadesHUDLayout::OpenInventory(FName TabId)
{
	UModularUISubsystem* UI = UModularUISubsystem::Get(GetOwningLocalPlayer());
	if (!UI || InventoryScreenClass.IsNull())
	{
		return;
	}

	// Told its tab as it's pushed, before it shows, so it opens there rather than switching to it once open.
	UI->PushStreamedWidgetToLayer(HadesGameplayTags::UI_Layer_GameMenu, InventoryScreenClass, /*bSuspendInput*/ true, [TabId](UCommonActivatableWidget& Widget)
	{
		UInventoryScreenWidget* Screen = Cast<UInventoryScreenWidget>(&Widget);
		if (Screen && !TabId.IsNone())
		{
			Screen->SetStartTab(TabId);
		}
	});
}
