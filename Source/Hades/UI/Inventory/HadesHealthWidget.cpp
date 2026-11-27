#include "HadesHealthWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Hades/AbilitySystem/Attributes/HadesHealthSet.h"

#include "Widgets/ModularSegmentedMeter.h"

#define LOCTEXT_NAMESPACE "HadesHealth"

UHadesHealthWidget::UHadesHealthWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Bone white while steady, ochre once hurt, arterial red near death; the heart speeds up with each.
	FHadesHealthStatus Stable;
	Stable.MinFraction = 0.6f;
	Stable.Label = LOCTEXT("Stable", "Stable");
	Stable.Color = FLinearColor(0.80f, 0.76f, 0.66f);
	Stable.BeatsPerMinute = 62.0f;

	FHadesHealthStatus Wounded;
	Wounded.MinFraction = 0.3f;
	Wounded.Label = LOCTEXT("Wounded", "Wounded");
	Wounded.Color = FLinearColor(0.78f, 0.45f, 0.14f);
	Wounded.BeatsPerMinute = 94.0f;

	FHadesHealthStatus Critical;
	Critical.MinFraction = 0.0f;
	Critical.Label = LOCTEXT("Critical", "Critical");
	Critical.Color = FLinearColor(0.70f, 0.07f, 0.05f);
	Critical.BeatsPerMinute = 128.0f;

	Statuses = { Stable, Wounded, Critical };
	ValueFormat = LOCTEXT("ValueFormat", "{Percent}%");
}

void UHadesHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// A HUD is often made before the pawn exists, so health is found again as the pawn changes.
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	}
	BindAbilitySystem(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerPawn()));
}

void UHadesHealthWidget::NativeDestruct()
{
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}
	UnbindAbilitySystem();

	Super::NativeDestruct();
}

void UHadesHealthWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindAbilitySystem(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewPawn));
}

void UHadesHealthWidget::BindAbilitySystem(UAbilitySystemComponent* NewAbilitySystem)
{
	UnbindAbilitySystem();

	if (NewAbilitySystem && NewAbilitySystem->HasAttributeSetForAttribute(UHadesHealthSet::GetHealthAttribute()))
	{
		AbilitySystem = NewAbilitySystem;
		HealthChangedHandle = NewAbilitySystem->GetGameplayAttributeValueChangeDelegate(UHadesHealthSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleAttributeChanged);
		MaxHealthChangedHandle = NewAbilitySystem->GetGameplayAttributeValueChangeDelegate(UHadesHealthSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleAttributeChanged);
	}

	// A new pawn's health shows as it is, without a trail from the last one's.
	Refresh(false);
}

