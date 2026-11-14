#include "Data/InventoryItemDefinition.h"

#include "Data/InventoryStackRules.h"

const FPrimaryAssetType UInventoryItemDefinition::AssetType(FName(TEXT("InventoryItem")));

FPrimaryAssetId UInventoryItemDefinition::GetPrimaryAssetId() const
{
	// Class defaults aren't items.
	return HasAnyFlags(RF_ClassDefaultObject) ? FPrimaryAssetId() : FPrimaryAssetId(AssetType, GetFName());
}

int32 UInventoryItemDefinition::GetMaxStackSize() const
{
	if (!StackRules || !Containers.IsEmpty())
	{
		return 1;
	}
	return FMath::Max(1, StackRules->MaxStackSize);
}

bool UInventoryItemDefinition::CanSplit() const
{
	return StackRules && StackRules->bCanSplit && GetMaxStackSize() > 1;
}

const FInventoryTagValue* UInventoryItemDefinition::FindTagValue(const FGameplayTag& Tag) const
{
	return TagValues.FindByPredicate([&Tag](const FInventoryTagValue& TagValue) { return TagValue.Tag == Tag; });
}

const FInstancedStruct* UInventoryItemDefinition::FindFragment(const UScriptStruct* Type) const
{
	return Fragments.FindByPredicate([Type](const FInstancedStruct& Fragment) { return Fragment.GetScriptStruct() == Type; });
}

#if WITH_EDITOR
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"

#include "Data/InventoryContainerLayout.h"
#include "InventoryValidation.h"

#define LOCTEXT_NAMESPACE "InventoryItemDefinition"

EDataValidationResult UInventoryItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (Size.X < 1 || Size.Y < 1)
	{
		Context.AddError(LOCTEXT("InvalidSize", "Size must be at least 1x1."));
		Result = EDataValidationResult::Invalid;
	}

	FPrimaryAssetTypeInfo TypeInfo;
	const UAssetManager* AssetManager = GEngine ? GEngine->AssetManager.Get() : nullptr;
	if (AssetManager && !AssetManager->GetPrimaryAssetTypeInfo(AssetType, TypeInfo))
	{
		Context.AddWarning(LOCTEXT("NotRegistered", "Items aren't registered with the Asset Manager, so they can't be found by ID or given with Inventory.Give, and items that nothing references won't be cooked. Use Tools > Modular Inventory > Register Items With Asset Manager."));
	}

	if (!Containers.IsEmpty() && StackRules && StackRules->MaxStackSize > 1)
	{
		Context.AddWarning(LOCTEXT("StackingContainer", "Items with containers never stack; the stack rules' size is ignored."));
	}

	TSet<FGameplayTag> ContainerTags;
	for (int32 Index = 0; Index < Containers.Num(); ++Index)
	{
		const FInventoryItemContainer& Container = Containers[Index];
		if (!Container.Layout)
		{
			Context.AddError(FText::Format(LOCTEXT("MissingLayout", "Container {0} has no layout."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}

		bool bAlreadySeen = false;
		ContainerTags.Add(Container.Tag, &bAlreadySeen);
		if (bAlreadySeen)
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateContainerTag", "Container {0} reuses tag {1}; container tags must be unique."),
				FText::AsNumber(Index), FText::FromName(Container.Tag.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int32 Index = 0; Index < Sockets.Num(); ++Index)
	{
		if (Sockets[Index].Cells.IsEmpty())
		{
			Context.AddWarning(FText::Format(LOCTEXT("EmptySocket", "Socket {0} reaches no cells and does nothing."), FText::AsNumber(Index)));
		}
	}

	const bool bHasAttachmentModifiers = !AttachmentValueModifiers.IsEmpty() || !AttachmentGrantedTags.IsEmpty();
	if (bHasAttachmentModifiers && !EquipRules)
	{
		Context.AddWarning(LOCTEXT("UnattachableModifiers", "The attachment modifiers do nothing: without equip rules, this item can't go in an attachment slot."));
	}

	Result = CombineDataValidationResults(Result, InventoryValidation::ValidateFragments(Fragments, LOCTEXT("Fragments", "Fragments"), Context));
	Result = CombineDataValidationResults(Result, InventoryValidation::ValidateFragments(InstanceFragments, LOCTEXT("InstanceFragments", "Instance Fragments"), Context));

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
