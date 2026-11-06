#include "HadesPlayerController.h"

#include "Hades/Hades.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Camera/HadesPlayerCameraManager.h"
#include "Hades/UI/WorldMarker/HadesWorldMarkerManagerComponent.h"

#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"

#include "EnhancedInputSubsystems.h"

#include "Engine/LocalPlayer.h"

#include "InputMappingContext.h"

AHadesPlayerController::AHadesPlayerController()
{
	PlayerCameraManagerClass = AHadesPlayerCameraManager::StaticClass();

	WorldMarkerManager = CreateDefaultSubobject<UHadesWorldMarkerManagerComponent>(TEXT("WorldMarkerManager"));
}

void AHadesPlayerController::ReceivedPlayer()
{
	// CommonGame creates the local player's root layout in here, so the HUD can only be pushed after it.
	Super::ReceivedPlayer();

	if (!IsLocalController() || HUDLayout.IsValid() || HUDLayoutClass.IsNull())
	{
		return;
	}

	if (const TSubclassOf<UCommonActivatableWidget> LayoutClass = HUDLayoutClass.LoadSynchronous())
	{
		HUDLayout = UCommonUIExtensions::PushContentToLayer_ForPlayer(GetLocalPlayer(), HadesGameplayTags::UI_Layer_Game, LayoutClass);
	}

	if (!HUDLayout.IsValid())
	{
		UE_LOG(LogHadesUI, Warning, TEXT("[UI] %s couldn't push its HUD layout. Check the game instance, local player and UI policy setup."), *GetNameSafe(this));
	}
}

void AHadesPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCommonActivatableWidget* Layout = HUDLayout.Get())
	{
		Layout->DeactivateWidget();
	}

	Super::EndPlay(EndPlayReason);
}

void AHadesPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* MappingContext : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}