#pragma once

#include "CoreMinimal.h"

#include "CommonPlayerController.h"
#include "GameplayTagContainer.h"

#include "HadesPlayerController.generated.h"

class UCommonActivatableWidget;
class UInputMappingContext;
class UInventoryComponent;
class UModularWorldMarkerManagerComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHadesMessageSignature, const FText&, Message);


UCLASS()
class HADES_API AHadesPlayerController : public ACommonPlayerController
{
	GENERATED_BODY()
	
public:
	AHadesPlayerController();
	
	virtual void ReceivedPlayer() override;

	/**
	 * Tells the player something, such as "It's locked.": a message on Message Channel, which the message feeds showing that channel show,
	 * such as a banner across the screen. On Message fires too.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowMessage(const FText& Message);

	/** Show Message on the controller of a player's pawn. Does nothing for other actors. */
	static void ShowMessageTo(const AActor* PlayerPawn, const FText& Message);

	UInventoryComponent* GetItemBoxInventory() const { return ItemBoxInventory; }

	/** Show Message was called, for anything else that shows messages its own way. */
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
	TObjectPtr<UModularWorldMarkerManagerComponent> WorldMarkerManager;

	/** The Modular UI message channel Show Message sends to, such as UI.Message.Hint. */
	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (Categories = "UI.Message"))
	FGameplayTag MessageChannel;

	/** Pushed onto the Game layer for the local player. Place a World Marker Layer in it to draw world markers. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> HUDLayoutClass;

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