#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "HadesMenuButton.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;


/** Where a menu button's icon and label sit in the button, together. */
UENUM(BlueprintType)
enum class EHadesMenuButtonJustify : uint8
{
	Left,
	Center,
	Right
};


/**
 * A menu row with an icon and a label, such as in the item menu. The label takes the text style the button Style has for the button's
 * state: normal, hovered, selected or disabled. While hovered, or focused on a gamepad, the icon's tint changes too.
 * Put Icon, then Label, in a Horizontal Box, and that Horizontal Box in an Overlay at the root; Justification moves the Horizontal Box.
 */
UCLASS(Abstract)
class HADES_API UHadesMenuButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Menu Button")
	void SetLabelText(const FText& InText);

	/** None hides the icon. */
	UFUNCTION(BlueprintCallable, Category = "Menu Button")
	void SetIconTexture(UTexture2D* InTexture);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonButtonBase Interface
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnCurrentTextStyleChanged() override;
	//~ End UCommonButtonBase Interface

	/**
	 * Shows Icon Texture at the Image Size set on it in the designer, or the size of a Size Box around it, tinted Icon Color, or Lit Icon
	 * Color while the row is highlighted. Without a texture it collapses, with whatever holds it beside the label.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Menu Button", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	/** Shows Label Text in the button Style's text style for the button's state. Make it a Common Text, which text styles need. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu Button", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	FText LabelText;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	FLinearColor IconColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	FLinearColor LitIconColor = FLinearColor::Black;

	/** Where the icon and label sit in the button, together. A label that wraps lines up its lines the same way. */
	UPROPERTY(EditAnywhere, Category = "Menu Button")
	EHadesMenuButtonJustify Justification = EHadesMenuButtonJustify::Left;

private:
	void SetLit(bool bInLit);
	void ApplyTextStyle();
	void ApplyJustification();

	/** The widget beside the label that holds the icon, such as a Size Box around it, or the icon itself. */
	UWidget* FindIconInRow() const;
};
