#include "Widgets/ModularContextMenuWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"

#include "Layers/ModularUISubsystem.h"
#include "ModularUI.h"
#include "Widgets/ModularButton.h"
#include "Widgets/ModularPromptWidget.h"

namespace
{
	FBox2D GetAbsoluteBox(const FGeometry& Geometry)
	{
		return FBox2D(Geometry.GetAbsolutePosition(), Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
	}

	/** Top Left moved in as far as it takes for something of Size to fit inside Bounds, or onto their top or left edge where it's bigger. */
	FVector2D KeepIn(const FVector2D& TopLeft, const FVector2D& Size, const FBox2D& Bounds)
	{
		const FVector2D LastTopLeft = (Bounds.Max - Size).ComponentMax(Bounds.Min);
		return FVector2D(FMath::Clamp(TopLeft.X, Bounds.Min.X, LastTopLeft.X), FMath::Clamp(TopLeft.Y, Bounds.Min.Y, LastTopLeft.Y));
	}

	/** The first button that can take focus in Root, skipping whatever is hidden, such as a prompt that isn't asking. */
	UCommonButtonBase* FindFirstButtonIn(UWidget* Root)
	{
		if (!Root || !Root->IsVisible())
		{
			return nullptr;
		}
		if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(Root))
		{
			return Button->GetIsEnabled() ? Button : nullptr;
		}
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (UCommonButtonBase* Found = FindFirstButtonIn(Panel->GetChildAt(Index)))
				{
					return Found;
				}
			}
		}
		return nullptr;
	}

	/** Whether the widget is on the path, such as the path to the widget taking focus. */
	bool IsOnPath(const FWidgetPath& Path, const SWidget& Widget)
	{
		for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
		{
			if (&Path.Widgets[Index].Widget.Get() == &Widget)
			{
				return true;
			}
		}
		return false;
	}

	/** Presses and lets go of the left mouse button where the cursor is, as Common UI does for a gamepad's Accept while its cursor shows. */
	void ClickAtCursor(ULocalPlayer* LocalPlayer)
	{
		const TSharedPtr<FSlateUser> SlateUser = LocalPlayer && FSlateApplication::IsInitialized() ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
		if (!SlateUser)
		{
			return;
		}

		FSlateApplication& SlateApp = FSlateApplication::Get();
		const FVector2D Position = SlateUser->GetCursorPosition();
		const FModifierKeysState Modifiers = SlateApp.GetPlatformApplication()->GetModifierKeys();
		const uint32 UserIndex = static_cast<uint32>(SlateUser->GetUserIndex());

		const FPointerEvent Press(UserIndex, FSlateApplication::CursorPointerIndex, Position, Position, SlateApp.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Modifiers);
		SlateApp.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(), Press);

		const FPointerEvent Release(UserIndex, FSlateApplication::CursorPointerIndex, Position, Position, SlateApp.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Modifiers);
		SlateApp.ProcessMouseButtonUpEvent(Release);
	}

	/**
	 * Moves the cursor nowhere, as Slate does itself when what's under a still cursor changes: widgets no longer under it get Mouse Leave
	 * and stop looking hovered, and the one under it now gets Mouse Enter. Synthetic, so it changes neither the input method nor tooltips.
	 */
	void MoveCursorInPlace(ULocalPlayer* LocalPlayer)
	{
		const TSharedPtr<FSlateUser> SlateUser = LocalPlayer && FSlateApplication::IsInitialized() ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
		if (!SlateUser)
		{
			return;
		}

		FSlateApplication& SlateApp = FSlateApplication::Get();
		const FVector2D Position = SlateUser->GetCursorPosition();
		const FModifierKeysState Modifiers = SlateApp.GetPlatformApplication()->GetModifierKeys();
		const uint32 UserIndex = static_cast<uint32>(SlateUser->GetUserIndex());
		const FPointerEvent Move(UserIndex, FSlateApplication::CursorPointerIndex, Position, Position, SlateApp.GetPressedMouseButtons(), EKeys::Invalid, 0.0f, Modifiers);
		SlateApp.ProcessMouseMoveEvent(Move, /*bIsSynthetic*/ true);
	}

	/** Whether a widget was hidden in the designer, so it's shown as Visibility once something asks for it. */
	ESlateVisibility GetShownVisibility(const UWidget& Widget, ESlateVisibility Visibility)
	{
		const ESlateVisibility Designed = Widget.GetVisibility();
		const bool bDesignedHidden = Designed == ESlateVisibility::Collapsed || Designed == ESlateVisibility::Hidden;
		return bDesignedHidden ? Visibility : Designed;
	}
}

UModularContextMenuWidget::UModularContextMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;

	// Whatever is under the menu keeps its actions to itself while the menu is open, even in a stack inside a screen.
	bIsModal = true;
	InputConfig = EModularUIInputMode::Menu;
}

