#include "HadesHeroCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"

#include "InputActionValue.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "Hades/Hades.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/AbilitySystem/HadesAbilitySystemComponent.h"
#include "Hades/Input/HadesInputComponent.h"
#include "Hades/Input/HadesInputConfig.h"

AHadesHeroCharacter::AHadesHeroCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 90.0f);
	
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(FName("NoCollision"));
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	ViewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewRoot"));
	ViewRoot->SetupAttachment(GetCapsuleComponent());
	ViewRoot->SetRelativeLocation(FVector(0.0f, 0.0f, BaseEyeHeight));

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(ViewRoot);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->bEnableFirstPersonFieldOfView = true;
	FirstPersonCamera->bEnableFirstPersonScale = true;
	FirstPersonCamera->FirstPersonFieldOfView = 70.0f;
	FirstPersonCamera->FirstPersonScale = 0.6f;

	ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(FirstPersonCamera);
	ArmsMesh->SetOnlyOwnerSee(true);
	ArmsMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	ArmsMesh->SetCollisionProfileName(FName("NoCollision"));
	ArmsMesh->SetCastShadow(false);
	ArmsMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void AHadesHeroCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	if (ArmsMesh->PrimaryComponentTick.bCanEverTick && GetCharacterMovement())
	{
		ArmsMesh->PrimaryComponentTick.AddPrerequisite(GetCharacterMovement(), GetCharacterMovement()->PrimaryComponentTick);
	}
}

void AHadesHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	SetEyeHeight(BaseEyeHeight);
}

void AHadesHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	FMath::CriticallyDampedSmoothing(EyeHeight, EyeHeightRate, BaseEyeHeight, 0.0f, DeltaSeconds, EyeHeightSmoothingTime);
	SetEyeHeight(EyeHeight);
}

void AHadesHeroCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	
	if (GetCharacterMovement()->bCrouchMaintainsBaseLocation)
	{
		SetEyeHeight(EyeHeight + ScaledHalfHeightAdjust);
	}
}

void AHadesHeroCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	
	if (GetCharacterMovement()->bCrouchMaintainsBaseLocation)
	{
		SetEyeHeight(EyeHeight - ScaledHalfHeightAdjust);
	}
}

void AHadesHeroCharacter::SetEyeHeight(float NewEyeHeight)
{
	EyeHeight = NewEyeHeight;
	ViewRoot->SetRelativeLocation(FVector(0.0f, 0.0f, EyeHeight));
}

UInputComponent* AHadesHeroCharacter::CreatePlayerInputComponent()
{
	static const FName InputComponentName(TEXT("PawnInputComponent0"));
	return NewObject<UHadesInputComponent>(this, InputComponentName);
}

void AHadesHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!InputConfig)
	{
		UE_LOG(LogHadesInput, Error, TEXT("[Input] %s has no InputConfig; player input is unbound."), *GetNameSafe(this));
		return;
	}

	UHadesInputComponent* HadesInput = CastChecked<UHadesInputComponent>(PlayerInputComponent);

	HadesInput->BindActionByTag(InputConfig, HadesGameplayTags::Input_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	HadesInput->BindActionByTag(InputConfig, HadesGameplayTags::Input_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	HadesInput->BindActionByTag(InputConfig, HadesGameplayTags::Input_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
	HadesInput->BindActionByTag(InputConfig, HadesGameplayTags::Input_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

	if (HadesAbilitySystemComponent)
	{
		HadesInput->BindPressedAndReleased(InputConfig, HadesAbilitySystemComponent.Get(),
			&UHadesAbilitySystemComponent::AbilityInputTagPressed, &UHadesAbilitySystemComponent::AbilityInputTagReleased);
	}
	HadesInput->BindInputValues(InputConfig);
}

FInputActionValue AHadesHeroCharacter::GetInputValue(FGameplayTag InputTag) const
{
	const UHadesInputComponent* HadesInput = Cast<UHadesInputComponent>(InputComponent);
	return HadesInput ? HadesInput->GetInputValue(InputTag) : FInputActionValue();
}

void AHadesHeroCharacter::UnPossessed()
{
	if (UHadesInputComponent* HadesInput = Cast<UHadesInputComponent>(InputComponent))
	{
		HadesInput->ReleaseHeldInputs();
	}

	Super::UnPossessed();
}

void AHadesHeroCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Input.Y);
	AddMovementInput(GetActorRightVector(), Input.X);
} 

void AHadesHeroCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}
