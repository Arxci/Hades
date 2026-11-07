#include "Details/InventoryItemDefinitionCustomization.h"

#include "Brushes/SlateColorBrush.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/Texture2D.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

#include "Assets/InventoryItemCreation.h"
#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "Icons/InventoryIconGenerator.h"
#include "Icons/SInventoryIconStudio.h"

#define LOCTEXT_NAMESPACE "InventoryItemDefinitionCustomization"

namespace
{
	FLinearColor GetSocketColor(int32 SocketIndex)
	{
		static const FLinearColor Colors[] = {
			FLinearColor(0.90f, 0.50f, 0.10f), FLinearColor(0.20f, 0.60f, 0.90f), FLinearColor(0.40f, 0.80f, 0.30f), FLinearColor(0.80f, 0.30f, 0.70f) };
		return Colors[SocketIndex % UE_ARRAY_COUNT(Colors)];
	}

	/** Paints the item's cells and the cells its sockets reach. It reads the item every frame, so edits show at once. */
	class SInventoryFootprint : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SInventoryFootprint) {}
			SLATE_ARGUMENT(TWeakObjectPtr<const UInventoryItemDefinition>, Definition)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Definition = InArgs._Definition;
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			FVector2D PanelsSize(0.0, 0.0);
			for (const bool bRotated : GetOrientations())
			{
				const FIntRect Bounds = GetBounds(bRotated);
				PanelsSize.X += Bounds.Width() * CellSize + PanelGap;
				PanelsSize.Y = FMath::Max(PanelsSize.Y, CaptionHeight + Bounds.Height() * CellSize);
			}
			return PanelsSize;
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const UInventoryItemDefinition* Item = Definition.Get();
			if (!Item)
			{
				return LayerId;
			}

			static const FSlateColorBrush CellBrush(FLinearColor::White);
			const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
			const FLinearColor ItemColor(0.55f, 0.55f, 0.6f);
			const FLinearColor EmptyColor(0.1f, 0.1f, 0.1f);
			double PanelX = 0.0;

			for (const bool bRotated : GetOrientations())
			{
				const FIntRect Bounds = GetBounds(bRotated);
				const FIntPoint ItemSize = Item->GetSize(bRotated);
				PaintLabel(AllottedGeometry, OutDrawElements, LayerId + 1, FVector2D(PanelX, 0.0), bRotated ? TEXT("Rotated") : TEXT("Unrotated"), Font);

				for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
				{
					for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
					{
						const FIntPoint Cell(X, Y);
						const bool bItemCell = X >= 0 && Y >= 0 && X < ItemSize.X && Y < ItemSize.Y;
						FLinearColor Color = bItemCell ? ItemColor : EmptyColor;

						// Socket numbers, as listed in the Sockets array below.
						FString SocketLabel;
						for (int32 SocketIndex = 0; SocketIndex < Item->Sockets.Num(); ++SocketIndex)
						{
							const bool bReached = Item->Sockets[SocketIndex].Cells.ContainsByPredicate([&](const FIntPoint& SocketCell)
							{
								return Item->RotateCell(SocketCell, bRotated) == Cell;
							});
							if (bReached)
							{
								Color = GetSocketColor(SocketIndex);
								SocketLabel += SocketLabel.IsEmpty() ? FString() : FString(TEXT(","));
								SocketLabel += FString::FromInt(SocketIndex);
							}
						}

						const FVector2D Position(PanelX + (X - Bounds.Min.X) * CellSize, CaptionHeight + (Y - Bounds.Min.Y) * CellSize);
						FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(CellSize - CellGap), FSlateLayoutTransform(Position)),
							&CellBrush, ESlateDrawEffect::None, Color);
						if (!SocketLabel.IsEmpty())
						{
							PaintLabel(AllottedGeometry, OutDrawElements, LayerId + 1, Position + FVector2D(3.0, 2.0), SocketLabel, Font);
						}
					}
				}
				PanelX += Bounds.Width() * CellSize + PanelGap;
			}
			return LayerId + 1;
		}

	private:
		static constexpr double CellSize = 18.0;
		static constexpr double CellGap = 2.0;
		static constexpr double PanelGap = 24.0;
		static constexpr double CaptionHeight = 16.0;

		static void PaintLabel(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer, const FVector2D& Position, const FString& Label, const FSlateFontInfo& Font)
		{
			FSlateDrawElement::MakeText(OutDrawElements, Layer, Geometry.ToPaintGeometry(FVector2D(100.0, CaptionHeight), FSlateLayoutTransform(Position)), Label, Font);
		}

		TArray<bool, TInlineAllocator<2>> GetOrientations() const
		{
			TArray<bool, TInlineAllocator<2>> Orientations;
			if (const UInventoryItemDefinition* Item = Definition.Get())
			{
				Orientations.Add(false);
				if (Item->bCanRotate)
				{
					Orientations.Add(true);
				}
			}
			return Orientations;
		}

		/** The item's cells plus every cell a socket reaches, relative to the item's top-left cell. */
		FIntRect GetBounds(bool bRotated) const
		{
			const UInventoryItemDefinition* Item = Definition.Get();
			FIntRect Bounds(FIntPoint::ZeroValue, Item->GetSize(bRotated));
			for (const FInventoryItemSocket& Socket : Item->Sockets)
			{
				for (const FIntPoint& SocketCell : Socket.Cells)
				{
					const FIntPoint Cell = Item->RotateCell(SocketCell, bRotated);
					Bounds.Min = Bounds.Min.ComponentMin(Cell);
					Bounds.Max = Bounds.Max.ComponentMax(Cell + FIntPoint(1, 1));
				}
			}
			return Bounds;
		}

		TWeakObjectPtr<const UInventoryItemDefinition> Definition;
	};

	/** Paints the item's icon stretched over its cells, the way inventory widgets do, so a wrongly shaped icon looks wrong here too. */
	class SInventoryItemIcon : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SInventoryItemIcon) {}
			SLATE_ARGUMENT(TWeakObjectPtr<const UInventoryItemDefinition>, Definition)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Definition = InArgs._Definition;
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			const UInventoryItemDefinition* Item = Definition.Get();
			const FIntPoint Cells = Item ? Item->Size.ComponentMax(FIntPoint(1, 1)) : FIntPoint(1, 1);
			return FVector2D(Cells) * (LongSide / FMath::Max(Cells.X, Cells.Y));
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			static const FSlateColorBrush CellBrush(FLinearColor::White);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &CellBrush, ESlateDrawEffect::None, FLinearColor(0.1f, 0.1f, 0.1f));

			const UInventoryItemDefinition* Item = Definition.Get();
			const UInventoryItemCosmetics* Cosmetics = Item ? Item->Cosmetics.Get() : nullptr;
			UTexture2D* Icon = Cosmetics ? Cosmetics->Icon.Get() : nullptr;
			if (!Icon)
			{
				return LayerId;
			}

			IconBrush.SetResourceObject(Icon);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), &IconBrush);
			return LayerId + 1;
		}

	private:
		static constexpr double LongSide = 96.0;

		TWeakObjectPtr<const UInventoryItemDefinition> Definition;
		mutable FSlateBrush IconBrush;
	};

	void AddCosmeticsRow(IDetailCategoryBuilder& Category, const TSharedRef<IPropertyHandle>& CosmeticsHandle, TWeakObjectPtr<UInventoryItemDefinition> Item)
	{
		IDetailPropertyRow& Row = Category.AddProperty(CosmeticsHandle);
		TSharedPtr<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
		Row.GetDefaultWidgets(NameWidget, ValueWidget);

		Row.CustomWidget()
			.NameContent()
			[
				NameWidget.ToSharedRef()
			]
			.ValueContent()
			.MinDesiredWidth(300.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					ValueWidget.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateCosmetics", "Create"))
					.ToolTipText(LOCTEXT("CreateCosmeticsTooltip", "Makes cosmetics next to this item, named after it, and assigns them."))
					.Visibility_Lambda([CosmeticsHandle]()
					{
						UObject* Cosmetics = nullptr;
						CosmeticsHandle->GetValue(Cosmetics);
						return Cosmetics ? EVisibility::Collapsed : EVisibility::Visible;
					})
					.OnClicked_Lambda([CosmeticsHandle, Item]()
					{
						const UInventoryItemDefinition* Definition = Item.Get();
						UObject* Cosmetics = Definition ? InventoryItemCreation::CreateCosmetics(*Definition) : nullptr;
						if (Cosmetics)
						{
							CosmeticsHandle->SetValue(Cosmetics);
						}
						return FReply::Handled();
					})
				]
			];
	}

	void AddIconRow(IDetailCategoryBuilder& Category, TWeakObjectPtr<UInventoryItemDefinition> Item)
	{
		Category.AddCustomRow(LOCTEXT("Icon", "Icon"))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Icon", "Icon"))
				.ToolTipText(LOCTEXT("IconTooltip", "The cosmetics' icon over the item's cells, as inventory widgets show it."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f)
				[
					SNew(SInventoryItemIcon)
					.Definition(Item.Get())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("EditIcon", "Edit Icon..."))
					.ToolTipText(LOCTEXT("EditIconTooltip", "Opens the Icon Studio, which renders an icon from the item's World Mesh."))
					.OnClicked_Lambda([Item]()
					{
						if (UInventoryItemDefinition* Definition = Item.Get())
						{
							SInventoryIconStudio::Open(*Definition);
						}
						return FReply::Handled();
					})
				]
			];
	}

	void AddFootprintRow(IDetailCategoryBuilder& Category, const UInventoryItemDefinition* Item)
	{
		Category.AddCustomRow(LOCTEXT("Footprint", "Footprint"))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Footprint", "Footprint"))
				.ToolTipText(LOCTEXT("FootprintTooltip", "Grey cells are the item. Colored cells are what its sockets reach, numbered by socket. The top-left grey cell is (0,0)."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SInventoryFootprint)
					.Definition(Item)
				]
			];
	}
}

TSharedRef<IDetailCustomization> FInventoryItemDefinitionCustomization::MakeInstance()
{
	return MakeShared<FInventoryItemDefinitionCustomization>();
}

void FInventoryItemDefinitionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	UInventoryItemDefinition* Item = Objects.Num() == 1 ? Cast<UInventoryItemDefinition>(Objects[0].Get()) : nullptr;
	if (!Item)
	{
		return;
	}

	// The icon preview only shows icons that are already loaded.
	InventoryIcons::LoadIcon(*Item);

	// Every Display property is added, so they keep their order around the new rows.
	IDetailCategoryBuilder& Display = DetailBuilder.EditCategory("Display");
	Display.AddProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UInventoryItemDefinition, DisplayName)));
	Display.AddProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UInventoryItemDefinition, Description)));
	AddCosmeticsRow(Display, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UInventoryItemDefinition, Cosmetics)), Item);
	AddIconRow(Display, Item);

	AddFootprintRow(DetailBuilder.EditCategory("Sockets"), Item);
}

#undef LOCTEXT_NAMESPACE
