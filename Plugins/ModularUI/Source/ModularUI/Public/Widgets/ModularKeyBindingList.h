#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "CommonInputTypeEnum.h"
#include "InputCoreTypes.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "ModularKeyBindingList.generated.h"

class FModularKeyCapture;
class UCommonButtonBase;
class UEnhancedInputLocalPlayerSubsystem;
class UModularKeyBindingList;
class UModularKeyBindingSlot;
class UPanelWidget;
class UTextBlock;


/** Which kind of device a key binding list shows the keys of. */
UENUM(BlueprintType)
enum class EModularKeyBindingDevice : uint8
{
	/** The keyboard's keys and the mouse's buttons. */
	KeyboardAndMouse,
	/** A gamepad's buttons. */
	Gamepad
};


/** What a key binding list does with a key the player picks that another action already has. */
UENUM(BlueprintType)
enum class EModularKeyConflict : uint8
{
	/** Puts it on this action too, so both have it. On Key Conflict says which actions share it, such as to warn the player. */
	Allow,
	/**
	 * Puts it on this action, and the other action takes the key this one had, so neither loses a key. Where this one had none, both keep
	 * the key, as with Allow.
	 */
	Swap,
	/** Leaves the keys as they are, so the player picks another. On Key Conflict says which action has it. */
	Refuse
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularKeysChangedSignature, FName, MappingName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FModularKeyConflictSignature, FText, ActionName, FKey, Key, const TArray<FText>&, OtherActions, bool, bBound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularKeyWaitingSignature, bool, bWaiting);


/** A heading over a key binding list's actions of one category, such as Movement or Menus: the Display Category they share. */
UCLASS(Abstract)
class MODULARUI_API UModularKeyBindingHeader : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCategory(const FText& InCategory);

	UFUNCTION(BlueprintPure, Category = "Key Binding")
	FText GetCategory() const { return ShownCategory; }

protected:
	/** The category heads another set of actions, such as to style a category of its own. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Key Binding")
	void OnCategoryChanged(const FText& NewCategory);

	/** The category's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Key Binding", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CategoryText;

private:
	FText ShownCategory;
};


/**
 * One action in a key binding list: its Display Name in Display Name Text, and its keys for the list's device, each on a Key Binding Slot
 * placed in the row's designer, the first key on the slot with Key Index 0 and the second on the one with 1. Players change the keys the
 * game mapped for the device, in the Input Mapping Context, and don't add more: a slot past them is hidden, keeping its place. Reset Button
 * puts this action's keys for the device back as the game set them, and shows only while one differs.
 */
