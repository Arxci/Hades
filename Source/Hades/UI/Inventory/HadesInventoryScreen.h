#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Foundation/HadesActivatableWidget.h"

#include "HadesInventoryScreen.generated.h"

class UInventoryGridWidget;
class UInventoryUISubsystem;

/**
 * The player's inventory, pushed onto the GameMenu layer by the HUD. The Rotate Item and Split Stack actions work on the held item,
 * and Back puts a held item down before it closes the screen.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesInventoryScreen : public UHadesActivatableWidget
{
	GENERATED_BODY()

public:
	UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	/** Its first cell takes gamepad focus when the screen opens, unless the Blueprint implements Get Desired Focus Target. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryGridWidget> MainGrid;

private:
	UInventoryUISubsystem* GetInteraction() const;
	void HandleRotateAction();
	void HandleSplitAction();
	void HandleCloseAction();
};
