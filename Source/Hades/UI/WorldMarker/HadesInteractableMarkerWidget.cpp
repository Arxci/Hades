#include "HadesInteractableMarkerWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Interaction/HadesInteractableComponent.h"

UHadesInteractableComponent* UHadesInteractableMarkerWidget::GetInteractable() const
{
	return Cast<UHadesInteractableComponent>(GetContextObject());
}

void UHadesInteractableMarkerWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation == FocusAnimation)
	{
		ShowIndicatorOrPrompt();
	}
	else if (Animation == CompleteAnimation)
	{
		bRingHeldFull = false;
		SetRingProgress(GetProgress());
	}
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

void UHadesInteractableMarkerWidget::NativeOnEventReceived(const FGameplayTag& EventTag)
{
	if (CompleteAnimation && EventTag == HadesGameplayTags::Interactable_Event_Completed)
	{
		PlayAnimation(CompleteAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, /*bRestoreState*/ true);
		bRingHeldFull = true;
	}

	Super::NativeOnEventReceived(EventTag);
}

bool UHadesInteractableMarkerWidget::IsTransitionPlaying() const
{
	return FocusAnimation && IsAnimationPlaying(FocusAnimation);
}

void UHadesInteractableMarkerWidget::SetFocused(bool bFocused)
{
	SetForceClamp(bFocused && bClampWhileFocused);

	if (FocusAnimation && bPromptShown != bFocused)
	{
		bPromptShown = bFocused;

		Indicator->SetVisibility(ESlateVisibility::HitTestInvisible);
		Prompt->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (bFocused)
		{
			PlayAnimationForward(FocusAnimation);
		}
		else
		{
			PlayAnimationReverse(FocusAnimation);
		}
	}
	else if (!FocusAnimation || !IsAnimationPlaying(FocusAnimation))
	{
		bPromptShown = bFocused;
		ShowIndicatorOrPrompt();
	}

	if (FocusLoopAnimation)
	{
		if (!bFocused)
		{
			StopAnimation(FocusLoopAnimation);
		}
		else if (!IsAnimationPlaying(FocusLoopAnimation))
		{
			PlayAnimation(FocusLoopAnimation, 0.0f, /*NumLoopsToPlay*/ 0, EUMGSequencePlayMode::Forward, 1.0f, /*bRestoreState*/ true);
		}
	}
}

void UHadesInteractableMarkerWidget::ShowIndicatorOrPrompt()
{
	Indicator->SetVisibility(bPromptShown ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Prompt->SetVisibility(bPromptShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
		const bool bShowRing = Interactable && Interactable->RequiresHold() && !bDisabled;
		ProgressRing->SetVisibility(bShowRing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHadesInteractableMarkerWidget::SetRingProgress(float NewProgress)
{
	// The ring stays full while the completion plays, unless a new interaction has started.
	if (UMaterialInstanceDynamic* Material = ProgressRing ? ProgressRing->GetDynamicMaterial() : nullptr)
	{
		Material->SetScalarParameterValue(TEXT("Progress"), bRingHeldFull && NewProgress <= 0.0f ? 1.0f : NewProgress);
	}
}
