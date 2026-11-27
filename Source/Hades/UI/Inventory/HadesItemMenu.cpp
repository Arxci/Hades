#include "HadesItemMenu.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include "Hades/UI/Inventory/HadesItemActions.h"

#include "Actions/InventoryItemActions.h"
#include "Widgets/InventoryRecipeMenuWidget.h"
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

	// Slides out from behind the item, as a drawer pulled out from under it, and back in as it closes. Needs Menu Content inside Menu Panel.
	OpenTransition = MakeTransition();
	bSlideFromBehindAnchor = true;
	bCloseReversesOpen = true;
	ActionTiming = EInventoryMenuActionTiming::AsMenuCloses;

	// A prompt, such as Discard's, starts on its first button, as the menu starts on its first entry.
	bConfirmStartsOnCancel = false;
}

FModularUITransition UHadesItemMenu::MakeTransition()
{
	// Quick, and settling as it arrives, as a drawer does. With Slide From Behind Anchor the slide covers the menu's whole width; without it,
	// as for a menu laid out without Menu Content, it slides the 20 pixels out of the item it opened beside. The fade only softens the start.
	FModularUITransition Transition;
	Transition.Duration = 0.12f;
	Transition.Easing = EEasingFunc::EaseOut;
	Transition.EasingStrength = 3.0f;
	Transition.bFade = true;
	Transition.FadeFrom = 0.2f;
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
		const UHadesItemAction_Combine* Combine = Cast<UHadesItemAction_Combine>(Action);
		if (Combine && Combine->GetStyle() == EHadesCombineStyle::RecipeList && !Combine->GetMenuClass())
		{
			CompileLog.Note(LOCTEXT("NoRecipeMenu", "The Combine action's Style is Recipe List, but it has no Menu Class, so it has the player choose the other item instead. Set Style to Choose Item if that's what you want."));
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
