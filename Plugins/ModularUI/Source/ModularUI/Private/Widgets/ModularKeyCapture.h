#pragma once

#include "CoreMinimal.h"

#include "Framework/Application/IInputProcessor.h"
#include "InputCoreTypes.h"

class ULocalPlayer;


/** What a key pressed while a key binding list waits for one does. */
enum class EModularKeyCaptureResult : uint8
{
	/** It's the key to use. */
	Bind,
	/** It stops the wait without changing anything. */
	Cancel,
	/** It can't be used here, so the wait goes on. */
	Ignore
};


namespace ModularUI
{
	/**
	 * What a key pressed while waiting does: one of Cancel Keys stops the wait; a key of the other kind of device, one of Reserved Keys, a
	 * touch, or a mouse button while they aren't allowed, is ignored; anything else is the key to use.
	 */
	EModularKeyCaptureResult ClassifyCapturedKey(const FKey& Key, bool bGamepad, const TArray<FKey>& CancelKeys, const TArray<FKey>& ReservedKeys,
		bool bAllowMouseButtons);
}


/**
 * Takes one local player's key and mouse button presses while a key binding list waits for a key, so nothing else sees them, such as Back
 * or a button under the cursor, and hands each press to On Press, which says whether the wait is over. A key let go goes where its press
 * went: kept from everything if this took the press, even after the wait, and passed on otherwise, such as for a key held since before it.
 * Another player's presses, and moving the cursor or a stick, go where they would anyway.
 */
class FModularKeyCapture final : public IInputProcessor
{
public:
	/** On Press gets each key pressed while it waits, and returns true once the wait is over. */
	FModularKeyCapture(ULocalPlayer& InPlayer, TFunction<bool(const FKey&)>&& InOnPress);

	/** Takes no more presses. Keys it took are still kept from everything until they're let go. */
	void Stop();

	/** Whether it has stopped, and every key it took has been let go, so it can go. */
	bool IsDone() const { return !bCapturing && TakenKeys.IsEmpty(); }

	//~ Begin IInputProcessor Interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("ModularKeyCapture"); }
	//~ End IInputProcessor Interface

private:
	bool IsPlayersInput(uint32 UserIndex) const;

	/** Takes a press while it waits, giving it to On Press unless bOffer is false, such as for the second click of a double-click. */
	bool TakePress(const FKey& Key, uint32 UserIndex, bool bOffer);
	bool TakeRelease(const FKey& Key, uint32 UserIndex);

	TWeakObjectPtr<ULocalPlayer> Player;
	TFunction<bool(const FKey&)> OnPress;
	bool bCapturing = true;

	/** The keys whose presses it took, which are kept from everything until they're let go. */
	TSet<FKey> TakenKeys;
};
