#include "InventoryUITypes.h"

#include "InventorySubsystem.h"

void FInventoryRegistryListener::Bind(const UObject* WorldContextObject, FOnInventoryRegistryChanged::FDelegate&& Delegate)
{
	Unbind();
	if (UInventorySubsystem* Found = UInventorySubsystem::Get(WorldContextObject))
	{
		Subsystem = Found;
		Handle = Found->GetRegistry().OnChanged.Add(MoveTemp(Delegate));
	}
}

void FInventoryRegistryListener::Unbind()
{
	if (UInventorySubsystem* Found = Subsystem.Get())
	{
		Found->GetRegistry().OnChanged.Remove(Handle);
	}
	Subsystem.Reset();
	Handle.Reset();
}
