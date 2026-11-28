#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "ModularButton.generated.h"

class UImage;
class UModularInputKeyWidget;
class UTextBlock;
class UTexture2D;


/** Where a button's icon and label sit in the button, together. */
UENUM(BlueprintType)
enum class EModularButtonJustify : uint8
{
	Left,
	Center,
	Right,
	/** Where the designer has them: the button leaves the row's alignment and the label's justification alone. */
	AsDesigned
};


/**
 * A Common UI button with an icon and a label, such as a row of a menu or a tab. The label takes the text style the button Style has for
 * the button's state: normal, hovered, selected or disabled. While hovered, focused on a gamepad, or selected, the icon takes Lit Icon
 * Color.
 * Put Icon, then Label, in a Horizontal Box, and that Horizontal Box in an Overlay; Justification moves the Horizontal Box within it.
 * Key Widget shows the button's own key, its Triggering Enhanced Input Action. Give it a column of its own, so it never covers the label:
 * an outer Horizontal Box holding that Overlay, filling, then Key Widget at the edge. Context menus and prompts set the label of entries
 * and buttons made from this class, and item menus their keys.
 * While the mouse is over it or focus is on it, its Description shows in the Description Text of the screen it's on, such as what an
 * option of an options menu does.
 */
UCLASS(Abstract)
class MODULARUI_API UModularButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Modular Button")
	void SetLabelText(const FText& InText);

	UFUNCTION(BlueprintPure, Category = "Modular Button")
	FText GetLabelText() const { return LabelText; }

	/** None hides the icon. */
	UFUNCTION(BlueprintCallable, Category = "Modular Button")
	void SetIconTexture(UTexture2D* InTexture);

	/** What the button does, for the Description Text of the screen it's on. Updates it at once while the button is highlighted. */
	UFUNCTION(BlueprintCallable, Category = "Modular Button")
	void SetDescription(const FText& InDescription);

	UFUNCTION(BlueprintPure, Category = "Modular Button")
	FText GetDescription() const { return Description; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonButtonBase Interface
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;
	virtual void NativeOnCurrentTextStyleChanged() override;
	virtual void UpdateInputActionWidget() override;
	//~ End UCommonButtonBase Interface

	/**
	 * Shows Icon Texture at the Image Size set on it in the designer, or the size of a Size Box around it, tinted Icon Color, or Lit Icon
	 * Color while the button is hovered or selected. Without a texture it collapses, with whatever holds it beside the label.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Modular Button", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	/** Shows Label Text in the button Style's text style for the button's state. Make it a Common Text, which text styles need. */
	UPROPERTY(BlueprintReadOnly, Category = "Modular Button", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	/**
	 * Shows the button's own key, its Triggering Enhanced Input Action, such as an item action's key on its button in an item menu, in a
	 * Modular Input Key Widget of your own, so buttons show keys as the rest of the game does. Collapsed while the button has no such action,
	 * and while it's disabled, unless Display Input Action When Not Interactable is ticked. Its own Show On says which devices it shows the
	 * key for, such as Gamepad on a menu the mouse clicks. Use it instead of Common UI's Input Action Widget, not as well.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Modular Button", meta = (BindWidgetOptional))
	TObjectPtr<UModularInputKeyWidget> KeyWidget;

	UPROPERTY(EditAnywhere, Category = "Modular Button")
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, Category = "Modular Button")
	FText LabelText;

	UPROPERTY(EditAnywhere, Category = "Modular Button")
	FLinearColor IconColor = FLinearColor::White;

	/** The icon's tint while the button is hovered, focused on a gamepad, or selected, such as the open tab. */
	UPROPERTY(EditAnywhere, Category = "Modular Button")
	FLinearColor LitIconColor = FLinearColor::Black;

	/**
	 * Where the icon and label sit in the button, together. A label that wraps lines up its lines the same way. As Designed leaves both as
	 * they're laid out in the designer.
	 */
	UPROPERTY(EditAnywhere, Category = "Modular Button")
	EModularButtonJustify Justification = EModularButtonJustify::Left;

	/**
	 * What the button does, such as "Adjust mouse and keyboard control settings.", shown in the Description Text of the screen it's on
	 * while the mouse is over it or focus is on it. Empty leaves Description Text to the rest.
	 */
	UPROPERTY(EditAnywhere, Category = "Modular Button", meta = (MultiLine = true))
	FText Description;

private:
	/** Tells the screen it's on that it's highlighted, or isn't any more, for its Description Text. */
	void SetHighlightedForDescription(bool bHighlighted);

	void SetHoverLit(bool bInHoverLit);
	void RefreshIconColor();
	void ApplyTextStyle();
	void ApplyJustification();

	/** Shows the button's own key in Key Widget while it can be pressed, or nothing. */
	void RefreshKeyWidget();

	/** The widget beside the label that holds the icon, such as a Size Box around it, or the icon itself. */
	UWidget* FindIconInRow() const;

	bool bHoverLit = false;
};
