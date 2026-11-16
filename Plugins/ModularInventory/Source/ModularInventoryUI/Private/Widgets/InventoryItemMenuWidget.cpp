#include "Widgets/InventoryItemMenuWidget.h"

#include "Animation/WidgetAnimation.h"
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
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "ModularInventory.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryGridWidget.h"
#include "Widgets/InventoryItemActionButton.h"
#include "Widgets/InventoryItemWidget.h"
#include "Widgets/InventoryScreenWidget.h"

namespace
{
	FBox2D GetAbsoluteBox(const FGeometry& Geometry)
	{
		return FBox2D(Geometry.GetAbsolutePosition(), Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
	}

	/**
	 * The widget of type T a widget is drawn in, such as its inventory screen or grid: up through its parents, and from the top of each user
	 * widget to the user widget.
	 */
	template <typename T>
	T* FindEnclosing(UWidget* Widget)
	{
		while (Widget)
		{
			if (T* Found = Cast<T>(Widget))
			{
				return Found;
			}
			UWidget* Parent = Widget->GetParent();
			Widget = Parent ? Parent : Widget->GetTypedOuter<UUserWidget>();
		}
		return nullptr;
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
}

UInventoryItemMenuWidget::UInventoryItemMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UInventoryItemMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UInventoryItemMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

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

	// The panels show again as the designer left them, or plainly if the prompt was left collapsed.
	if (ActionPanel)
	{
		ActionPanelVisibility = ActionPanel->GetVisibility();
	}
	if (PromptPanel)
	{
		const ESlateVisibility Designed = PromptPanel->GetVisibility();
		const bool bDesignedHidden = Designed == ESlateVisibility::Collapsed || Designed == ESlateVisibility::Hidden;
		PromptPanelVisibility = bDesignedHidden ? ESlateVisibility::SelfHitTestInvisible : Designed;
	}
	HidePrompt();
}

void UInventoryItemMenuWidget::SetItem(FInventoryItemHandle InItem, UInventoryItemWidget* ItemWidget)
{
	Item = InItem;
	AnchorWidget = ItemWidget;
	Anchor = FindAnchor(ItemWidget);
	OpeningCursor = GetCursorInViewport();
	if (UInventoryScreenWidget* FoundScreen = FindEnclosing<UInventoryScreenWidget>(ItemWidget))
	{
		BindScreen(FoundScreen);
	}

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->CancelHold();
		Interaction->SetSelectedItem(Item);
	}

	if (ItemNameText)
	{
		const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
		const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Item) : nullptr;
		ItemNameText->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
	}

	HidePrompt();
	RefreshActions();
	NativeOnItemChanged();
	OnItemChanged();

	// Last, so the menu is measured with the buttons this item shows.
	PlaceBeside(Anchor);
}

void UInventoryItemMenuWidget::SetScreen(UInventoryScreenWidget* InScreen)
{
	BindScreen(InScreen);
}

UInventoryScreenWidget* UInventoryItemMenuWidget::GetScreen() const
{
	return Screen.Get();
}

void UInventoryItemMenuWidget::BindScreen(UInventoryScreenWidget* NewScreen)
{
	if (UInventoryScreenWidget* Previous = Screen.Get())
	{
		Previous->OnDeactivated().RemoveAll(this);
	}
	Screen = NewScreen;
	if (NewScreen && IsActivated())
	{
		NewScreen->OnDeactivated().AddUObject(this, &ThisClass::HandleScreenDeactivated);
	}
}

void UInventoryItemMenuWidget::HandleScreenDeactivated()
{
	// The game closed the screen under the menu, such as for a cutscene.
	CloseNow();
}

void UInventoryItemMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	bClosing = false;
	AfterClosing = TFunction<void()>();
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	BindScreen(Screen.Get());
	RefreshConfirm();

	if (OpenSound)
	{
		UGameplayStatics::PlaySound2D(this, OpenSound);
	}
	if (OpenAnimation)
	{
		PlayAnimation(OpenAnimation);
	}
}

