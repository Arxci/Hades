#include "Widgets/InventoryShortcutWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

#include "InventoryRegistry.h"
#include "InventoryUISubsystem.h"
#include "Shortcuts/InventoryShortcutComponent.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemWidget.h"

void UInventoryShortcutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnClicked().AddUObject(this, &ThisClass::HandleClicked);
}

void UInventoryShortcutWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// A HUD is often made before the pawn exists, so the component is found again as the pawn changes.
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	}
	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateWeakLambda(this, [this](const FInventoryChangeSet& Changes)
	{
		const FInventoryItemHandle Item = GetSlotItem();
		if (Item.IsSet() && Changes.Items.Contains(Item))
		{
			Refresh();
		}
	}));
	BindShortcuts(UInventoryShortcutComponent::FindShortcutComponent(GetOwningPlayerPawn()));
}

void UInventoryShortcutWidget::NativeDestruct()
{
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}
	RegistryListener.Unbind();

	// Unbound without refreshing, so no Blueprint event fires as the widget goes.
	if (UInventoryShortcutComponent* Previous = Shortcuts.Get())
	{
		Previous->OnShortcutsChanged.RemoveDynamic(this, &ThisClass::HandleShortcutsChanged);
		Previous->OnShortcutActivated.RemoveDynamic(this, &ThisClass::HandleShortcutActivated);
	}
	Shortcuts.Reset();

	Super::NativeDestruct();
}

void UInventoryShortcutWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindShortcuts(UInventoryShortcutComponent::FindShortcutComponent(NewPawn));
}

void UInventoryShortcutWidget::BindShortcuts(UInventoryShortcutComponent* NewShortcuts)
{
	if (UInventoryShortcutComponent* Previous = Shortcuts.Get())
	{
		Previous->OnShortcutsChanged.RemoveDynamic(this, &ThisClass::HandleShortcutsChanged);
		Previous->OnShortcutActivated.RemoveDynamic(this, &ThisClass::HandleShortcutActivated);
	}

	Shortcuts = NewShortcuts;
	if (NewShortcuts)
	{
		NewShortcuts->OnShortcutsChanged.AddUniqueDynamic(this, &ThisClass::HandleShortcutsChanged);
		NewShortcuts->OnShortcutActivated.AddUniqueDynamic(this, &ThisClass::HandleShortcutActivated);
	}
	Refresh();
}

void UInventoryShortcutWidget::SetSlotIndex(int32 InSlotIndex)
{
	SlotIndex = FMath::Max(0, InSlotIndex);
	Refresh();
}

int32 UInventoryShortcutWidget::GetSlotIndex() const
{
	const UInventoryShortcutComponent* Component = Shortcuts.Get();
	if (SlotTag.IsValid())
	{
		return Component ? Component->FindSlotWithTag(SlotTag) : INDEX_NONE;
	}
	return SlotIndex;
}

FInventoryItemHandle UInventoryShortcutWidget::GetSlotItem() const
{
	const UInventoryShortcutComponent* Component = Shortcuts.Get();
	return Component ? Component->GetSlotItem(GetSlotIndex()) : FInventoryItemHandle();
}

void UInventoryShortcutWidget::HandleShortcutsChanged()
{
	Refresh();
}

void UInventoryShortcutWidget::HandleShortcutActivated(int32 ActivatedSlot, FInventoryItemHandle Item)
{
	if (ActivatedSlot != INDEX_NONE && ActivatedSlot == GetSlotIndex())
	{
		OnShortcutActivated(Item);
	}
}

void UInventoryShortcutWidget::Refresh()
{
	const UInventoryShortcutComponent* Component = Shortcuts.Get();
	const int32 ShownSlot = GetSlotIndex();
	const FInventoryItemHandle Item = GetSlotItem();

	if (ItemWidget)
	{
		// Hidden, not collapsed, so a Size Box around it keeps its size while the slot is empty. It never takes clicks itself: an item
		// widget would start dragging the item out of its real container.
		ItemWidget->SetItem(Item);
		ItemWidget->SetVisibility(Item.IsSet() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (NameText)
	{
		NameText->SetText(Component ? Component->GetSlotDisplayName(ShownSlot) : FText::GetEmpty());
	}
	if (NumberText)
	{
		NumberText->SetText(ShownSlot != INDEX_NONE ? FText::AsNumber(ShownSlot + 1) : FText::GetEmpty());
	}
	if (EmptyMarker)
	{
		EmptyMarker->SetVisibility(Item.IsSet() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Item != ShownItem || !bShownOnce)
	{
		ShownItem = Item;
		bShownOnce = true;
		OnShortcutChanged(Item);
	}
}

void UInventoryShortcutWidget::HandleClicked()
{
	AssignHeldItem();
}

bool UInventoryShortcutWidget::AssignHeldItem()
{
	UInventoryUISubsystem* Interaction = UInventoryUISubsystem::Get(GetOwningLocalPlayer());
	UInventoryShortcutComponent* Component = Shortcuts.Get();
	if (!Interaction || !Component || !Interaction->IsHolding())
	{
		return false;
	}

	const FInventoryItemHandle Held = Interaction->GetHeldItem();
	if (!Component->AssignItem(GetSlotIndex(), Held))
	{
		OnAssignRefused(Held);
		return false;
	}

	// The slot only points at the item, so the item goes back where it was.
	Interaction->CancelHold();
	return true;
}

bool UInventoryShortcutWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (Cast<UInventoryDragDropOperation>(InOperation) && AssignHeldItem())
	{
		return true;
	}
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

FReply UInventoryShortcutWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryShortcutComponent* Component = Shortcuts.Get();
	if (bRightClickClears && Component && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Component->ClearSlot(GetSlotIndex());
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
