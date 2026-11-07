#include "InventoryRegistry.h"

#include "Algo/AllOf.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemDefinition.h"
#include "Loot/InventoryLootTable.h"

namespace
{
	const FInstancedStruct* FindFragmentOfType(const TArray<FInstancedStruct>& Fragments, const UScriptStruct* Type)
	{
		return Fragments.FindByPredicate([Type](const FInstancedStruct& Fragment) { return Fragment.GetScriptStruct() == Type; });
	}

	void SetFragment(TArray<FInstancedStruct>& Fragments, const FInstancedStruct& Fragment)
	{
		const UScriptStruct* Type = Fragment.GetScriptStruct();
		if (FInstancedStruct* Existing = Fragments.FindByPredicate([Type](const FInstancedStruct& Candidate) { return Candidate.GetScriptStruct() == Type; }))
		{
			*Existing = Fragment;
		}
		else
		{
			Fragments.Add(Fragment);
		}
	}

	void SetRecordTagValue(FInventoryItemRecord& Record, const FGameplayTag& Tag, float Value)
	{
		// Values matching the definition aren't stored, so items in the same state compare equal and can stack.
		const FInventoryTagValue* Default = Record.Definition->FindTagValue(Tag);
		const int32 Index = Record.TagValues.IndexOfByPredicate([&Tag](const FInventoryTagValue& TagValue) { return TagValue.Tag == Tag; });

		if (Default && Default->Value == Value)
		{
			if (Index != INDEX_NONE)
			{
				Record.TagValues.RemoveAt(Index);
			}
		}
		else if (Index != INDEX_NONE)
		{
			Record.TagValues[Index].Value = Value;
		}
		else
		{
			Record.TagValues.Emplace(Tag, Value);
		}
	}

	template <typename T>
	bool HaveSameElements(const TArray<T>& A, const TArray<T>& B)
	{
		return A.Num() == B.Num() && Algo::AllOf(A, [&B](const T& Element) { return B.Contains(Element); });
	}
}

// Item state
FInventoryPlacement FInventoryRegistry::GetItemPlacement(FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return FInventoryPlacement();
	}

	FInventoryPlacement Placement = Record->Placement;
	const FInventoryContainerRecord* Container = FindContainer(Record->Container);
	if (Container && Container->Layout->Type == EInventoryContainerType::List)
	{
		Placement.Index = Container->Items.IndexOfByKey(Item);
	}
	return Placement;
}

FIntPoint FInventoryRegistry::GetItemSize(FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	return Record ? Record->Definition->GetSize(Record->Placement.bRotated) : FIntPoint::ZeroValue;
}

FGameplayTagContainer FInventoryRegistry::GetItemTags(FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	return Record ? GetRecordTags(*Record) : FGameplayTagContainer();
}

float FInventoryRegistry::GetTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	return Record ? GetRecordTagValue(*Record, Tag) : 0.0f;
}

bool FInventoryRegistry::HasTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	return Record && (Record->Definition->FindTagValue(Tag) || Record->TagValues.ContainsByPredicate([&Tag](const FInventoryTagValue& TagValue) { return TagValue.Tag == Tag; }));
}

const FInstancedStruct* FInventoryRegistry::FindItemFragment(FInventoryItemHandle Item, const UScriptStruct* Type) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	return Record ? FindFragmentOfType(Record->Fragments, Type) : nullptr;
}

const FInstancedStruct* FInventoryRegistry::FindContainerFragment(FInventoryContainerHandle Container, const UScriptStruct* Type) const
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	return Record ? FindFragmentOfType(Record->Fragments, Type) : nullptr;
}

bool FInventoryRegistry::MatchesQuery(FInventoryItemHandle Item, const FInventoryItemQuery& Query) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	return Record && RecordMatchesQuery(*Record, Query);
}

bool FInventoryRegistry::CanStack(FInventoryItemHandle Item, FInventoryItemHandle Other) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	const FInventoryItemRecord* OtherRecord = FindItem(Other);
	return Record && OtherRecord && Item != Other && RecordsCanStack(*Record, *OtherRecord);
}

FGameplayTagContainer FInventoryRegistry::GetRecordTags(const FInventoryItemRecord& Item) const
{
	FGameplayTagContainer Tags = Item.Definition->Tags;
	Tags.AppendTags(Item.Tags);
	return Tags;
}

float FInventoryRegistry::GetRecordTagValue(const FInventoryItemRecord& Item, const FGameplayTag& Tag) const
{
	if (const FInventoryTagValue* Own = Item.TagValues.FindByPredicate([&Tag](const FInventoryTagValue& TagValue) { return TagValue.Tag == Tag; }))
	{
		return Own->Value;
	}
	const FInventoryTagValue* Default = Item.Definition->FindTagValue(Tag);
	return Default ? Default->Value : 0.0f;
}