void UModularContextMenuWidget::NativeOnInitialized()
{
	// Before the base makes the reveal, so the entries and the prompt are known by then.
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &ThisClass::HandlePromptConfirmed);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &ThisClass::HandlePromptCancelled);
	}
	if (QuantitySlider)
	{
		QuantitySlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleQuantityChanged);
	}

	PromptView.TitleText = PromptTitleText;
	PromptView.MessageText = PromptText;
	PromptView.ConfirmButton = ConfirmButton;
	PromptView.CancelButton = CancelButton;
	PromptView.QuantitySlider = QuantitySlider;
	PromptView.QuantityText = QuantityText;
	PromptView.QuantityFormat = QuantityFormat;

	// The panels show again as the designer left them, or plainly if they were left hidden.
	if (const UWidget* Entries = GetEntriesWidget())
	{
		EntriesVisibility = Entries->GetVisibility();
	}
	if (PromptPanel)
	{
		PromptPanelVisibility = GetShownVisibility(*PromptPanel, ESlateVisibility::SelfHitTestInvisible);
		PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PromptBackdrop)
	{
		PromptBackdropVisibility = GetShownVisibility(*PromptBackdrop, ESlateVisibility::Visible);
		PromptBackdrop->SetVisibility(ESlateVisibility::Collapsed);
	}

	Super::NativeOnInitialized();
}

void UModularContextMenuWidget::AddRevealParts(FModularUIReveal& InReveal)
{
	// The whole menu fades, a full-screen backdrop included, and Menu Panel moves, as it alone is laid out around its anchor.
	if (MenuPanel)
	{
		InReveal.Add(*this, *MenuPanel, OpenTransition);
	}
}

void UModularContextMenuWidget::RegisterConfirmAction()
{
	// Bound as the menu opens, and again as the mouse comes and goes over its entries.
}

// Anchor
void UModularContextMenuWidget::SetAnchorWidget(UWidget* InAnchorWidget)
{
	if (!InAnchorWidget)
	{
		SetAnchorToFocusOrCursor();
		return;
	}

	AnchorWidget = InAnchorWidget;
	AnchorBox = GetAbsoluteBox(InAnchorWidget->GetCachedGeometry());
	OpeningCursor = GetCursorPosition();
	bHasAnchor = true;
	PlaceMenu();
}

void UModularContextMenuWidget::SetAnchorBox(FVector2D AbsolutePosition, FVector2D AbsoluteSize)
{
	AnchorWidget.Reset();
	AnchorBox = FBox2D(AbsolutePosition, AbsolutePosition + AbsoluteSize.ComponentMax(FVector2D::ZeroVector));
	OpeningCursor = GetCursorPosition();
	bHasAnchor = true;
	PlaceMenu();
}

void UModularContextMenuWidget::SetAnchorToFocusOrCursor()
{
	AnchorWidget.Reset();
	AnchorBox = FBox2D(ForceInit);
	OpeningCursor = GetCursorPosition();

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	if (SlateUser)
	{
		const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
		const TSharedPtr<SWidget> Focused = SlateUser->GetFocusedWidget();
		if (Focused && Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad)
		{
			AnchorBox = GetAbsoluteBox(Focused->GetTickSpaceGeometry());
		}
		else
		{
			const FVector2D CursorPosition = SlateUser->GetCursorPosition();
			AnchorBox = FBox2D(CursorPosition, CursorPosition);
		}
	}
	bHasAnchor = true;
	PlaceMenu();
}

// Entries
UCommonButtonBase* UModularContextMenuWidget::AddEntry(const FModularMenuEntry& Entry)
{
	UCommonButtonBase* Button = EntryButtonClass && EntryPanel ? CreateWidget<UCommonButtonBase>(this, EntryButtonClass) : nullptr;
	if (!Button)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s: nowhere to add the entry %s. Give the menu an Entry Panel and an Entry Button Class."),
			*GetPathNameSafe(this), *Entry.Id.ToString());
		return nullptr;
	}

	EntryPanel->AddChild(Button);
	if (UModularButton* ModularButton = Cast<UModularButton>(Button))
	{
		ModularButton->SetLabelText(Entry.Label);
		ModularButton->SetIconTexture(Entry.Icon);
	}
	Button->SetIsEnabled(Entry.bEnabled);
	Button->OnClicked().AddUObject(this, &ThisClass::HandleMadeEntryClicked, MadeEntries.Num());

	FMadeEntry& Made = MadeEntries.AddDefaulted_GetRef();
	Made.Button = Button;
	Made.Entry = Entry;
	MadeEntryButtons.Add(Button);

	// Added while open: placed again for its new size, and the first entry on screen is highlighted if none was.
	if (IsActivated() && !IsClosing())
	{
		PlaceMenu();
		if (!HighlightedEntry.IsValid())
		{
			HighlightEntry(Cast<UCommonButtonBase>(GetDesiredFocusTarget()), /*bFocus*/ false);
			FocusDesiredTarget();
		}
	}
	return Button;
}

void UModularContextMenuWidget::ClearEntries()
{
	for (UCommonButtonBase* Button : MadeEntryButtons)
	{
		if (Button)
		{
			if (Button == HighlightedEntry.Get())
			{
				ClearHighlight();
			}
			Button->OnClicked().RemoveAll(this);
			Button->RemoveFromParent();
		}
	}
	MadeEntryButtons.Reset();
	MadeEntries.Reset();
}

