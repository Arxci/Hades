#include "HadesInteractableMarkerWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Interaction/HadesInteractableComponent.h"

UHadesInteractableComponent* UHadesInteractableMarkerWidget::GetInteractable() const
{
	return Cast<UHadesInteractableComponent>(GetContextObject());
}

void UHadesInteractableMarkerWidget::NativeOnContextChanged()
{
	SetFocused(HasStateTag(HadesGameplayTags::Interactable_State_Focused));
	RefreshPrompt();
	SetRingProgress(GetProgress());

	Super::NativeOnContextChanged();
}

void UHadesInteractableMarkerWidget::NativeOnStateTagChanged(const FGameplayTag& Tag, bool bAdded)
{
	if (Tag == HadesGameplayTags::Interactable_State_Focused)
	{
		SetFocused(bAdded);
	}
	else if (Tag == HadesGameplayTags::Interactable_State_Disabled)
	{
		RefreshPrompt();
	}

	Super::NativeOnStateTagChanged(Tag, bAdded);
}

void UHadesInteractableMarkerWidget::NativeOnProgressChanged(float NewProgress)
{
	SetRingProgress(NewProgress);

	Super::NativeOnProgressChanged(NewProgress);
}

void UHadesInteractableMarkerWidget::SetFocused(bool bFocused)
{
	Indicator->SetVisibility(bFocused ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Prompt->SetVisibility(bFocused ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UHadesInteractableMarkerWidget::RefreshPrompt()
{
	const UHadesInteractableComponent* Interactable = GetInteractable();
	const bool bDisabled = HasStateTag(HadesGameplayTags::Interactable_State_Disabled);

	if (ActionText)
	{
		FText Text;
		if (Interactable)
		{
			const FText DisabledText = Interactable->GetDisabledText();
			Text = bDisabled && !DisabledText.IsEmpty() ? DisabledText : Interactable->GetInteractionText();
		}

		ActionText->SetText(Text);
		ActionText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (ProgressRing)
	{
		// Showing the empty ring while focused tells the player it's a hold before they press anything.
		const bool bShowRing = Interactable && Interactable->RequiresHold() && !bDisabled;
		ProgressRing->SetVisibility(bShowRing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHadesInteractableMarkerWidget::SetRingProgress(float NewProgress)
{
	if (UMaterialInstanceDynamic* Material = ProgressRing ? ProgressRing->GetDynamicMaterial() : nullptr)
	{
		Material->SetScalarParameterValue(TEXT("Progress"), NewProgress);
	}
}
