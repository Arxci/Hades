#include "HadesPlayerController.h"

#include "Hades/Hades.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Camera/HadesPlayerCameraManager.h"
#include "Hades/UI/WorldMarker/HadesWorldMarkerManagerComponent.h"

#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"

#include "EnhancedInputSubsystems.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"

#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "InventoryComponent.h"
#include "ModularUIPauseSubsystem.h"

AHadesPlayerController::AHadesPlayerController()
{
	PlayerCameraManagerClass = AHadesPlayerCameraManager::StaticClass();

	WorldMarkerManager = CreateDefaultSubobject<UHadesWorldMarkerManagerComponent>(TEXT("WorldMarkerManager"));
	ItemBoxInventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("ItemBoxInventory"));
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

void AHadesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Modular UI pauses the world for menus, and fades in the look set in Project Settings > Plugins > Modular UI > Paused Look. The HUD
	// hides meanwhile.
	UModularUIPauseSubsystem* Pause = IsLocalController() ? UModularUIPauseSubsystem::Get(this) : nullptr;
	if (Pause)
	{
		Pause->OnPauseChanged.AddUniqueDynamic(this, &ThisClass::HandlePauseChanged);
		HandlePauseChanged(Pause->IsPauseRequested());
	}

	// Again here, as Enhanced Input may only make its user settings once the player is set up, which can be after input was.
	if (IsLocalController() && bRegisterMappingsWithSettings && !RegisterMappingContextsWithSettings())
	{
		UE_LOG(LogHadesInput, Warning, TEXT("[Input] %s: Enhanced Input has no user settings, so the keys in Default Mapping Contexts can't be rebound. Turn on Enable User Settings in Project Settings > Engine > Enhanced Input."),
			*GetNameSafe(this));
	}
}

void AHadesPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UModularUIPauseSubsystem* Pause = UModularUIPauseSubsystem::Get(this))
	{
		Pause->OnPauseChanged.RemoveDynamic(this, &ThisClass::HandlePauseChanged);
	}

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
		// Registered before they're added, so the keys the player rebound are the ones added, when the settings exist yet.
		// Begin Play tries again if they don't.
		RegisterMappingContextsWithSettings();
		for (UInputMappingContext* MappingContext : DefaultMappingContexts)
		{
			if (MappingContext)
			{
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}
}

bool AHadesPlayerController::RegisterMappingContextsWithSettings()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	UEnhancedInputUserSettings* Settings = bRegisterMappingsWithSettings && Subsystem ? Subsystem->GetUserSettings() : nullptr;
	if (!Settings)
	{
		return false;
	}

	// Registering a context twice does nothing. One registered after it was added only uses the player's keys once the mappings rebuild.
	bool bRegisteredAny = false;
	for (UInputMappingContext* MappingContext : DefaultMappingContexts)
	{
		if (MappingContext && Settings->RegisterInputMappingContext(MappingContext))
		{
			bRegisteredAny = true;
			UE_LOG(LogHadesInput, Log, TEXT("[Input] Registered %s with Enhanced Input's user settings. Its player mappable keys can be rebound."), *GetNameSafe(MappingContext));
		}
	}
	if (bRegisteredAny)
	{
		Subsystem->RequestRebuildControlMappings();
	}
	return true;
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

void AHadesPlayerController::HandlePauseChanged(bool bPaused)
{
	if (UCommonActivatableWidget* Layout = HUDLayout.Get())
	{
		Layout->SetVisibility(bPaused ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}