#include "HadesItemMenu.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include "Hades/UI/Inventory/HadesItemActions.h"
#include "Hades/UI/Inventory/HadesQuickSlotMenu.h"

#define LOCTEXT_NAMESPACE "HadesItemMenu"

UHadesItemMenu::UHadesItemMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	auto AddAction = [this](UInventoryItemAction* Action, const TCHAR* ButtonName)
	{
		Action->ButtonName = ButtonName;
		Actions.Add(Action);
	};
	AddAction(CreateDefaultSubobject<UHadesItemAction_AssignQuickSlot>(TEXT("AssignQuickSlot")), TEXT("QuickSlotButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Use>(TEXT("Use")), TEXT("UseButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Equip>(TEXT("Equip")), TEXT("EquipButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Unequip>(TEXT("Unequip")), TEXT("UnequipButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Combine>(TEXT("Combine")), TEXT("CombineButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Discard>(TEXT("Discard")), TEXT("DiscardButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Store>(TEXT("Store")), TEXT("StoreButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Take>(TEXT("Take")), TEXT("TakeButton"));
}

#if WITH_EDITOR
void UHadesItemMenu::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);

	TArray<UInventoryItemAction*> Gathered;
	GatherActions(Gathered);
	for (const UInventoryItemAction* Action : Gathered)
	{
		const UHadesItemAction_AssignQuickSlot* AssignQuickSlot = Cast<UHadesItemAction_AssignQuickSlot>(Action);
		if (AssignQuickSlot && !AssignQuickSlot->GetMenuClass())
		{
			CompileLog.Warning(LOCTEXT("NoQuickSlotMenu", "The Assign Quick Slot action has no Menu Class, so the menu never offers it."));
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