void UModularContextMenuWidget::HandleMadeEntryClicked(int32 Index)
{
	if (IsClosing() || bPrompting || !MadeEntries.IsValidIndex(Index))
	{
		return;
	}

	const FModularMenuEntry& Entry = MadeEntries[Index].Entry;
	if (!Entry.bEnabled)
	{
		return;
	}
	if (!Entry.bAskFirst)
	{
		ChooseEntry(Entry.Id, 0, Entry.bKeepsMenuOpen);
		return;
	}

	const TWeakObjectPtr<ThisClass> WeakThis = this;
	const FName EntryId = Entry.Id;
	const bool bKeepsMenuOpen = Entry.bKeepsMenuOpen;
	ShowPrompt(Entry.Prompt, [WeakThis, EntryId, bKeepsMenuOpen](bool bConfirmed, int32 Count)
	{
		ThisClass* Menu = WeakThis.Get();
		if (Menu && bConfirmed)
		{
			Menu->ChooseEntry(EntryId, Count, bKeepsMenuOpen);
		}
	});
}

void UModularContextMenuWidget::ChooseEntry(FName EntryId, int32 Count, bool bKeepsMenuOpen)
{
	// The menu starts to close first, so it takes no more input, and a menu opened for the entry in the same stack replaces it at once.
	if (!bKeepsMenuOpen)
	{
		CloseMenu();
	}
	OnEntryChosen.Broadcast(EntryId, Count);
}

FText UModularContextMenuWidget::GetEntryDescription(UCommonButtonBase* Entry) const
{
	const FMadeEntry* Made = MadeEntries.FindByPredicate([Entry](const FMadeEntry& Candidate) { return Candidate.Button.Get() == Entry; });
	return Made ? Made->Entry.Description : FText::GetEmpty();
}

UWidget* UModularContextMenuWidget::GetEntriesWidget() const
{
	return EntryPanel;
}

// Closing
void UModularContextMenuWidget::CloseMenu()
{
	CloseWidget();
}

void UModularContextMenuWidget::CloseMenuNow()
{
	CloseWidgetNow();
}

void UModularContextMenuWidget::NativeOnActivated()
{
	// Placed before the base plays the open transition, so it comes out of wherever it opened. A menu pushed without an anchor opens beside
	// the focus, or at the cursor.
	if (!bHasAnchor)
	{
		SetAnchorToFocusOrCursor();
	}

	Super::NativeOnActivated();

	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	RefreshConfirm();

	// The first entry is highlighted at once, and takes focus once the menu is laid out: Common UI focuses it as the menu activates, but
	// that can be before the menu was ever drawn, when it can't take focus yet. A menu shown again from under another keeps its entry,
	// highlighted afresh so its description shows again.
	UCommonButtonBase* Entry = Cast<UCommonButtonBase>(GetDesiredFocusTarget());
	ClearHighlight();
	HighlightEntry(Entry, /*bFocus*/ false);
	FocusDesiredTarget();

	// The menu is reused, and an entry it last closed under, or hid under the cursor for a prompt, can still think it's hovered.
	RefreshHover();
}

void UModularContextMenuWidget::NativeOnDeactivated()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PlaceHandle);
	PlaceHandle = FTSTicker::FDelegateHandle();
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.RemoveAll(this);
	}

	// A question still asked goes unanswered: its dialog closes with the menu, and the prompt is ready for the next opening.
	PromptAnswer = TFunction<void(bool, int32)>();
	if (UModularPromptWidget* Dialog = PromptDialog.Get())
	{
		PromptDialog.Reset();
		Dialog->CloseWidgetNow();
	}
	bPrompting = false;
	HidePrompt();

	// Anchored afresh the next time it opens. The entries it goes from under stop looking hovered, so they don't come back lit.
	bHasAnchor = false;
	AnchorWidget.Reset();
	RefreshHover();

	Super::NativeOnDeactivated();
}

UWidget* UModularContextMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}

	// While a prompt asks here: its button the mouse or focus reached last, or else where it starts: the count to choose, then the safe
	// answer to a confirmation, or the prompt's first button.
	UCommonButtonBase* Highlighted = HighlightedEntry.Get();
	const bool bHighlightUsable = Highlighted && Highlighted->IsVisible() && Highlighted->GetIsEnabled();
	if (bPromptShown)
	{
		if (bHighlightUsable && IsPromptButton(Highlighted))
		{
			return Highlighted;
		}
		if (PromptView.GetRequest().Kind == EModularPromptKind::Quantity && QuantitySlider && QuantitySlider->IsVisible())
		{
			return QuantitySlider.Get();
		}
		if (bConfirmStartsOnCancel && CancelButton && CancelButton->IsVisible())
		{
			return CancelButton.Get();
		}
		if (UCommonButtonBase* First = FindFirstButtonIn(PromptPanel.Get()))
		{
			return First;
		}
		return ConfirmButton.Get();
	}

	// The highlighted entry, such as the one a prompt was answered for, or else the first.
	if (bHighlightUsable)
	{
		return Highlighted;
	}
	return FindFirstButtonIn(MenuPanel.Get());
}

