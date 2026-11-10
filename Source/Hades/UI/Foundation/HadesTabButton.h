#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "HadesTabButton.generated.h"

class UImage;
class UTexture2D;
class UTextBlock;


/**
 * One tab of a screen's tab bar. The open tab's icon grows, its label shows and Active Frame appears around it.
 * Without an icon the label always shows, so a tab still reads before it has its art.
 */
UCLASS(Abstract)
class HADES_API UHadesTabButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UHadesTabButton(const FObjectInitializer& ObjectInitializer);

	void SetActive(bool bInActive);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	//~ End UUserWidget Interface

	UPROPERTY(BlueprintReadOnly, Category = "Tab", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Tab", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	/** Shown only on the open tab. */
	UPROPERTY(BlueprintReadOnly, Category = "Tab", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActiveFrame;

	UPROPERTY(EditAnywhere, Category = "Tab")
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, Category = "Tab")
	FText LabelText;

	/** How much the open tab's icon grows. */
	UPROPERTY(EditAnywhere, Category = "Tab", meta = (ClampMin = "1"))
	float ActiveIconScale = 1.2f;

private:
	bool bActive = false;
};
