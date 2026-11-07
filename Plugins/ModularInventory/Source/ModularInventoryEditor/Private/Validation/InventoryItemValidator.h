#pragma once

#include "CoreMinimal.h"

#include "EditorValidatorBase.h"

#include "InventoryItemValidator.generated.h"


/** The item checks that need the rest of the project: unique Asset Manager IDs, and an icon widgets can show unstretched. */
UCLASS()
class UInventoryItemValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	//~ Begin UEditorValidatorBase Interface
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
	//~ End UEditorValidatorBase Interface
};
