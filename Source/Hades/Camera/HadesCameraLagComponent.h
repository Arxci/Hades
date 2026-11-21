#pragma once

#include "CoreMinimal.h"

#include "Components/SceneComponent.h"

#include "HadesCameraLagComponent.generated.h"

class AController;

/**
 * Eases a first-person view after the eye and the aim, so the camera has weight. Attach the camera to it; whatever is on the
 * camera, such as the arms, lags with the view.
 *
 * Unlike a spring arm's lag, moving at a steady speed doesn't trail. Starting, stopping, steps, jumps and landings push the view
 * off the eye, and it settles back over the lag times. Turning does trail, by Rotation Lag Time.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesCameraLagComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHadesCameraLagComponent();

	/** The lagged view. Attaching without a socket gives the same. */
	static const FName SocketName;

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	//~ End UActorComponent Interface

	//~ Begin USceneComponent Interface
	virtual bool HasAnySockets() const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	//~ End USceneComponent Interface

	/** Puts the view straight where it's headed, with no lag. Call after teleports and camera cuts. */
	UFUNCTION(BlueprintCallable, Category = "Camera Lag")
	void ResetLag();

	/** Turn the view to the controller's aim, rather than with this component's own rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Lag")
	bool bUsePawnControlRotation = true;

	/**
	 * How long the view runs behind the aim. It eases into and out of turns instead of snapping with the input. 0 turns rotation
	 * lag off. Around 0.03 to 0.08 reads as weight; much more starts to feel like input delay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Lag", meta = (ClampMin = "0", Units = "s"))
	float RotationLagTime = 0.05f;

	/** How long the view takes to settle when the character starts, stops or changes direction. Moving at a steady speed doesn't trail. 0 turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Lag", meta = (ClampMin = "0", Units = "s"))
	float HorizontalLagTime = 0.1f;

	/** How long the view takes to settle after steps, jumps and landings. Higher smooths stairs more and dips deeper on landing. 0 turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Lag", meta = (ClampMin = "0", Units = "s"))
	float VerticalLagTime = 0.08f;

	/** Furthest the view drifts across from the eye. On a character it also stays 10 cm inside the capsule, so it can't reach into walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Lag", meta = (ClampMin = "0", Units = "cm"))
	float MaxHorizontalLag = 15.0f;

	/** Furthest the view drops below or rises above the eye. On a character it never rises within 10 cm of the top of the capsule, so it can't poke through ceilings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Lag", meta = (ClampMin = "0", Units = "cm"))
	float MaxVerticalLag = 40.0f;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	//~ End UActorComponent Interface

private:
	void UpdateLag(float DeltaTime);
	void StepLocationLag(double DeltaTime, const FVector& UnexplainedMove, float HorizontalTime, float VerticalTime);
	void ClampLocationLag(const FVector& TargetLocation);
	void UpdateSocketTransform(ETeleportType Teleport);

	/** Where the view turns to without lag. */
	FRotator GetTargetRotation() const;

	/** How the eye moves on its own: the owner's velocity, following the floor up and down ramps. */
	FVector GetTargetVelocity() const;

	/** The lagged view relative to this component, which is what's attached to it follows. */
	FVector RelativeSocketLocation = FVector::ZeroVector;
	FQuat RelativeSocketRotation = FQuat::Identity;

	/** The view minus the eye, and its rate of change on top of the eye's own velocity. */
	FVector LocationOffset = FVector::ZeroVector;
	FVector LocationOffsetRate = FVector::ZeroVector;

	FRotator ViewRotation = FRotator::ZeroRotator;
	/** Degrees per second on each axis. */
	FRotator ViewRotationRate = FRotator::ZeroRotator;

	FVector PreviousTargetLocation = FVector::ZeroVector;
	FVector PreviousTargetVelocity = FVector::ZeroVector;
	FRotator PreviousTargetRotation = FRotator::ZeroRotator;

	TWeakObjectPtr<const AController> LastController;
};
