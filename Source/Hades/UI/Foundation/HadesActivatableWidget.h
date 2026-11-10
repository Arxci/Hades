#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Engine/DataTable.h"

#include "HadesActivatableWidget.generated.h"

struct FUIInputConfig;

UENUM(BlueprintType)
enum class EHadesWidgetInputMode : uint8
{
	Default,
	GameAndMenu,
	Game,
	Menu
};

/** An activatable widget that sets the player's input mode while it is active, and can pause the game. */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	//~ Begin UCommonActivatableWidget Interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget Interface

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~ End UCommonActivatableWidget Interface

	/** Whether input reaches the game, the UI or both while this is active. Default leaves the input mode unchanged. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	EHadesWidgetInputMode InputConfig = EHadesWidgetInputMode::Default;

	/** Mouse behavior while the game gets input. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	/** The world stops while this is active. */
	UPROPERTY(EditDefaultsOnly, Category = "Pause")
	bool bPauseGame = false;

	/** Shown in an action bar for what Accept does here. Accept itself still goes to the focused button. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ConfirmAction;
};