void UInventoryItemMenuWidget::NativeOnDeactivated()
{
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	RegistryListener.Unbind();
	FTSTicker::GetCoreTicker().RemoveTicker(FocusHandle);
	FocusHandle = FTSTicker::FDelegateHandle();
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.RemoveAll(this);
	}

	// Closed some other way while Close Animation played, such as with its screen.
	const bool bWasClosing = bClosing;
	bClosing = false;
	AfterClosing = TFunction<void()>();
	if (bWasClosing && CloseAnimation)
	{
		StopAnimation(CloseAnimation);
	}

	// The screen says its own hint again. The buttons stay as they are while a layer's transition fades the menu out; Set Item replaces them.
	ShowHint(FText::GetEmpty());
	if (UInventoryScreenWidget* ClosedOn = Screen.Get())
	{
		ClosedOn->OnDeactivated().RemoveAll(this);
	}
	Screen.Reset();

	// Choosing keeps the item selected while another one is picked for it.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && Interaction->GetSelectedItem() == Item)
	{
		Interaction->SetSelectedItem(FInventoryItemHandle());
	}
	if (CloseSound)
	{
		UGameplayStatics::PlaySound2D(this, CloseSound);
	}

	Super::NativeOnDeactivated();
}

UWidget* UInventoryItemMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}

	// While a prompt asks: the count to choose, or the safe answer.
	if (IsPrompting())
	{
		if (bPromptQuantity && QuantitySlider && QuantitySlider->IsVisible())
		{
			return QuantitySlider.Get();
		}
		if (CancelButton && CancelButton->IsVisible())
		{
			return CancelButton.Get();
		}
		return ConfirmButton.Get();
	}
	return FindFirstButtonIn(MenuPanel.Get());
}

bool UInventoryItemMenuWidget::NativeOnHandleBackAction()
{
	if (!bIsBackHandler)
	{
		return Super::NativeOnHandleBackAction();
	}

	// Back answers a prompt first, then closes the menu, as the base class does, but after Close Animation.
	if (!bClosing && IsPrompting())
	{
		HandlePromptCancelled();
	}
	else if (!bClosing && !BP_OnHandleBackAction())
	{
		CloseMenu();
	}
	return true;
}

UInventoryUISubsystem* UInventoryItemMenuWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

// Closing
void UInventoryItemMenuWidget::CloseMenu()
{
	CloseThen(TFunction<void()>());
}

void UInventoryItemMenuWidget::CloseThen(TFunction<void()>&& AfterClose)
{
	if (bClosing || !IsActivated())
	{
		return;
	}

	if (CloseAnimation)
	{
		bClosing = true;
		AfterClosing = MoveTemp(AfterClose);
		PlayAnimation(CloseAnimation);
		return;
	}

	DeactivateWidget();
	if (AfterClose)
	{
		AfterClose();
	}
}

void UInventoryItemMenuWidget::CloseNow()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
}

void UInventoryItemMenuWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (bClosing && Animation && Animation == CloseAnimation.Get())
	{
		FinishClosing();
	}
}

void UInventoryItemMenuWidget::FinishClosing()
{
	// Deactivating clears what was to happen after, so it's taken first.
	TFunction<void()> AfterClose = MoveTemp(AfterClosing);
	AfterClosing = TFunction<void()>();
	DeactivateWidget();
	if (AfterClose)
	{
		AfterClose();
	}
}

void UInventoryItemMenuWidget::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	if (Item.IsSet() && Inventory && !Inventory->IsValidItem(Item))
	{
		CloseNow();
	}
}

// Actions
FInventoryItemActionContext UInventoryItemMenuWidget::GetActionContext() const
{
	FInventoryItemActionContext Context;
	Context.Item = Item;
	Context.Player = GetOwningPlayer();
	Context.Screen = Screen.Get();
	Context.Menu = const_cast<UInventoryItemMenuWidget*>(this);
	return Context;
}

