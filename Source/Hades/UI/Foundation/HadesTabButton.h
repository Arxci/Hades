#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Foundation/HadesMenuButton.h"

#include "HadesTabButton.generated.h"


/**
 * A tab of a Hades Tab List. The open tab is the selected button, so the button Style's Selected brushes and text styles show it, as for
 * any Common UI button, and its icon takes Lit Icon Color and grows. A tab with an icon shows its label only while open, unless Always
 * Show Label; a tab without one always shows it.
 */
UCLASS(Abstract)
class HADES_API UHadesTabButton : public UHadesMenuButton
{
	GENERATED_BODY()

public:
	UHadesTabButton(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonButtonBase Interface
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	//~ End UCommonButtonBase Interface

	/** Shown only on the open tab, such as a frame or an underline. */
	UPROPERTY(BlueprintReadOnly, Category = "Tab", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedFrame;

	/** How much the open tab's icon grows. */
	UPROPERTY(EditAnywhere, Category = "Tab", meta = (ClampMin = "1"))
	float SelectedIconScale = 1.2f;

	/** Shows the label while the tab isn't open too. */
	UPROPERTY(EditAnywhere, Category = "Tab")
	bool bAlwaysShowLabel = false;

private:
	void RefreshTab();
};