bool FInventoryRegistry::RecordMatchesQuery(const FInventoryItemRecord& Item, const FInventoryItemQuery& Query) const
{
	if (!Item.Definition)
	{
		return false;
	}
	if (Query.IsEmpty())
	{
		return true;
	}

	const bool bDefinitionMatches = Query.Definitions.IsEmpty() || Query.Definitions.ContainsByPredicate([&Item](const TObjectPtr<UInventoryItemDefinition>& Definition)
	{
		return Definition.Get() == Item.Definition.Get();
	});

	return bDefinitionMatches
		&& InventoryTags::PassesQuery(Query.TagQuery, GetRecordTags(Item))
		&& Algo::AllOf(Query.ValueConditions, [this, &Item](const FInventoryTagValueCondition& Condition)
		{
			return Condition.Test(GetRecordTagValue(Item, Condition.Tag));
		});
}

bool FInventoryRegistry::RecordsCanStack(const FInventoryItemRecord& Item, const FInventoryItemRecord& Other) const
{
	return Item.Definition
		&& Item.Definition == Other.Definition
		&& Item.Definition->GetMaxStackSize() > 1
		&& Item.Containers.IsEmpty() && Other.Containers.IsEmpty()
		&& Item.Tags == Other.Tags
		&& HaveSameElements(Item.TagValues, Other.TagValues)
		&& HaveSameElements(Item.Fragments, Other.Fragments);
}

FInventoryItemRecord FInventoryRegistry::MakeItemRecord(const FInventoryItemSpec& Spec) const
{
	FInventoryItemRecord Record;
	Record.Definition = Spec.Definition.Get();
	Record.Count = Spec.Count;
	Record.Tags = Spec.Tags;
	Record.Fragments = Spec.Definition->InstanceFragments;

	for (const FInstancedStruct& Fragment : Spec.Fragments)
	{
		if (Fragment.IsValid())
		{
			SetFragment(Record.Fragments, Fragment);
		}
	}
	for (const FInventoryTagValue& TagValue : Spec.TagValues)
	{
		SetRecordTagValue(Record, TagValue.Tag, TagValue.Value);
	}
	return Record;
}

// Sockets
void FInventoryRegistry::ForEachSocketAffecting(FInventoryItemHandle Item, TFunctionRef<void(FInventoryItemHandle Source, const FInventoryItemSocket& Socket)> Callback) const
{
	const FInventoryItemRecord* Target = FindItem(Item);
	const FInventoryContainerRecord* Container = Target ? FindContainer(Target->Container) : nullptr;
	if (!Container || Container->Layout->Type != EInventoryContainerType::Grid)
	{
		return;
	}

	const FIntPoint TargetMin = Target->Placement.Cell;
	const FIntPoint TargetMax = TargetMin + Target->Definition->GetSize(Target->Placement.bRotated);

	for (const FInventoryItemHandle& SourceHandle : Container->Items)
	{
		const FInventoryItemRecord& Source = Items[SourceHandle.Index];
		if (SourceHandle == Item || Source.Definition->Sockets.IsEmpty())
		{
			continue;
		}

		for (const FInventoryItemSocket& Socket : Source.Definition->Sockets)
		{
			const bool bReachesTarget = Socket.Cells.ContainsByPredicate([&](const FIntPoint& LocalCell)
			{
				const FIntPoint Cell = Source.Placement.Cell + Source.Definition->RotateCell(LocalCell, Source.Placement.bRotated);
				return Cell.X >= TargetMin.X && Cell.X < TargetMax.X && Cell.Y >= TargetMin.Y && Cell.Y < TargetMax.Y;
			});

			if (bReachesTarget && RecordMatchesQuery(*Target, Socket.Filter))
			{
				Callback(SourceHandle, Socket);
			}
		}
	}
}

float FInventoryRegistry::GetEffectiveTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag) const
{
	float Value = GetTagValue(Item, Tag);
	ForEachSocketAffecting(Item, [&](FInventoryItemHandle Source, const FInventoryItemSocket& Socket)
	{
		for (const FInventoryTagValue& Modifier : Socket.ValueModifiers)
		{
			if (Modifier.Tag == Tag)
			{
				Value += Modifier.Value;
			}
		}
	});
	return Value;
}

FGameplayTagContainer FInventoryRegistry::GetEffectiveTags(FInventoryItemHandle Item) const
{
	FGameplayTagContainer Tags = GetItemTags(Item);
	ForEachSocketAffecting(Item, [&Tags](FInventoryItemHandle Source, const FInventoryItemSocket& Socket)
	{
		Tags.AppendTags(Socket.GrantedTags);
	});
	return Tags;
}

