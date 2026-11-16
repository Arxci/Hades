#include "Actions/InventoryItemAction.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "Widgets/InventoryItemMenuWidget.h"
#include "Widgets/InventoryScreenWidget.h"

namespace
{
	UWorld* FindContextWorld(const FInventoryItemActionContext& Context)
	{
		if (Context.Player)
		{
			return Context.Player->GetWorld();
		}
		if (Context.Screen)
		{
			return Context.Screen->GetWorld();
		}
		return Context.Menu ? Context.Menu->GetWorld() : nullptr;
	}
}

UWorld* UInventoryItemAction::GetWorld() const
{
	// Only the context's world, and never the outer's: the class default object must have none, so Blueprint subclasses get world context.
	return ContextWorld.Get();
}

bool UInventoryItemAction::CanRunFor(const FInventoryItemActionContext& Context) const
{
	TGuardValue<TWeakObjectPtr<UWorld>> World(ContextWorld, FindContextWorld(Context));
	return CanRun(Context);
}

FText UInventoryItemAction::GetUnavailableReasonFor(const FInventoryItemActionContext& Context) const
{
	TGuardValue<TWeakObjectPtr<UWorld>> World(ContextWorld, FindContextWorld(Context));
	return GetUnavailableReason(Context);
}

void UInventoryItemAction::GetQuantityRangeFor(const FInventoryItemActionContext& Context, int32& OutMin, int32& OutMax, int32& OutDefault) const
{
	TGuardValue<TWeakObjectPtr<UWorld>> World(ContextWorld, FindContextWorld(Context));
	OutMin = 1;
	OutMax = 1;
	OutDefault = 1;
	GetQuantityRange(Context, OutMin, OutMax, OutDefault);

	// Whatever the subclass says, the prompt needs a range to choose from.
	OutMin = FMath::Max(1, OutMin);
	OutMax = FMath::Max(OutMin, OutMax);
	OutDefault = FMath::Clamp(OutDefault, OutMin, OutMax);
}

void UInventoryItemAction::RunFor(const FInventoryItemActionContext& Context)
{
	TGuardValue<TWeakObjectPtr<UWorld>> World(ContextWorld, FindContextWorld(Context));
	Run(Context);
}

FText UInventoryItemAction::FormatText(const FText& Text, const FInventoryItemActionContext& Context) const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(GetContextObject(Context));
	const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Context.Item) : nullptr;
	return FText::FormatNamed(Text,
		TEXT("Item"), Definition ? Definition->DisplayName : FText::GetEmpty(),
		TEXT("Count"), FText::AsNumber(Context.Count));
}

bool UInventoryItemAction::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	return true;
}

FText UInventoryItemAction::GetUnavailableReason_Implementation(const FInventoryItemActionContext& Context) const
{
	return FText::GetEmpty();
}

void UInventoryItemAction::GetQuantityRange_Implementation(const FInventoryItemActionContext& Context, int32& OutMin, int32& OutMax, int32& OutDefault) const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(GetContextObject(Context));
	const int32 Count = Inventory ? Inventory->GetItemCount(Context.Item) : 1;
	OutMin = 1;
	OutMax = FMath::Max(1, Count);
	OutDefault = FMath::Max(1, OutMax / 2);
}

void UInventoryItemAction::Run_Implementation(const FInventoryItemActionContext& Context)
{
}

UInventoryUISubsystem* UInventoryItemAction::GetInteraction(const FInventoryItemActionContext& Context)
{
	return Context.Player ? UInventoryUISubsystem::Get(Context.Player->GetLocalPlayer()) : nullptr;
}

const UObject* UInventoryItemAction::GetContextObject(const FInventoryItemActionContext& Context)
{
	if (Context.Player)
	{
		return Context.Player;
	}
	if (Context.Screen)
	{
		return Context.Screen;
	}
	return Context.Menu;
}
