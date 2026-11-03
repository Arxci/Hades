#include "HadesUIUtility.h"


//Size box
void UHadesUIUtility::SetSizeBoxSize(USizeBox* Target, const FVector2D NewSize)
{
	if (!IsValid(Target))
	{
		return;
	}

	Target->SetWidthOverride(NewSize.X);
	Target->SetHeightOverride(NewSize.Y);
}


//Image
void UHadesUIUtility::SetImageBrushSize(UImage* Target, const FVector2D NewSize)
{
	if (!IsValid(Target))
	{
		return;
	}

	FSlateBrush NewBrush = Target->GetBrush();
	NewBrush.ImageSize = FVector2D(NewSize.X, NewSize.Y);
	Target->SetBrush(NewBrush);
}


//Interpolation
void UHadesUIUtility::InterpolateWidgetRenderOpacity(UWidget* Target, const float TargetOpacity, const float InterpSpeed)
{
	if (!IsValid(Target))
	{
		return;
	}

	const UWorld* World = Target->GetWorld();
	if (!World)
	{
		return;
	}

	const float DeltaTime = World->GetDeltaSeconds();
	const float CurrentOpacity = Target->GetRenderOpacity();
	const float NewOpacity = FMath::FInterpTo(CurrentOpacity, TargetOpacity, DeltaTime, InterpSpeed);
	Target->SetRenderOpacity(NewOpacity);
}