TArray<FInventoryItemHandle> FInventoryRegistry::GetSocketSources(FInventoryItemHandle Item) const
{
	TArray<FInventoryItemHandle> Sources;
	ForEachSocketAffecting(Item, [&Sources](FInventoryItemHandle Source, const FInventoryItemSocket& Socket)
	{
		Sources.AddUnique(Source);
	});
	return Sources;
}

// Search
void FInventoryRegistry::FindItems(TConstArrayView<FInventoryContainerHandle> SearchRoots, const FInventoryItemQuery& Query, bool bRecursive, TArray<FInventoryItemHandle>& OutItems) const
{
	TArray<FInventoryContainerHandle> Searched;
	if (bRecursive)
	{
		CollectContainers(SearchRoots, Searched);
	}
	else
	{
		Searched.Append(SearchRoots.GetData(), SearchRoots.Num());
	}

	for (const FInventoryContainerHandle& Container : Searched)
	{
		if (!IsValid(Container))
		{
			continue;
		}

		for (const FInventoryItemHandle& Item : Containers[Container.Index].Items)
		{
			if (RecordMatchesQuery(Items[Item.Index], Query))
			{
				OutItems.Add(Item);
			}
		}
	}
}

int32 FInventoryRegistry::CountItems(TConstArrayView<FInventoryContainerHandle> SearchRoots, const FInventoryItemQuery& Query, bool bRecursive) const
{
	TArray<FInventoryItemHandle> Found;
	FindItems(SearchRoots, Query, bRecursive, Found);

	int32 Total = 0;
	for (const FInventoryItemHandle& Item : Found)
	{
		Total += Items[Item.Index].Count;
	}
	return Total;
}