bool UModularContextMenuWidget::NativeOnHandleBackAction()
{
	if (!bIsBackHandler)
	{
		return Super::NativeOnHandleBackAction();
	}

	// Back answers a prompt first, then closes the menu, as the base does, after its close transition and Close Animation.
	if (!IsClosing() && bPrompting)
	{
		CancelPrompt();
	}
	else if (!IsClosing() && !BP_OnHandleBackAction())
	{
		CloseMenu();
	}
	return true;
}

void UModularContextMenuWidget::GoBack()
{
	if (IsClosing())
	{
		return;
	}
	if (bPrompting)
	{
		CancelPrompt();
	}
	else if (!BP_OnHandleBackAction())
	{
		CloseMenu();
	}
}

// Highlight
void UModularContextMenuWidget::SetHighlightedEntry(UCommonButtonBase* Entry, bool bFocus)
{
	HighlightEntry(Entry, bFocus);
}

void UModularContextMenuWidget::HighlightEntry(UCommonButtonBase* Entry, bool bFocus)
{
	if (IsClosing() || !IsEntry(Entry))
	{
		return;
	}

	UCommonButtonBase* Previous = HighlightedEntry.Get();
	if (Entry == Previous)
	{
		return;
	}
	HighlightedEntry = Entry;

	if (bKeepEntryHighlighted)
	{
		// Only one entry is ever selected: not only the one highlighted before, but any other left selected, such as a prompt's button
		// from the last time this reused menu was open, or one a button style selects as it takes focus.
		ForEachEntry([Entry](UCommonButtonBase& Other)
		{
			if (&Other != Entry && Other.GetSelected())
			{
				Other.ClearSelection();
			}
		});

		// Selected, so its style's Selected look shows it, yet still clickable, and only the menu unselects it.
		Entry->SetIsSelectable(true);
		Entry->SetIsToggleable(false);
		Entry->SetIsInteractableWhenSelected(true);
		Entry->SetIsSelected(true, /*bGiveClickFeedback*/ false);
	}

	// The prompt's buttons say nothing of their own: the question is on screen.
	if (!IsPromptButton(Entry))
	{
		const FText Description = GetEntryDescription(Entry);
		ShowDescription(Description);
		NativeOnEntryHighlighted(Entry, Description);
		OnEntryHighlighted(Entry, Description);
	}
	if (bFocus)
	{
		Entry->SetFocus();
	}
}

void UModularContextMenuWidget::ClearHighlight()
{
	UCommonButtonBase* Previous = HighlightedEntry.Get();
	if (Previous && bKeepEntryHighlighted)
	{
		Previous->ClearSelection();
	}
	HighlightedEntry.Reset();
}

void UModularContextMenuWidget::ShowDescription(const FText& Description)
{
	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
	}
}

bool UModularContextMenuWidget::IsEntry(UCommonButtonBase* Button) const
{
	return Button && ((MenuPanel && Button->IsChildOf(MenuPanel.Get())) || IsPromptButton(Button));
}

bool UModularContextMenuWidget::IsPromptButton(UCommonButtonBase* Button) const
{
	return Button && (Button == ConfirmButton || Button == CancelButton || (PromptPanel && Button->IsChildOf(PromptPanel.Get())));
}

void UModularContextMenuWidget::ForEachEntry(TFunctionRef<void(UCommonButtonBase&)> Callback) const
{
	// The prompt's buttons are usually inside Menu Panel, but needn't be, so each button is visited once wherever it is.
	TSet<const UCommonButtonBase*> Visited;
	for (UWidget* Root : { static_cast<UWidget*>(MenuPanel.Get()), static_cast<UWidget*>(PromptPanel.Get()),
		static_cast<UWidget*>(ConfirmButton.Get()), static_cast<UWidget*>(CancelButton.Get()) })
	{
		if (!Root)
		{
			continue;
		}
		UWidgetTree::ForWidgetAndChildren(Root, [&Visited, &Callback](UWidget* Widget)
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget);
			bool bAlreadyVisited = false;
			if (Button)
			{
				Visited.Add(Button, &bAlreadyVisited);
			}
			if (Button && !bAlreadyVisited)
			{
				Callback(*Button);
			}
		});
	}
}

void UModularContextMenuWidget::RefreshHover()
{
	// Slate only sends a widget Mouse Leave as the cursor moves, so one hidden or moved from under a still cursor stays hovered. A move in
	// place sends it, on the next frame, once the change has been drawn: what's under the cursor is found from what was drawn last.
	FTSTicker::GetCoreTicker().RemoveTicker(HoverHandle);
	HoverHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		HoverHandle = FTSTicker::FDelegateHandle();
		MoveCursorInPlace(GetOwningLocalPlayer());
		return false;
	}));
}

void UModularContextMenuWidget::FocusDesiredTarget()
{
	// Chosen once it's drawn rather than now, as what shows can still change this frame, such as entries listed again.
	CallWhenDrawn(MenuPanel.Get(), [this](UWidget&)
	{
		if (UWidget* Target = GetDesiredFocusTarget())
		{
			Target->SetFocus();
		}
	});
}

