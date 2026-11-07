#include "Debug/GameplayDebuggerCategory_Inventory.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemDefinition.h"
#include "Equipment/InventoryEquipmentComponent.h"
#include "InventoryComponent.h"
#include "InventoryRegistry.h"
#include "InventorySubsystem.h"

namespace
{
	// Keeps a huge stash from pushing everything else off screen.
	constexpr int32 MaxLines = 80;

	FString Indent(int32 Depth)
	{
		return FString::ChrN(Depth * 2, TEXT(' '));
	}

	FString DescribeItem(const FInventoryRegistry& Registry, FInventoryItemHandle Item)
	{
		const FInventoryItemRecord* Record = Registry.FindItem(Item);
		if (!Record)
		{
			return TEXT("{red}<destroyed>{white}");
		}

		FString Description = FString::Printf(TEXT("%s x%d"), *GetNameSafe(Record->Definition), Record->Count);
		for (const FInventoryTagValue& TagValue : Record->TagValues)
		{
			Description += FString::Printf(TEXT(" {grey}%s=%g{white}"), *TagValue.Tag.ToString(), TagValue.Value);
		}
		return Description;
	}

	FString DescribePlacement(const FInventoryRegistry& Registry, FInventoryItemHandle Item, const UInventoryContainerLayout& Layout)
	{
		const FInventoryPlacement Placement = Registry.GetItemPlacement(Item);
		switch (Layout.Type)
		{
		case EInventoryContainerType::Grid:
			return FString::Printf(TEXT("(%d,%d)%s"), Placement.Cell.X, Placement.Cell.Y, Placement.bRotated ? TEXT(" rotated") : TEXT(""));
		case EInventoryContainerType::Slots:
			return Layout.Slots.IsValidIndex(Placement.Index) ? Layout.Slots[Placement.Index].Tag.ToString() : TEXT("?");
		case EInventoryContainerType::List:
			return FString::Printf(TEXT("#%d"), Placement.Index);
		}
		return FString();
	}

	const TCHAR* DescribeType(EInventoryContainerType Type)
	{
		switch (Type)
		{
		case EInventoryContainerType::Grid:  return TEXT("Grid");
		case EInventoryContainerType::Slots: return TEXT("Slots");
		case EInventoryContainerType::List:  return TEXT("List");
		}
		return TEXT("");
	}
}

FGameplayDebuggerCategory_Inventory::FGameplayDebuggerCategory_Inventory()
{
	bShowOnlyWithDebugActor = false;
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Inventory::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_Inventory());
}

void FGameplayDebuggerCategory_Inventory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	NumLines = 0;

	const AActor* Target = DebugActor ? DebugActor : (OwnerPC ? OwnerPC->GetPawn() : nullptr);
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(OwnerPC);
	if (!Target || !Subsystem)
	{
		AddLine(TEXT("{red}Nothing to show: no target or no inventory subsystem."));
		return;
	}

	const FInventoryRegistry& Registry = Subsystem->GetRegistry();
	AddLine(FString::Printf(TEXT("{yellow}%s{white}   world: %d items in %d containers"), *GetNameSafe(Target), Registry.GetNumItems(), Registry.GetNumContainers()));

	if (const UInventoryComponent* Inventory = UInventoryComponent::FindInventoryComponent(Target))
	{
		for (const FInventoryContainerHandle& Container : Inventory->GetContainers())
		{
			AddContainerLines(Registry, Container, 1);
		}
	}
	else
	{
		AddLine(TEXT("{grey}No inventory component."));
	}

	AddEquipmentLines(*Target);
}

