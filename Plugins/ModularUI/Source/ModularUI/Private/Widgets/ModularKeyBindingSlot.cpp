#include "Widgets/ModularKeyBindingSlot.h"

#include "CommonInputBaseTypes.h"
#include "Components/Image.h"

#include "Widgets/ModularKeyBindingList.h"

void UModularKeyBindingSlot::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnClicked().AddUObject(this, &ThisClass::HandleClicked);
}

void UModularKeyBindingSlot::SetRow(UModularKeyBindingRow& InRow)
{
	OwningRow = &InRow;
}

void UModularKeyBindingSlot::ShowKey(const FKey& Key, ECommonInputType InputType, FName GamepadName)
{
	ShownKey = Key;
	ShownInputType = InputType;
	ShownGamepadName = GamepadName;
	RefreshShown();
}

void UModularKeyBindingSlot::SetWaitingForKey(bool bInWaitingForKey)
{
	if (bWaitingForKey != bInWaitingForKey)
	{
		bWaitingForKey = bInWaitingForKey;
		RefreshShown();
	}
}

void UModularKeyBindingSlot::HandleClicked()
{
	UModularKeyBindingRow* Row = OwningRow.Get();
	UModularKeyBindingList* List = Row ? Row->GetList() : nullptr;
	if (List && !bWaitingForKey)
	{
		List->WaitForKey(*this);
	}
}

void UModularKeyBindingSlot::RefreshShown()
{
	FSlateBrush KeyBrush;
	const bool bHasIcon = !bWaitingForKey && ShownKey.IsValid() && KeyIcon
		&& UCommonInputPlatformSettings::Get()->TryGetInputBrush(KeyBrush, ShownKey, ShownInputType, ShownGamepadName);
	if (KeyIcon)
	{
		if (bHasIcon)
		{
			KeyIcon->SetBrush(KeyBrush);
		}
		KeyIcon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	FText ShownText;
	if (bWaitingForKey)
	{
		ShownText = WaitingText;
	}
	else if (!ShownKey.IsValid())
	{
		ShownText = EmptyText;
	}
	else if (!bHasIcon || bShowNameWithIcon)
	{
		ShownText = ShownKey.GetDisplayName();
	}
	SetLabelText(ShownText);

	OnKeyShown(ShownKey, bWaitingForKey);
}
