#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

#include "HadesCameraLibrary.generated.h"


UCLASS()
class HADES_API UHadesCameraLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Hades|Camera")
	static void GetCameraViewPoint(const APawn* Pawn, FVector& OutLocation, FRotator& OutRotation);
	
	UFUNCTION(BlueprintCallable, Category = "Hades|Camera", meta = (AdvancedDisplay = "bTraceComplex,SpreadHalfAngleDegrees,DrawDebugType,DrawTime"))
	static bool CameraForwardTrace(
		const APawn* Pawn,
		float Distance,
		ETraceTypeQuery TraceChannel,
		FHitResult& OutHit,
		bool bTraceComplex = false,
		float SpreadHalfAngleDegrees = 0.0f,
		EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::None,
		float DrawTime = 0.0f);
};