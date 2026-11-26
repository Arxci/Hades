#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InputTriggers.h"

#include "InventoryTypes.h"

#include "InventoryShortcutComponent.generated.h"

class UEnhancedInputComponent;
class UInputAction;
class UInventoryComponent;
class UInventoryItemDefinition;
struct FInventoryRegistry;


/** One shortcut slot: what tells it apart, what it's called, and what it takes. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryShortcutSlotDefinition
{
	GENERATED_BODY()

	/** Tells the slot apart, such as the input tag of the key that activates it, like Input.Shortcut.1. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcut")
	FGameplayTag Tag;

	/** Shown by shortcut widgets, such as 1 or Quick Use. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcut")
	FText DisplayName;

	/** Only items that match go on this slot, on top of the component's Filter, such as only consumables on a quick-use slot. Empty takes any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcut")
	FInventoryItemQuery Filter;

	/**
	 * The input action that activates the slot, such as IA_Shortcut1. Bind Slot Inputs binds it, and shortcut widgets show the key the
	 * player has mapped to it, so rebinding the key changes both. Optional.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcut")
	TObjectPtr<const UInputAction> InputAction = nullptr;
};


/** What a set of shortcut slots holds, and how it follows the inventory. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryShortcutRules
{
	GENERATED_BODY()

	/** One entry per slot, in order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcuts", meta = (TitleProperty = "Tag"))
	TArray<FInventoryShortcutSlotDefinition> Slots;

	/** What can go on any slot, such as Tag Query Item.Type.Weapon or Item.Type.Consumable. Empty takes any item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcuts")
	FInventoryItemQuery Filter;

	/**
	 * Items the owner gains go on the first empty slot that takes them, such as each new weapon found, unless an item of the same kind is
	 * on a slot already. What the owner starts with counts too. Off, slots are only filled by hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcuts")
	bool bAutoAssignNewItems = false;

	/** Only new items that match are put on a slot by themselves, on top of Filter. Empty: any that Filter takes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcuts", meta = (EditCondition = "bAutoAssignNewItems"))
	FInventoryItemQuery AutoAssignFilter;

	/**
	 * When a slot's item goes away, such as a stack used up or merged into another, the slot points at another item of the same kind the
	 * owner has, if there is one, so a quick slot for a potion keeps working while potions are left. Off, the slot empties.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shortcuts")
	bool bFollowSameKind = true;
};


/**
 * Which item each shortcut slot points at, and the rules for changing that, apart from any actor so the logic can be reused and tested.
 * Items count as held while they're anywhere inside Roots, such as an Inventory Component's containers, nested ones included.
 */
struct MODULARINVENTORY_API FInventoryShortcutSlots
{
	/** Sizes the slots, keeping the items that still have one. */
	void SetNum(int32 NumSlots);
	int32 Num() const { return Items.Num(); }

	/** The slot's item; unset for an empty slot or one that doesn't exist. */
	FInventoryItemHandle Get(int32 SlotIndex) const;

	/** The slot the item is on, or INDEX_NONE. */
	int32 Find(FInventoryItemHandle Item) const;

	/** Whether the item is held and the slot takes it, or with INDEX_NONE, whether any slot does. */
	bool CanAssign(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, const FInventoryShortcutRules& Rules,
		FInventoryItemHandle Item, int32 SlotIndex) const;

	/** Puts the item on the slot, taking it off any other, as an item is on one slot at most. False, changing nothing, when Can Assign says no. */
	bool Assign(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, const FInventoryShortcutRules& Rules,
		int32 SlotIndex, FInventoryItemHandle Item);

	/** The slot the item is on already, or else the first empty slot that takes it, which it's put on. INDEX_NONE when none does. */
	int32 AssignToFreeSlot(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, const FInventoryShortcutRules& Rules,
		FInventoryItemHandle Item);

	/** Empties the slot. True when it had an item. */
	bool Clear(int32 SlotIndex);

