#include "HadesItemMenu.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include "Hades/UI/Inventory/HadesItemActions.h"

#include "Actions/InventoryItemActions.h"
#include "Widgets/InventoryShortcutMenuWidget.h"

#define LOCTEXT_NAMESPACE "HadesItemMenu"

UHadesItemMenu::UHadesItemMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	auto AddAction = [this](UInventoryItemAction* Action, const TCHAR* ButtonName)
	{
		Action->ButtonName = ButtonName;
		Actions.Add(Action);
	};

	// The subobject names stay as they were when these were Hades actions, so menus that changed them keep their changes.
	AddAction(CreateDefaultSubobject<UInventoryItemAction_AssignShortcut>(TEXT("AssignQuickSlot")), TEXT("QuickSlotButton"));
	AddAction(CreateDefaultSubobject<UInventoryItemAction_Use>(TEXT("Use")), TEXT("UseButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Equip>(TEXT("Equip")), TEXT("EquipButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Unequip>(TEXT("Unequip")), TEXT("UnequipButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Combine>(TEXT("Combine")), TEXT("CombineButton"));
	UInventoryItemAction_Drop* Discard = CreateDefaultSubobject<UInventoryItemAction_Drop>(TEXT("Discard"));
	Discard->DisplayName = LOCTEXT("DiscardName", "Discard");
	Discard->PromptText = LOCTEXT("DiscardPrompt", "Discard {Item}?");
	AddAction(Discard, TEXT("DiscardButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Store>(TEXT("Store")), TEXT("StoreButton"));
	AddAction(CreateDefaultSubobject<UHadesItemAction_Take>(TEXT("Take")), TEXT("TakeButton"));

	// Right-click goes back, as the gamepad's Back button does, and a click off the menu closes it. A question takes the actions' place.
	RightClick = EModularMenuRightClick::Back;
	OutsideClick = EModularMenuOutsideClick::Close;
	PromptPresentation = EModularPromptPresentation::Inline;

	OpenTransition = MakeTransition();
	bCloseReversesOpen = true;
	ActionTiming = EInventoryMenuActionTiming::AsMenuCloses;

	// A prompt, such as Discard's, starts on its first button, as the menu starts on its first entry.
	bConfirmStartsOnCancel = false;
}

FModularUITransition UHadesItemMenu::MakeTransition()
{
	FModularUITransition Transition;
	Transition.Duration = 5.0f / 60.0f;
	Transition.Easing = EEasingFunc::Linear;
	Transition.bFade = true;
	Transition.FadeFrom = 0.0f;
	Transition.bSlide = true;
	Transition.SlideDistance = 20.0f;
	Transition.SlideFrom = EModularUISlide::FromOrigin;
	Transition.bScale = false;
	return Transition;
}

#if WITH_EDITOR
void UHadesItemMenu::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);

	TArray<UInventoryItemAction*> Gathered;
	GatherActions(Gathered);
	for (const UInventoryItemAction* Action : Gathered)
	{
		const UInventoryItemAction_AssignShortcut* AssignShortcut = Cast<UInventoryItemAction_AssignShortcut>(Action);
		if (AssignShortcut && !AssignShortcut->GetMenuClass())
		{
			CompileLog.Warning(LOCTEXT("NoQuickSlotMenu", "The Assign Shortcut action has no Menu Class, so the menu never offers it."));
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
