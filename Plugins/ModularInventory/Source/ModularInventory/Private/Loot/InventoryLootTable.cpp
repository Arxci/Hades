#include "Loot/InventoryLootTable.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "ModularInventory.h"

namespace
{
	// Tables can roll tables; this stops a table that reaches itself.
	constexpr int32 MaxTableDepth = 16;

	const FInventoryLootEntry* PickEntry(const FInventoryLootPool& Pool, FRandomStream& Stream, const FGameplayTagContainer& ContextTags)
	{
		TArray<const FInventoryLootEntry*, TInlineAllocator<16>> Candidates;
		float TotalWeight = 0.0f;
		for (const FInventoryLootEntry& Entry : Pool.Entries)
		{
			if (Entry.Weight > 0.0f && InventoryTags::PassesQuery(Entry.Condition, ContextTags))
			{
				Candidates.Add(&Entry);
				TotalWeight += Entry.Weight;
			}
		}

		if (Candidates.IsEmpty())
		{
			return nullptr;
		}

		float Pick = Stream.FRandRange(0.0f, TotalWeight);
		for (const FInventoryLootEntry* Entry : Candidates)
		{
			if (Pick < Entry->Weight)
			{
				return Entry;
			}
			Pick -= Entry->Weight;
		}
		return Candidates.Last();
	}

	int32 RollCount(FRandomStream& Stream, int32 Min, int32 Max)
	{
		return Stream.RandRange(Min, FMath::Max(Min, Max));
	}
}

float FInventoryTagValueRange::Roll(FRandomStream& Stream) const
{
	if (bWholeNumber)
	{
		return static_cast<float>(RollCount(Stream, FMath::RoundToInt(Min), FMath::RoundToInt(Max)));
	}
	return Stream.FRandRange(Min, FMath::Max(Min, Max));
}

void UInventoryLootTable::Roll(FRandomStream& Stream, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemSpec>& OutItems) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_RollLoot);
	RollTable(Stream, ContextTags, OutItems, 0);
}

void UInventoryLootTable::RollTable(FRandomStream& Stream, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemSpec>& OutItems, int32 Depth) const
{
	if (Depth >= MaxTableDepth)
	{
		UE_LOG(LogInventory, Warning, TEXT("[Loot] %s nests deeper than %d tables; a table probably rolls itself."), *GetNameSafe(this), MaxTableDepth);
		return;
	}

	RollPools(Pools, Stream, ContextTags, OutItems, Depth);
	for (const FInventoryInjectedLootPools& Injected : InjectedPools)
	{
		RollPools(Injected.Pools, Stream, ContextTags, OutItems, Depth);
	}
}

void UInventoryLootTable::RollPools(const TArray<FInventoryLootPool>& InPools, FRandomStream& Stream, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemSpec>& OutItems, int32 Depth) const
{
	for (const FInventoryLootPool& Pool : InPools)
	{
		if (!InventoryTags::PassesQuery(Pool.Condition, ContextTags))
		{
			continue;
		}

		const int32 Rolls = RollCount(Stream, Pool.MinRolls, Pool.MaxRolls);
		for (int32 RollIndex = 0; RollIndex < Rolls; ++RollIndex)
		{
			const FInventoryLootEntry* Entry = PickEntry(Pool, Stream, ContextTags);
			if (!Entry)
			{
				break;
			}

			const int32 Count = RollCount(Stream, Entry->MinCount, Entry->MaxCount);
			if (Entry->Type == EInventoryLootEntryType::Item && Entry->Item)
			{
				FInventoryItemSpec& Spec = OutItems.Emplace_GetRef(Entry->Item, Count);
				for (const FInventoryTagValueRange& Range : Entry->TagValues)
				{
					Spec.TagValues.Emplace(Range.Tag, Range.Roll(Stream));
				}
			}
			else if (Entry->Type == EInventoryLootEntryType::Table && Entry->Table)
			{
				for (int32 TableRoll = 0; TableRoll < Count; ++TableRoll)
				{
					Entry->Table->RollTable(Stream, ContextTags, OutItems, Depth + 1);
				}
			}
		}
	}
}