	/** Trades two slots' items, when each slot takes the other's. An empty slot can be swapped with. */
	bool Swap(const FInventoryRegistry& Registry, const FInventoryShortcutRules& Rules, int32 SlotA, int32 SlotB);

	/** A further check whether a slot takes an item, on top of the rules' filters, such as a component's Accepts Item. Unset takes what they do. */
	void SetSlotFilter(TFunction<bool(int32 SlotIndex, FInventoryItemHandle Item)>&& InSlotFilter) { SlotFilter = MoveTemp(InSlotFilter); }

	/**
	 * Follows the inventory after it changed: a slot whose item is no longer held points at another of the same kind with Follow Same Kind,
	 * or empties; with Auto Assign New Items, items held now that weren't at the last refresh go on free slots. True when any slot changed.
	 */
	bool Refresh(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, const FInventoryShortcutRules& Rules);

private:
	bool IsHeld(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, FInventoryItemHandle Item) const;
	bool SlotTakes(const FInventoryRegistry& Registry, const FInventoryShortcutRules& Rules, int32 SlotIndex, FInventoryItemHandle Item) const;
	bool HasKindOnSlot(const FInventoryRegistry& Registry, FInventoryItemHandle Item) const;
	void Set(const FInventoryRegistry& Registry, int32 SlotIndex, FInventoryItemHandle Item);

	TArray<FInventoryItemHandle> Items;

	/** The definition of each slot's item, to find another of its kind once the item is gone. */
	TArray<TWeakObjectPtr<const UInventoryItemDefinition>> Kinds;

	/** The items held at the last refresh, to tell new finds. */
	TSet<FInventoryItemHandle> Known;

	TFunction<bool(int32, FInventoryItemHandle)> SlotFilter;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryShortcutsChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryShortcutActivatedSignature, int32, SlotIndex, FInventoryItemHandle, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryShortcutSelectedSignature, int32, SlotIndex, FInventoryItemHandle, Item);


/**
 * Shortcut slots that point at items the owner carries, without moving them: quick slots, favorites, a weapon wheel, a hotbar. A slot
 * follows its item wherever it moves in the owner's inventory, and when the item leaves, the slot empties, or points at another of the
 * same kind. Activating a slot only reports it, with On Shortcut Activated, so each game decides what that does: draw a weapon, use a
 * potion, cast. One slot can be selected, such as a hotbar's, moved with Cycle Selected Slot and activated with Activate Selected Slot.
 * Each slot can have an input action: Bind Slot Inputs activates the slot with it, and shortcut widgets show its key.
 * Needs an Inventory Component on the same actor. For a hotbar that holds the items themselves, use a container instead.
 */