UCLASS(Abstract)
class MODULARUI_API UModularKeyBindingRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The name Enhanced Input keeps the action's keys under: the Name in its Player Mappable Key Settings. */
	UFUNCTION(BlueprintPure, Category = "Key Binding")
	FName GetMappingName() const { return ActionMappingName; }

	/** The action's name for players: the Display Name in its Player Mappable Key Settings. */
	UFUNCTION(BlueprintPure, Category = "Key Binding")
	FText GetActionDisplayName() const { return ActionDisplayName; }

	/** Whether one of the action's keys for the list's device differs from the one the game set. */
	UFUNCTION(BlueprintPure, Category = "Key Binding")
	bool AreKeysCustomized() const { return bKeysCustomized; }

	/** Puts the action's keys for the list's device back as the game set them. */
	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void ResetToDefault();

	/** The list it's in. */
	UModularKeyBindingList* GetList() const { return OwningList.Get(); }

	/** The slot showing the action's first key, for gamepad focus to start on. */
	UModularKeyBindingSlot* FindFirstSlot() const;

	// Called by the list.
	void SetMapping(UModularKeyBindingList& InList, FName InMappingName);
	void Refresh();

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	/** The row shows the action as it is now, such as once a key changed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Key Binding")
	void OnRowRefreshed(const FText& ShownActionName, bool bNowCustomized);

	/** The action's Display Name. */
	UPROPERTY(BlueprintReadOnly, Category = "Key Binding", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DisplayNameText;

	/** Puts the action's keys back as the game set them. Hidden, not collapsed, while they're as the game set them, so nothing moves. */
	UPROPERTY(BlueprintReadOnly, Category = "Key Binding", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ResetButton;

private:
	void HandleResetClicked();

	/** The slots placed in the designer, found once. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UModularKeyBindingSlot>> KeySlots;

	TWeakObjectPtr<UModularKeyBindingList> OwningList;
	FName ActionMappingName;
	FText ActionDisplayName;
	bool bKeysCustomized = false;
};


/**
 * The keys a player can rebind, for one kind of device, as a list: a Row Class row for each action, under a Header Class heading for each
 * Display Category. It lists the actions Enhanced Input's user settings know: those with Player Mappable Key Settings, mapped in an Input
 * Mapping Context registered with the user settings, for the player's active key profile.
 * Pressing a row's key slot waits for the player to press the key to use instead. Meanwhile nothing else sees the player's keys, such as
 * Back, and one of Cancel Keys stops the wait. A key that another action has already is put on both, swapped, or refused, as Key Conflict
 * says. Keys change at once, everywhere: action bars, key widgets and holds use them, and with Save On Change they're kept for next time.
 * Put it in a Scroll Box for long lists, on a screen whose Get Desired Focus Target returns Get First Slot, for gamepads.
 */
UCLASS(Abstract)
class MODULARUI_API UModularKeyBindingList : public UUserWidget
{
	GENERATED_BODY()

public:
	UModularKeyBindingList(const FObjectInitializer& ObjectInitializer);

	/** Lists the actions again, as Enhanced Input's user settings have them now. It does this itself as keys change. */
	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void Refresh();

	/** Puts the keys of every action listed back as the game set them, for this list's device. */
	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void ResetAllToDefault();

	/** Saves the player's keys, so they're used next time too. Save On Change does it after each change. */
	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void SaveKeys();

	/** Whether a slot is waiting for the player to press a key. */
	UFUNCTION(BlueprintPure, Category = "Key Binding")
	bool IsWaitingForKey() const;

	/** Stops waiting for a key, changing nothing, such as as the screen closes. */
	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void StopWaiting();

	/** The slot of the first action's first key, for gamepad focus to start on. Null while nothing is listed. */
	UFUNCTION(BlueprintPure, Category = "Key Binding")
	UModularKeyBindingSlot* GetFirstSlot() const;

	UFUNCTION(BlueprintPure, Category = "Key Binding")
	EModularKeyBindingDevice GetDevice() const { return Device; }

	/** The keys of an action changed, after the player picked one or reset them. */
	UPROPERTY(BlueprintAssignable, Category = "Key Binding")
	FModularKeysChangedSignature OnKeysChanged;

	/**
	 * The player picked a key other actions listed have too, as Key Conflict counts them: Action Name, the key, the other actions' names,
	 * and whether the key was put on the action anyway, as Allow and Swap do, or refused.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Key Binding")
	FModularKeyConflictSignature OnKeyConflict;

	/** A slot started or stopped waiting for a key, such as to show a "Press a key, or Esc to cancel" line. */
	UPROPERTY(BlueprintAssignable, Category = "Key Binding")
	FModularKeyWaitingSignature OnWaitingChanged;

	// Called by rows and slots.
	/** Waits for the player to press the key for the slot. */
	void WaitForKey(UModularKeyBindingSlot& KeySlot);

	/** The action's mappings for this list's device, in the order Enhanced Input keeps them. */
	TArray<FPlayerKeyMapping> GetDeviceMappings(FName MappingName) const;

	/** Puts the action's keys for this list's device back as the game set them. */
	void ResetMapping(FName MappingName);

	/** The input type of this list's device, for key icons. */
	ECommonInputType GetInputType() const;

	/** The gamepad in use, for gamepad key icons. */
	FName GetGamepadName() const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** The device whose keys it lists. Make one list for each, such as on a Keyboard tab and a Gamepad tab. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	EModularKeyBindingDevice Device = EModularKeyBindingDevice::KeyboardAndMouse;

	/** Made for each action listed. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	TSubclassOf<UModularKeyBindingRow> RowClass;

	/** Made over each Display Category's actions. None lists every action without headings. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	TSubclassOf<UModularKeyBindingHeader> HeaderClass;

	/** Keys that stop the wait for a key, changing nothing, so players can't put them on actions. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	TArray<FKey> CancelKeys;

	/** Keys players can't put on actions, such as ones the game keeps for itself. Pressed while waiting, they're ignored. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	TArray<FKey> ReservedKeys;

	/** Mouse buttons can be put on actions, on a Keyboard And Mouse list. Off, pressing one while waiting is ignored. */
	UPROPERTY(EditAnywhere, Category = "Key Binding", meta = (EditCondition = "Device == EModularKeyBindingDevice::KeyboardAndMouse"))
	bool bAllowMouseButtons = true;

	/** What happens when the key the player picks is on another action already. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	EModularKeyConflict KeyConflict = EModularKeyConflict::Allow;

	/**
	 * Only actions in the same Display Category count as having a key already, so a key can be both a menu action's and a gameplay action's,
	 * such as R for Rotate and Reload. Off, every action counts.
	 */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	bool bConflictsWithinCategory = true;

	/** Saves the player's keys after each change, so they're used next time too. Off, call Save Keys yourself, such as as the screen closes. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	bool bSaveOnChange = true;

#if WITH_EDITORONLY_DATA
	/** Rows drawn in the designer, to see the list laid out, after one heading when there's a Header Class. */
	UPROPERTY(EditAnywhere, Category = "Key Binding", meta = (ClampMin = "0", ClampMax = "12"))
	int32 NumDesignerPreviewRows = 4;
#endif

	/** Holds the headings and rows, such as a Vertical Box inside a Scroll Box. Leave it empty: they're made for it. */
	UPROPERTY(BlueprintReadOnly, Category = "Key Binding", meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowPanel;

private:
	UFUNCTION()
	void HandleControlMappingsRebuilt();

	void HandleInputMethodChanged(ECommonInputType InputType);

	/** A key pressed while waiting. True once the wait is over. */
	bool HandleCapturedKey(const FKey& Key);

	/** Puts the key on the slot's action, as Key Conflict says. */
	void BindKey(UModularKeyBindingSlot& KeySlot, const FKey& NewKey);

	/** The other actions with the key, as Conflicts Within Category counts them. */
	TArray<FName> FindActionsWithKey(const UEnhancedPlayerMappableKeyProfile& Profile, FName MappingName, const FKey& Key) const;

	/** Puts the action's customized keys for this list's device back as the game set them. True when one changed. */
	bool ResetMappingKeys(FName MappingName);

	/** The keys changed: Enhanced Input uses them now, they're saved with Save On Change, and the rows show them. */
	void ApplyKeys();

	/** The capture stops taking presses now, and goes from Slate once the keys it took are let go. */
	void RetireCapture();

	/** Whether a mapping is for this list's device, by the key the game set, so it stays listed while the player has it on no key. */
	bool IsDeviceMapping(const FPlayerKeyMapping& Mapping) const;

	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInput() const;
	UEnhancedInputUserSettings* GetUserSettings() const;
	UEnhancedPlayerMappableKeyProfile* GetKeyProfile() const;

#if WITH_EDITOR
	void RefreshDesignerPreview();
#endif

	/** The headings and rows made, in order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> Entries;

	/** What Entries list, in order, such as "Header:Menus" and "Row:Rotate", to tell whether a refresh needs them made again. */
	TArray<FString> EntryLayout;

	TSharedPtr<FModularKeyCapture> Capture;
	TWeakObjectPtr<UModularKeyBindingSlot> WaitingSlot;
};
