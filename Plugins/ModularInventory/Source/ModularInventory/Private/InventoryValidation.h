#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

#include "Misc/DataValidation.h"
#include "StructUtils/InstancedStruct.h"

namespace InventoryValidation
{
	/** Fragments are looked up by type, so each type may appear only once in a list. */
	inline EDataValidationResult ValidateFragments(const TArray<FInstancedStruct>& Fragments, const FText& ListName, FDataValidationContext& Context)
	{
		EDataValidationResult Result = EDataValidationResult::Valid;
		TSet<const UScriptStruct*> SeenTypes;

		for (int32 Index = 0; Index < Fragments.Num(); ++Index)
		{
			const UScriptStruct* Type = Fragments[Index].GetScriptStruct();
			if (!Type)
			{
				Context.AddError(FText::Format(NSLOCTEXT("Inventory", "EmptyFragment", "{0} [{1}] has no struct type."), ListName, FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
				continue;
			}

			bool bAlreadySeen = false;
			SeenTypes.Add(Type, &bAlreadySeen);
			if (bAlreadySeen)
			{
				Context.AddError(FText::Format(NSLOCTEXT("Inventory", "DuplicateFragment", "{0} has more than one {1}."), ListName, Type->GetDisplayNameText()));
				Result = EDataValidationResult::Invalid;
			}
		}

		return Result;
	}
}

#endif // WITH_EDITOR
