#include "HadesQuickSlotWidget.h"

#include "CommonActionWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"
#include "Hades/Player/HadesPlayerController.h"

#include "InventoryUISubsystem.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemWidget.h"

#define LOCTEXT_NAMESPACE "HadesQuickSlotWidget"

void UHadesQuickSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnClicked().AddUObject(this, &ThisClass::HandleClicked);
}

void UHadesQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (NumberText)
	{
		NumberText->SetText(FText::AsNumber(SlotIndex + 1));
	}
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	}
	HandlePawnChanged(nullptr, GetOwningPlayerPawn());
	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateWeakLambda(this, [this](const FInventoryChangeSet&) { Refresh(); }));
}

void UHadesQuickSlotWidget::NativeDestruct()
{
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}
	if (UHadesQuickSlotComponent* Component = QuickSlots.Get())
	{
		Component->OnChanged.RemoveDynamic(this, &ThisClass::Refresh);
	}
	QuickSlots.Reset();
	RegistryListener.Unbind();

	Super::NativeDestruct();
}

void UHadesQuickSlotWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (UHadesQuickSlotComponent* Previous = QuickSlots.Get())
	{
		Previous->OnChanged.RemoveDynamic(this, &ThisClass::Refresh);
	}

	QuickSlots = NewPawn ? NewPawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
	if (UHadesQuickSlotComponent* Current = QuickSlots.Get())
	{
		Current->OnChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
	}
	Refresh();
}

void UHadesQuickSlotWidget::Refresh()
{
	const UHadesQuickSlotComponent* Component = QuickSlots.Get();
	const FInventoryItemHandle Item = Component ? Component->GetSlotItem(SlotIndex) : FInventoryItemHandle();
	if (KeyIcon && Component)
	{
		KeyIcon->SetInputAction(Component->GetSlotKey(SlotIndex));
	}
	if (ItemWidget)
	{
		// Hidden, not collapsed, so a Size Box around it keeps its size while the slot is empty.
		ItemWidget->SetItem(Item);
		ItemWidget->SetVisibility(Item.IsSet() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	const UHadesInventoryComponent* Inventory = Component ? Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(Component->GetOwner())) : nullptr;
	const int32 Capacity = Inventory ? Inventory->GetCapacity(Item) : 0;
	const ESlateVisibility AmmoVisibility = Capacity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (AmmoText)
	{
		AmmoText->SetVisibility(AmmoVisibility);
		if (Capacity > 0)
		{
			const int32 Rounds = Inventory->GetLoadedRounds(Item);
			AmmoText->SetText(FText::AsNumber(Rounds));
			AmmoText->SetColorAndOpacity(AmmoColors.Pick(Rounds, Capacity));
		}
	}
	if (SpareAmmoText)
	{
		SpareAmmoText->SetVisibility(AmmoVisibility);
		if (Capacity > 0)
		{
			SpareAmmoText->SetText(FText::Format(LOCTEXT("SpareRounds", "/{0}"), Inventory->GetSpareRounds(Item)));
		}
	}

	const bool bNowInHand = Inventory && Inventory->IsInHand(Item);
	if (bNowInHand != bInHand)
	{
		bInHand = bNowInHand;
		OnInHandChanged(bInHand);
	}
}

void UHadesQuickSlotWidget::HandleClicked()
{
	AssignHeldItem();
}

bool UHadesQuickSlotWidget::AssignHeldItem()
{
	UInventoryUISubsystem* Interaction = UInventoryUISubsystem::Get(GetOwningLocalPlayer());
	UHadesQuickSlotComponent* Component = QuickSlots.Get();
	if (!Interaction || !Component || !Interaction->IsHolding())
	{
		return false;
	}

	if (!Component->CanAssign(Interaction->GetHeldItem()))
	{
		if (AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer()))
		{
			Controller->ShowMessage(LOCTEXT("CantAssign", "Only things to hold or use go in quick slots."));
		}
		return false;
	}

	// The slot only points at the item, so the item stays where it is.
	Component->AssignItem(SlotIndex, Interaction->GetHeldItem());
	Interaction->CancelHold();
	return true;
}

bool UHadesQuickSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (Cast<UInventoryDragDropOperation>(InOperation) && AssignHeldItem())
	{
		return true;
	}
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

FReply UHadesQuickSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UHadesQuickSlotComponent* Component = QuickSlots.Get();
	if (Component && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Component->ClearSlot(SlotIndex);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#undef LOCTEXT_NAMESPACE