void UInventoryItemMenuWidget::GatherActions(TArray<UInventoryItemAction*>& OutActions) const
{
	if (ActionSet)
	{
		for (UInventoryItemAction* Action : ActionSet->Actions)
		{
			if (Action)
			{
				OutActions.Add(Action);
			}
		}
	}
	for (UInventoryItemAction* Action : Actions)
	{
		if (Action)
		{
			OutActions.Add(Action);
		}
	}
}

void UInventoryItemMenuWidget::RefreshActions()
{
	ClearActionButtons();

	TArray<UInventoryItemAction*> Gathered;
	GatherActions(Gathered);
	const FInventoryItemActionContext Context = GetActionContext();
	for (UInventoryItemAction* Action : Gathered)
	{
		const bool bAvailable = Action->CanRunFor(Context);
		const bool bShown = bAvailable || Action->WhenUnavailable == EInventoryActionUnavailable::ShowDisabled;

		// A button placed in the designer and named by the action, or one made for it.
		UCommonButtonBase* Button = nullptr;
		if (!Action->ButtonName.IsNone())
		{
			Button = WidgetTree ? Cast<UCommonButtonBase>(WidgetTree->FindWidget(Action->ButtonName)) : nullptr;
			if (!Button)
			{
				UE_LOG(LogInventory, Warning, TEXT("[Inventory UI] %s: no button named %s for the action %s."),
					*GetPathNameSafe(this), *Action->ButtonName.ToString(), *GetNameSafe(Action));
				continue;
			}
			Button->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		else if (bShown && ActionButtonClass && ActionPanel)
		{
			UInventoryItemActionButton* Made = CreateWidget<UInventoryItemActionButton>(this, ActionButtonClass);
			if (Made)
			{
				ActionPanel->AddChild(Made);
				MadeButtons.Add(Made);
				Button = Made;
			}
		}
		if (!Button || !bShown)
		{
			continue;
		}

		const FText Reason = bAvailable ? FText::GetEmpty() : Action->GetUnavailableReasonFor(Context);
		if (UInventoryItemActionButton* ActionButton = Cast<UInventoryItemActionButton>(Button))
		{
			ActionButton->SetAction(Action, Action->DisplayName, bAvailable, Reason);
		}
		if (!Action->InputAction.IsNull())
		{
			Button->SetTriggeringInputAction(Action->InputAction);
		}

		// Gamepad focus moves Common UI's hidden cursor onto the button, so hovering covers both.
		const int32 Index = ShownActions.Num();
		Button->OnClicked().AddUObject(this, &ThisClass::HandleActionClicked, Index);
		Button->OnHovered().AddUObject(this, &ThisClass::HandleActionHovered, Index);

		FShownAction& Shown = ShownActions.AddDefaulted_GetRef();
		Shown.Action = Action;
		Shown.Button = Button;
		Shown.bAvailable = bAvailable;
		Shown.UnavailableReason = Reason;
	}

	// What the first entry does, until another is highlighted.
	if (!ShownActions.IsEmpty())
	{
		HandleActionHovered(0);
	}
}

void UInventoryItemMenuWidget::ClearActionButtons()
{
	// The stack reuses menus: buttons made for the last item go, and placed ones forget it.
	for (const FShownAction& Shown : ShownActions)
	{
		if (UCommonButtonBase* Button = Shown.Button.Get())
		{
			Button->OnClicked().RemoveAll(this);
			Button->OnHovered().RemoveAll(this);
		}
	}
	ShownActions.Reset();

	for (UInventoryItemActionButton* Made : MadeButtons)
	{
		if (Made)
		{
			Made->RemoveFromParent();
		}
	}
	MadeButtons.Reset();
}

void UInventoryItemMenuWidget::HandleActionHovered(int32 Index)
{
	const FShownAction* Shown = ShownActions.IsValidIndex(Index) ? &ShownActions[Index] : nullptr;
	const UInventoryItemAction* Action = Shown ? Shown->Action.Get() : nullptr;
	if (Action && !IsPrompting())
	{
		const bool bShowReason = !Shown->bAvailable && !Shown->UnavailableReason.IsEmpty();
		ShowHint(bShowReason ? Shown->UnavailableReason : Action->Description);
	}
}

void UInventoryItemMenuWidget::HandleActionClicked(int32 Index)
{
	if (bClosing || IsPrompting() || !ShownActions.IsValidIndex(Index))
	{
		return;
	}

	// What can run may have changed since the menu opened.
	const FShownAction Shown = ShownActions[Index];
	UInventoryItemAction* Action = Shown.Action.Get();
	const FInventoryItemActionContext Context = GetActionContext();
	if (!Action || !Shown.bAvailable || !Action->CanRunFor(Context))
	{
		const FText Reason = Action ? Action->GetUnavailableReasonFor(Context) : FText::GetEmpty();
		if (!Reason.IsEmpty())
		{
			ShowHint(Reason);
		}
		return;
	}

	if (Action->Prompt != EInventoryActionPrompt::None && PromptPanel && ConfirmButton)
	{
		ShowPrompt(Index);
		return;
	}

	// Without a prompt to ask in, a count is the one the prompt would start at.
	int32 Min = 0;
	int32 Max = 0;
	int32 Count = 0;
	if (Action->Prompt == EInventoryActionPrompt::Quantity)
	{
		Action->GetQuantityRangeFor(Context, Min, Max, Count);
	}
	RunAction(Action, Count);
}

void UInventoryItemMenuWidget::RunAction(UInventoryItemAction* Action, int32 Count)
{
	// The menu closes first, so the screen has focus again for whatever the action starts, such as choosing another item or opening another
	// menu. It's asked again as it runs, as a close animation may have played.
	FInventoryItemActionContext Context = GetActionContext();
	Context.Count = Count;
	const TWeakObjectPtr<UInventoryItemAction> WeakAction = Action;
	CloseThen([WeakAction, Context]()
	{
		UInventoryItemAction* ToRun = WeakAction.Get();
		if (ToRun && ToRun->CanRunFor(Context))
		{
			ToRun->RunFor(Context);
		}
	});
}

void UInventoryItemMenuWidget::ShowHint(const FText& Hint)
{
	if (UInventoryScreenWidget* ShownOn = Screen.Get())
	{
		ShownOn->ShowActionHint(Hint);
	}
}

// Prompt
void UInventoryItemMenuWidget::ShowPrompt(int32 Index)
{
	const UInventoryItemAction* Action = ShownActions.IsValidIndex(Index) ? ShownActions[Index].Action.Get() : nullptr;
	if (!Action || !PromptPanel)
	{
		return;
	}

	PromptIndex = Index;
	bPromptQuantity = Action->Prompt == EInventoryActionPrompt::Quantity;
	int32 Min = 1;
	int32 Max = 1;
	int32 Default = 1;
	if (bPromptQuantity)
	{
		Action->GetQuantityRangeFor(GetActionContext(), Min, Max, Default);
	}
	PromptDefaultCount = Default;
	PromptMaxCount = Max;

	const bool bSlider = bPromptQuantity && QuantitySlider;
	if (QuantitySlider)
	{
		QuantitySlider->SetMinValue(static_cast<float>(Min));
		QuantitySlider->SetMaxValue(static_cast<float>(Max));
		QuantitySlider->SetStepSize(1.0f);
		QuantitySlider->SetValue(static_cast<float>(Default));
		QuantitySlider->SetVisibility(bSlider ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (QuantityText)
	{
		QuantityText->SetVisibility(bPromptQuantity ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshPrompt();

	if (ActionPanel)
	{
		ActionPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	PromptPanel->SetVisibility(PromptPanelVisibility);
	PlaceBeside(Anchor);
	FocusNextFrame();
}

void UInventoryItemMenuWidget::HidePrompt()
{
	const bool bWasPrompting = IsPrompting();
	PromptIndex = INDEX_NONE;
	bPromptQuantity = false;
	if (PromptPanel)
	{
		PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ActionPanel)
	{
		ActionPanel->SetVisibility(ActionPanelVisibility);
	}
	if (bWasPrompting && IsActivated())
	{
		PlaceBeside(Anchor);
	}
}

void UInventoryItemMenuWidget::RefreshPrompt()
{
	const UInventoryItemAction* Action = ShownActions.IsValidIndex(PromptIndex) ? ShownActions[PromptIndex].Action.Get() : nullptr;
	if (!Action)
	{
		return;
	}

	FInventoryItemActionContext Context = GetActionContext();
	Context.Count = GetChosenCount();
	if (PromptText)
	{
		PromptText->SetText(Action->FormatText(Action->PromptText, Context));
	}
	if (QuantityText)
	{
		QuantityText->SetText(FText::FormatNamed(QuantityFormat, TEXT("Count"), FText::AsNumber(Context.Count), TEXT("Max"), FText::AsNumber(PromptMaxCount)));
	}
}

int32 UInventoryItemMenuWidget::GetChosenCount() const
{
	if (!bPromptQuantity)
	{
		return 0;
	}
	return QuantitySlider ? FMath::RoundToInt32(QuantitySlider->GetValue()) : PromptDefaultCount;
}

void UInventoryItemMenuWidget::HandleQuantityChanged(float Value)
{
	RefreshPrompt();
}

void UInventoryItemMenuWidget::HandlePromptConfirmed()
{
	UInventoryItemAction* Action = ShownActions.IsValidIndex(PromptIndex) ? ShownActions[PromptIndex].Action.Get() : nullptr;
	const int32 Count = GetChosenCount();
	HidePrompt();
	if (Action && !bClosing)
	{
		RunAction(Action, Count);
	}
}

void UInventoryItemMenuWidget::HandlePromptCancelled()
{
	HidePrompt();
	FocusNextFrame();
}

void UInventoryItemMenuWidget::FocusNextFrame()
{
	FTSTicker::GetCoreTicker().RemoveTicker(FocusHandle);
	FocusHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		FocusHandle = FTSTicker::FDelegateHandle();
		if (IsActivated() && !bClosing)
		{
			if (UWidget* Target = GetDesiredFocusTarget())
			{
				Target->SetFocus();
			}
		}
		return false;
	}));
}

// Placing
FBox2D UInventoryItemMenuWidget::FindAnchor(const UInventoryItemWidget* ItemWidget) const
{
	if (ItemWidget)
	{
		return GetAbsoluteBox(ItemWidget->GetCachedGeometry());
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	if (!SlateUser)
	{
		return FBox2D(ForceInit);
	}

	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
	const TSharedPtr<SWidget> Focused = SlateUser->GetFocusedWidget();
	if (Focused && Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		return GetAbsoluteBox(Focused->GetTickSpaceGeometry());
	}

	const FVector2D CursorPosition = SlateUser->GetCursorPosition();
	return FBox2D(CursorPosition, CursorPosition);
}

void UInventoryItemMenuWidget::PlaceBeside(const FBox2D& Box)
{
	UCanvasPanelSlot* PanelSlot = MenuPanel ? Cast<UCanvasPanelSlot>(MenuPanel->Slot) : nullptr;
	if (!PanelSlot || Placement == EInventoryMenuPlacement::AsDesigned)
	{
		return;
	}

	const FBox2D ItemBox = AbsoluteBoxToViewport(Box);
	const FVector2D Min = ItemBox.Min;
	const FVector2D Max = ItemBox.Max;
	const FVector2D ScreenSize = UWidgetLayoutLibrary::GetViewportSize(this) / UWidgetLayoutLibrary::GetViewportScale(this);
	const FBox2D ScreenBox(FVector2D::ZeroVector, ScreenSize);
	const FBox2D Inside = FindBounds(ScreenBox);

	PanelSlot->SetAutoSize(true);
	const FVector2D MenuSize = MeasureMenuPanel();

	// Where the menu goes, by the corner the canvas slot's alignment picks: the top left, unless the menu opens toward the middle.
	FVector2D Alignment = FVector2D::ZeroVector;
	FVector2D Point;

	const TOptional<FVector2D> CursorPoint = Placement == EInventoryMenuPlacement::AtCursor ? OpeningCursor : TOptional<FVector2D>();
	if (CursorPoint.IsSet())
	{
		// At the cursor while there was one as the menu opened: down and to the right of it, unless only the other side has room.
		const FVector2D At = CursorPoint.GetValue();
		const bool bOpenLeft = At.X + MenuGap + MenuSize.X > Inside.Max.X && At.X - MenuGap - MenuSize.X >= Inside.Min.X;
		const bool bOpenUp = At.Y + MenuGap + MenuSize.Y > Inside.Max.Y && At.Y - MenuGap - MenuSize.Y >= Inside.Min.Y;
		Point = FVector2D(bOpenLeft ? At.X - MenuGap - MenuSize.X : At.X + MenuGap, bOpenUp ? At.Y - MenuGap - MenuSize.Y : At.Y + MenuGap);
	}
	else if (Placement == EInventoryMenuPlacement::TowardCenter || Placement == EInventoryMenuPlacement::AtCursor)
	{
		// Toward the middle, as At Cursor does without the mouse too, such as on a gamepad. Placed by the corner nearest the item, so the menu
		// keeps its edge level with the item's as it changes size.
		const FVector2D Middle = Inside.GetCenter();
		const bool bOpenLeft = Max.X > Middle.X;
		const bool bOpenUp = Max.Y > Middle.Y;
		Alignment = FVector2D(bOpenLeft ? 1.0 : 0.0, bOpenUp ? 1.0 : 0.0);
		Point = FVector2D(bOpenLeft ? Min.X - MenuGap : Max.X + MenuGap, bOpenUp ? Max.Y : Min.Y);
	}
	else
	{
		// On the side asked for, unless only the other side has room. Level with the item's top.
		const bool bRoomRight = Max.X + MenuGap + MenuSize.X <= Inside.Max.X;
		const bool bRoomLeft = Min.X - MenuGap - MenuSize.X >= Inside.Min.X;
		const bool bOpenLeft = Placement == EInventoryMenuPlacement::Left ? bRoomLeft || !bRoomRight : !bRoomRight && bRoomLeft;
		Point = FVector2D(bOpenLeft ? Min.X - MenuGap - MenuSize.X : Max.X + MenuGap, Min.Y);
	}

	// Moved in as far as it takes to stay inside, and then on screen, whatever Keep Inside is.
	FVector2D TopLeft = Point - Alignment * MenuSize;
	TopLeft = KeepIn(TopLeft, MenuSize, Inside);
	TopLeft = KeepIn(TopLeft, MenuSize, ScreenBox);

	PanelSlot->SetAlignment(Alignment);
	PanelSlot->SetPosition(TopLeft + Alignment * MenuSize);
}

FBox2D UInventoryItemMenuWidget::FindBounds(const FBox2D& ScreenBox)
{
	FBox2D Bounds = ScreenBox;
	if (KeepInside == EInventoryMenuBounds::Grid)
	{
		const UInventoryGridWidget* Grid = FindEnclosing<UInventoryGridWidget>(AnchorWidget.Get());
		const FBox2D Cells = Grid ? Grid->GetCellsAbsoluteBox() : FBox2D(ForceInit);
		if (Cells.bIsValid)
		{
			// Only the part of the grid that's on screen.
			const FBox2D CellsInViewport = AbsoluteBoxToViewport(Cells);
			Bounds = FBox2D(CellsInViewport.Min.ComponentMax(ScreenBox.Min), CellsInViewport.Max.ComponentMin(ScreenBox.Max));
		}
	}

	Bounds.Min += FVector2D(KeepInsidePadding.Left, KeepInsidePadding.Top);
	Bounds.Max -= FVector2D(KeepInsidePadding.Right, KeepInsidePadding.Bottom);
	return Bounds;
}

FBox2D UInventoryItemMenuWidget::AbsoluteBoxToViewport(const FBox2D& AbsoluteBox)
{
	FVector2D PixelPosition;
	FVector2D Min;
	FVector2D Max;
	USlateBlueprintLibrary::AbsoluteToViewport(this, AbsoluteBox.Min, PixelPosition, Min);
	USlateBlueprintLibrary::AbsoluteToViewport(this, AbsoluteBox.Max, PixelPosition, Max);
	return FBox2D(Min, Max);
}

TOptional<FVector2D> UInventoryItemMenuWidget::GetCursorInViewport()
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
	if (!SlateUser || !Input || Input->GetCurrentInputType() != ECommonInputType::MouseAndKeyboard)
	{
		return TOptional<FVector2D>();
	}

	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, FVector2D(SlateUser->GetCursorPosition()), PixelPosition, ViewportPosition);
	return TOptional<FVector2D>(ViewportPosition);
}

FVector2D UInventoryItemMenuWidget::MeasureMenuPanel()
{
	// Pushed menus are placed before they're first drawn, so their size isn't known yet without a layout pass.
	TakeWidget();
	ForceLayoutPrepass();
	return MenuPanel ? MenuPanel->GetDesiredSize() : FVector2D::ZeroVector;
}

// Mouse: a right press on the menu closes it, as Back does. A press outside Menu Panel closes it too, and with Pass Through it goes on to
// the item under it, as if the menu weren't open. A left press then waits, as the item would, to see whether it ends in a click or a drag.
// The screen may open another menu as the press reaches an item, perhaps this same one again, so nothing here is touched after that.
FReply UInventoryItemMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	if (bClosing)
	{
		return FReply::Handled();
	}

	const FKey Button = InMouseEvent.GetEffectingButton();
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	if (MenuPanel && MenuPanel->GetCachedGeometry().IsUnderLocation(Position))
	{
		if (Button == EKeys::RightMouseButton)
		{
			CloseMenu();
			return FReply::Handled();
		}
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const bool bLeft = Button == EKeys::LeftMouseButton;
	const bool bRight = Button == EKeys::RightMouseButton;
	const bool bPassThrough = (bLeft && LeftPressOutside == EInventoryMenuOutsidePress::PassThrough)
		|| (bRight && RightPressOutside == EInventoryMenuOutsidePress::PassThrough);
	UInventoryItemWidget* ItemWidget = bPassThrough ? FindItemWidgetUnder(Position) : nullptr;
	if (ItemWidget && bLeft)
	{
		PressedItemWidget = ItemWidget;
		PressedItem = ItemWidget->GetItem();
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	// A right press reaches the item at once, as it does without a menu. The menu covered the item, so the item takes the hover first.
	UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bRightClickItem = ItemWidget && Interaction && !WouldReopen(ItemWidget, Interaction->GetStyle().RightClick);
	if (!bRightClickItem)
	{
		CloseMenu();
		return FReply::Handled();
	}

	CloseNow();
	Interaction->SetHoveredItem(ItemWidget->GetItem());
	ItemWidget->RightClick(InMouseEvent);
	return FReply::Handled();
}

FReply UInventoryItemMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryItemWidget* ItemWidget = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? TakePressedItemWidget() : nullptr;
	if (!ItemWidget || bClosing)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	// The press closes the menu, and is a click on the item if it's let go over it, as a click must start and end on the item. A click that
	// would only open a menu for this menu's item again leaves it closed. The menu covered the item, so the item takes the hover first.
	UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bClicked = Interaction && ItemWidget->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& !WouldReopen(ItemWidget, Interaction->GetStyle().LeftClick);
	CloseNow();
	if (bClicked)
	{
		Interaction->SetHoveredItem(ItemWidget->GetItem());
		ItemWidget->Click(InMouseEvent);
	}
	return FReply::Handled();
}

FReply UInventoryItemMenuWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bClosing)
	{
		return FReply::Handled();
	}

	// The second click of a double-click on the menu's own item, whose first click opened the menu: the double-click is the item's.
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	const bool bOnMenu = MenuPanel && MenuPanel->GetCachedGeometry().IsUnderLocation(Position);
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bDoubleClicks = Interaction && Interaction->GetStyle().DoubleClick != EInventoryItemPress::None;
	UInventoryItemWidget* ItemWidget = !bOnMenu && bDoubleClicks && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FindItemWidgetUnder(Position) : nullptr;
	if (ItemWidget && ItemWidget == AnchorWidget.Get())
	{
		CloseNow();
		ItemWidget->DoubleClick(InMouseEvent);
		return FReply::Handled();
	}

	// Otherwise it's a press like any other.
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventoryItemMenuWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	// The press became a drag of the item it was on, so it won't be a click: the drag picks the item up, as it does without a menu. The menu
	// closes either way, even when the item can't be picked up, as any press outside it does.
	UInventoryItemWidget* ItemWidget = TakePressedItemWidget();
	if (!ItemWidget || bClosing)
	{
		return;
	}

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SetHoveredItem(ItemWidget->GetItem());
	}
	OutOperation = ItemWidget->BeginMouseDrag(InMouseEvent);
	CloseNow();
}

