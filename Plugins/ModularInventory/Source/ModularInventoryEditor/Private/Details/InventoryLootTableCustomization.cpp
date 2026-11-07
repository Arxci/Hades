#include "Details/InventoryLootTableCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Math/RandomStream.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#include "Data/InventoryItemDefinition.h"
#include "Loot/InventoryLootTable.h"

#define LOCTEXT_NAMESPACE "InventoryLootTableCustomization"

namespace
{
	struct FDropRate
	{
		const UInventoryItemDefinition* Item = nullptr;
		int32 RollsWithItem = 0;
		int64 TotalCount = 0;
		int32 MinCount = MAX_int32;
		int32 MaxCount = 0;
	};

	struct FDropRates
	{
		int32 Rolls = 0;
		int32 EmptyRolls = 0;

		/** Most likely first. */
		TArray<FDropRate> Items;
	};

	FDropRates SimulateDrops(const UInventoryLootTable& Table)
	{
		FDropRates Result;
		Result.Rolls = FMath::Max(1, Table.SimulationRolls);

		// A fixed seed keeps the numbers still while something unrelated is edited.
		FRandomStream Stream(1234);
		TMap<const UInventoryItemDefinition*, FDropRate> Rates;
		TMap<const UInventoryItemDefinition*, int32> RollCounts;
		TArray<FInventoryItemSpec> Rolled;
		for (int32 RollIndex = 0; RollIndex < Result.Rolls; ++RollIndex)
		{
			Rolled.Reset();
			Table.Roll(Stream, Table.SimulationContext, Rolled);

			RollCounts.Reset();
			for (const FInventoryItemSpec& Spec : Rolled)
			{
				const UInventoryItemDefinition* Definition = Spec.Definition;
				RollCounts.FindOrAdd(Definition) += Spec.Count;
			}
			Result.EmptyRolls += RollCounts.IsEmpty() ? 1 : 0;

			for (const TPair<const UInventoryItemDefinition*, int32>& RollCount : RollCounts)
			{
				FDropRate& Rate = Rates.FindOrAdd(RollCount.Key);
				Rate.Item = RollCount.Key;
				++Rate.RollsWithItem;
				Rate.TotalCount += RollCount.Value;
				Rate.MinCount = FMath::Min(Rate.MinCount, RollCount.Value);
				Rate.MaxCount = FMath::Max(Rate.MaxCount, RollCount.Value);
			}
		}

		Rates.GenerateValueArray(Result.Items);
		Result.Items.Sort([](const FDropRate& A, const FDropRate& B) { return A.RollsWithItem > B.RollsWithItem; });
		return Result;
	}

	/** Simulates the table again whenever any loot table changes, since tables can roll other tables. */
	class SInventoryDropRates : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SInventoryDropRates) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const UInventoryLootTable* InTable)
		{
			Table = InTable;

			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 6.0f)
				[
					SAssignNew(Summary, STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(Grid, SGridPanel)
					.FillColumn(0, 1.0f)
				]
			];

			FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SInventoryDropRates::HandleObjectPropertyChanged);
			Refresh();
		}

		virtual ~SInventoryDropRates() override
		{
			FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			if (bOutOfDate)
			{
				Refresh();
			}
		}

	private:
		void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
		{
			if (Object && Object->IsA<UInventoryLootTable>())
			{
				bOutOfDate = true;
			}
		}

		void Refresh()
		{
			bOutOfDate = false;
			Grid->ClearChildren();

			const UInventoryLootTable* LootTable = Table.Get();
			if (!LootTable)
			{
				return;
			}

			const FDropRates Rates = SimulateDrops(*LootTable);
			FNumberFormattingOptions OneDecimal;
			OneDecimal.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
			FNumberFormattingOptions TwoDecimals;
			TwoDecimals.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);

			Summary->SetText(FText::Format(LOCTEXT("Summary", "Nothing drops in {0} of {1} rolls."),
				FText::AsPercent(static_cast<double>(Rates.EmptyRolls) / Rates.Rolls, &OneDecimal), FText::AsNumber(Rates.Rolls)));

			AddRow(0, { LOCTEXT("Item", "Item"), LOCTEXT("Chance", "Chance"), LOCTEXT("Count", "Count"), LOCTEXT("PerRoll", "Per Roll") }, true);
			for (int32 Index = 0; Index < Rates.Items.Num(); ++Index)
			{
				const FDropRate& Rate = Rates.Items[Index];
				const FText Count = Rate.MinCount == Rate.MaxCount
					? FText::AsNumber(Rate.MinCount)
					: FText::Format(LOCTEXT("CountRange", "{0}-{1}"), Rate.MinCount, Rate.MaxCount);

				AddRow(Index + 1, {
					FText::FromString(GetNameSafe(Rate.Item)),
					FText::AsPercent(static_cast<double>(Rate.RollsWithItem) / Rates.Rolls, &OneDecimal),
					Count,
					FText::AsNumber(static_cast<double>(Rate.TotalCount) / Rates.Rolls, &TwoDecimals) }, false);
			}
		}

		void AddRow(int32 Row, const TArray<FText>& Cells, bool bHeader)
		{
			for (int32 Column = 0; Column < Cells.Num(); ++Column)
			{
				Grid->AddSlot(Column, Row)
					.HAlign(Column == 0 ? HAlign_Left : HAlign_Right)
					.Padding(Column == 0 ? FMargin(0.0f, 1.0f) : FMargin(16.0f, 1.0f, 0.0f, 1.0f))
					[
						SNew(STextBlock)
						.Text(Cells[Column])
						.Font(bHeader ? IDetailLayoutBuilder::GetDetailFontBold() : IDetailLayoutBuilder::GetDetailFont())
					];
			}
		}

		TWeakObjectPtr<const UInventoryLootTable> Table;
		TSharedPtr<STextBlock> Summary;
		TSharedPtr<SGridPanel> Grid;
		bool bOutOfDate = false;
	};
}

TSharedRef<IDetailCustomization> FInventoryLootTableCustomization::MakeInstance()
{
	return MakeShared<FInventoryLootTableCustomization>();
}

void FInventoryLootTableCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	const UInventoryLootTable* Table = Objects.Num() == 1 ? Cast<UInventoryLootTable>(Objects[0].Get()) : nullptr;
	if (!Table)
	{
		return;
	}

	// The table's own Simulate button opens a dialog; the live rates below replace it.
	DetailBuilder.HideCategory("Simulation");

	// Edited first so the pools stay above the rates.
	DetailBuilder.EditCategory("Loot");

	IDetailCategoryBuilder& DropRates = DetailBuilder.EditCategory("DropRates", LOCTEXT("DropRates", "Drop Rates"));
	DropRates.AddProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UInventoryLootTable, SimulationRolls)));
	DropRates.AddProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UInventoryLootTable, SimulationContext)));
	DropRates.AddCustomRow(LOCTEXT("DropRates", "Drop Rates"))
		.WholeRowContent()
		[
			SNew(SInventoryDropRates, Table)
		];
}

#undef LOCTEXT_NAMESPACE
