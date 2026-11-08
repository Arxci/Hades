#include "HadesInventoryScreen.h"

#include "Input/CommonUIInputTypes.h"
#include "PrimaryGameLayout.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Player/HadesPlayerController.h"
#include "Hades/UI/Inventory/HadesItemMenu.h"

#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "Widgets/InventoryCellWidget.h"
#include "Widgets/InventoryGridWidget.h"
#include "Widgets/InventoryListWidget.h"

#define LOCTEXT_NAMESPACE "HadesInventoryScreen"

UHadesInventoryScreen::UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	InputConfig = EHadesWidgetInputMode::Menu;
	bPauseGame = true;
}

void UHadesInventoryScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_RotateItem), true,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRotateAction)));
	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_SplitStack), true,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSplitAction)));
	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_ItemMenu), true,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleItemMenuAction)));
	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_Organize), true,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleOrganizeAction)));
	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_Inventory), false,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCloseAction)));
}

void UHadesInventoryScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnContextRequested.AddUniqueDynamic(this, &ThisClass::OpenItemMenu);
		Interaction->OnItemChosen.AddUniqueDynamic(this, &ThisClass::HandleItemChosen);
	}
	if (ItemBoxList)
	{
		ItemBoxList->SetContainer(GetItemBox());
	}
}

void UHadesInventoryScreen::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnContextRequested.RemoveDynamic(this, &ThisClass::OpenItemMenu);
		Interaction->OnItemChosen.RemoveDynamic(this, &ThisClass::HandleItemChosen);
		Interaction->ClearInteraction();
	}
	CombineItem.Reset();

	Super::NativeOnDeactivated();
}

bool UHadesInventoryScreen::NativeOnHandleBackAction()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsChoosingItem())
	{
		Interaction->CancelChoosingItem();
		CombineItem.Reset();
		return true;
	}
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

// Item actions
bool UHadesInventoryScreen::CanRunItemAction(EHadesItemAction Action, FInventoryItemHandle Item) const
{
	const UHadesInventoryComponent* Inventory = GetPlayerInventory();
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	if (!Inventory || !Subsystem || !Subsystem->IsValidItem(Item))
	{
		return false;
	}

	const FInventoryContainerHandle ItemBox = GetItemBox();
	switch (Action)
	{
	case EHadesItemAction::Use:     return Inventory->CanUseItem(Item);
	case EHadesItemAction::Equip:   return Inventory->CanDraw(Item) && !Inventory->IsInHand(Item);
	case EHadesItemAction::Unequip: return Inventory->IsInHand(Item);
	case EHadesItemAction::Combine: return Inventory->CanCombine(Item);
	case EHadesItemAction::Discard: return Inventory->CanDiscardItem(Item);
	case EHadesItemAction::Store:   return ItemBox.IsSet() && Inventory->OwnsItem(Item) && !Inventory->IsKeyItem(Item);
	case EHadesItemAction::Take:    return ItemBox.IsSet() && Subsystem->GetRootContainer(Subsystem->GetItemContainer(Item)) == ItemBox;
	}
	return false;
}

void UHadesInventoryScreen::RunItemAction(EHadesItemAction Action, FInventoryItemHandle Item)
{
	UHadesInventoryComponent* Inventory = GetPlayerInventory();
	UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!Interaction || !CanRunItemAction(Action, Item))
	{
		return;
	}

	EInventoryError Error = EInventoryError::None;
	switch (Action)
	{
	case EHadesItemAction::Use:
		Inventory->UseItem(Item);
		break;
	case EHadesItemAction::Equip:
		Inventory->DrawItem(Item);
		break;
	case EHadesItemAction::Unequip:
		Inventory->StowItem(Item);
		break;
	case EHadesItemAction::Combine:
		CombineItem = Item;
		Interaction->BeginChoosingItem();
		AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("ChooseCombine", "Combine with what?"));
		break;
	case EHadesItemAction::Discard:
		Inventory->DiscardItem(Item);
		break;
	case EHadesItemAction::Store:
		if (!Subsystem->MoveItemToContainers(Item, { GetItemBox() }, 0, false, Error))
		{
			AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("CantStore", "That can't go in the item box."));
		}
		break;
	case EHadesItemAction::Take:
		if (!Inventory->TakeItem(Item, 0, true, Error))
		{
			AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("NoRoomToTake", "Not enough space."));
		}
		break;
	}
}

void UHadesInventoryScreen::OpenItemMenu(FInventoryItemHandle Item)
{
	bool bHasActions = false;
	for (const EHadesItemAction Action : TEnumRange<EHadesItemAction>())
	{
		bHasActions |= CanRunItemAction(Action, Item);
	}

	UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningLocalPlayer());
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!bHasActions || !ItemMenuClass || !Layout || !Interaction || Interaction->IsChoosingItem())
	{
		return;
	}

	Interaction->CancelHold();
	Layout->PushWidgetToLayerStack<UHadesItemMenu>(HadesGameplayTags::UI_Layer_Modal, ItemMenuClass.Get(), [this, Item](UHadesItemMenu& Menu)
	{
		Menu.SetItem(this, Item);
	});
}

void UHadesInventoryScreen::HandleItemChosen(FInventoryItemHandle Item)
{
	const FInventoryItemHandle First = CombineItem;
	CombineItem.Reset();

	UHadesInventoryComponent* Inventory = GetPlayerInventory();
	if (Inventory && First.IsSet())
	{
		Inventory->CombineItems(First, Item);
	}
}

// Helpers
UInventoryUISubsystem* UHadesInventoryScreen::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

UHadesInventoryComponent* UHadesInventoryScreen::GetPlayerInventory() const
{
	return Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn()));
}

FInventoryContainerHandle UHadesInventoryScreen::GetItemBox() const
{
	const AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer());
	const UInventoryComponent* Box = ItemBoxList && Controller ? Controller->GetItemBoxInventory() : nullptr;
	return Box ? Box->GetContainer(HadesGameplayTags::Container_ItemBox) : FInventoryContainerHandle();
}

// UI actions
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

void UHadesInventoryScreen::HandleItemMenuAction()
{
	if (const UInventoryUISubsystem* Interaction = GetInteraction())
	{
		OpenItemMenu(Interaction->GetHoveredItem());
	}
}

void UHadesInventoryScreen::HandleOrganizeAction()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	UHadesInventoryComponent* Inventory = GetPlayerInventory();
	if (!Interaction || !Inventory || Interaction->IsChoosingItem())
	{
		return;
	}

	Interaction->CancelHold();
	if (!Inventory->OrganizePockets())
	{
		AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("CantOrganize", "Couldn't organize your pockets."));
	}
}

void UHadesInventoryScreen::HandleCloseAction()
{
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
