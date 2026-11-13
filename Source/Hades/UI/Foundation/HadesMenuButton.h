#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"
#include "Styling/SlateBrush.h"

#include "HadesMenuButton.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;


/**
 * A menu row with an icon on a round badge and a label, such as in the item menu. While hovered, or focused on a gamepad,
 * the badge and the icon's color change; the row's own highlight comes from the button Style.
 */
UCLASS(Abstract)
class HADES_API UHadesMenuButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Menu Button")
	void SetLabelText(const FText& InText);

	/** None hides the icon and its badge. */
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
	//~ End UCommonButtonBase Interface

	/** The round shape behind the icon. Gets Badge Brush, or Lit Badge Brush while the row is highlighted. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu Button", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Badge;

	/** Shows Icon Texture at the designer's size, tinted Icon Color, or Lit Icon Color while the row is highlighted. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu Button", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Menu Button", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	FText LabelText;

	/** Left without an image, the badge keeps the brush set in the designer. */
	UPROPERTY(EditAnywhere, Category = "Menu Button")
	FSlateBrush BadgeBrush;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	FSlateBrush LitBadgeBrush;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	FLinearColor IconColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Menu Button")
	FLinearColor LitIconColor = FLinearColor::Black;

private:
	void SetLit(bool bInLit);
};