void UInventoryLootTable::AddInjectedPools(const UObject* Source, const TArray<FInventoryLootPool>& InPools)
{
	FInventoryInjectedLootPools& Injected = InjectedPools.AddDefaulted_GetRef();
	Injected.Source = FObjectKey(Source);
	Injected.Pools = InPools;
}

void UInventoryLootTable::RemoveInjectedPools(const UObject* Source)
{
	const FObjectKey SourceKey(Source);
	InjectedPools.RemoveAll([&SourceKey](const FInventoryInjectedLootPools& Injected) { return Injected.Source == SourceKey; });
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Misc/MessageDialog.h"

#include "Data/InventoryItemDefinition.h"

#define LOCTEXT_NAMESPACE "InventoryLootTable"

EDataValidationResult UInventoryLootTable::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (int32 PoolIndex = 0; PoolIndex < Pools.Num(); ++PoolIndex)
	{
		const FInventoryLootPool& Pool = Pools[PoolIndex];
		if (Pool.MinRolls > Pool.MaxRolls)
		{
			Context.AddWarning(FText::Format(LOCTEXT("PoolRolls", "Pool {0} has Min Rolls above Max Rolls; it always rolls Min Rolls."), FText::AsNumber(PoolIndex)));
		}

		for (int32 EntryIndex = 0; EntryIndex < Pool.Entries.Num(); ++EntryIndex)
		{
			const FInventoryLootEntry& Entry = Pool.Entries[EntryIndex];
			const FText EntryName = FText::Format(LOCTEXT("EntryName", "Pool {0} entry {1}"), FText::AsNumber(PoolIndex), FText::AsNumber(EntryIndex));

			if (Entry.Type == EInventoryLootEntryType::Item && !Entry.Item)
			{
				Context.AddError(FText::Format(LOCTEXT("MissingItem", "{0} has no item."), EntryName));
				Result = EDataValidationResult::Invalid;
			}
			else if (Entry.Type == EInventoryLootEntryType::Table && (!Entry.Table || Entry.Table == this))
			{
				Context.AddError(FText::Format(LOCTEXT("InvalidTable", "{0} needs a table other than this one."), EntryName));
				Result = EDataValidationResult::Invalid;
			}

			if (Entry.MinCount > Entry.MaxCount)
			{
				Context.AddWarning(FText::Format(LOCTEXT("EntryCount", "{0} has Min Count above Max Count; it always uses Min Count."), EntryName));
			}
		}
	}

	return Result;
}

void UInventoryLootTable::Simulate() const
{
	struct FDropStats
	{
		int32 RollsWithItem = 0;
		int64 TotalCount = 0;
	};

	TMap<const UInventoryItemDefinition*, FDropStats> DropStats;
	int32 EmptyRolls = 0;
	FRandomStream Stream(FMath::Rand());

	for (int32 RollIndex = 0; RollIndex < SimulationRolls; ++RollIndex)
	{
		TArray<FInventoryItemSpec> Rolled;
		Roll(Stream, SimulationContext, Rolled);
		EmptyRolls += Rolled.IsEmpty() ? 1 : 0;

		TSet<const UInventoryItemDefinition*> DroppedThisRoll;
		for (const FInventoryItemSpec& Spec : Rolled)
		{
			const UInventoryItemDefinition* Definition = Spec.Definition;
			FDropStats& ItemStats = DropStats.FindOrAdd(Definition);
			ItemStats.TotalCount += Spec.Count;
			if (!DroppedThisRoll.Contains(Definition))
			{
				DroppedThisRoll.Add(Definition);
				++ItemStats.RollsWithItem;
			}
		}
	}

	DropStats.ValueSort([](const FDropStats& A, const FDropStats& B) { return A.RollsWithItem > B.RollsWithItem; });

	const double Rolls = FMath::Max(1, SimulationRolls);
	FString Report = FString::Printf(TEXT("%s, %d rolls. Nothing dropped in %.1f%% of them.\n"), *GetName(), SimulationRolls, 100.0 * EmptyRolls / Rolls);
	for (const TPair<const UInventoryItemDefinition*, FDropStats>& Pair : DropStats)
	{
		Report += FString::Printf(TEXT("\n%s: in %.1f%% of rolls, %.2f per roll on average"),
			*GetNameSafe(Pair.Key), 100.0 * Pair.Value.RollsWithItem / Rolls, Pair.Value.TotalCount / Rolls);
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Report));
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
