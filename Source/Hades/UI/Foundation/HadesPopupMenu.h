#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Foundation/HadesActivatableWidget.h"

#include "HadesPopupMenu.generated.h"

class UDragDropOperation;


DECLARE_DELEGATE_RetVal(UDragDropOperation*, FHadesPopupAnchorDragSignature);


/**
 * A small menu pushed onto the Modal layer beside something on screen, such as an item.
 * Back, or a click anywhere outside Menu Panel, closes it, except on the thing it's beside: as in Requiem, a click there does nothing.
 * Give the root a full-screen border that is hit-testable, so those clicks reach the menu and nothing behind it can be hovered while it's open.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesPopupMenu : public UHadesActivatableWidget
{
	GENERATED_BODY()

public:
	UHadesPopupMenu(const FObjectInitializer& ObjectInitializer);

	/** Opens beside InAnchor, a box in absolute screen coordinates, toward the middle of the screen so it never runs off the edge. */
	void PlaceBeside(const FBox2D& InAnchor);

	/** Dragging the thing the menu is beside closes the menu and starts the drag this returns, such as of the item. */
	FHadesPopupAnchorDragSignature OnAnchorDragged;

protected:
	//~ Begin UUserWidget Interface
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	//~ End UUserWidget Interface

	/** The menu itself. Put it in a Canvas Panel that fills the screen, with Size To Content ticked, so it can be moved beside the anchor. */
	UPROPERTY(BlueprintReadOnly, Category = "Popup Menu", meta = (BindWidget))
	TObjectPtr<UWidget> MenuPanel;

private:
	FBox2D Anchor = FBox2D(ForceInit);
};
