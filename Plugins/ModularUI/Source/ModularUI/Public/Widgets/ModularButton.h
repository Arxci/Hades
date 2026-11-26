#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "ModularButton.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;


/** Where a button's icon and label sit in the button, together. */
UENUM(BlueprintType)
enum class EModularButtonJustify : uint8
{
	Left,
	Center,
	Right
};


/**
 * A Common UI button with an icon and a label, such as a row of a menu or a tab. The label takes the text style the button Style has for
 * the button's state: normal, hovered, selected or disabled. While hovered, focused on a gamepad, or selected, the icon takes Lit Icon
 * Color.
 * Put Icon, then Label, in a Horizontal Box, and that Horizontal Box in an Overlay at the root; Justification moves the Horizontal Box.
 * Context menus and prompts set the label of entries and buttons made from this class.
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

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonButtonBase Interface
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual void NativeOnCurrentTextStyleChanged() override;
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

	UPROPERTY(EditAnywhere, Category = "Modular Button")
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, Category = "Modular Button")
	FText LabelText;

	UPROPERTY(EditAnywhere, Category = "Modular Button")
	FLinearColor IconColor = FLinearColor::White;

	/** The icon's tint while the button is hovered, focused on a gamepad, or selected, such as the open tab. */
	UPROPERTY(EditAnywhere, Category = "Modular Button")
	FLinearColor LitIconColor = FLinearColor::Black;

	/** Where the icon and label sit in the button, together. A label that wraps lines up its lines the same way. */
	UPROPERTY(EditAnywhere, Category = "Modular Button")
	EModularButtonJustify Justification = EModularButtonJustify::Left;

private:
	void SetHoverLit(bool bInHoverLit);
	void RefreshIconColor();
	void ApplyTextStyle();
	void ApplyJustification();

	/** The widget beside the label that holds the icon, such as a Size Box around it, or the icon itself. */
	UWidget* FindIconInRow() const;

	bool bHoverLit = false;
};
