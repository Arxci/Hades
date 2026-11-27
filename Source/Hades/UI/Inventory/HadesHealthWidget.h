#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"

#include "HadesHealthWidget.generated.h"

class APawn;
class UAbilitySystemComponent;
class UImage;
class UModularSegmentedMeter;
class UTextBlock;
struct FOnAttributeChangeData;


/** How the player's condition reads at some health, such as Wounded below 60%. */
USTRUCT(BlueprintType)
struct FHadesHealthStatus
{
	GENERATED_BODY()

	/** Shown while health is at least this part of Max Health, from 0 to 1, unless a status with a higher Min Fraction is also met. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "0", ClampMax = "1"))
	float MinFraction = 0.0f;

	/** What Status Text says, such as Stable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	FText Label;

	/** Tints Status Text, the Meter's lit segments and the Pulse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	FLinearColor Color = FLinearColor::White;

	/** How fast the Pulse beats, in beats per minute. 0 keeps it still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "0", ClampMax = "240"))
	float BeatsPerMinute = 60.0f;
};


/**
 * The player's condition, read from Hades Health Set on the owning player's pawn, such as in the inventory: a word for how hurt they are, a
 * segmented meter that leaves a trail behind what was just lost, and a pulse that beats faster and turns redder as health drops. The word,
 * the meter and the pulse all say the same thing, so it reads without telling colors apart.
 * Statuses decides the words, colors and heart rates. Every part is optional: leave out what the layout doesn't want, such as Value Text to
 * keep exact numbers hidden. The pulse beats in real time, so it keeps beating behind a paused game's menus.
 */
UCLASS(Abstract)
class HADES_API UHadesHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHadesHealthWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	/** Health as a part of Max Health, from 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthFraction() const;

	/** Which of Statuses shows; -1 while the pawn has no health. */
	UFUNCTION(BlueprintPure, Category = "Health")
	int32 GetStatusIndex() const { return StatusIndex; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	/** Health or Max Health changed, including as the widget first shows, when Old Health is Health. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthChanged(float NewHealth, float NewMaxHealth, float OldHealth);

	/** Another of Statuses shows, such as to play a sting as the player becomes Critical. Index is -1 while the pawn has no health. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnStatusChanged(int32 NewStatusIndex, const FHadesHealthStatus& Status);

	/** The pulse beat, such as for a faint heartbeat sound while Critical. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHeartbeat(float BeatsPerMinute);

	/** Which status shows at a part of Max Health: the one with the highest Min Fraction it meets. */
	UFUNCTION(BlueprintNativeEvent, Category = "Health")
	int32 FindStatus(float HealthFraction) const;

	/** The words, colors and heart rates for each level of health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (TitleProperty = "{Label} from {MinFraction}"))
	TArray<FHadesHealthStatus> Statuses;

	/** What Value Text says. {Health} and {MaxHealth} are the numbers, rounded up, and {Percent} health out of 100. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	FText ValueFormat;

	/** Tints the Meter's lit segments with the status's color. Off, they keep the color the Meter has. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	bool bTintMeter = true;

	/** How much of the Pulse shows between beats, from 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health|Pulse", meta = (ClampMin = "0", ClampMax = "1"))
	float PulseRestOpacity = 0.2f;

	/** How big the Pulse swells on a beat, as a multiple of its size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health|Pulse", meta = (ClampMin = "1"))
	float PulseBeatScale = 1.25f;

	/** The status's Label, in its color. */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** Health out of Max Health, such as a row of segments. */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (BindWidgetOptional))
	TObjectPtr<UModularSegmentedMeter> Meter;

	/** Swells and brightens with each heartbeat, tinted with the status's color, such as a soft glow. Centered, so it swells from its middle. */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Pulse;

	/** Tinted with the status's color and still, such as a dot in front of the Pulse. */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (BindWidgetOptional))
	TObjectPtr<UImage> PulseCore;

	/** The numbers, as Value Format says. Leave it out to keep exact health hidden. */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

private:
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleAttributeChanged(const FOnAttributeChangeData& Data);

	void BindAbilitySystem(UAbilitySystemComponent* NewAbilitySystem);
	void UnbindAbilitySystem();

	/** Shows health as it is now. Animate leaves a trail on the meter behind what was lost. */
	void Refresh(bool bAnimate);

	/** A heartbeat's brightness at a point of its cycle, from 0 to 1: a strong beat, then a weaker one just after it. */
	static float GetBeatIntensity(float Phase);

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;

	float Health = 0.0f;
	float MaxHealth = 0.0f;
	int32 StatusIndex = INDEX_NONE;
	bool bShownOnce = false;

	/** Where the pulse is in its cycle, from 0 to 1. */
	float BeatPhase = 0.0f;
};