void UHadesHealthWidget::UnbindAbilitySystem()
{
	if (UAbilitySystemComponent* Previous = AbilitySystem.Get())
	{
		Previous->GetGameplayAttributeValueChangeDelegate(UHadesHealthSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		Previous->GetGameplayAttributeValueChangeDelegate(UHadesHealthSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	}
	AbilitySystem.Reset();
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
}

void UHadesHealthWidget::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	Refresh(true);
}

float UHadesHealthWidget::GetHealthFraction() const
{
	return MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

int32 UHadesHealthWidget::FindStatus_Implementation(float HealthFraction) const
{
	int32 Found = INDEX_NONE;
	for (int32 Index = 0; Index < Statuses.Num(); ++Index)
	{
		if (HealthFraction >= Statuses[Index].MinFraction && (Found == INDEX_NONE || Statuses[Index].MinFraction > Statuses[Found].MinFraction))
		{
			Found = Index;
		}
	}
	return Found;
}

void UHadesHealthWidget::Refresh(bool bAnimate)
{
	const UAbilitySystemComponent* Current = AbilitySystem.Get();
	const float OldHealth = Health;
	const float OldMaxHealth = MaxHealth;
	Health = Current ? Current->GetNumericAttribute(UHadesHealthSet::GetHealthAttribute()) : 0.0f;
	MaxHealth = Current ? Current->GetNumericAttribute(UHadesHealthSet::GetMaxHealthAttribute()) : 0.0f;

	const float Fraction = GetHealthFraction();
	const int32 NewStatusIndex = Current ? FindStatus(Fraction) : INDEX_NONE;
	const FHadesHealthStatus* Status = Statuses.IsValidIndex(NewStatusIndex) ? &Statuses[NewStatusIndex] : nullptr;

	if (Meter)
	{
		if (bAnimate && bShownOnce)
		{
			Meter->SetPercent(Fraction);
		}
		else
		{
			Meter->SetPercentImmediately(Fraction);
		}
		if (bTintMeter && Status)
		{
			Meter->SetFillColor(Status->Color);
		}
	}
	if (StatusText)
	{
		StatusText->SetText(Status ? Status->Label : FText::GetEmpty());
		if (Status)
		{
			StatusText->SetColorAndOpacity(FSlateColor(Status->Color));
		}
	}
	for (UImage* Image : { Pulse.Get(), PulseCore.Get() })
	{
		if (Image && Status)
		{
			Image->SetColorAndOpacity(Status->Color);
		}
	}
	if (ValueText)
	{
		// Rounded up, so a sliver of health never reads as none.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Health"), FMath::CeilToInt32(Health));
		Args.Add(TEXT("MaxHealth"), FMath::CeilToInt32(MaxHealth));
		Args.Add(TEXT("Percent"), FMath::CeilToInt32(Fraction * 100.0f - UE_KINDA_SMALL_NUMBER));
		ValueText->SetText(Current ? FText::Format(ValueFormat, Args) : FText::GetEmpty());
	}

	const bool bFirstShow = !bShownOnce;
	bShownOnce = true;
	if (bFirstShow || Health != OldHealth || MaxHealth != OldMaxHealth)
	{
		OnHealthChanged(Health, MaxHealth, bFirstShow ? Health : OldHealth);
	}
	if (bFirstShow || NewStatusIndex != StatusIndex)
	{
		StatusIndex = NewStatusIndex;
		OnStatusChanged(NewStatusIndex, Status ? *Status : FHadesHealthStatus());
	}
}

float UHadesHealthWidget::GetBeatIntensity(float Phase)
{
	auto Thump = [](float Distance, float Width) { return FMath::Exp(-FMath::Square(Distance / Width)); };

	// The strong beat at the start of the cycle, measured both ways round so it rises just before the cycle ends, then a weaker one after it.
	const float FromFirst = FMath::Min(Phase, 1.0f - Phase);
	const float FromSecond = FMath::Abs(Phase - 0.3f);
	return FMath::Max(Thump(FromFirst, 0.07f), 0.55f * Thump(FromSecond, 0.06f));
}

void UHadesHealthWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Pulse)
	{
		return;
	}

	const FHadesHealthStatus* Status = Statuses.IsValidIndex(StatusIndex) ? &Statuses[StatusIndex] : nullptr;
	const float BeatsPerMinute = Status ? Status->BeatsPerMinute : 0.0f;
	float Intensity = 0.0f;
	if (BeatsPerMinute > 0.0f)
	{
		// Slate's time, which runs while the game is paused, so the pulse keeps beating behind a paused game's menus.
		BeatPhase += InDeltaTime * BeatsPerMinute / 60.0f;
		if (BeatPhase >= 1.0f)
		{
			BeatPhase = FMath::Fractional(BeatPhase);
			OnHeartbeat(BeatsPerMinute);
		}
		Intensity = GetBeatIntensity(BeatPhase);
	}

	Pulse->SetRenderOpacity(FMath::Lerp(PulseRestOpacity, 1.0f, Intensity));
	Pulse->SetRenderScale(FVector2D(FMath::Lerp(1.0f, PulseBeatScale, Intensity)));
}

#undef LOCTEXT_NAMESPACE