void FGameplayDebuggerCategory_Inventory::AddContainerLines(const FInventoryRegistry& Registry, FInventoryContainerHandle Container, int32 Depth)
{
	const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
	if (!Record)
	{
		return;
	}

	const UInventoryContainerLayout& Layout = *Record->Layout;
	const FString Size = Layout.Type == EInventoryContainerType::Grid ? FString::Printf(TEXT(" %dx%d"), Layout.GridSize.X, Layout.GridSize.Y) : FString();
	AddLine(FString::Printf(TEXT("%s{green}%s{white} %s%s%s  %d items"), *Indent(Depth), *Record->Tag.ToString(), DescribeType(Layout.Type), *Size,
		Layout.bEquipment ? TEXT(" equipment") : TEXT(""), Record->Items.Num()));

	for (const FInventoryItemHandle& Item : Record->Items)
	{
		AddLine(FString::Printf(TEXT("%s%s  {grey}%s{white}"), *Indent(Depth + 1), *DescribeItem(Registry, Item), *DescribePlacement(Registry, Item, Layout)));

		if (const FInventoryItemRecord* ItemRecord = Registry.FindItem(Item))
		{
			for (const FInventoryContainerHandle& Nested : ItemRecord->Containers)
			{
				AddContainerLines(Registry, Nested, Depth + 2);
			}
		}
	}

	if (Layout.Type == EInventoryContainerType::Grid)
	{
		AddGridLines(Registry, Container, Depth + 1);
	}
}

void FGameplayDebuggerCategory_Inventory::AddGridLines(const FInventoryRegistry& Registry, FInventoryContainerHandle Container, int32 Depth)
{
	const FInventoryContainerRecord& Record = *Registry.FindContainer(Container);
	const FIntPoint GridSize = Record.Layout->GridSize;

	// Each item gets a letter in the order it's listed above.
	TMap<int32, TCHAR> Letters;
	for (int32 Index = 0; Index < Record.Items.Num(); ++Index)
	{
		Letters.Add(Record.Items[Index].Index, static_cast<TCHAR>(TEXT('A') + Index % 26));
	}

	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		FString Row = Indent(Depth) + TEXT("{grey}");
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			const int32 CellIndex = Y * GridSize.X + X;
			const int32 Occupant = Record.Cells.IsValidIndex(CellIndex) ? Record.Cells[CellIndex] : INDEX_NONE;
			const TCHAR* Letter = Letters.Find(Occupant);
			Row.AppendChar(Letter ? *Letter : TEXT('.'));
		}
		AddLine(Row + TEXT("{white}"));
	}
}

void FGameplayDebuggerCategory_Inventory::AddEquipmentLines(const AActor& Target)
{
	const UInventoryEquipmentComponent* Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(&Target);
	if (!Equipment)
	{
		return;
	}

	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(&Target);
	AddLine(TEXT("{yellow}Equipped"));

	for (const FInventoryEquippedItem& Entry : Equipment->GetEquippedEntries())
	{
		const FString ItemName = Subsystem ? GetNameSafe(Subsystem->GetItemDefinition(Entry.Item)) : FString();
		AddLine(FString::Printf(TEXT("%s{green}%s{white} %s  level %d  abilities %d  effects %d  sets %d  pieces %d%s"),
			*Indent(Entry.ParentItem.IsSet() ? 2 : 1), *Entry.Slot.ToString(), *ItemName, Entry.Level,
			Entry.Abilities.Num(), Entry.Effects.Num(), Entry.AttributeSets.Num(), Entry.Pieces.Num(),
			Entry.bCosmeticsApplied ? TEXT("") : TEXT("  {orange}loading{white}")));

		for (const FInventorySpawnedPiece& Piece : Entry.Pieces)
		{
			const USceneComponent* PieceRoot = Cast<USceneComponent>(Piece.Spawned);
			if (const AActor* PieceActor = Cast<AActor>(Piece.Spawned))
			{
				PieceRoot = PieceActor->GetRootComponent();
			}
			if (PieceRoot)
			{
				AddShape(FGameplayDebuggerShape::MakePoint(PieceRoot->GetComponentLocation(), 4.0f, FColor::Green, ItemName));
			}
		}
	}
}

void FGameplayDebuggerCategory_Inventory::AddLine(const FString& Line)
{
	if (NumLines < MaxLines)
	{
		AddTextLine(Line);
	}
	else if (NumLines == MaxLines)
	{
		AddTextLine(TEXT("{grey}..."));
	}
	++NumLines;
}

#endif // WITH_GAMEPLAY_DEBUGGER