UCLASS(Blueprintable, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class MODULARINVENTORY_API UInventoryShortcutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryShortcutComponent();

	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts", meta = (DefaultToSelf = "Actor"))
	static UInventoryShortcutComponent* FindShortcutComponent(const AActor* Actor);

	/** Puts the item on the slot, taking it off any other slot. False when the owner doesn't have the item, or the slot doesn't take it. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool AssignItem(int32 SlotIndex, FInventoryItemHandle Item);

	/** Puts the item on the first empty slot that takes it, unless it's on a slot already, such as for a new find. The slot it's on, or -1. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	int32 AssignToFreeSlot(FInventoryItemHandle Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void ClearSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void ClearAllSlots();

	/** Trades two slots' items, when each slot takes the other's item, such as when the player rearranges a shortcut bar. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	bool SwapSlots(int32 SlotA, int32 SlotB);

	/**
	 * Reports the slot as activated, even an empty one, such as when its key is pressed. What it does is up to On Shortcut Activated, or
	 * a C++ subclass.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void ActivateSlot(int32 SlotIndex);

	/** Activate Slot for the slot with this tag, such as the input tag of the key pressed. Nothing when no slot has it. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void ActivateSlotWithTag(FGameplayTag SlotTag);

	/**
	 * Binds each slot's input action, from Get Slot Input Action, so it activates the slot on Trigger Event, once as it's pressed by default,
	 * such as from the pawn's Setup Player Input Component. Slots without one are left out. Bind once per input component.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void BindSlotInputs(UEnhancedInputComponent* InputComponent, ETriggerEvent TriggerEvent = ETriggerEvent::Started);

	/** The input action that activates the slot: its definition's Input Action by default. Shortcut widgets show the key mapped to it. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Inventory|Shortcuts")
	const UInputAction* GetSlotInputAction(int32 SlotIndex) const;

	// Selection: one slot can be the selected one, such as a hotbar's, which a key activates whatever it holds.
	/** Selects the slot, or none with -1. On Shortcut Selected fires when that changes the selection. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void SelectSlot(int32 SlotIndex);

	/**
	 * Moves the selection Step slots on, such as 1 for the mouse wheel down and -1 for up, wrapping around the ends. With Skip Empty, empty
	 * slots are passed over, unless every slot is empty. With nothing selected, it starts from before the first slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void CycleSelectedSlot(int32 Step = 1, bool bSkipEmpty = false);

	/** The selected slot, or -1. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	int32 GetSelectedSlot() const { return SelectedSlot; }

	/** Activates the selected slot, such as with a hotbar's Use key. Nothing while no slot is selected. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcuts")
	void ActivateSelectedSlot();

	/** The slot's item; unset while it's empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	FInventoryItemHandle GetSlotItem(int32 SlotIndex) const;

	/** Every slot's item, in slot order, unset for empty slots. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	TArray<FInventoryItemHandle> GetSlotItems() const;

	/** The slot the item is on, or -1. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	int32 FindSlotOfItem(FInventoryItemHandle Item) const;

	/** The slot with this tag, or -1. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	int32 FindSlotWithTag(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	int32 GetNumSlots() const { return Rules.Slots.Num(); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	FGameplayTag GetSlotTag(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	FText GetSlotDisplayName(int32 SlotIndex) const;

	/** Whether the owner has the item and the slot takes it, such as to refuse a drop. Slot -1 asks whether any slot does. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcuts")
	bool CanAssign(FInventoryItemHandle Item, int32 SlotIndex = -1) const;

	/** A slot's item changed: by hand, or as the inventory changed. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Shortcuts")
	FInventoryShortcutsChangedSignature OnShortcutsChanged;

	/** Activate Slot was called, with the slot's item, unset for an empty slot. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Shortcuts")
	FInventoryShortcutActivatedSignature OnShortcutActivated;

	/** Another slot was selected, or none with -1, with its item, unset for an empty slot. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Shortcuts")
	FInventoryShortcutSelectedSignature OnShortcutSelected;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	/** Before On Shortcut Activated, for C++ subclasses that decide what activating a slot does. */
	virtual void NativeOnShortcutActivated(int32 SlotIndex, FInventoryItemHandle Item) {}

	/**
	 * Whether the slot takes the item, on top of Filter and the slot's own, such as only what the owner can hold or use. True by default.
	 * Asked of items being put on slots by hand, by Auto Assign New Items and by Follow Same Kind alike.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory|Shortcuts")
	bool AcceptsItem(int32 SlotIndex, FInventoryItemHandle Item) const;

	/** The owner's Inventory Component, found as play begins. */
	UInventoryComponent* GetInventory() const { return Inventory; }

	/** The slots, what they take, and how they follow the inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Shortcuts", meta = (ShowOnlyInnerProperties))
	FInventoryShortcutRules Rules;

private:
	UFUNCTION()
	void HandleInventoryChanged(UInventoryComponent* ChangedInventory);

	/** Follows the inventory, and tells listeners when a slot changed. */
	void Refresh();

	const FInventoryRegistry* GetRegistry() const;

	/** The owner's root containers: everything inside them counts as held. */
	TArray<FInventoryContainerHandle> GetRoots() const;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;

	FInventoryShortcutSlots Slots;
	int32 SelectedSlot = INDEX_NONE;
};