// Item state changes
EInventoryError FInventoryRegistry::SetTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag, float Value)
{
	if (!IsValid(Item))
	{
		return EInventoryError::InvalidItem;
	}
	if (!Tag.IsValid())
	{
		return EInventoryError::InvalidTag;
	}
	if (HasTagValue(Item, Tag) && GetTagValue(Item, Tag) == Value)
	{
		return EInventoryError::None;
	}

	return RunTransaction([&]()
	{
		SetRecordTagValue(EditItem(Item.Index), Tag, Value);
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::AddToTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag, float Delta)
{
	return SetTagValue(Item, Tag, GetTagValue(Item, Tag) + Delta);
}

EInventoryError FInventoryRegistry::RemoveTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag)
{
	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return EInventoryError::InvalidItem;
	}

	const int32 Index = Record->TagValues.IndexOfByPredicate([&Tag](const FInventoryTagValue& TagValue) { return TagValue.Tag == Tag; });
	if (Index == INDEX_NONE)
	{
		return EInventoryError::None;
	}

	return RunTransaction([&]()
	{
		EditItem(Item.Index).TagValues.RemoveAt(Index);
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::AddItemTag(FInventoryItemHandle Item, const FGameplayTag& Tag)
{
	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return EInventoryError::InvalidItem;
	}
	if (!Tag.IsValid())
	{
		return EInventoryError::InvalidTag;
	}
	if (Record->Tags.HasTagExact(Tag))
	{
		return EInventoryError::None;
	}

	return RunTransaction([&]()
	{
		EditItem(Item.Index).Tags.AddTag(Tag);
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::RemoveItemTag(FInventoryItemHandle Item, const FGameplayTag& Tag)
{
	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return EInventoryError::InvalidItem;
	}
	if (!Record->Tags.HasTagExact(Tag))
	{
		return EInventoryError::None;
	}

	return RunTransaction([&]()
	{
		EditItem(Item.Index).Tags.RemoveTag(Tag);
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::SetItemFragment(FInventoryItemHandle Item, const FInstancedStruct& Fragment)
{
	if (!IsValid(Item))
	{
		return EInventoryError::InvalidItem;
	}
	if (!Fragment.IsValid())
	{
		return EInventoryError::InvalidFragment;
	}

	return RunTransaction([&]()
	{
		SetFragment(EditItem(Item.Index).Fragments, Fragment);
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::RemoveItemFragment(FInventoryItemHandle Item, const UScriptStruct* Type)
{
	if (!IsValid(Item))
	{
		return EInventoryError::InvalidItem;
	}
	if (!FindItemFragment(Item, Type))
	{
		return EInventoryError::None;
	}

	return RunTransaction([&]()
	{
		EditItem(Item.Index).Fragments.RemoveAll([Type](const FInstancedStruct& Fragment) { return Fragment.GetScriptStruct() == Type; });
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::SetContainerFragment(FInventoryContainerHandle Container, const FInstancedStruct& Fragment)
{
	if (!IsValid(Container))
	{
		return EInventoryError::InvalidContainer;
	}
	if (!Fragment.IsValid())
	{
		return EInventoryError::InvalidFragment;
	}

	return RunTransaction([&]()
	{
		SetFragment(EditContainer(Container.Index).Fragments, Fragment);
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::RemoveContainerFragment(FInventoryContainerHandle Container, const UScriptStruct* Type)
{
	if (!IsValid(Container))
	{
		return EInventoryError::InvalidContainer;
	}
	if (!FindContainerFragment(Container, Type))
	{
		return EInventoryError::None;
	}

	return RunTransaction([&]()
	{
		EditContainer(Container.Index).Fragments.RemoveAll([Type](const FInstancedStruct& Fragment) { return Fragment.GetScriptStruct() == Type; });
		return EInventoryError::None;
	});
}

// Crafting
bool FInventoryRegistry::AllocateIngredients(const UInventoryRecipe& Recipe, TConstArrayView<FInventoryContainerHandle> Sources, int32 Times, TArray<FIngredientUse>& OutUses) const
{
	TArray<FInventoryContainerHandle> Searched;
	CollectContainers(Sources, Searched);

	// What earlier ingredients already claimed, so no item counts twice.
	TMap<int32, int32> Claimed;

	for (const FInventoryRecipeIngredient& Ingredient : Recipe.Ingredients)
	{
		// Tools are only needed once, however many times the recipe runs.
		int32 Needed = Ingredient.bConsume ? Ingredient.Count * Times : Ingredient.Count;

		for (const FInventoryContainerHandle& Container : Searched)
		{
			for (const FInventoryItemHandle& Item : Containers[Container.Index].Items)
			{
				const FInventoryItemRecord& Record = Items[Item.Index];
				const int32 Available = Record.Count - Claimed.FindRef(Item.Index);
				if (Needed == 0 || Available <= 0 || !RecordMatchesQuery(Record, Ingredient.Query))
				{
					continue;
				}

				const int32 Taken = FMath::Min(Available, Needed);
				Claimed.FindOrAdd(Item.Index) += Taken;
				Needed -= Taken;

				FIngredientUse& Use = OutUses.AddDefaulted_GetRef();
				Use.Item = Item;
				Use.Count = Taken;
				Use.bConsume = Ingredient.bConsume;
			}
		}

		if (Needed > 0)
		{
			return false;
		}
	}
	return true;
}

EInventoryError FInventoryRegistry::CanCraft(const UInventoryRecipe& Recipe, TConstArrayView<FInventoryContainerHandle> Sources, const FGameplayTagContainer& ContextTags, int32 Times) const
{
	if (Times < 1)
	{
		return EInventoryError::InvalidCount;
	}
	if (!Recipe.IsAvailable(ContextTags))
	{
		return EInventoryError::RequirementsNotMet;
	}

	TArray<FIngredientUse> Uses;
	return AllocateIngredients(Recipe, Sources, Times, Uses) ? EInventoryError::None : EInventoryError::MissingIngredients;
}

EInventoryError FInventoryRegistry::Craft(const UInventoryRecipe& Recipe, TConstArrayView<FInventoryContainerHandle> Sources, TConstArrayView<FInventoryContainerHandle> Outputs,
	const FGameplayTagContainer& ContextTags, int32 Times, FRandomStream& Stream, TArray<FInventoryItemHandle>* OutItems)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_Craft);

	if (Times < 1)
	{
		return EInventoryError::InvalidCount;
	}
	if (!Recipe.IsAvailable(ContextTags))
	{
		return EInventoryError::RequirementsNotMet;
	}

	return RunTransaction([&]() -> EInventoryError
	{
		TArray<FIngredientUse> Uses;
		if (!AllocateIngredients(Recipe, Sources, Times, Uses))
		{
			return EInventoryError::MissingIngredients;
		}

		// Consumed first, so the outputs can use the space the ingredients leave.
		for (const FIngredientUse& Use : Uses)
		{
			if (Use.bConsume)
			{
				RemoveItem(Use.Item, Use.Count);
			}
		}

		TArray<FInventoryItemSpec> Results;
		for (const FInventoryItemSpec& Output : Recipe.Outputs)
		{
			Results.Add_GetRef(Output).Count *= Times;
		}
		if (Recipe.RandomOutputs)
		{
			for (int32 RollIndex = 0; RollIndex < Times; ++RollIndex)
			{
				Recipe.RandomOutputs->Roll(Stream, ContextTags, Results);
			}
		}

		for (const FInventoryItemSpec& Result : Results)
		{
			EInventoryError Error = EInventoryError::None;
			AddItemToContainers(Result, Outputs, false, OutItems, &Error);
			if (Error != EInventoryError::None)
			{
				return Error;
			}
		}
		return EInventoryError::None;
	});
}