UInventoryItemWidget* UInventoryItemMenuWidget::FindItemWidgetUnder(const FVector2D& ScreenPosition) const
{
	// A pooled item widget may show another item by now.
	UInventoryItemWidget* AnchorItemWidget = AnchorWidget.Get();
	if (AnchorItemWidget && AnchorItemWidget->GetItem() == Item && Anchor.IsInside(ScreenPosition))
	{
		return AnchorItemWidget;
	}

	const UInventoryScreenWidget* ShownOn = Screen.Get();
	return ShownOn ? ShownOn->FindItemWidgetAt(ScreenPosition) : nullptr;
}

bool UInventoryItemMenuWidget::WouldReopen(const UInventoryItemWidget* ItemWidget, EInventoryItemPress Press) const
{
	return Press == EInventoryItemPress::OpenItemMenu && ItemWidget && Item.IsSet() && ItemWidget->GetItem() == Item;
}

FReply UInventoryItemMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	RefreshConfirm();
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// Accept goes to the focused button, before any action binding sees the key, but focus doesn't follow the mouse. So with the mouse on a
// button, Accept clicks that one. The menu sees the key before its buttons do.
FReply UInventoryItemMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
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

// Confirm: shown for what Accept does. If focus has left the buttons, its key comes here and clicks the button under the mouse.
void UInventoryItemMenuWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	RefreshConfirm();
}

