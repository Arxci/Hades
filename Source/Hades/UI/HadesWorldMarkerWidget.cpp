#include "HadesWorldMarkerWidget.h"

#include "Components/SceneComponent.h"

//Context
void UHadesWorldMarkerWidget::SetContext(USceneComponent* InAnchor, AActor* InContextActor)
{
	Anchor = InAnchor;
	ContextActor = InContextActor ? InContextActor : (InAnchor ? InAnchor->GetOwner() : nullptr);

	OnContextChanged();
}

//State
void UHadesWorldMarkerWidget::SetStateTag(const FGameplayTag& Tag, bool bEnabled)
{
	if (StateTags.HasTagExact(Tag) == bEnabled)
	{
		return;
	}

	if (bEnabled)
	{
		StateTags.AddTag(Tag);
	}
	else
	{
		StateTags.RemoveTag(Tag);
	}

	OnStateTagChanged(Tag, bEnabled);
}

void UHadesWorldMarkerWidget::SetProgress(float NewProgress)
{
	if (FMath::IsNearlyEqual(Progress, NewProgress))
	{
		return;
	}

	Progress = NewProgress;
	OnProgressChanged(Progress);
}