void UModularContextMenuWidget::NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	// Gamepad and keyboard focus highlight the entry they reach, the prompt's buttons included. Focus on anything else, such as Quantity
	// Slider, leaves the highlight where it was.
	UCommonButtonBase* Focused = nullptr;
	ForEachEntry([&Focused, &NewWidgetPath](UCommonButtonBase& Button)
	{
		const TSharedPtr<SWidget> SlateButton = Button.GetCachedWidget();
		if (!Focused && SlateButton && IsOnPath(NewWidgetPath, *SlateButton))
		{
			Focused = &Button;
		}
	});
	HighlightEntry(Focused, /*bFocus*/ false);
}

FReply UModularContextMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	RefreshConfirm();

	// The mouse highlights any entry it's over, and focus goes with it so Accept picks what's highlighted. Off the entries, the last one
	// stays highlighted.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	if (Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard)
	{
		HighlightEntry(FindHoveredButton(), /*bFocus*/ true);
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

UCommonButtonBase* UModularContextMenuWidget::FindHoveredButton() const
{
	UCommonButtonBase* Hovered = nullptr;
	ForEachEntry([&Hovered](UCommonButtonBase& Button)
	{
		if (!Hovered && Button.IsHovered() && Button.IsVisible() && Button.GetIsEnabled())
		{
			Hovered = &Button;
		}
	});
	return Hovered;
}

// Mouse: Right Click and Outside Click decide what presses do, on the menu and off it. While a prompt asks over the menu, presses off it never
// close the menu: the question has to be answered, or gone back from.
bool UModularContextMenuWidget::IsOnMenu(const FVector2D& ScreenPosition) const
{
	const bool bOnPanel = MenuPanel && MenuPanel->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	const bool bOnPrompt = bPromptShown && PromptPanel && PromptPanel->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	return bOnPanel || bOnPrompt;
}

FReply UModularContextMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsClosing())
	{
		return FReply::Handled();
	}

	const FKey Button = InMouseEvent.GetEffectingButton();
	const bool bOnMenu = IsOnMenu(InMouseEvent.GetScreenSpacePosition());
	const bool bOverlayAsks = bPromptShown && ShownPresentation == EModularPromptPresentation::Overlay;
	if (Button == EKeys::RightMouseButton)
	{
		switch (RightClick)
		{
		case EModularMenuRightClick::Back:
			GoBack();
			return FReply::Handled();
		case EModularMenuRightClick::PassThrough:
			return bOnMenu || bOverlayAsks ? FReply::Handled() : PressOutside(InGeometry, InMouseEvent, /*bPassThrough*/ true);
		case EModularMenuRightClick::Ignore:
			break;
		}
		return FReply::Handled();
	}

	if (bOnMenu)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (bOverlayAsks)
	{
		return FReply::Handled();
	}

	switch (OutsideClick)
	{
	case EModularMenuOutsideClick::Close:
		return PressOutside(InGeometry, InMouseEvent, /*bPassThrough*/ false);
	case EModularMenuOutsideClick::PassThrough:
		return PressOutside(InGeometry, InMouseEvent, /*bPassThrough*/ true);
	case EModularMenuOutsideClick::Block:
		break;
	}
	return FReply::Handled();
}

FReply UModularContextMenuWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// A double-click is a press like any other.
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UModularContextMenuWidget::PressOutside(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, bool bPassThrough)
{
	FReply Reply = FReply::Handled();
	if (bPassThrough && PassPressThrough(InGeometry, InMouseEvent, Reply))
	{
		return Reply;
	}
	CloseMenu();
	return FReply::Handled();
}

bool UModularContextMenuWidget::PassPressThrough(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, FReply& OutReply)
{
	return false;
}

// Accept goes to the focused button, before any action binding sees the key, but focus doesn't follow the mouse. So with the mouse on an
// entry, Accept clicks that one. The menu sees the key before its buttons do.
FReply UModularContextMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsClosing())
	{
		return FReply::Handled();
	}

	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bMouse = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
	if (bMouse && FindHoveredButton() && FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		if (!InKeyEvent.IsRepeat())
		{
			ClickAtCursor(GetOwningLocalPlayer());
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

// Confirm: shown for what Accept does. If focus has left the entries, its key comes here and clicks the entry under the mouse.
void UModularContextMenuWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	RefreshConfirm();
}

void UModularContextMenuWidget::RefreshConfirm()
{
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bClicks = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard && FindHoveredButton();
	if (ConfirmHandle.IsValid() && bConfirmClicks != bClicks)
	{
		ConfirmHandle.Unregister();
		ConfirmHandle = FUIActionBindingHandle();
	}
	if (!ConfirmHandle.IsValid() && !ConfirmAction.IsNull())
	{
		// Without the mouse on an entry, it's only shown: not consuming it lets Accept reach the focused button.
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, bClicks ? FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfirmAction) : FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = bClicks;
		ConfirmHandle = RegisterUIActionBinding(ConfirmArgs);
		bConfirmClicks = bClicks;
	}
}

void UModularContextMenuWidget::HandleConfirmAction()
{
	// A real click, so the entry does all it does for one, its sounds included.
	if (FindHoveredButton())
	{
		ClickAtCursor(GetOwningLocalPlayer());
	}
}

