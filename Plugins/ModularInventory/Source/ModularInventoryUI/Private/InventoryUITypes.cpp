#include "InventoryUITypes.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"

bool FInventoryItemPreview::FindTagValue(const FGameplayTag& Tag, float& OutValue) const
{
	const FInventoryTagValue* Found = TagValues.FindByPredicate([&Tag](const FInventoryTagValue& TagValue) { return TagValue.Tag == Tag; });
	if (!Found && Definition)
	{
		Found = Definition->FindTagValue(Tag);
	}
	OutValue = Found ? Found->Value : 0.0f;
	return Found != nullptr;
}

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
