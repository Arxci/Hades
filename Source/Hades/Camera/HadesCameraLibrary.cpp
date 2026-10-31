#include "HadesCameraLibrary.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

void UHadesCameraLibrary::GetCameraViewPoint(const APawn* Pawn, FVector& OutLocation, FRotator& OutRotation)
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	if (!Pawn)
	{
		return;
	}
	
	if (const AController* Controller = Pawn->GetController())
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	}
	else
	{
		Pawn->GetActorEyesViewPoint(OutLocation, OutRotation);
	}
}

bool UHadesCameraLibrary::CameraForwardTrace(
	const APawn* Pawn,
	float Distance,
	ETraceTypeQuery TraceChannel,
	FHitResult& OutHit,
	bool bTraceComplex,
	float SpreadHalfAngleDegrees,
	EDrawDebugTrace::Type DrawDebugType,
	float DrawTime)
{
	if (!Pawn)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetCameraViewPoint(Pawn, ViewLocation, ViewRotation);

	const FVector Forward = ViewRotation.Vector();
	const FVector Direction = SpreadHalfAngleDegrees > 0.0f
		? FMath::VRandCone(Forward, FMath::DegreesToRadians(SpreadHalfAngleDegrees))
		: Forward;
	
	return UKismetSystemLibrary::LineTraceSingle(
		Pawn,
		ViewLocation,
		ViewLocation + Direction * Distance,
		TraceChannel,
		bTraceComplex,
		{},
		DrawDebugType,
		OutHit,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		DrawTime);
}