#include "HadesItemMenu.h"

#include "CommonButtonBase.h"

#define LOCTEXT_NAMESPACE "HadesItemMenu"

namespace
{
	FText GetActionName(EHadesItemAction Action)
	{
		switch (Action)
		{
		case EHadesItemAction::Shortcut: return LOCTEXT("ShortcutName", "Shortcut");
		case EHadesItemAction::Use:      return LOCTEXT("UseName", "Use");
		case EHadesItemAction::Equip:    return LOCTEXT("EquipName", "Equip");
		case EHadesItemAction::Unequip:  return LOCTEXT("UnequipName", "Unequip");
		case EHadesItemAction::Combine:  return LOCTEXT("CombineName", "Combine");
		case EHadesItemAction::Discard:  return LOCTEXT("DiscardName", "Discard");
		case EHadesItemAction::Store:    return LOCTEXT("StoreName", "Store");
		case EHadesItemAction::Take:     return LOCTEXT("TakeName", "Take");
		}
		return FText::GetEmpty();
	}
}

// Action
bool UHadesItemAction::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UHadesInventoryScreen* InventoryScreen = Cast<UHadesInventoryScreen>(Context.Screen);
	return InventoryScreen && InventoryScreen->CanRunItemAction(Action, Context.Item);
}

void UHadesItemAction::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UHadesInventoryScreen* InventoryScreen = Cast<UHadesInventoryScreen>(Context.Screen))
	{
		InventoryScreen->RunItemAction(Action, Context.Item);
	}
}

// Menu
UHadesItemMenu::UHadesItemMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// A click outside the menu only closes it. A right-click on another item opens that item's menu instead.
	LeftPressOutside = EInventoryMenuOutsidePress::Close;
	RightPressOutside = EInventoryMenuOutsidePress::PassThrough;

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

	// Every Hades action, each on the button placed for it. Without one, a button is made in Action Panel, if the menu has one.
	for (const EHadesItemAction Each : TEnumRange<EHadesItemAction>())
	{
		UHadesItemAction* BuiltIn = NewObject<UHadesItemAction>(this);
		BuiltIn->Action = Each;
		BuiltIn->DisplayName = GetActionName(Each);
		BuiltIn->Description = Descriptions.FindRef(Each);
		const UCommonButtonBase* Button = GetButton(Each);
		BuiltIn->ButtonName = Button ? Button->GetFName() : NAME_None;
		if (Each == EHadesItemAction::Discard)
		{
			BuiltIn->Prompt = EInventoryActionPrompt::Confirm;
			BuiltIn->PromptText = LOCTEXT("DiscardPrompt", "Discard {Item}?");
		}
		BuiltInActions.Add(BuiltIn);
	}
}

void UHadesItemMenu::GatherActions(TArray<UInventoryItemAction*>& OutActions) const
{
	Super::GatherActions(OutActions);

	if (OutActions.IsEmpty())
	{
		for (UHadesItemAction* BuiltIn : BuiltInActions)
		{
			OutActions.Add(BuiltIn);
		}
	}
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

#undef LOCTEXT_NAMESPACE
