#include "HadesPlayerController.h"

#include "Hades/Player/HadesPlayerCameraManager.h"

#include "EnhancedInputSubsystems.h"

#include "Engine/LocalPlayer.h"

#include "InputMappingContext.h"

AHadesPlayerController::AHadesPlayerController()
{
	PlayerCameraManagerClass = AHadesPlayerCameraManager::StaticClass();
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