// Prompt
void UModularContextMenuWidget::K2_ShowPrompt(const FModularPromptRequest& Request)
{
	const TWeakObjectPtr<ThisClass> WeakThis = this;
	ShowPrompt(Request, [WeakThis](bool bConfirmed, int32 Count)
	{
		if (ThisClass* Menu = WeakThis.Get())
		{
			Menu->OnPromptAnswered(bConfirmed, Count);
		}
	});
}

void UModularContextMenuWidget::ShowPrompt(const FModularPromptRequest& Request, TFunction<void(bool bConfirmed, int32 Count)>&& OnAnswered)
{
	if (IsClosing() || !IsActivated())
	{
		return;
	}

	// A question still waiting goes without an answer.
	if (bPrompting)
	{
		PromptAnswer = TFunction<void(bool, int32)>();
		CancelPrompt();
	}

	// A dialog of its own, on a layer above the menu.
	UModularUISubsystem* UI = UModularUISubsystem::Get(GetOwningLocalPlayer());
	const bool bWantsDialog = PromptPresentation == EModularPromptPresentation::Dialog && PromptClass && PromptLayer.IsValid();
	UModularPromptWidget* Dialog = bWantsDialog && UI ? UI->PushWidgetToLayer<UModularPromptWidget>(PromptLayer, PromptClass.Get(), [&Request](UModularPromptWidget& Prompt)
	{
		Prompt.SetRequest(Request);
	}) : nullptr;
	if (Dialog)
	{
		bPrompting = true;
		ShownPresentation = EModularPromptPresentation::Dialog;
		PromptAnswer = MoveTemp(OnAnswered);
		PromptEntry = HighlightedEntry;
		PromptDialog = Dialog;

		const TWeakObjectPtr<ThisClass> WeakThis = this;
		Dialog->SetOnAnswered([WeakThis](bool bConfirmed, int32 Count)
		{
			if (ThisClass* Menu = WeakThis.Get())
			{
				Menu->HandleDialogAnswered(bConfirmed, Count);
			}
		});
		return;
	}
	if (bWantsDialog)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s: no layer %s to push its prompt dialog onto, so it asks here instead."), *GetPathNameSafe(this), *PromptLayer.ToString());
	}

	// Without anywhere to ask, it's as if the question were answered yes at once, with the count it would start at.
	if (!PromptPanel || !ConfirmButton)
	{
		const bool bQuantity = Request.Kind == EModularPromptKind::Quantity;
		const int32 Count = bQuantity ? FMath::Clamp(Request.DefaultCount, FMath::Max(0, Request.MinCount), FMath::Max(Request.MinCount, Request.MaxCount)) : 0;
		if (OnAnswered)
		{
			OnAnswered(true, Count);
		}
		return;
	}

	bPrompting = true;
	PromptAnswer = MoveTemp(OnAnswered);
	PromptEntry = HighlightedEntry;
	ShowPromptHere(Request, PromptPresentation == EModularPromptPresentation::Overlay ? EModularPromptPresentation::Overlay : EModularPromptPresentation::Inline);
}

void UModularContextMenuWidget::ShowPromptHere(const FModularPromptRequest& Request, EModularPromptPresentation Presentation)
{
	ShownPresentation = Presentation;
	bPromptShown = true;
	PromptView.Show(Request);
	PromptPanel->SetVisibility(PromptPanelVisibility);

	// Inline, the question takes the entries' place. Over the menu, the entries stay where they are but can't be picked meanwhile.
	if (UWidget* Entries = GetEntriesWidget())
	{
		if (Presentation == EModularPromptPresentation::Inline)
		{
			Entries->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Entries->SetIsEnabled(false);
		}
	}
	if (PromptBackdrop && Presentation == EModularPromptPresentation::Overlay)
	{
		PromptBackdrop->SetVisibility(PromptBackdropVisibility);
	}
	PlaceMenu();

	// Where the prompt starts is highlighted at once, as the menu's first entry is as it opens, and takes focus once it's laid out. The entry
	// just picked was under the cursor as its panel went, so it's told it no longer is.
	HighlightEntry(Cast<UCommonButtonBase>(GetDesiredFocusTarget()), /*bFocus*/ false);
	FocusDesiredTarget();
	RefreshHover();
}

void UModularContextMenuWidget::HidePrompt()
{
	const bool bWasShown = bPromptShown;
	bPromptShown = false;
	if (PromptPanel)
	{
		PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PromptBackdrop)
	{
		PromptBackdrop->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UWidget* Entries = GetEntriesWidget())
	{
		Entries->SetVisibility(EntriesVisibility);
		Entries->SetIsEnabled(true);
	}

	if (bWasShown && IsActivated() && !IsClosing())
	{
		PlaceMenu();

		// Back on the entry that asked, which takes focus again, or if the answer listed the entries again without it, on what's highlighted
		// now. The prompt's buttons went from under the cursor, so they're told they no longer are under it.
		RestoreHighlightAfterPrompt();
		FocusDesiredTarget();
		RefreshHover();
	}
	PromptEntry.Reset();
}

