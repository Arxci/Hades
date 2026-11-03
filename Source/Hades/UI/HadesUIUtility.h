#pragma once

#include "CoreMinimal.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "HadesUIUtility.generated.h"

UCLASS()
class HADES_API UHadesUIUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//~ Start Size Box
	UFUNCTION(BlueprintCallable, Category = "UI|SizeBox", meta = (WorldContext = "WorldContextObject"))
	static void SetSizeBoxSize(USizeBox* Target, const FVector2D NewSize);
	//~ Stop Size Box
	
	//~ Start Image
	UFUNCTION(BlueprintCallable, Category = "UI|Image", meta = (WorldContext = "WorldContextObject"))
	static void SetImageBrushSize(UImage* Target, const FVector2D NewSize);
	//~ Stop Image
	
	//~ Start Interpolation
	UFUNCTION(BlueprintCallable, Category = "UI|Interpolation", meta = (WorldContext = "WorldContextObject"))
	static void InterpolateWidgetRenderOpacity(UWidget* Target, const float TargetOpacity, const float InterpSpeed);
	//~ Stop Interpolation
};