#include "HadesItemMenu.h"

#include "CommonButtonBase.h"
#include "Components/TextBlock.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"

#define LOCTEXT_NAMESPACE "HadesItemMenu"

UHadesItemMenu::UHadesItemMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Descriptions.Add(EHadesItemAction::Shortcut, LOCTEXT("ShortcutDescription", "Put the item on a shortcut key."));
	Descriptions.Add(EHadesItemAction::Use, LOCTEXT("UseDescription", "Use the item."));
	Descriptions.Add(EHadesItemAction::Equip, LOCTEXT("EquipDescription", "Hold the item in your hands."));
	Descriptions.Add(EHadesItemAction::Unequip, LOCTEXT("UnequipDescription", "Put the item away."));
	Descriptions.Add(EHadesItemAction::Combine, LOCTEXT("CombineDescription", "Combine the item with another one."));
	Descriptions.Add(EHadesItemAction::Discard, LOCTEXT("DiscardDescription", "Drop the item here."));
	Descriptions.Add(EHadesItemAction::Store, LOCTEXT("StoreDescription", "Put the item in the item box."));
	Descriptions.Add(EHadesItemAction::Take, LOCTEXT("TakeDescription", "Take the item from the item box."));
}

void UHadesItemMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Gamepad focus moves Common UI's hidden cursor onto the button, so hovering covers both.
	for (const EHadesItemAction Action : TEnumRange<EHadesItemAction>())
	{
		if (UCommonButtonBase* Button = GetButton(Action))
		{
			Button->OnClicked().AddUObject(this, &ThisClass::HandleButtonClicked, Action);
			Button->OnHovered().AddUObject(this, &ThisClass::ShowDescription, Action);
		}
	}
}

void UHadesItemMenu::SetItem(UHadesInventoryScreen* InScreen, FInventoryItemHandle InItem, const FBox2D& InAnchor)
{
	Screen = InScreen;
	Item = InItem;

	TOptional<EHadesItemAction> FirstAction;
	for (const EHadesItemAction Action : TEnumRange<EHadesItemAction>())
	{
		if (UCommonButtonBase* Button = GetButton(Action))
		{
			const bool bCanRun = InScreen && InScreen->CanRunItemAction(Action, InItem);
			Button->SetVisibility(bCanRun ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			if (bCanRun && !FirstAction.IsSet())
			{
				FirstAction = Action;
			}
		}
	}
	if (FirstAction.IsSet())
	{
		ShowDescription(FirstAction.GetValue());
	}

	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = Subsystem ? Subsystem->GetItemDefinition(InItem) : nullptr;
	if (ItemNameText)
	{
		ItemNameText->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
	}

	PlaceBeside(InAnchor);
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
	case EHadesItemAction::Shortcut: return ShortcutButton;
	case EHadesItemAction::Use:      return UseButton;
	case EHadesItemAction::Equip:    return EquipButton;
	case EHadesItemAction::Unequip:  return UnequipButton;
	case EHadesItemAction::Combine:  return CombineButton;
	case EHadesItemAction::Discard:  return DiscardButton;
	case EHadesItemAction::Store:    return StoreButton;
	case EHadesItemAction::Take:     return TakeButton;
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

void UHadesItemMenu::ShowDescription(EHadesItemAction Action)
{
	if (UHadesInventoryScreen* InventoryScreen = Screen.Get())
	{
		InventoryScreen->ShowActionHint(Descriptions.FindRef(Action));
	}
}

#undef LOCTEXT_NAMESPACE
