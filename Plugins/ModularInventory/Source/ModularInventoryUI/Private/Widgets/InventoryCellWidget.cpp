#include "Widgets/InventoryCellWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"

#define LOCTEXT_NAMESPACE "InventoryCellWidget"

#if WITH_EDITOR
void UInventoryCellWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	if (!BlueprintWidgetTree.RootWidget)
	{
		CompileLog.Warning(LOCTEXT("EmptyCell", "Give the cell a root widget, such as an empty Overlay: Common UI only builds a button around designer content, so an empty cell draws nothing and can't be clicked."));
	}
}
#endif

void UInventoryCellWidget::SetPlacement(FInventoryContainerHandle InContainer, const FInventoryPlacement& InPlacement)
{
	Container = InContainer;
	Placement = InPlacement;
}

FInventoryItemHandle UInventoryCellWidget::GetItem() const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	return Inventory ? Inventory->GetItemAt(Container, Placement) : FInventoryItemHandle();
}

void UInventoryCellWidget::SetLocked(bool bInLocked)
{
	// Disabled: the style's Disabled brush, no clicks or hover, and gamepad navigation passes over it.
	if (bInLocked != bLocked)
	{
		bLocked = bInLocked;
		SetIsEnabled(!bLocked);
	}
}

UInventoryUISubsystem* UInventoryCellWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

bool UInventoryCellWidget::IsTargeted() const
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	return Interaction && Container.IsSet() && Interaction->GetTargetContainer() == Container && Interaction->GetTargetPlacement() == Placement;
}

void UInventoryCellWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnHovered().AddUObject(this, &ThisClass::HandleHovered);
	OnUnhovered().AddUObject(this, &ThisClass::StopAiming);
	OnClicked().AddUObject(this, &ThisClass::HandleClicked);
	IdleStyle = GetStyle() ? GetStyle()->GetClass() : nullptr;
}

void UInventoryCellWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
	}
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleCellInputMethodChanged);
	}
	ApplyCellStyle();
}

void UInventoryCellWidget::NativeDestruct()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
	}
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UInventoryCellWidget::HandleHeldItemChanged()
{
	ApplyCellStyle();
}

void UInventoryCellWidget::HandleCellInputMethodChanged(ECommonInputType InputType)
{
	ApplyCellStyle();
}

void UInventoryCellWidget::ApplyCellStyle()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bMouse = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;

	TSubclassOf<UCommonButtonStyle> Wanted = IdleStyle;
	if (HoldingStyle && Interaction && Interaction->IsHolding())
	{
		Wanted = HoldingStyle;
	}
	else if (MouseStyle && bMouse)
	{
		Wanted = MouseStyle;
	}

	const UCommonButtonStyle* Current = GetStyle();
	if (Wanted && (!Current || Current->GetClass() != Wanted))
	{
		SetStyle(Wanted);
	}
}

// Gamepads aim by focus. Hover alone isn't enough: an item drawn over a grid cell takes the cell's hover.
// With a mouse, Common UI still focuses a cell as a screen opens or a menu closes, but the mouse does the aiming.
void UInventoryCellWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	if (bGamepad || InFocusEvent.GetCause() == EFocusCause::Navigation)
	{
		Aim(true);
	}
}

void UInventoryCellWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	StopAiming();
}

// The focused button takes Accept before any action binding sees the key, and keyboard focus doesn't follow the mouse. So with the
// mouse in use, Accept on a focused cell acts on what the mouse aims at, such as the hovered item, rather than on the cell. Keyboard
// navigation aims at the cell it focuses, so it still acts on that one.
FReply UInventoryCellWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bMouse = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
	if (bMouse && Interaction && FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		if (!InKeyEvent.IsRepeat())
		{
			Interaction->ActivateAimed();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UInventoryCellWidget::HandleHovered()
{
	Aim(false);
}

void UInventoryCellWidget::Aim(bool bByFocus)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!Interaction)
	{
		return;
	}

	Interaction->SetTarget(Container, Placement);
	const FInventoryItemHandle CellItem = GetItem();
	if (bByFocus || CellItem.IsSet() || !Interaction->GetStyle().bKeepLastHoveredItem)
	{
		Interaction->SetHoveredItem(CellItem);
	}
}

void UInventoryCellWidget::StopAiming()
{
	// A snapped held item stays on the last cells aimed at.
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (IsTargeted() && !(Interaction->GetStyle().bSnapHeldItem && Interaction->IsHolding()))
	{
		GetInteraction()->ClearTarget();
	}
}

void UInventoryCellWidget::HandleClicked()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->ActivatePlacement(Container, Placement);
	}
}

#undef LOCTEXT_NAMESPACE
