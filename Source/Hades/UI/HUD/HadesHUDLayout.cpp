#include "HadesHUDLayout.h"

#include "Animation/WidgetAnimation.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"

#include "Layers/ModularUISubsystem.h"

UHadesHUDLayout::UHadesHUDLayout(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InputConfig = EModularUIInputMode::GameAndMenu;
}

void UHadesHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_Escape), false,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleEscapeAction)));
	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_Inventory), false,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleInventoryAction)));
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

void UHadesHUDLayout::HandleEscapeAction()
{
	UModularUISubsystem* UI = UModularUISubsystem::Get(GetOwningLocalPlayer());
	if (UI && ensure(!EscapeMenuClass.IsNull()))
	{
		UI->K2_PushStreamedWidgetToLayer(HadesGameplayTags::UI_Layer_Menu, EscapeMenuClass);
	}
}

void UHadesHUDLayout::HandleInventoryAction()
{
	UModularUISubsystem* UI = UModularUISubsystem::Get(GetOwningLocalPlayer());
	if (UI && !InventoryScreenClass.IsNull())
	{
		UI->K2_PushStreamedWidgetToLayer(HadesGameplayTags::UI_Layer_GameMenu, InventoryScreenClass);
	}
}
