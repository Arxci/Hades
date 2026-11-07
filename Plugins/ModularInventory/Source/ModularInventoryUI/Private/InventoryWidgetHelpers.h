#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"

#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"

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

	/** Puts the definition's icon in Image, loading it first if needed. Hidden while loading, collapsed when there is none. */
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
			Image->SetBrushResourceObject(Loaded);
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
			return;
		}

		Image->SetVisibility(ESlateVisibility::Hidden);
		const TWeakObjectPtr<UImage> WeakImage = Image;
		LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(IconAsset.ToSoftObjectPath(), FStreamableDelegate::CreateLambda([WeakImage, IconAsset]()
		{
			if (UImage* LoadedInto = WeakImage.Get())
			{
				LoadedInto->SetBrushResourceObject(IconAsset.Get());
				LoadedInto->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}));
	}

	/** The container with Tag on the owning player's pawn, for widgets that bind themselves. */
	inline FInventoryContainerHandle FindPlayerContainer(const UUserWidget& Widget, const FGameplayTag& Tag)
	{
		const UInventoryComponent* Inventory = UInventoryComponent::FindInventoryComponent(Widget.GetOwningPlayerPawn());
		return Inventory && Tag.IsValid() ? Inventory->GetContainer(Tag) : FInventoryContainerHandle();
	}
}
