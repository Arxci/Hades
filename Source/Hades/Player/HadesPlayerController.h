#pragma once

#include "CoreMinimal.h"

#include "CommonPlayerController.h"

#include "HadesPlayerController.generated.h"

class UCommonActivatableWidget;
class UInputMappingContext;
class UInventoryComponent;
class UHadesWorldMarkerManagerComponent;
class UPostProcessComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHadesMessageSignature, const FText&, Message);


UCLASS()
class HADES_API AHadesPlayerController : public ACommonPlayerController
{
	GENERATED_BODY()
	
public:
	AHadesPlayerController();
	
	virtual void ReceivedPlayer() override;

	/** Shown by every Hades Message widget, such as "It's locked." */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowMessage(const FText& Message);

	/** Show Message on the controller of a player's pawn. Does nothing for other actors. */
	static void ShowMessageTo(const AActor* PlayerPawn, const FText& Message);

	UInventoryComponent* GetItemBoxInventory() const { return ItemBoxInventory; }

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FHadesMessageSignature OnMessage;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	/**
	 * Registers Default Mapping Contexts with Enhanced Input's user settings, so the player can rebind their keys and the keys they chose
	 * are used. Needs Enable User Settings in Project Settings > Enhanced Input, and Player Mappable Key Settings on each input action that
	 * can be rebound.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	bool bRegisterMappingsWithSettings = true;
	
	UPROPERTY(VisibleAnywhere, Category = "UI")
	TObjectPtr<UHadesWorldMarkerManagerComponent> WorldMarkerManager;

	/** Pushed onto the Game layer for the local player. Place a World Marker Layer in it to draw world markers. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> HUDLayoutClass;

	/**
	 * Fades in while a menu pauses the world, greying it out, and back out as the world starts again. Its Blend Weight is how strong it is
	 * once fully in. Change its settings for another look. The HUD hides meanwhile.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UPostProcessComponent> PausedPostProcess;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ClampMin = "0", Units = "s"))
	float PausedFadeInTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (ClampMin = "0", Units = "s"))
	float PausedFadeOutTime = 0.2f;

	/** What every item box in the game opens, so it is one storage. Give it a container tagged Container.ItemBox. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryComponent> ItemBoxInventory;

private:
	/** Menus that pause the world cover the screen, so the HUD would only show through them. */
	UFUNCTION()
	void HandlePauseChanged(bool bPaused);

	/**
	 * Registers Default Mapping Contexts with Enhanced Input's user settings, once each. False while there are no user settings: Enable User
	 * Settings is off, or Enhanced Input hasn't made them yet.
	 */
	bool RegisterMappingContextsWithSettings();

	TWeakObjectPtr<UCommonActivatableWidget> HUDLayout;
};