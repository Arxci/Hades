//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "EMSData.h"
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/SaveGame.h"
#include "EMSInfoSaveGame.generated.h"

UCLASS()
class EASYMULTISAVE_API UEMSInfoSaveGame : public USaveGame
{
	GENERATED_BODY()

protected:

	inline virtual UWorld* GetWorld() const override
	{
		if (HasAnyFlags(RF_ClassDefaultObject))
		{
			return nullptr;
		}

		if (UObject* Outer = GetOuter())
		{
			return Outer->GetWorld();
		}

		return nullptr;
	}
	
public:

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Slots")
	FSaveSlotInfo SlotInfo;

	//Prevent too many slot info writes in a row
	UPROPERTY(Transient)
	double LastSaveAttemptTime = 0.0;

public:

	inline void UpdateMultiLevelSave(const FName& LevelName, TArray<FName>& OutLevels)
	{
		SlotInfo.Levels.AddUnique(LevelName);
		OutLevels = SlotInfo.Levels;
	}

	inline void KeepMultiLevelSave(const TArray<FName>& InLevels)
	{
		if (!InLevels.IsEmpty() && SlotInfo.Levels.IsEmpty())
		{
			SlotInfo.Levels = InLevels;
		}
	}

	inline bool HasLevel(const FName& InLevel) const
	{
		return SlotInfo.Level == InLevel || SlotInfo.Levels.Contains(InLevel);
	}
};
