//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "EMSTypes.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EMSAsyncWait.generated.h"

class UEMSObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAsyncWaitOutputPin);

UCLASS()
class EASYMULTISAVE_API UEMSAsyncWait : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	UEMSAsyncWait();

public:

	UPROPERTY(BlueprintAssignable)
	FAsyncWaitOutputPin OnCompleted;
	
private:

	double StartTime;
	uint32 CheckCounter;

	UPROPERTY()
	TObjectPtr<UEMSObject> EMS;

	UPROPERTY()
	TSoftObjectPtr<AActor> TargetActor;

	ESaveGameMode LoadMode;

	uint8 bLoadTaskWasActive :1;
	uint8 bTimeoutTriggered : 1;
	uint8 bCompleted : 1;
	uint8 bCachedSaveFileExists : 1;
	uint8 bCachedHasLevel : 1;

	FTimerHandle WaitTimerHandle;

public:

	/** 
	* Wait until a LoadGameActors call has been completed.
	* Does logically not catch LoadGameActors that were finished before this nodes executes.
	* Can be used to wait for a loaded reference from the ActorLoaded event. 
	* Or if you want to access loaded variables from BeginPlay.
	* 
	* @param Mode - The load data to wait for, either Player, Level or Both.
	* @param CheckActor - If set, this will only check if one particular Actor is in its loaded state. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Easy Multi Save | Actors",
		meta = (DisplayName = "Wait For Load Actors Completed", BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "CheckActor"))
	static UEMSAsyncWait* AsyncWaitForOperation(UObject* WorldContextObject, ESaveGameMode Mode = ESaveGameMode::MODE_All, TSoftObjectPtr<AActor> CheckActor = nullptr);

	void Activate() override;

private:

	void StartWaitTask();
	void CompleteWaitTask();
	void ForceProceed();

};
