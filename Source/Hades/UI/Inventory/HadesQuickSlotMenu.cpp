#include "HadesQuickSlotMenu.h"

#include "Hades/UI/Inventory/HadesItemMenu.h"

#define LOCTEXT_NAMESPACE "HadesQuickSlotMenu"

UHadesQuickSlotMenu::UHadesQuickSlotMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Dismissed as the item menu is, and comes in the same way.
	RightClick = EModularMenuRightClick::Back;
	OutsideClick = EModularMenuOutsideClick::Close;
	OpenTransition = UHadesItemMenu::MakeTransition();
	bSlideFromBehindAnchor = true;
	bCloseReversesOpen = true;

	AssignHint = LOCTEXT("AssignHint", "Assign the item to this quick slot.");
	ClearHint = LOCTEXT("ClearHint", "Remove the item from this quick slot.");
	RefusedHint = LOCTEXT("RefusedHint", "Only things to hold or use go in quick slots.");
}

#undef LOCTEXT_NAMESPACE
