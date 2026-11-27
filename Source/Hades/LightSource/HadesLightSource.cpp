#include "HadesLightSource.h"

#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"

#include "Hades/Hades.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Flashlight/HadesFlashlightComponent.h"

#include "Equipment/InventoryEquipmentComponent.h"
#include "InventorySubsystem.h"

AHadesLightSource::AHadesLightSource()
{
	// Ticks only while it's on and uses charge.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	LightOnTag = HadesGameplayTags::Item_State_LightOn;
	ChargeTag = HadesGameplayTags::Item_Stat_Charge;
}

void AHadesLightSource::BeginPlay()
{
	Super::BeginPlay();

	// Off until its item says otherwise, which it does as it's equipped, right after this. Components that start on, such as a Hades
	// Flashlight Component with Start On, go out in the same frame, before anything is drawn.
	ApplyLightOn(false);
}

void AHadesLightSource::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CommitCharge();

	Super::EndPlay(EndPlayReason);
}

UInventorySubsystem* AHadesLightSource::GetInventory() const
{
	return UInventorySubsystem::Get(this);
}

// Equipping: the item remembers whether its light was on, so it comes back as it was.
void AHadesLightSource::OnEquipped_Implementation(UInventoryEquipmentComponent* Equipment, FInventoryItemHandle InItem, FGameplayTag Slot)
{
	Item = InItem;
	EquipmentComponent = Equipment;
	PendingDrain = 0.0f;

	const UInventorySubsystem* Inventory = GetInventory();
	const bool bWasOn = Inventory && LightOnTag.IsValid() && Inventory->GetTagValue(Item, LightOnTag) > 0.5f;
	bLightOn = false;
	if (bWasOn && CanTurnOn())
	{
		SetLightOn(true);
		return;
	}

	// Off, as it was left, or as it can't be on any more, such as with its battery run down while it was put away.
	ApplyLightOn(false);
	StoreLightOn(false);
	RefreshTickEnabled();
}

void AHadesLightSource::OnUnequipped_Implementation(UInventoryEquipmentComponent* Equipment, FInventoryItemHandle InItem, FGameplayTag Slot)
{
	// What it used is kept on the item, and whether it was on stays there too, for the next time it's equipped.
	CommitCharge();
}

// On and off
bool AHadesLightSource::SetLightOn(bool bOn)
{
	if (bOn == bLightOn)
	{
		return true;
	}
	if (bOn && !CanTurnOn())
	{
		OnTurnOnFailed();
		return false;
	}

	bLightOn = bOn;
	if (!bOn)
	{
		CommitCharge();
	}
	StoreLightOn(bOn);
	ApplyLightOn(bOn);
	RefreshTickEnabled();

	UE_LOG(LogHadesFlashlight, Verbose, TEXT("[Light] %s turned %s."), *GetNameSafe(this), bOn ? TEXT("on") : TEXT("off"));
	OnLightChanged.Broadcast(this, bOn);
	return true;
}

bool AHadesLightSource::ToggleLight()
{
	return SetLightOn(!bLightOn);
}

bool AHadesLightSource::CanTurnOn_Implementation() const
{
	return !UsesCharge() || GetCharge() > 0.0f;
}

void AHadesLightSource::ApplyLightOn_Implementation(bool bOn)
{
	TInlineComponentArray<ULightComponent*> Lights(this);
	for (ULightComponent* Light : Lights)
	{
		Light->SetVisibility(bOn);
	}

	TInlineComponentArray<UHadesFlashlightComponent*> Flashlights(this);
	for (UHadesFlashlightComponent* Flashlight : Flashlights)
	{
		Flashlight->SetFlashlightOn(bOn);
	}
}

void AHadesLightSource::StoreLightOn(bool bOn) const
{
	UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory || !LightOnTag.IsValid() || !Inventory->IsValidItem(Item))
	{
		return;
	}

	// Only when it changed: every change to an item is a change widgets showing it hear about.
	const bool bStoredOn = Inventory->GetTagValue(Item, LightOnTag) > 0.5f;
	if (bStoredOn != bOn)
	{
		Inventory->SetTagValue(Item, LightOnTag, bOn ? 1.0f : 0.0f);
	}
}

AActor* AHadesLightSource::GetHolder() const
{
	AActor* Parent = GetAttachParentActor();
	return Parent ? Parent : GetOwner();
}

// Charge
bool AHadesLightSource::UsesCharge() const
{
	return ChargeTag.IsValid() && ChargePerSecond > 0.0f;
}

float AHadesLightSource::GetCharge() const
{
	const UInventorySubsystem* Inventory = GetInventory();
	if (!UsesCharge() || !Inventory || !Inventory->IsValidItem(Item))
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, Inventory->GetTagValue(Item, ChargeTag) - PendingDrain);
}

float AHadesLightSource::GetChargeFraction() const
{
	const UInventorySubsystem* Inventory = GetInventory();
	if (!UsesCharge() || !Inventory)
	{
		return 1.0f;
	}
	const float Full = Inventory->GetTagValueMax(Item, ChargeTag, FGameplayTag());
	return Full > 0.0f ? FMath::Clamp(GetCharge() / Full, 0.0f, 1.0f) : 0.0f;
}

void AHadesLightSource::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bLightOn || !UsesCharge())
	{
		RefreshTickEnabled();
		return;
	}

	PendingDrain += ChargePerSecond * DeltaSeconds;
	const bool bRanOut = GetCharge() <= 0.0f;
	if (bRanOut || PendingDrain >= ChargeStep)
	{
		CommitCharge();
	}
	if (bRanOut)
	{
		HandleChargeRanOut();
	}
}

void AHadesLightSource::CommitCharge()
{
	UInventorySubsystem* Inventory = GetInventory();
	if (PendingDrain <= 0.0f || !UsesCharge() || !Inventory || !Inventory->IsValidItem(Item))
	{
		PendingDrain = 0.0f;
		return;
	}

	const float Left = FMath::Max(0.0f, Inventory->GetTagValue(Item, ChargeTag) - PendingDrain);
	PendingDrain = 0.0f;
	Inventory->SetTagValue(Item, ChargeTag, Left);
}

void AHadesLightSource::HandleChargeRanOut()
{
	OnChargeRanOut();
	SetLightOn(false);

	// Used up, such as a flare: the item goes, and with it this light, as the item leaves the slot.
	UInventorySubsystem* Inventory = GetInventory();
	if (bUseUpWhenEmpty && Inventory && Inventory->IsValidItem(Item))
	{
		Inventory->RemoveItem(Item, 1);
	}
}

void AHadesLightSource::RefreshTickEnabled()
{
	SetActorTickEnabled(bLightOn && UsesCharge());
}
