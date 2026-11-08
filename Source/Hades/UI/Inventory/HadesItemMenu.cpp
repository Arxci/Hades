#include "HadesItemMenu.h"

#include "CommonButtonBase.h"
#include "Components/TextBlock.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"

UHadesItemMenu::UHadesItemMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	InputConfig = EHadesWidgetInputMode::Menu;
}

void UHadesItemMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	for (const EHadesItemAction Action : TEnumRange<EHadesItemAction>())
	{
		if (UCommonButtonBase* Button = GetButton(Action))
		{
			Button->OnClicked().AddUObject(this, &ThisClass::HandleButtonClicked, Action);
		}
	}
}

void UHadesItemMenu::SetItem(UHadesInventoryScreen* InScreen, FInventoryItemHandle InItem)
{
	Screen = InScreen;
	Item = InItem;

	for (const EHadesItemAction Action : TEnumRange<EHadesItemAction>())
	{
		if (UCommonButtonBase* Button = GetButton(Action))
		{
			const bool bCanRun = InScreen && InScreen->CanRunItemAction(Action, InItem);
			Button->SetVisibility(bCanRun ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}

	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = Subsystem ? Subsystem->GetItemDefinition(InItem) : nullptr;
	if (ItemNameText)
	{
		ItemNameText->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
	}
}

UWidget* UHadesItemMenu::NativeGetDesiredFocusTarget() const
{
	for (const EHadesItemAction Action : TEnumRange<EHadesItemAction>())
	{
		UCommonButtonBase* Button = GetButton(Action);
		if (Button && Button->IsVisible())
		{
			return Button;
		}
	}
	return Super::NativeGetDesiredFocusTarget();
}

UCommonButtonBase* UHadesItemMenu::GetButton(EHadesItemAction Action) const
{
	switch (Action)
	{
	case EHadesItemAction::Use:     return UseButton;
	case EHadesItemAction::Equip:   return EquipButton;
	case EHadesItemAction::Unequip: return UnequipButton;
	case EHadesItemAction::Combine: return CombineButton;
	case EHadesItemAction::Discard: return DiscardButton;
	case EHadesItemAction::Store:   return StoreButton;
	case EHadesItemAction::Take:    return TakeButton;
	}
	return nullptr;
}

void UHadesItemMenu::HandleButtonClicked(EHadesItemAction Action)
{
	// Closed first, so the screen has focus again for whatever the action starts, like choosing an item to combine with.
	DeactivateWidget();

	if (UHadesInventoryScreen* InventoryScreen = Screen.Get())
	{
		InventoryScreen->RunItemAction(Action, Item);
	}
}