void UInventoryItemMenuWidget::RefreshConfirm()
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
		// Without the mouse on a button, it's only shown: not consuming it lets Accept reach the focused button.
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, bClicks ? FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfirmAction) : FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = bClicks;
		ConfirmHandle = RegisterUIActionBinding(ConfirmArgs);
		bConfirmClicks = bClicks;
	}
}

void UInventoryItemMenuWidget::HandleConfirmAction()
{
	// A real click, so the button does all it does for one, its sounds included.
	if (FindHoveredButton())
	{
		ClickAtCursor(GetOwningLocalPlayer());
	}
}

UCommonButtonBase* UInventoryItemMenuWidget::FindHoveredButton() const
{
	UCommonButtonBase* Hovered = nullptr;
	if (MenuPanel)
	{
		UWidgetTree::ForWidgetAndChildren(MenuPanel, [&Hovered](UWidget* Widget)
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget);
			if (!Hovered && Button && Button->IsHovered() && Button->IsVisible() && Button->GetIsEnabled())
			{
				Hovered = Button;
			}
		});
	}
	return Hovered;
}

UInventoryItemWidget* UInventoryItemMenuWidget::TakePressedItemWidget()
{
	UInventoryItemWidget* ItemWidget = PressedItemWidget.Get();
	const bool bSameItem = ItemWidget && PressedItem.IsSet() && ItemWidget->GetItem() == PressedItem;
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	return bSameItem ? ItemWidget : nullptr;
}
