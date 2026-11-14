#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"

#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"
#include "InventoryUITypes.h"

namespace InventoryWidgets
{
	inline void CancelLoad(TSharedPtr<FStreamableHandle>& LoadHandle)
	{
		if (LoadHandle.IsValid() && LoadHandle->IsLoadingInProgress())
		{
			LoadHandle->CancelHandle();
		}
		LoadHandle.Reset();
	}

	/**
	 * Puts the definition's icon in Image, loading it first if needed. Hidden while loading, collapsed when there is none.
	 * The image takes the texture's size, so icons keep their shape; a Scale Box fits them to a slot.
	 */
	inline void ShowIcon(UImage* Image, const UInventoryItemDefinition* Definition, TSharedPtr<FStreamableHandle>& LoadHandle)
	{
		CancelLoad(LoadHandle);
		if (!Image)
		{
			return;
		}

		const UInventoryItemCosmetics* Cosmetics = Definition ? Definition->Cosmetics.Get() : nullptr;
		const TSoftObjectPtr<UTexture2D> IconAsset = Cosmetics ? Cosmetics->Icon : TSoftObjectPtr<UTexture2D>();
		if (IconAsset.IsNull())
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		if (UTexture2D* Loaded = IconAsset.Get())
		{
			Image->SetBrushFromTexture(Loaded, /*bMatchSize*/ true);
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
			return;
		}

		Image->SetVisibility(ESlateVisibility::Hidden);
		const TWeakObjectPtr<UImage> WeakImage = Image;
		LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(IconAsset.ToSoftObjectPath(), FStreamableDelegate::CreateLambda([WeakImage, IconAsset]()
		{
			if (UImage* LoadedInto = WeakImage.Get())
			{
				LoadedInto->SetBrushFromTexture(IconAsset.Get(), /*bMatchSize*/ true);
				LoadedInto->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}));
	}

	/** The container with Tag on the pawn's inventory component, for widgets that bind themselves. */
	inline FInventoryContainerHandle FindPawnContainer(const APawn* Pawn, const FGameplayTag& Tag)
	{
		const UInventoryComponent* Inventory = UInventoryComponent::FindInventoryComponent(Pawn);
		return Inventory && Tag.IsValid() ? Inventory->GetContainer(Tag) : FInventoryContainerHandle();
	}

	inline FInventoryContainerHandle FindPlayerContainer(const UUserWidget& Widget, const FGameplayTag& Tag)
	{
		return FindPawnContainer(Widget.GetOwningPlayerPawn(), Tag);
	}

	/** The color of the first entry whose tag is among the item's tags, or null. */
	inline const FLinearColor* FindTagColor(const TArray<FInventoryTagColor>& TagColors, const FGameplayTagContainer& ItemTags)
	{
		const FInventoryTagColor* Found = TagColors.FindByPredicate([&ItemTags](const FInventoryTagColor& Entry) { return ItemTags.HasTag(Entry.Tag); });
		return Found ? &Found->Color : nullptr;
	}
}
