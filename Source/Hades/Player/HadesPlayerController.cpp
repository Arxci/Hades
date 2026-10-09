#include "HadesPlayerController.h"

#include "Hades/Player/HadesPlayerCameraManager.h"

AHadesPlayerController::AHadesPlayerController()
{
	PlayerCameraManagerClass = AHadesPlayerCameraManager::StaticClass();
}

