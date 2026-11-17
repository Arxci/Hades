#pragma once

#include "CoreMinimal.h"

#include "Widgets/InventoryItemMenuWidget.h"

#include "HadesItemMenu.generated.h"


/**
 * The Hades inventory screen's item menu. Its Actions start as every Hades item action, each on the button placed for it under its Button
 * Name, such as UseButton, or else on one made in Action Panel. Edit, reorder or remove them in Class Defaults.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesItemMenu : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

public:
	UHadesItemMenu(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif
};
