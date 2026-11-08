#pragma once

#include "CoreMinimal.h"

#include "CommonPlayerController.h"

#include "HadesPlayerController.generated.h"

class UCommonActivatableWidget;
class UInputMappingContext;
class UInventoryComponent;
class UHadesWorldMarkerManagerComponent;


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

	/** The game stays paused while anything has asked for it, such as an open inventory. */
	void AddPauseRequest(const UObject* Requester);
	void RemovePauseRequest(const UObject* Requester);

	UInventoryComponent* GetItemBoxInventory() const { return ItemBoxInventory; }

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FHadesMessageSignature OnMessage;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;
	
	UPROPERTY(VisibleAnywhere, Category = "UI")
	TObjectPtr<UHadesWorldMarkerManagerComponent> WorldMarkerManager;

	/** Pushed onto the Game layer for the local player. Place a World Marker Layer in it to draw world markers. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> HUDLayoutClass;

	/** What every item box in the game opens, so it is one storage. Give it a container tagged Container.ItemBox. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryComponent> ItemBoxInventory;

private:
	void UpdatePause();

	TWeakObjectPtr<UCommonActivatableWidget> HUDLayout;
	TArray<TWeakObjectPtr<const UObject>> PauseRequests;
};