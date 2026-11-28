#include "Widgets/ModularBoundActionButton.h"

#include "Input/UIActionBinding.h"
#include "InputAction.h"

#include "ModularUIInput.h"

void UModularBoundActionButton::SetRepresentedAction(FUIActionBindingHandle InBindingHandle)
{
	StopListening();
	Super::SetRepresentedAction(InBindingHandle);
	ShownBinding = InBindingHandle;

	// Modular UI's holds report their progress through the binding. Common UI's button listens to every binding it shows, so this only adds
	// itself should an engine version not.
	if (const TSharedPtr<FUIActionBinding> FoundBinding = FUIActionBinding::FindBinding(ShownBinding))
	{
		if (!FoundBinding->OnHoldActionProgressed.IsBoundToObject(this))
		{
			FoundBinding->OnHoldActionProgressed.AddUObject(this, &ThisClass::NativeOnActionProgress);
		}
	}

	const float ActionHoldTime = GetActionHoldTime();
	OnRepresentedActionChanged(ActionHoldTime > 0.0f, ActionHoldTime);
}

bool UModularBoundActionButton::IsHoldAction() const
{
	return GetActionHoldTime() > 0.0f;
}

float UModularBoundActionButton::GetActionHoldTime() const
{
	const TSharedPtr<FUIActionBinding> FoundBinding = FUIActionBinding::FindBinding(ShownBinding);
	return FoundBinding ? ModularUI::GetHoldTime(FoundBinding->InputAction.Get()) : 0.0f;
}

void UModularBoundActionButton::NativeDestruct()
{
	StopListening();

	Super::NativeDestruct();
}

void UModularBoundActionButton::StopListening()
{
	// Everything this listened to on the binding goes, Common UI's button's own listening included, as it moves to another binding or goes.
	if (const TSharedPtr<FUIActionBinding> FoundBinding = FUIActionBinding::FindBinding(ShownBinding))
	{
		FoundBinding->OnHoldActionProgressed.RemoveAll(this);
	}
	ShownBinding = FUIActionBindingHandle();
}
