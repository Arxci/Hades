#include "Widgets/ModularKeyCapture.h"

#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

namespace ModularUI
{
	EModularKeyCaptureResult ClassifyCapturedKey(const FKey& Key, bool bGamepad, const TArray<FKey>& CancelKeys, const TArray<FKey>& ReservedKeys,
		bool bAllowMouseButtons)
	{
		if (!Key.IsValid())
		{
			return EModularKeyCaptureResult::Ignore;
		}
		if (CancelKeys.Contains(Key))
		{
			return EModularKeyCaptureResult::Cancel;
		}
		if (ReservedKeys.Contains(Key) || Key.IsTouch() || Key.IsGamepadKey() != bGamepad || (Key.IsMouseButton() && !bAllowMouseButtons))
		{
			return EModularKeyCaptureResult::Ignore;
		}
		return EModularKeyCaptureResult::Bind;
	}
}

FModularKeyCapture::FModularKeyCapture(ULocalPlayer& InPlayer, TFunction<bool(const FKey&)>&& InOnPress)
	: Player(&InPlayer)
	, OnPress(MoveTemp(InOnPress))
{
}

void FModularKeyCapture::Stop()
{
	bCapturing = false;
}

void FModularKeyCapture::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	// Keys let go while the game is in the background never say so.
	if (!SlateApp.IsActive())
	{
		TakenKeys.Reset();
	}
}

bool FModularKeyCapture::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// A key held down repeats: only its first press is offered.
	return TakePress(InKeyEvent.GetKey(), InKeyEvent.GetUserIndex(), /*bOffer*/ !InKeyEvent.IsRepeat());
}

bool FModularKeyCapture::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	return TakeRelease(InKeyEvent.GetKey(), InKeyEvent.GetUserIndex());
}

bool FModularKeyCapture::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return TakePress(MouseEvent.GetEffectingButton(), MouseEvent.GetUserIndex(), /*bOffer*/ true);
}

bool FModularKeyCapture::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return TakeRelease(MouseEvent.GetEffectingButton(), MouseEvent.GetUserIndex());
}

bool FModularKeyCapture::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// Such as the second click on the button that started the wait: taken, but never the key to use.
	return TakePress(MouseEvent.GetEffectingButton(), MouseEvent.GetUserIndex(), /*bOffer*/ false);
}

bool FModularKeyCapture::IsPlayersInput(uint32 UserIndex) const
{
	ULocalPlayer* LocalPlayer = Player.Get();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	return SlateUser && static_cast<uint32>(SlateUser->GetUserIndex()) == UserIndex;
}

bool FModularKeyCapture::TakePress(const FKey& Key, uint32 UserIndex, bool bOffer)
{
	if (!bCapturing || !IsPlayersInput(UserIndex))
	{
		return false;
	}

	TakenKeys.Add(Key);
	if (bOffer && OnPress && OnPress(Key))
	{
		bCapturing = false;
	}
	return true;
}

bool FModularKeyCapture::TakeRelease(const FKey& Key, uint32 UserIndex)
{
	// A release goes where its press went.
	return IsPlayersInput(UserIndex) && TakenKeys.Remove(Key) > 0;
}
