#pragma once

#include "CoreMinimal.h"

#include "Hades/AbilitySystem/HadesAbility.h"

#include "HadesAbility_Run.generated.h"

UCLASS(Blueprintable)
class HADES_API UHadesAbility_Run : public UHadesAbility
{
	GENERATED_BODY()

protected:
	UHadesAbility_Run();

protected:
	virtual bool RequestActivateAbility() override;
	virtual bool RequestDeactivateAbility(bool bForce) override;
	virtual void ForceEndAbility() override;
	virtual void CommitAbility() override;
	virtual void CommitAbilityEnd() override;
	
	virtual void TickAbility(float DeltaTime) override;
	virtual bool CanTick() override;
	virtual bool IsActivationRequested() const override;

protected:
	float BoostThreshold = 0.2f;
	
private:
	bool CanCharacterRun() const;
	bool CanCharacterWalk() const;
};