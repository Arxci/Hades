#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"

#include "GameplayTagContainer.h"

#include "HadesInputConfig.generated.h"

class UInputAction;


/** How an ability input drives its ability. */
UENUM(BlueprintType)
enum class EHadesAbilityInputMode : uint8
{
	/** Press activates. Release is ignored (interact, reload). */
	Press   UMETA(DisplayName = "Press"),
	/** Press activates, release deactivates (run, crouch). The player can switch these to toggle. */
	Hold    UMETA(DisplayName = "Hold"),
	/** Each press flips the ability between on and off. */
	Toggle  UMETA(DisplayName = "Toggle"),
};


/** An Input Action bound to a C++ handler by its input tag (move, look, jump). */
USTRUCT(BlueprintType)
struct FHadesInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;
};


/** An Input Action that drives an ability on the pawn's ability system. Needs no handler code. */
USTRUCT(BlueprintType)
struct FHadesAbilityInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	/** The ability this input drives, matched against the ability's AbilityTags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EHadesAbilityInputMode InputMode = EHadesAbilityInputMode::Hold;
};


/**
 * Maps a pawn's Input Actions to input tags. Native actions are bound to C++ handlers by tag;
 * ability actions route to the ability system with no handler code. Keys live in the Input
 * Mapping Context, never here, so gameplay code only ever deals in Input.* tags.
 */
UCLASS(BlueprintType, Const)
class HADES_API UHadesInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;
	const FHadesAbilityInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	/** Actions the pawn binds to its own handlers (Input.Move, Input.Look, Input.Jump). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FHadesInputAction> NativeInputActions;

	/** Actions that activate abilities (Input.Run, Input.Crouch). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FHadesAbilityInputAction> AbilityInputActions;
};