void UModularContextMenuWidget::RestoreHighlightAfterPrompt()
{
	UCommonButtonBase* Asked = PromptEntry.Get();
	if (Asked && IsEntry(Asked) && !IsPromptButton(Asked) && Asked->IsVisible())
	{
		ClearHighlight();
		HighlightEntry(Asked, /*bFocus*/ false);
	}
	else if (!HighlightedEntry.IsValid() || IsPromptButton(HighlightedEntry.Get()))
	{
		ClearHighlight();
		HighlightEntry(Cast<UCommonButtonBase>(GetDesiredFocusTarget()), /*bFocus*/ false);
	}
}

void UModularContextMenuWidget::CancelPrompt()
{
	if (UModularPromptWidget* Dialog = PromptDialog.Get())
	{
		// The dialog answers through Handle Dialog Answered.
		Dialog->Cancel();
		return;
	}
	AnswerPrompt(false);
}

void UModularContextMenuWidget::HandlePromptConfirmed()
{
	if (!IsClosing() && bPromptShown)
	{
		AnswerPrompt(true);
	}
}

void UModularContextMenuWidget::HandlePromptCancelled()
{
	if (!IsClosing() && bPromptShown)
	{
		AnswerPrompt(false);
	}
}

void UModularContextMenuWidget::HandleQuantityChanged(float Value)
{
	PromptView.Refresh();
}

void UModularContextMenuWidget::AnswerPrompt(bool bConfirmed)
{
	if (!bPrompting)
	{
		return;
	}

	bPrompting = false;
	const int32 Count = bConfirmed ? PromptView.GetCount() : 0;
	TFunction<void(bool, int32)> Answer = MoveTemp(PromptAnswer);
	PromptAnswer = TFunction<void(bool, int32)>();

	// A no goes back to the entries at once. A yes leaves the question up while its answer runs, so a menu that closes for it goes with the
	// question rather than with its entries coming back; one that stays open shows its entries again after.
	if (!bConfirmed)
	{
		HidePrompt();
	}
	if (Answer)
	{
		Answer(bConfirmed, Count);
	}
	if (bConfirmed && bPromptShown && IsActivated() && !IsClosing())
	{
		HidePrompt();
	}
}

void UModularContextMenuWidget::HandleDialogAnswered(bool bConfirmed, int32 Count)
{
	if (!bPrompting || !PromptDialog.IsValid())
	{
		return;
	}

	bPrompting = false;
	PromptDialog.Reset();
	TFunction<void(bool, int32)> Answer = MoveTemp(PromptAnswer);
	PromptAnswer = TFunction<void(bool, int32)>();
	if (Answer)
	{
		Answer(bConfirmed, Count);
	}

	// The dialog went, so the menu takes focus again, on the entry that asked, unless the answer closed it.
	if (IsActivated() && !IsClosing())
	{
		RestoreHighlightAfterPrompt();
		FocusDesiredTarget();
	}
	PromptEntry.Reset();
}

// Placing
void UModularContextMenuWidget::PlaceMenu()
{
	PlaceBeside(AnchorBox);
	PlaceNextFrame();
}

void UModularContextMenuWidget::PlaceNextFrame()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PlaceHandle);
	PlaceHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		PlaceHandle = FTSTicker::FDelegateHandle();
		if (IsActivated() && !IsClosing())
		{
			PlaceBeside(AnchorBox);
		}
		return false;
	}));
}

