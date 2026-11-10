#include "HadesHUDLayout.h"

#include "Animation/WidgetAnimation.h"
#include "CommonUIExtensions.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"

UHadesHUDLayout::UHadesHUDLayout(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InputConfig = EHadesWidgetInputMode::GameAndMenu;
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
		Previous->OnSlotActivated.RemoveDynamic(this, &ThisClass::HandleQuickSlotActivated);
	}

	QuickSlots = NewPawn ? NewPawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
	if (UHadesQuickSlotComponent* Current = QuickSlots.Get())
	{
		Current->OnSlotActivated.AddUniqueDynamic(this, &ThisClass::HandleQuickSlotActivated);
	}
}

void UHadesHUDLayout::HandleQuickSlotActivated(int32 SlotIndex)
{
	if (ShowQuickSlotsAnimation)
	{
		PlayAnimation(ShowQuickSlotsAnimation);
	}
}

void UHadesHUDLayout::HandleEscapeAction()
{
	if (ensure(!EscapeMenuClass.IsNull()))
	{
		UCommonUIExtensions::PushStreamedContentToLayer_ForPlayer(GetOwningLocalPlayer(), HadesGameplayTags::UI_Layer_Menu, EscapeMenuClass);
	}
}

void UHadesHUDLayout::HandleInventoryAction()
{
	if (!InventoryScreenClass.IsNull())
	{
		UCommonUIExtensions::PushStreamedContentToLayer_ForPlayer(GetOwningLocalPlayer(), HadesGameplayTags::UI_Layer_GameMenu, InventoryScreenClass);
	}
}
