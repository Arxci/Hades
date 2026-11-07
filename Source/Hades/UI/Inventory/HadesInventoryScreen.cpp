#include "HadesInventoryScreen.h"

#include "Input/CommonUIInputTypes.h"

#include "Hades/HadesGameplayTags.h"

#include "InventoryUISubsystem.h"
#include "Widgets/InventoryCellWidget.h"
#include "Widgets/InventoryGridWidget.h"

UHadesInventoryScreen::UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

void UHadesInventoryScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_RotateItem), true,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRotateAction)));
	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_SplitStack), true,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSplitAction)));
	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_Inventory), false,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCloseAction)));
}

void UHadesInventoryScreen::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->ClearInteraction();
	}

	Super::NativeOnDeactivated();
}

bool UHadesInventoryScreen::NativeOnHandleBackAction()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsHolding())
	{
		Interaction->CancelHold();
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

UWidget* UHadesInventoryScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}
	return MainGrid ? MainGrid->GetCellWidget(FIntPoint::ZeroValue) : nullptr;
}

UInventoryUISubsystem* UHadesInventoryScreen::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

void UHadesInventoryScreen::HandleRotateAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->RotateHeld();
	}
}

void UHadesInventoryScreen::HandleSplitAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SplitHeld();
	}
}

void UHadesInventoryScreen::HandleCloseAction()
{
	DeactivateWidget();
}