void UModularContextMenuWidget::PlaceBeside(const FBox2D& Box)
{
	UCanvasPanelSlot* PanelSlot = MenuPanel ? Cast<UCanvasPanelSlot>(MenuPanel->Slot) : nullptr;
	if (!PanelSlot || Placement == EModularMenuPlacement::AsDesigned || !Box.bIsValid)
	{
		// Not laid out around an anchor, so transitions From Origin come up from below, and grow from the middle.
		SetTransitionOrigin(FVector2D(0.0, -1.0), FVector2D(0.5, 0.5));
		return;
	}

	// In the canvas's own space, where its slots are positioned.
	const FVector2D Min = AbsoluteToCanvas(Box.Min);
	const FVector2D Max = AbsoluteToCanvas(Box.Max);
	const FVector2D ScreenSize = GetCanvasSize();
	const FBox2D ScreenBox(FVector2D::ZeroVector, ScreenSize);
	const FBox2D Inside(FVector2D(ScreenPadding.Left, ScreenPadding.Top), ScreenSize - FVector2D(ScreenPadding.Right, ScreenPadding.Bottom));

	PanelSlot->SetAutoSize(true);
	const FVector2D MenuSize = MeasureMenuPanel();

	// Where the menu goes, by the corner the canvas slot's alignment picks: the top left, unless the menu opens toward the middle.
	FVector2D Alignment = FVector2D::ZeroVector;
	FVector2D Point;

	// For transitions that come From Origin: the way from the anchor to the menu, and the menu's corner nearest the anchor.
	FVector2D Away;
	FVector2D Corner;

	const bool bAtCursor = Placement == EModularMenuPlacement::AtCursor && OpeningCursor.IsSet();
	if (bAtCursor)
	{
		// At the cursor while there was one as the menu opened: down and to the right of it, unless only the other side has room.
		const FVector2D At = AbsoluteToCanvas(OpeningCursor.GetValue());
		const bool bOpenLeft = At.X + MenuGap + MenuSize.X > Inside.Max.X && At.X - MenuGap - MenuSize.X >= Inside.Min.X;
		const bool bOpenUp = At.Y + MenuGap + MenuSize.Y > Inside.Max.Y && At.Y - MenuGap - MenuSize.Y >= Inside.Min.Y;
		Point = FVector2D(bOpenLeft ? At.X - MenuGap - MenuSize.X : At.X + MenuGap, bOpenUp ? At.Y - MenuGap - MenuSize.Y : At.Y + MenuGap);

		// Dropping down from the cursor, or up where it opened above it, as desktop menus do, from the corner at the cursor.
		Away = FVector2D(0.0, bOpenUp ? -1.0 : 1.0);
		Corner = FVector2D(bOpenLeft ? 1.0 : 0.0, bOpenUp ? 1.0 : 0.0);
	}
	else if (Placement == EModularMenuPlacement::TowardCenter || Placement == EModularMenuPlacement::AtCursor)
	{
		// Toward the middle, as At Cursor does without the mouse too, such as on a gamepad. Placed by the corner nearest the anchor, so the
		// menu keeps its edge level with the anchor's as it changes size.
		const FVector2D Middle = Inside.GetCenter();
		const bool bOpenLeft = Max.X > Middle.X;
		const bool bOpenUp = Max.Y > Middle.Y;
		Alignment = FVector2D(bOpenLeft ? 1.0 : 0.0, bOpenUp ? 1.0 : 0.0);
		Point = FVector2D(bOpenLeft ? Min.X - MenuGap : Max.X + MenuGap, bOpenUp ? Max.Y : Min.Y);

		// Out of the anchor's side, from the corner level with its edge.
		Away = FVector2D(bOpenLeft ? -1.0 : 1.0, 0.0);
		Corner = Alignment;
	}
	else
	{
		// On the side asked for, unless only the other side has room. Level with the anchor's top.
		const bool bRoomRight = Max.X + MenuGap + MenuSize.X <= Inside.Max.X;
		const bool bRoomLeft = Min.X - MenuGap - MenuSize.X >= Inside.Min.X;
		const bool bOpenLeft = Placement == EModularMenuPlacement::Left ? bRoomLeft || !bRoomRight : !bRoomRight && bRoomLeft;
		Point = FVector2D(bOpenLeft ? Min.X - MenuGap - MenuSize.X : Max.X + MenuGap, Min.Y);

		// Out of the anchor's side, from the top corner nearest it.
		Away = FVector2D(bOpenLeft ? -1.0 : 1.0, 0.0);
		Corner = FVector2D(bOpenLeft ? 1.0 : 0.0, 0.0);
	}

	// A negative padding can't push the menu off screen.
	FVector2D TopLeft = Point - Alignment * MenuSize;
	TopLeft = KeepIn(TopLeft, MenuSize, Inside);
	TopLeft = KeepIn(TopLeft, MenuSize, ScreenBox);

	PanelSlot->SetAlignment(Alignment);
	PanelSlot->SetPosition(TopLeft + Alignment * MenuSize);
	SetTransitionOrigin(Away, Corner);
}

bool UModularContextMenuWidget::GetCanvasGeometry(FGeometry& OutGeometry) const
{
	const UWidget* Canvas = MenuPanel ? MenuPanel->GetParent() : nullptr;
	if (!Canvas)
	{
		return false;
	}

	OutGeometry = Canvas->GetCachedGeometry();
	const FVector2D Size = OutGeometry.GetLocalSize();
	return Size.X > 0.0 && Size.Y > 0.0;
}

FVector2D UModularContextMenuWidget::AbsoluteToCanvas(const FVector2D& AbsolutePoint)
{
	FGeometry CanvasGeometry;
	if (GetCanvasGeometry(CanvasGeometry))
	{
		return CanvasGeometry.AbsoluteToLocal(AbsolutePoint);
	}

	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, AbsolutePoint, PixelPosition, ViewportPosition);
	return ViewportPosition;
}

FVector2D UModularContextMenuWidget::GetCanvasSize()
{
	FGeometry CanvasGeometry;
	if (GetCanvasGeometry(CanvasGeometry))
	{
		return CanvasGeometry.GetLocalSize();
	}
	return UWidgetLayoutLibrary::GetViewportSize(this) / UWidgetLayoutLibrary::GetViewportScale(this);
}

TOptional<FVector2D> UModularContextMenuWidget::GetCursorPosition() const
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
	if (!SlateUser || !Input || Input->GetCurrentInputType() != ECommonInputType::MouseAndKeyboard)
	{
		return TOptional<FVector2D>();
	}
	return TOptional<FVector2D>(FVector2D(SlateUser->GetCursorPosition()));
}

FVector2D UModularContextMenuWidget::MeasureMenuPanel()
{
	// Pushed menus are placed before they're first drawn, so their size isn't known yet without a layout pass.
	TakeWidget();
	ForceLayoutPrepass();
	return MenuPanel ? MenuPanel->GetDesiredSize() : FVector2D::ZeroVector;
}
