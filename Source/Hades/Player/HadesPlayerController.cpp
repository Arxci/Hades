#include "HadesPlayerController.h"

#include "Hades/Hades.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Camera/HadesPlayerCameraManager.h"
#include "Hades/UI/WorldMarker/HadesWorldMarkerManagerComponent.h"

#include "Components/PostProcessComponent.h"

#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"

#include "EnhancedInputSubsystems.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"

#include "InputMappingContext.h"

#include "InventoryComponent.h"

AHadesPlayerController::AHadesPlayerController()
{
	PlayerCameraManagerClass = AHadesPlayerCameraManager::StaticClass();

	WorldMarkerManager = CreateDefaultSubobject<UHadesWorldMarkerManagerComponent>(TEXT("WorldMarkerManager"));
	ItemBoxInventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("ItemBoxInventory"));

	PausedPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PausedPostProcess"));
	PausedPostProcess->SetupAttachment(RootComponent);
	PausedPostProcess->bUnbound = true;
	PausedPostProcess->bEnabled = false;
	PausedPostProcess->Priority = 1000.0f;
	PausedPostProcess->Settings.bOverride_ColorSaturation = true;
	PausedPostProcess->Settings.ColorSaturation = FVector4(0.0, 0.0, 0.0, 1.0);
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

void AHadesPlayerController::ShowMessage(const FText& Message)
{
	OnMessage.Broadcast(Message);
}

void AHadesPlayerController::ShowMessageTo(const AActor* PlayerPawn, const FText& Message)
{
	const APawn* AsPawn = Cast<APawn>(PlayerPawn);
	if (AHadesPlayerController* Controller = AsPawn ? AsPawn->GetController<AHadesPlayerController>() : nullptr)
	{
		Controller->ShowMessage(Message);
	}
}

void AHadesPlayerController::AddPauseRequest(const UObject* Requester)
{
	PauseRequests.AddUnique(Requester);
	UpdatePause();
}

void AHadesPlayerController::RemovePauseRequest(const UObject* Requester)
{
	if (PauseRequests.Remove(Requester) > 0)
	{
		UpdatePause();
	}
}

void AHadesPlayerController::UpdatePause()
{
	PauseRequests.RemoveAll([](const TWeakObjectPtr<const UObject>& Requester) { return !Requester.IsValid(); });

	const bool bWantsPause = !PauseRequests.IsEmpty();
	if (bWantsPause != IsPaused())
	{
		SetPause(bWantsPause);
	}

	// Menus that stop the world cover the screen, so the HUD would only show through them.
	if (UCommonActivatableWidget* Layout = HUDLayout.Get())
	{
		Layout->SetVisibility(bWantsPause ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	PausedPostProcess->bEnabled = bWantsPause;
}