#pragma once

#include "CoreMinimal.h"

#include "Hades/AbilitySystem/HadesAbility.h"

#include "HadesAbility_Crouch.generated.h"

UCLASS()
class HADES_API UHadesAbility_Crouch : public UHadesAbility
{
	GENERATED_BODY()

protected:
	UHadesAbility_Crouch();

protected:
	virtual bool RequestActivateAbility() override;
	virtual bool RequestDeactivateAbility(bool bForce) override;
	virtual void ForceEndAbility() override;
	virtual void CommitAbility() override;
	virtual void CommitAbilityEnd() override;
	
	virtual void TickAbility(float DeltaTime) override;
	virtual bool CanTick() override;
	virtual bool IsActivationRequested() const override;
	
private:
	bool CanCharacterCrouch() const;
	bool CanCharacterUnCrouch() const;
};
	