#pragma once

#include "CoreMinimal.h"

#include "CommonInputTypeEnum.h"
#include "InputCoreTypes.h"
#include "Widgets/ModularButton.h"

#include "ModularKeyBindingSlot.generated.h"

class UImage;
class UModularKeyBindingRow;


/**
 * One of an action's keys in a key binding list's row, as a button: the key's icon from Common Input's controller data in Key Icon, and its
 * name in Label, or Empty Text while that key is unbound. Pressed, it waits for the player to press the key to use instead, saying Waiting
 * Text meanwhile, until one of the list's Cancel Keys stops it. Place one in a row's designer for each key the row shows, each with its own
 * Key Index. It's hidden, keeping its place, while the action has no key there in the Input Mapping Context. It's a Modular Button, so its
 * Description shows in the screen's description line while it's highlighted.
 */
UCLASS(Abstract)
class MODULARUI_API UModularKeyBindingSlot : public UModularButton
{
	GENERATED_BODY()

public:
	/** Which of the action's keys for the list's device it shows: 0 the first, 1 the second. */
	UFUNCTION(BlueprintPure, Category = "Key Binding")
	int32 GetKeyIndex() const { return KeyIndex; }

	/** The key shown, or none. */
	UFUNCTION(BlueprintPure, Category = "Key Binding")
	FKey GetShownKey() const { return ShownKey; }

	/** Whether it's waiting for the player to press a key. */
	UFUNCTION(BlueprintPure, Category = "Key Binding")
	bool IsWaitingForKey() const { return bWaitingForKey; }

	/** The row it's in. */
	UModularKeyBindingRow* GetRow() const { return OwningRow.Get(); }

	// Called by the row and the list.
	void SetRow(UModularKeyBindingRow& InRow);
	void ShowKey(const FKey& Key, ECommonInputType InputType, FName GamepadName);
	void SetWaitingForKey(bool bInWaitingForKey);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	/** What it shows changed: its key, or whether it's waiting for one, such as to play an animation while it waits. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Key Binding")
	void OnKeyShown(FKey NewKey, bool bNowWaiting);

	/**
	 * Which of the action's keys for the list's device it shows: 0 the first, 1 the second, in the order Enhanced Input keeps them. Hidden
	 * while the Input Mapping Context maps fewer keys than that for the device.
	 */
	UPROPERTY(EditAnywhere, Category = "Key Binding", meta = (ClampMin = "0"))
	int32 KeyIndex = 0;

	/**
	 * Shows the key's icon, at the Image Size of the controller data's brush, when Common Input's controller data has one for the key.
	 * Collapsed otherwise, and while it waits for a key, so Label shows the key's name instead.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Key Binding", meta = (BindWidgetOptional))
	TObjectPtr<UImage> KeyIcon;

	/** Label shows the key's name beside its icon too. Off, Label is empty while Key Icon shows the key, so only the icon shows. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	bool bShowNameWithIcon = false;

	/** What Label says while the key it shows is unbound. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	FText EmptyText = NSLOCTEXT("ModularKeyBindingSlot", "EmptyText", "Not Bound");

	/** What Label says while it waits for the player to press a key. */
	UPROPERTY(EditAnywhere, Category = "Key Binding")
	FText WaitingText = NSLOCTEXT("ModularKeyBindingSlot", "WaitingText", "Press a key");

private:
	void HandleClicked();
	void RefreshShown();

	TWeakObjectPtr<UModularKeyBindingRow> OwningRow;
	FKey ShownKey;
	ECommonInputType ShownInputType = ECommonInputType::MouseAndKeyboard;
	FName ShownGamepadName;
	bool bWaitingForKey = false;
};
