#pragma once

#include "CoreMinimal.h"

#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

#include "Icons/InventoryIconRecipe.h"

class FInventoryIconRenderer;
class UInventoryItemDefinition;
class UStaticMesh;
class UTexture2D;
struct FPropertyChangedEvent;


/** Frames one item's icon with a live preview and saves it. The preview follows the item, its cosmetics and the icon settings as they change. */
class SInventoryIconStudio : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInventoryIconStudio) {}
	SLATE_END_ARGS()

	/** Opens the studio for the item in its own window. */
	static void Open(UInventoryItemDefinition& Item);

	void Construct(const FArguments& InArgs, UInventoryItemDefinition* InItem);
	virtual ~SInventoryIconStudio() override;

	//~ Begin SWidget Interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget Interface

private:
	void RenderPreview(UStaticMesh* Mesh, FIntPoint Size);
	FText GetStatusText() const;
	bool CanSave() const;

	void HandleDragged(FVector2D Delta, bool bMove);
	void HandleZoomed(float WheelDelta);
	void HandleSettingsChanged(UObject* Settings, FPropertyChangedEvent& Event);
	FReply HandleResetClicked();
	FReply HandleQuarterTurnClicked();
	FReply HandleSaveClicked();

	TWeakObjectPtr<UInventoryItemDefinition> Item;

	/** The framing being edited, shown in the details panel. */
	TStrongObjectPtr<UInventoryIconRecipe> Recipe;

	TUniquePtr<FInventoryIconRenderer> Renderer;
	TStrongObjectPtr<UTexture2D> PreviewTexture;
	FSlateBrush PreviewBrush;

	// What the preview was rendered from, to notice when it's out of date.
	TWeakObjectPtr<UStaticMesh> PreviewMesh;
	FIntPoint PreviewSize = FIntPoint::ZeroValue;
	FInventoryIconFraming PreviewFraming;
	bool bSettingsChanged = false;

	FDelegateHandle SettingsChangedHandle;
};
