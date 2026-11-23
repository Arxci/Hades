#include "Documentation/SInventoryDocumentation.h"

#include "Algo/AllOf.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "CoreGlobals.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Children.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#include "InventoryEditorStyle.h"
#include "ModularInventoryEditor.h"

#define LOCTEXT_NAMESPACE "InventoryDocumentation"

/** An image of the guide's, kept while the window is open. */
struct FInventoryDocumentationImage
{
	TStrongObjectPtr<UTexture2D> Texture;
	FSlateBrush Brush;
	FIntPoint Size = FIntPoint::ZeroValue;

	/** The file's time when it was loaded, to load it again once it's saved. */
	FDateTime Timestamp;
};

namespace InventoryDocumentation
{
	using namespace InventoryMarkdown;

	/** Where the window remembers the chapter it showed, per project. */
	const TCHAR* const ConfigSection = TEXT("ModularInventoryDocumentation");

	FName TabName()
	{
		return FName(TEXT("ModularInventoryDocumentation"));
	}

	TWeakPtr<SInventoryDocumentation> OpenWindow;

	/** The rich text's styles. Not registered: only the window's text uses them. */
	TSharedPtr<FSlateStyleSet> TextStyles;

	/** How wide the guide is drawn at most, about as wide as GitHub shows a README, so lines stay easy to read. */
	constexpr float ColumnWidth = 880.0f;

	/** Point sizes of body text and headings 1 to 6. */
	constexpr int32 ScaleCount = 7;
	constexpr int32 TextSizes[ScaleCount] = { 11, 26, 22, 15, 12, 11, 11 };

	/** Colors for the guide's alerts, such as > [!NOTE], as GitHub draws them. */
	FLinearColor AlertColor(const FString& Kind)
	{
		if (Kind == TEXT("Note"))
		{
			return FLinearColor(FColor(47, 129, 247));
		}
		if (Kind == TEXT("Tip"))
		{
			return FLinearColor(FColor(63, 185, 80));
		}
		if (Kind == TEXT("Important"))
		{
			return FLinearColor(FColor(163, 113, 247));
		}
		if (Kind == TEXT("Warning"))
		{
			return FLinearColor(FColor(210, 153, 34));
		}
		if (Kind == TEXT("Caution"))
		{
			return FLinearColor(FColor(248, 81, 73));
		}
		return FLinearColor(FColor(110, 110, 110));
	}

	const FSlateBrush* WhiteBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	/** Rounded, and white, so a border tints it with its background color. */
	const FSlateBrush* RoundedBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 6.0f);
		return &Brush;
	}

	const FSlateBrush* NoBrush()
	{
		static const FSlateNoResource Brush;
		return &Brush;
	}

	/** Cards: a raised panel with a faint outline. */
	const FSlateBrush* PanelBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FStyleColors::Background, 6.0f, FStyleColors::Hover, 1.0f);
		return &Brush;
	}

	/** Code blocks: darker than the page, as in a code editor, with an outline. */
	const FLinearColor CodeBackground(FColor(17, 17, 17));
	const FLinearColor CodeOutline(FColor(58, 58, 58));

	const FSlateBrush* CodeBrush()
	{
		static const FSlateRoundedBoxBrush Brush(CodeBackground, 6.0f, CodeOutline, 1.0f);
		return &Brush;
	}

	/** A code editor's colors for each kind of code, those of Visual Studio Code's dark theme. */
	FLinearColor CodeColor(ECodeToken Token)
	{
		switch (Token)
		{
		case ECodeToken::Keyword:
			return FLinearColor(FColor(86, 156, 214));
		case ECodeToken::Type:
			return FLinearColor(FColor(78, 201, 176));
		case ECodeToken::Function:
			return FLinearColor(FColor(220, 220, 170));
		case ECodeToken::String:
			return FLinearColor(FColor(206, 145, 120));
		case ECodeToken::Number:
			return FLinearColor(FColor(181, 206, 168));
		case ECodeToken::Comment:
			return FLinearColor(FColor(106, 153, 85));
		case ECodeToken::Preprocessor:
			return FLinearColor(FColor(197, 134, 192));
		case ECodeToken::Plain:
		default:
			return FLinearColor(FColor(212, 212, 212));
		}
	}

	/** Table cells' room around their text, and the grid line between them, which is half on each side. */
	const FMargin TableCellPadding(10.0f, 6.0f);
	constexpr float TableLine = 0.5f;

	/** Entries in the contents, and other flat buttons: nothing until hovered. */
	const FButtonStyle& FlatButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
			.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Header, 4.0f))
			.SetDisabled(FSlateNoResource())
			.SetNormalPadding(FMargin(0.0f))
			.SetPressedPadding(FMargin(0.0f));
		return Style;
	}

	/** The style of a run, named so the rich text can ask for it: md, the scale, then b, i, c and s for its formatting, and l for a link. */
	FName StyleName(int32 Scale, EInlineStyle Style, bool bLink)
	{
		return FName(*FString::Printf(TEXT("md%d%s%s%s%s%s"), Scale,
			EnumHasAnyFlags(Style, EInlineStyle::Bold) ? TEXT("b") : TEXT(""),
			EnumHasAnyFlags(Style, EInlineStyle::Italic) ? TEXT("i") : TEXT(""),
			EnumHasAnyFlags(Style, EInlineStyle::Code) ? TEXT("c") : TEXT(""),
			EnumHasAnyFlags(Style, EInlineStyle::Strikethrough) ? TEXT("s") : TEXT(""),
			bLink ? TEXT("l") : TEXT("")));
	}

	/** The style of a kind of code in a code block: mdcode for plain code, else mdcode. and the kind, such as mdcode.keyword. */
	FName CodeStyleName(ECodeToken Token)
	{
		switch (Token)
		{
		case ECodeToken::Keyword:
			return FName(TEXT("mdcode.keyword"));
		case ECodeToken::Type:
			return FName(TEXT("mdcode.type"));
		case ECodeToken::Function:
			return FName(TEXT("mdcode.function"));
		case ECodeToken::String:
			return FName(TEXT("mdcode.string"));
		case ECodeToken::Number:
			return FName(TEXT("mdcode.number"));
		case ECodeToken::Comment:
			return FName(TEXT("mdcode.comment"));
		case ECodeToken::Preprocessor:
			return FName(TEXT("mdcode.preprocessor"));
		case ECodeToken::Plain:
		default:
			return FName(TEXT("mdcode"));
		}
	}

	const FSlateStyleSet& GetTextStyles()
	{
		if (TextStyles.IsValid())
		{
			return *TextStyles;
		}

		TextStyles = MakeShared<FSlateStyleSet>(TEXT("ModularInventoryDocumentationText"));
		const FTextBlockStyle& NormalText = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
		const FHyperlinkStyle& Hyperlink = FCoreStyle::Get().GetWidgetStyle<FHyperlinkStyle>(TEXT("Hyperlink"));
		const FSlateColor InlineCodeColor(FLinearColor(FColor(236, 184, 136)));

		for (int32 Scale = 0; Scale < ScaleCount; ++Scale)
		{
			for (int32 Flags = 0; Flags < 16; ++Flags)
			{
				const EInlineStyle Style = static_cast<EInlineStyle>(Flags);
				const bool bBold = Scale > 0 || EnumHasAnyFlags(Style, EInlineStyle::Bold);
				const bool bItalic = EnumHasAnyFlags(Style, EInlineStyle::Italic);
				const bool bCode = EnumHasAnyFlags(Style, EInlineStyle::Code);
				const TCHAR* Typeface = bCode ? TEXT("Mono") : ((bBold && bItalic) ? TEXT("BoldItalic") : (bBold ? TEXT("Bold") : (bItalic ? TEXT("Italic") : TEXT("Regular"))));
				const int32 Size = TextSizes[Scale] - (bCode ? 1 : 0);

				for (int32 LinkPass = 0; LinkPass < 2; ++LinkPass)
				{
					const bool bLink = LinkPass == 1;
					FSlateColor Color = FSlateColor::UseForeground();
					if (bLink)
					{
						Color = FStyleColors::AccentBlue;
					}
					else if (bCode)
					{
						Color = InlineCodeColor;
					}
					else if (EnumHasAnyFlags(Style, EInlineStyle::Strikethrough) || Scale >= 5)
					{
						Color = FSlateColor::UseSubduedForeground();
					}

					const FTextBlockStyle TextStyle = FTextBlockStyle(NormalText)
						.SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, Size))
						.SetColorAndOpacity(Color);
					if (bLink)
					{
						TextStyles->Set(StyleName(Scale, Style, true), FHyperlinkStyle(Hyperlink).SetTextStyle(TextStyle));
					}
					else
					{
						TextStyles->Set(StyleName(Scale, Style, false), TextStyle);
					}
				}
			}
		}

		// Code blocks: a monospaced font, colored by what each piece of code is.
		for (int32 Token = 0; Token <= static_cast<int32>(ECodeToken::Preprocessor); ++Token)
		{
			const ECodeToken CodeToken = static_cast<ECodeToken>(Token);
			TextStyles->Set(CodeStyleName(CodeToken), FTextBlockStyle(NormalText)
				.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 10))
				.SetColorAndOpacity(FSlateColor(CodeColor(CodeToken))));
		}
		return *TextStyles;
	}

	const FTextBlockStyle& BodyStyle(bool bBold = false)
	{
		return GetTextStyles().GetWidgetStyle<FTextBlockStyle>(StyleName(0, bBold ? EInlineStyle::Bold : EInlineStyle::None, false));
	}

	/** Text can't hold markup characters as they are. */
	FString EscapeMarkup(const FString& Text)
	{
		return Text.Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;")).Replace(TEXT("\""), TEXT("&quot;"));
	}

	bool IsWebLink(const FString& Link)
	{
		return Link.Contains(TEXT("://")) || Link.StartsWith(TEXT("mailto:")) || Link.StartsWith(TEXT("//"));
	}

	/** Anchors as headings make them: lower case, and spaces written %20 in links count as spaces. */
	FString NormalizeAnchor(const FString& Anchor)
	{
		FString Normalized = Anchor;
		Normalized.RemoveFromStart(TEXT("#"));
		return Normalized.Replace(TEXT("%20"), TEXT(" ")).ToLower();
	}

	/** Splits "3. Quick start" into 3 and Quick start. Headings without a number keep their whole text as the name. */
	void SplitNumber(const FString& Title, FString& OutNumber, FString& OutName)
	{
		int32 Digits = 0;
		while (Digits < Title.Len() && FChar::IsDigit(Title[Digits]))
		{
			++Digits;
		}
		if (Digits > 0 && Digits + 1 < Title.Len() && Title[Digits] == TEXT('.') && FChar::IsWhitespace(Title[Digits + 1]))
		{
			OutNumber = Title.Left(Digits);
			OutName = Title.RightChop(Digits + 1).TrimStartAndEnd();
			return;
		}
		OutNumber.Reset();
		OutName = Title;
	}

	/** How much room a table column gets: enough for its longest word, more for long text. */
	float ColumnWeight(const FBlock& Table, int32 Column)
	{
		int32 Longest = 0;
		int32 LongestWord = 0;
		auto Measure = [&Longest, &LongestWord](const FTableCell& Cell)
		{
			const FString Text = ToPlainText(Cell);
			Longest = FMath::Max(Longest, Text.Len());
			TArray<FString> Words;
			Text.ParseIntoArrayWS(Words);
			for (const FString& Word : Words)
			{
				LongestWord = FMath::Max(LongestWord, Word.Len());
			}
		};
		if (Table.Header.IsValidIndex(Column))
		{
			Measure(Table.Header[Column]);
		}
		for (const TArray<FTableCell>& Row : Table.Rows)
		{
			if (Row.IsValidIndex(Column))
			{
				Measure(Row[Column]);
			}
		}
		return static_cast<float>(FMath::Clamp(FMath::Max(LongestWord, FMath::Min(Longest, 60)), 4, 80));
	}

	/** Calls Visitor for the block and everything nested in it, in the order they're read. */
	void VisitBlock(const FBlock& Block, TFunctionRef<void(const FBlock&)> Visitor)
	{
		Visitor(Block);
		ForEachBlock(Block.Children, Visitor);
	}

	/** A block's own text, without what's nested in it: its runs, its table's cells, or its code. */
	FString BlockText(const FBlock& Block)
	{
		FString Text = ToPlainText(Block.Runs);
		for (const FTableCell& Cell : Block.Header)
		{
			Text += TEXT(" ") + ToPlainText(Cell);
		}
		for (const TArray<FTableCell>& Row : Block.Rows)
		{
			for (const FTableCell& Cell : Row)
			{
				Text += TEXT(" ") + ToPlainText(Cell);
			}
		}
		if (Block.Type == EBlockType::Code)
		{
			Text += TEXT(" ") + Block.Text;
		}
		return Text;
	}

	/** An image drawn as large as it is, or narrower to fit the column, keeping its shape, in the middle of the column. */
	class SDocumentationImage : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDocumentationImage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<FInventoryDocumentationImage>& InImage)
		{
			Image = InImage;
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			const FVector2D Natural(Image->Size);
			if (Natural.X <= 0.0 || Natural.Y <= 0.0)
			{
				return FVector2D::ZeroVector;
			}
			const double Width = (AvailableWidth > 0.0) ? FMath::Min(Natural.X, AvailableWidth) : Natural.X;
			return FVector2D(Width, Natural.Y * Width / Natural.X);
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			// Its height follows its width, which is only known once it's laid out.
			const double Width = FVector2D(AllottedGeometry.GetLocalSize()).X;
			if (!FMath::IsNearlyEqual(Width, AvailableWidth, 0.5))
			{
				AvailableWidth = Width;
				Invalidate(EInvalidateWidgetReason::Layout);
			}
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const FVector2D Natural(Image->Size);
			const FVector2D Area(AllottedGeometry.GetLocalSize());
			if (Natural.X <= 0.0 || Area.X <= 0.0)
			{
				return LayerId;
			}
			const FVector2D Size = Natural * FMath::Min(1.0, Area.X / Natural.X);
			const FVector2D Offset((Area.X - Size.X) * 0.5, 0.0);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Offset)), &Image->Brush);
			return LayerId;
		}

	private:
		TSharedPtr<FInventoryDocumentationImage> Image;
		double AvailableWidth = 0.0;
	};

	/**
	 * How wide the longest stretch of a table cell's text that can't wrap is, in the fonts it's drawn in, such as a long piece of code like
	 * UInventoryComponent::FindInventoryComponent(Actor). Header cells are bold.
	 */
	float MeasureLongestWord(const FTableCell& Cell, bool bHeader)
	{
		if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			return 0.0f;
		}
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FSlateStyleSet& Styles = GetTextStyles();
		float Longest = 0.0f;
		float Word = 0.0f;
		for (const FInlineRun& Run : Cell)
		{
			const EInlineStyle Style = bHeader ? (Run.Style | EInlineStyle::Bold) : Run.Style;
			const FSlateFontInfo& Font = Styles.GetWidgetStyle<FTextBlockStyle>(StyleName(0, Style, false)).Font;
			const FString Text = Run.bImage ? FString::Printf(TEXT("[%s]"), Run.Text.IsEmpty() ? TEXT("image") : *Run.Text) : Run.Text;

			// A word can run on from one run into the next, such as a bold name and the comma after it.
			int32 Start = 0;
			for (int32 Index = 0; Index <= Text.Len(); ++Index)
			{
				const bool bEnd = Index == Text.Len();
				if (!bEnd && !FChar::IsWhitespace(Text[Index]))
				{
					continue;
				}
				if (Index > Start)
				{
					Word += static_cast<float>(FontMeasure->Measure(Text.Mid(Start, Index - Start), Font).X);
				}
				if (!bEnd)
				{
					Longest = FMath::Max(Longest, Word);
					Word = 0.0f;
				}
				Start = Index + 1;
			}
		}
		return FMath::Max(Longest, Word);
	}

	/**
	 * A table's cells, laid out as a grid as wide as the table is given. Every column gets at least the room its longest word needs, so
	 * code that can't wrap isn't cut off, and the rest of the room is shared by how much text each column holds. Each row is as tall as
	 * its tallest cell. Cells go in row by row.
	 */
	class SDocumentationTable : public SPanel
	{
	public:
		SLATE_BEGIN_ARGS(SDocumentationTable) {}
		SLATE_END_ARGS()

		SDocumentationTable()
			: Children(this)
		{
		}

		void Construct(const FArguments& InArgs, const TArray<float>& InMinWidths, const TArray<float>& InWeights)
		{
			MinWidths = InMinWidths;
			Weights = InWeights;
			Weights.SetNumZeroed(MinWidths.Num());
		}

		void AddCell(const TSharedRef<SWidget>& Cell)
		{
			Children.Add(Cell);
			Cells.Add(Cell);
		}

		virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override
		{
			const int32 Columns = MinWidths.Num();
			if (Columns == 0)
			{
				return;
			}

			const TArray<float> Widths = FitColumns(static_cast<float>(FVector2D(AllottedGeometry.GetLocalSize()).X));
			float Top = 0.0f;
			for (int32 Row = 0; Row * Columns < Cells.Num(); ++Row)
			{
				const float Height = GetRowHeight(Row);
				float Left = 0.0f;
				for (int32 Column = 0; Column < Columns && Cells.IsValidIndex(Row * Columns + Column); ++Column)
				{
					ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(Cells[Row * Columns + Column], FVector2D(Left, Top), FVector2D(Widths[Column], Height)));
					Left += Widths[Column];
				}
				Top += Height;
			}
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			const int32 Columns = MinWidths.Num();
			float Width = 0.0f;
			for (const float MinWidth : MinWidths)
			{
				Width += MinWidth;
			}
			float Height = 0.0f;
			for (int32 Row = 0; Columns > 0 && Row * Columns < Cells.Num(); ++Row)
			{
				Height += GetRowHeight(Row);
			}
			return FVector2D(Width, Height);
		}

		virtual FChildren* GetChildren() override
		{
			return &Children;
		}

	private:
		float GetRowHeight(int32 Row) const
		{
			const int32 Columns = MinWidths.Num();
			float Height = 0.0f;
			for (int32 Column = 0; Column < Columns && Cells.IsValidIndex(Row * Columns + Column); ++Column)
			{
				Height = FMath::Max(Height, static_cast<float>(FVector2D(Cells[Row * Columns + Column]->GetDesiredSize()).Y));
			}
			return Height;
		}

		/**
		 * Each column's share of Width by its weight. A column whose share is less than it needs gets what it needs, and the others share
		 * what's left again, until every column has enough. Columns that need more than Width between them get what they need and stick out.
		 */
		TArray<float> FitColumns(float Width) const
		{
			const int32 Columns = MinWidths.Num();
			TArray<float> Widths = MinWidths;
			TArray<bool> AtMinimum;
			AtMinimum.Init(false, Columns);

			bool bChanged = true;
			while (bChanged)
			{
				bChanged = false;
				float Room = Width;
				float WeightTotal = 0.0f;
				for (int32 Column = 0; Column < Columns; ++Column)
				{
					if (AtMinimum[Column])
					{
						Room -= MinWidths[Column];
					}
					else
					{
						WeightTotal += Weights[Column];
					}
				}

				for (int32 Column = 0; Column < Columns; ++Column)
				{
					if (AtMinimum[Column])
					{
						continue;
					}
					const float Share = WeightTotal > 0.0f ? FMath::Max(Room, 0.0f) * Weights[Column] / WeightTotal : 0.0f;
					if (Share < MinWidths[Column])
					{
						AtMinimum[Column] = true;
						bChanged = true;
					}
					Widths[Column] = FMath::Max(Share, MinWidths[Column]);
				}
			}
			return Widths;
		}

		TSlotlessChildren<SWidget> Children;

		/** The same cells, to lay out. */
		TArray<TSharedRef<SWidget>> Cells;

		TArray<float> MinWidths;
		TArray<float> Weights;
	};

	/** A code block's code as rich text, each piece in the style of what it is. Markup can't span lines, so each line of a piece is tagged on its own. */
	FString MakeCodeMarkup(const FBlock& Code)
	{
		FString Markup;
		for (const FCodeSpan& Span : HighlightCode(Code.Text, Code.Info))
		{
			const FString Style = CodeStyleName(Span.Token).ToString();
			TArray<FString> Lines;
			Span.Text.ParseIntoArray(Lines, TEXT("\n"), false);
			for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
			{
				if (LineIndex > 0)
				{
					Markup += TEXT("\n");
				}
				if (Lines[LineIndex].IsEmpty())
				{
					continue;
				}
				const FString Escaped = EscapeMarkup(Lines[LineIndex]);
				Markup += Span.Token == ECodeToken::Plain ? Escaped : FString::Printf(TEXT("<%s>%s</>"), *Style, *Escaped);
			}
		}
		return Markup;
	}

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
	{
		const TSharedRef<SInventoryDocumentation> Window = SNew(SInventoryDocumentation);
		OpenWindow = Window;
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.Label(LOCTEXT("TabLabel", "Modular Inventory Docs"))
			[
				Window
			];
	}
}

// Tab
void SInventoryDocumentation::RegisterTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(InventoryDocumentation::TabName(), FOnSpawnTab::CreateStatic(&InventoryDocumentation::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Modular Inventory Docs"))
		.SetTooltipText(LOCTEXT("TabTooltip", "The Modular Inventory guide: setting it up, every asset and component, the UI, the editor tools and troubleshooting."))
		.SetIcon(FSlateIcon(FInventoryEditorStyle::GetStyleSetName(), "ModularInventory.Icon"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void SInventoryDocumentation::UnregisterTab()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(InventoryDocumentation::TabName());
	}
	InventoryDocumentation::TextStyles.Reset();
}

void SInventoryDocumentation::Open(const FString& Anchor)
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(InventoryDocumentation::TabName()));
	const TSharedPtr<SInventoryDocumentation> Window = InventoryDocumentation::OpenWindow.Pin();
	if (Window.IsValid() && !Anchor.IsEmpty())
	{
		Window->ShowAnchor(Anchor);
	}
}

// Construction
void SInventoryDocumentation::Construct(const FArguments& InArgs)
{
	using namespace InventoryDocumentation;

	ChildSlot
	[
		SNew(SVerticalBox)

		// The header: the plugin, its version, going back, and search.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FStyleColors::Header)
			.Padding(FMargin(16.0f, 10.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FInventoryEditorStyle::Get().GetBrush("ModularInventory.Logo"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "Modular Inventory"))
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SInventoryDocumentation::GetVersionText)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNullWidget::NullWidget
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FlatButtonStyle())
					.ContentPadding(FMargin(10.0f, 5.0f))
					.IsEnabled(this, &SInventoryDocumentation::CanGoBack)
					.ToolTipText(LOCTEXT("BackTooltip", "Goes back to the page you were reading before. The mouse's back button does too."))
					.OnClicked(this, &SInventoryDocumentation::HandleBackClicked)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("Back", "{0}  Back"), FText::FromString(TEXT("\u2190"))))
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(300.0f)
					[
						SAssignNew(SearchBox, SSearchBox)
						.HintText(LOCTEXT("SearchHint", "Search the guide"))
						.ToolTipText(LOCTEXT("SearchTooltip", "Finds the chapters and sections that mention every word you type. Enter opens the first one."))
						.OnTextChanged(this, &SInventoryDocumentation::HandleSearchChanged)
						.OnTextCommitted(this, &SInventoryDocumentation::HandleSearchCommitted)
					]
				]
			]
		]

		// The contents beside the page.
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.24f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FStyleColors::Panel)
				.Padding(FMargin(8.0f, 10.0f))
				[
					SAssignNew(SidebarScroll, SScrollBox)
					.AnimateWheelScrolling(false)
					.AllowOverscroll(EAllowOverscroll::No)
					+ SScrollBox::Slot()
					[
						SAssignNew(SidebarArea, SBox)
					]
				]
			]
			+ SSplitter::Slot()
			.Value(0.76f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FStyleColors::Recessed)
				.Padding(0.0f)
				[
					// The mouse wheel moves the page at once, without easing or bouncing at the ends.
					SAssignNew(DocumentScroll, SScrollBox)
					.AnimateWheelScrolling(false)
					.AllowOverscroll(EAllowOverscroll::No)
					+ SScrollBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(36.0f, 28.0f)
					[
						SNew(SBox)
						.WidthOverride(ColumnWidth)
						[
							SAssignNew(PageStack, SOverlay)
						]
					]
				]
			]
		]
	];

	RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SInventoryDocumentation::CheckForChanges));

	Load();

	// Back where the window was last left, or at the start.
	FString LastAnchor;
	GConfig->GetString(ConfigSection, TEXT("Anchor"), LastAnchor, GEditorPerProjectIni);
	const int32 LastPage = LastAnchor.IsEmpty() ? INDEX_NONE : FindPageOfAnchor(LastAnchor);
	ShowPage(LastPage != INDEX_NONE ? LastPage : 0, FString(), -1.0f, true);
}

SInventoryDocumentation::~SInventoryDocumentation()
{
	FTSTicker::RemoveTicker(PagesTicker);
}

// Reading the guide
FString SInventoryDocumentation::GetGuidePath() const
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ModularInventory"));
	return Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("README.md")) : FString();
}

void SInventoryDocumentation::Load()
{
	const FString Guide = GetGuidePath();
	FString Text;
	bGuideUnreadable = Guide.IsEmpty() || !FFileHelper::LoadFileToString(Text, *Guide);
	GuideTimestamp = Guide.IsEmpty() ? FDateTime() : IFileManager::Get().GetTimeStamp(*Guide);
	if (bGuideUnreadable)
	{
		Text = LOCTEXT("Unreadable", "# Modular Inventory\n\nThe guide, README.md in the plugin's folder, can't be read.").ToString();
	}

	Document = InventoryMarkdown::Parse(Text);
	BuildPages();
	BuildSections();
}

void SInventoryDocumentation::BuildPages()
{
	using namespace InventoryDocumentation;

	Pages.Reset();
	FPage& Introduction = Pages.AddDefaulted_GetRef();
	Introduction.Name = LOCTEXT("Introduction", "Introduction").ToString();
	Introduction.Title = Introduction.Name;

	// A chapter starts at each second-level heading. A Contents chapter is left out, as the window lists the chapters itself.
	bool bSkipping = false;
	for (int32 Index = 0; Index < Document.Blocks.Num(); ++Index)
	{
		const FBlock& Block = Document.Blocks[Index];
		if (Block.Type == EBlockType::Heading && Block.Level == 2)
		{
			bSkipping = Block.Anchor == TEXT("contents") || Block.Anchor == TEXT("table-of-contents");
			if (!bSkipping)
			{
				FPage& Page = Pages.AddDefaulted_GetRef();
				Page.Title = ToPlainText(Block.Runs);
				Page.Anchor = Block.Anchor;
				Page.FirstBlock = Index;
				Page.EndBlock = Index + 1;
				SplitNumber(Page.Title, Page.Number, Page.Name);
			}
			continue;
		}
		if (!bSkipping)
		{
			Pages.Last().EndBlock = Index + 1;
		}
	}
}

void SInventoryDocumentation::BuildSections()
{
	using namespace InventoryDocumentation;

	// Each chapter, and each of its third-level sections, with the text under it up to the next of either.
	Sections.Reset();
	for (int32 PageIndex = 0; PageIndex < Pages.Num(); ++PageIndex)
	{
		const FPage& Page = Pages[PageIndex];
		int32 Current = Sections.Num();
		FSection& Chapter = Sections.AddDefaulted_GetRef();
		Chapter.Page = PageIndex;
		Chapter.Level = 2;
		Chapter.Title = Page.Title;
		Chapter.Anchor = Page.Anchor;

		for (int32 Index = Page.FirstBlock; Index < Page.EndBlock; ++Index)
		{
			const FBlock& Top = Document.Blocks[Index];
			if (Top.Type == EBlockType::Heading && Top.Level == 3)
			{
				Current = Sections.Num();
				FSection& Section = Sections.AddDefaulted_GetRef();
				Section.Page = PageIndex;
				Section.Level = 3;
				Section.Title = ToPlainText(Top.Runs);
				Section.Anchor = Top.Anchor;
				continue;
			}
			if (Index == Page.FirstBlock && Top.Type == EBlockType::Heading)
			{
				continue;
			}

			VisitBlock(Top, [this, Current](const FBlock& Block)
			{
				const FString Text = BlockText(Block);
				if (!Text.IsEmpty())
				{
					FString& Into = Sections[Current].Text;
					Into += Into.IsEmpty() ? Text : TEXT(" ") + Text;
				}
			});
		}
	}

	for (FSection& Section : Sections)
	{
		Section.SearchText = (Section.Title + TEXT(" ") + Section.Text).ToLower();
	}
}

void SInventoryDocumentation::Reload()
{
	// The same chapter by its anchor, as chapters may have been added or moved.
	const FString Anchor = Pages.IsValidIndex(CurrentPage) ? Pages[CurrentPage].Anchor : FString();
	const float Offset = DocumentScroll.IsValid() ? DocumentScroll->GetScrollOffset() : 0.0f;

	Load();
	const int32 Page = FindPageOfAnchor(Anchor);
	ShowPage(Page != INDEX_NONE ? Page : 0, FString(), Offset, true);
}

// Showing a page
void SInventoryDocumentation::ShowAnchor(const FString& Anchor)
{
	const int32 Page = FindPageOfAnchor(Anchor);
	if (Page != INDEX_NONE)
	{
		RequestPage(Page, Anchor);
	}
}

void SInventoryDocumentation::RequestPage(int32 PageIndex, const FString& Anchor, bool bRemember)
{
	RequestedPage = PageIndex;
	RequestedAnchor = Anchor;
	bRememberRequest = bRemember;
	StartTickingPages();
}

void SInventoryDocumentation::StartTickingPages()
{
	// The core ticker runs before Slate lays the window out, so a page built here is measured before it's first drawn.
	if (!PagesTicker.IsValid())
	{
		PagesTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SInventoryDocumentation::TickPages));
	}
}

bool SInventoryDocumentation::TickPages(float DeltaTime)
{
	if (RequestedPage != INDEX_NONE)
	{
		const int32 Page = RequestedPage;
		const FString Anchor = RequestedAnchor;
		RequestedPage = INDEX_NONE;
		RequestedAnchor.Reset();

		if (Pages.IsValidIndex(Page))
		{
			// Remembered so Back returns here, unless only moving within the page.
			if (bRememberRequest && Page != CurrentPage)
			{
				FVisit& Visit = History.AddDefaulted_GetRef();
				Visit.Page = CurrentPage;
				Visit.ScrollOffset = DocumentScroll.IsValid() ? DocumentScroll->GetScrollOffset() : 0.0f;
			}
			ShowPage(Page, Anchor);
		}
	}
	else if (IncomingPage.IsValid())
	{
		if (--IncomingFramesLeft <= 0)
		{
			FinishPageChange();
		}
	}

	const bool bKeepTicking = RequestedPage != INDEX_NONE || IncomingPage.IsValid();
	if (!bKeepTicking)
	{
		PagesTicker.Reset();
	}
	return bKeepTicking;
}

void SInventoryDocumentation::ShowPage(int32 PageIndex, const FString& Anchor, float ScrollOffset, bool bRebuild)
{
	const int32 NewPage = Pages.IsValidIndex(PageIndex) ? PageIndex : 0;

	// A chapter's own heading is the top of its page.
	const FString Target = InventoryDocumentation::NormalizeAnchor(Anchor);
	PendingAnchor = Target == Pages[NewPage].Anchor ? FString() : Target;
	PendingScrollOffset = ScrollOffset;

	// The page on screen only jumps there, and one still settling jumps there once it's shown.
	const bool bAlreadyThere = NewPage == CurrentPage && (ShownPage.IsValid() || IncomingPage.IsValid());
	if (bAlreadyThere && !bRebuild)
	{
		if (!IncomingPage.IsValid())
		{
			ScrollToPending(false);
		}
		return;
	}

	CurrentPage = NewPage;
	GConfig->SetString(InventoryDocumentation::ConfigSection, TEXT("Anchor"), *Pages[CurrentPage].Anchor, GEditorPerProjectIni);

	// Drawn twice unseen first: wrapped text learns its width from one frame to the next, and images take their size the same way.
	RebuildPage();
	RebuildSidebar();
	IncomingFramesLeft = 2;
	StartTickingPages();
}

void SInventoryDocumentation::FinishPageChange()
{
	if (!IncomingPage.IsValid() || !PageStack.IsValid())
	{
		return;
	}

	if (ShownPage.IsValid())
	{
		PageStack->RemoveSlot(ShownPage.ToSharedRef());
	}
	ShownPage = IncomingPage;
	IncomingPage.Reset();
	ShownPage->SetRenderOpacity(1.0f);
	ShownPage->SetVisibility(EVisibility::SelfHitTestInvisible);

	Links = MoveTemp(IncomingLinks);
	AnchorWidgets = MoveTemp(IncomingAnchorWidgets);
	IncomingLinks.Reset();
	IncomingAnchorWidgets.Reset();

	ScrollToPending(false);
}

void SInventoryDocumentation::ScrollToPending(bool bAnimate)
{
	// Straight there, never animated: the page is long, and a smooth scroll over it judders.
	if (DocumentScroll.IsValid())
	{
		const TSharedRef<SWidget>* Heading = PendingAnchor.IsEmpty() ? nullptr : AnchorWidgets.Find(PendingAnchor);
		if (Heading)
		{
			DocumentScroll->ScrollDescendantIntoView(*Heading, /*bAnimateScroll*/ bAnimate, EDescendantScrollDestination::TopOrLeft, 12.0f);
		}
		else if (PendingScrollOffset >= 0.0f)
		{
			DocumentScroll->SetScrollOffset(PendingScrollOffset);
		}
		else
		{
			DocumentScroll->ScrollToStart();
		}
	}
	PendingAnchor.Reset();
	PendingScrollOffset = -1.0f;
}

int32 SInventoryDocumentation::FindPageOfAnchor(const FString& Anchor) const
{
	const FString Target = InventoryDocumentation::NormalizeAnchor(Anchor);
	if (Target.IsEmpty())
	{
		return INDEX_NONE;
	}

	for (int32 PageIndex = 0; PageIndex < Pages.Num(); ++PageIndex)
	{
		const FPage& Page = Pages[PageIndex];
		bool bFound = Page.Anchor == Target;
		for (int32 Index = Page.FirstBlock; !bFound && Index < Page.EndBlock; ++Index)
		{
			InventoryDocumentation::VisitBlock(Document.Blocks[Index], [&bFound, &Target](const FBlock& Block)
			{
				bFound |= Block.Type == InventoryMarkdown::EBlockType::Heading && Block.Anchor == Target;
			});
		}
		if (bFound)
		{
			return PageIndex;
		}
	}
	return INDEX_NONE;
}

void SInventoryDocumentation::RebuildPage()
{
	using namespace InventoryDocumentation;

	// The page on screen keeps its links and headings until this one takes its place.
	TArray<FString> ShownLinks = MoveTemp(Links);
	TMap<FString, TSharedRef<SWidget>> ShownAnchorWidgets = MoveTemp(AnchorWidgets);
	Links.Reset();
	AnchorWidgets.Reset();

	const FPage& Page = Pages[CurrentPage];
	const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	int32 First = Page.FirstBlock;
	if (First < Page.EndBlock && Document.Blocks[First].Type == EBlockType::Heading && Document.Blocks[First].Level == 2)
	{
		Body->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 18.0f)
			[
				MakePageTitle(Document.Blocks[First], Page)
			];
		++First;
	}

	// The rules between the guide's chapters only separate them, which a page of its own already does.
	int32 End = Page.EndBlock;
	while (End > First && Document.Blocks[End - 1].Type == EBlockType::Rule)
	{
		--End;
	}
	while (First < End && Document.Blocks[First].Type == EBlockType::Rule)
	{
		++First;
	}
	if (First < End)
	{
		AddBlocks(Body, &Document.Blocks[First], End - First, 0);
	}

	// The introduction leads into the chapters.
	if (CurrentPage == 0 && Pages.Num() > 1)
	{
		Body->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 20.0f, 0.0f, 0.0f)
			[
				MakeChapterCards()
			];
	}

	Body->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 36.0f, 0.0f, 12.0f)
		[
			MakePageFooter()
		];

	IncomingLinks = MoveTemp(Links);
	IncomingAnchorWidgets = MoveTemp(AnchorWidgets);
	Links = MoveTemp(ShownLinks);
	AnchorWidgets = MoveTemp(ShownAnchorWidgets);

	// Unseen and out of the mouse's way behind the page on screen, replacing a page that was still settling.
	if (IncomingPage.IsValid())
	{
		PageStack->RemoveSlot(IncomingPage.ToSharedRef());
	}
	IncomingPage = Body;
	Body->SetRenderOpacity(0.0f);
	Body->SetVisibility(EVisibility::HitTestInvisible);
	PageStack->AddSlot()
	[
		Body
	];
}

void SInventoryDocumentation::AddBlocks(const TSharedRef<SVerticalBox>& Box, const FBlock* Blocks, int32 Count, int32 Depth)
{
	const float Spacing = Depth == 0 ? 10.0f : 4.0f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FBlock& Block = Blocks[Index];

		// Headings get more room above than below, so they sit with their section.
		float Above = 0.0f;
		if (Block.Type == InventoryMarkdown::EBlockType::Heading && Index > 0)
		{
			Above = Block.Level <= 3 ? 22.0f : 12.0f;
		}
		Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, Above, 0.0f, Spacing)
			[
				MakeBlock(Block, Depth)
			];
	}
}

TSharedRef<SWidget> SInventoryDocumentation::MakeBlock(const FBlock& Block, int32 Depth)
{
	using InventoryMarkdown::EBlockType;

	switch (Block.Type)
	{
	case EBlockType::Heading:
		return MakeHeading(Block);
	case EBlockType::List:
		return MakeList(Block, Depth);
	case EBlockType::Table:
		return MakeTable(Block);
	case EBlockType::Code:
		return MakeCode(Block);
	case EBlockType::Quote:
		return MakeQuote(Block, Depth);
	case EBlockType::Rule:
		return SNew(SBox)
			.Padding(FMargin(0.0f, 8.0f))
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			];
	case EBlockType::Image:
		return MakeImage(Block);
	case EBlockType::Paragraph:
	case EBlockType::ListItem:
	default:
		return MakeText(Block.Runs, 0);
	}
}

TSharedRef<SWidget> SInventoryDocumentation::MakePageTitle(const FBlock& Heading, const FPage& Page)
{
	using namespace InventoryDocumentation;

	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (!Page.Number.IsEmpty())
	{
		Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ChapterLabel", "CHAPTER {0}"), FText::FromString(Page.Number)))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))
				.ColorAndOpacity(FStyleColors::AccentBlue)
			];
	}

	Box->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Page.Name))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), TextSizes[2]))
			.AutoWrapText(true)
			.HighlightText(this, &SInventoryDocumentation::GetHighlightText)
		];

	// A short bar under the title, in the accent color.
	Box->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.0f, 10.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(56.0f)
			.HeightOverride(3.0f)
			[
				SNew(SBorder)
				.BorderImage(RoundedBrush())
				.BorderBackgroundColor(FStyleColors::AccentBlue)
			]
		];

	const TSharedRef<SWidget> Widget = Box;
	AnchorWidgets.Add(Heading.Anchor, Widget);
	return Widget;
}

TSharedRef<SWidget> SInventoryDocumentation::MakeHeading(const FBlock& Heading)
{
	const int32 Scale = FMath::Clamp(Heading.Level, 1, 6);
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeText(Heading.Runs, Scale)
		];

	// The guide's title, and chapter titles that aren't a page's own, get a line under them as on GitHub.
	if (Heading.Level <= 2)
	{
		Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			];
	}

	const TSharedRef<SWidget> Widget = Box;
	AnchorWidgets.Add(Heading.Anchor, Widget);
	return Widget;
}

TSharedRef<SWidget> SInventoryDocumentation::MakeList(const FBlock& List, int32 Depth)
{
	using namespace InventoryDocumentation;

	const TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
	int32 Number = List.Start;
	for (const FBlock& Item : List.Children)
	{
		TSharedRef<SWidget> Marker = SNullWidget::NullWidget;
		if (Item.bTask)
		{
			Marker = SNew(SCheckBox)
				.IsChecked(Item.bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.IsEnabled(false);
		}
		else
		{
			const FString Label = List.bOrdered ? FString::Printf(TEXT("%d."), Number) : FString(Depth % 2 == 0 ? TEXT("\u2022") : TEXT("\u25E6"));
			Marker = SNew(STextBlock)
				.Text(FText::FromString(Label))
				.TextStyle(&BodyStyle(List.bOrdered))
				.ColorAndOpacity(List.bOrdered ? FSlateColor::UseForeground() : FSlateColor(FStyleColors::AccentBlue));
		}
		++Number;

		const TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
		if (Item.Children.Num() > 0)
		{
			AddBlocks(Content, Item.Children.GetData(), Item.Children.Num(), Depth + 1);
		}

		Items->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(Depth == 0 ? 6.0f : 0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBox)
					.MinDesiredWidth(List.bOrdered ? 22.0f : 12.0f)
					.HAlign(HAlign_Right)
					[
						Marker
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					Content
				]
			];
	}
	return Items;
}

TSharedRef<SWidget> SInventoryDocumentation::MakeTable(const FBlock& Table)
{
	using namespace InventoryDocumentation;

	// Each column needs at least the room of its longest word, which can't wrap, with the cell's padding and the grid line around it.
	const int32 Columns = Table.Alignments.Num();
	TArray<float> MinWidths;
	TArray<float> Weights;
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		float Longest = Table.Header.IsValidIndex(Column) ? MeasureLongestWord(Table.Header[Column], true) : 0.0f;
		for (const TArray<FTableCell>& Row : Table.Rows)
		{
			if (Row.IsValidIndex(Column))
			{
				Longest = FMath::Max(Longest, MeasureLongestWord(Row[Column], false));
			}
		}
		MinWidths.Add(FMath::CeilToFloat(Longest) + TableCellPadding.Left + TableCellPadding.Right + 2.0f * TableLine + 2.0f);
		Weights.Add(ColumnWeight(Table, Column));
	}
	const TSharedRef<SDocumentationTable> Grid = SNew(SDocumentationTable, MinWidths, Weights);

	// The gaps between cells show the border's color behind them as grid lines.
	auto AddCell = [this, &Grid, &Table](int32 CellColumn, int32 CellRow, const FTableCell& Cell)
	{
		const EColumnAlignment Alignment = Table.Alignments[CellColumn];
		const ETextJustify::Type Justification = (Alignment == EColumnAlignment::Center) ? ETextJustify::Center
			: ((Alignment == EColumnAlignment::Right) ? ETextJustify::Right : ETextJustify::Left);
		const bool bHeader = CellRow == 0;
		const FSlateColor Background = bHeader ? FSlateColor(FStyleColors::Header)
			: ((CellRow % 2 == 0) ? FSlateColor(FStyleColors::Panel) : FSlateColor(FStyleColors::Background));
		Grid->AddCell(
			SNew(SBox)
			.Padding(TableLine)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(Background)
				.Padding(TableCellPadding)
				[
					MakeText(Cell, 0, bHeader, Justification)
				]
			]);
	};

	for (int32 Column = 0; Column < Columns; ++Column)
	{
		AddCell(Column, 0, Table.Header.IsValidIndex(Column) ? Table.Header[Column] : FTableCell());
	}
	for (int32 Row = 0; Row < Table.Rows.Num(); ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			AddCell(Column, Row + 1, Table.Rows[Row].IsValidIndex(Column) ? Table.Rows[Row][Column] : FTableCell());
		}
	}

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FStyleColors::Hover)
		.Padding(TableLine)
		[
			Grid
		];
}

TSharedRef<SWidget> SInventoryDocumentation::MakeCode(const FBlock& Code)
{
	using namespace InventoryDocumentation;

	// A header with the language, if the fence names one, and a button to copy the code, over a line.
	const FText Language = FText::FromString(InventoryMarkdown::GetLanguageName(Code.Info));
	const TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Language)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(&FlatButtonStyle())
			.ContentPadding(FMargin(8.0f, 2.0f))
			.ToolTipText(LOCTEXT("CopyTooltip", "Copies the code to the clipboard."))
			.OnClicked(this, &SInventoryDocumentation::HandleCopyClicked, Code.Text)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Copy", "Copy"))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];

	// Colored as a code editor colors it, and selectable, to copy part of it. Long lines scroll sideways, as on GitHub, without taking the
	// mouse wheel from the page.
	const FSlateStyleSet& Styles = GetTextStyles();
	return SNew(SBorder)
		.BorderImage(CodeBrush())
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(14.0f, 5.0f, 6.0f, 5.0f))
			[
				Header
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(1.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(CodeOutline)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(14.0f, 10.0f, 14.0f, 12.0f))
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				.ConsumeMouseWheel(EConsumeMouseWheel::Never)
				+ SScrollBox::Slot()
				[
					SNew(SMultiLineEditableText)
					.Text(FText::FromString(MakeCodeMarkup(Code)))
					.Marshaller(FRichTextLayoutMarshaller::Create(TArray<TSharedRef<ITextDecorator>>(), &Styles))
					.TextStyle(&Styles.GetWidgetStyle<FTextBlockStyle>(CodeStyleName(ECodeToken::Plain)))
					.IsReadOnly(true)
				]
			]
		];
}

TSharedRef<SWidget> SInventoryDocumentation::MakeQuote(const FBlock& Quote, int32 Depth)
{
	using namespace InventoryDocumentation;

	// Alerts, such as > [!NOTE], are tinted by their kind and named above their text, as on GitHub. Plain quotes only get the bar.
	const bool bAlert = !Quote.Info.IsEmpty();
	const FLinearColor Color = AlertColor(Quote.Info);

	const TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (bAlert)
	{
		Content->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Quote.Info))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
				.ColorAndOpacity(Color)
			];
	}
	if (Quote.Children.Num() > 0)
	{
		AddBlocks(Content, Quote.Children.GetData(), Quote.Children.Num(), Depth + 1);
	}

	return SNew(SBorder)
		.BorderImage(RoundedBrush())
		.BorderBackgroundColor(bAlert ? Color.CopyWithNewOpacity(0.1f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.03f))
		.Padding(0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(3.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(Color)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(14.0f, 10.0f, 12.0f, 4.0f)
			[
				Content
			]
		];
}

TSharedRef<SWidget> SInventoryDocumentation::MakeImage(const FBlock& Image)
{
	using namespace InventoryDocumentation;

	const TSharedPtr<FInventoryDocumentationImage> Loaded = FindImage(Image.Text);
	if (!Loaded.IsValid() || !Loaded->Texture.IsValid())
	{
		// Web images aren't fetched.
		return SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("NoImage", "[Image: {0}]"), FText::FromString(Image.Info.IsEmpty() ? Image.Text : Image.Info)))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true);
	}

	// The alt text, which describes the picture, goes under it as a caption.
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(InventoryDocumentation::SDocumentationImage, Loaded.ToSharedRef())
			.ToolTipText(FText::FromString(Image.Info))
		];
	if (!Image.Info.IsEmpty())
	{
		Box->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(24.0f, 8.0f, 24.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Image.Info))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 9))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			];
	}
	return Box;
}

TSharedRef<SWidget> SInventoryDocumentation::MakeChapterCards()
{
	using namespace InventoryDocumentation;

	const TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel)
		.SlotPadding(FMargin(4.0f));

	// Every chapter, by number and name, two to a row.
	for (int32 PageIndex = 1; PageIndex < Pages.Num(); ++PageIndex)
	{
		const FPage& Page = Pages[PageIndex];
		const int32 Cell = PageIndex - 1;
		Grid->AddSlot(Cell % 2, Cell / 2)
		[
			SNew(SBorder)
			.BorderImage(PanelBrush())
			.Padding(0.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FlatButtonStyle())
				.ContentPadding(FMargin(14.0f, 10.0f))
				.OnClicked(this, &SInventoryDocumentation::HandlePageClicked, PageIndex)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SBox)
						.MinDesiredWidth(22.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Page.Number))
							.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
							.ColorAndOpacity(FStyleColors::AccentBlue)
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Page.Name))
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))
						.AutoWrapText(true)
					]
				]
			]
		];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("InThisGuide", "In this guide"))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), TextSizes[3]))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Grid
		];
}

TSharedRef<SWidget> SInventoryDocumentation::MakePageFooter()
{
	const bool bHasPrevious = Pages.IsValidIndex(CurrentPage - 1);
	const bool bHasNext = Pages.IsValidIndex(CurrentPage + 1);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				bHasPrevious ? MakeNeighbourCard(CurrentPage - 1, false) : SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				bHasNext ? MakeNeighbourCard(CurrentPage + 1, true) : SNullWidget::NullWidget
			]
		];
}

TSharedRef<SWidget> SInventoryDocumentation::MakeNeighbourCard(int32 PageIndex, bool bNext)
{
	using namespace InventoryDocumentation;

	const FPage& Page = Pages[PageIndex];
	const EHorizontalAlignment Alignment = bNext ? HAlign_Right : HAlign_Left;
	return SNew(SBorder)
		.BorderImage(PanelBrush())
		.Padding(0.0f)
		[
			SNew(SButton)
			.ButtonStyle(&FlatButtonStyle())
			.ContentPadding(FMargin(16.0f, 10.0f))
			.OnClicked(this, &SInventoryDocumentation::HandlePageClicked, PageIndex)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(Alignment)
				[
					SNew(STextBlock)
					.Text(bNext
						? FText::Format(LOCTEXT("Next", "Next  {0}"), FText::FromString(TEXT("\u2192")))
						: FText::Format(LOCTEXT("Previous", "{0}  Previous"), FText::FromString(TEXT("\u2190"))))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(Alignment)
				.Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Page.Title))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12))
					.ColorAndOpacity(FStyleColors::AccentBlue)
				]
			]
		];
}

TSharedRef<SWidget> SInventoryDocumentation::MakeText(const TArray<FInlineRun>& Runs, int32 Scale, bool bBold, ETextJustify::Type Justification)
{
	using namespace InventoryDocumentation;

	const FSlateStyleSet& Style = GetTextStyles();
	return SNew(SRichTextBlock)
		.Text(FText::FromString(MakeMarkup(Runs, Scale, bBold)))
		.TextStyle(&Style.GetWidgetStyle<FTextBlockStyle>(StyleName(Scale, bBold ? EInlineStyle::Bold : EInlineStyle::None, false)))
		.DecoratorStyleSet(&Style)
		.AutoWrapText(true)
		.Justification(Justification)
		.LineHeightPercentage(1.2f)
		.HighlightText(this, &SInventoryDocumentation::GetHighlightText)
		+ SRichTextBlock::HyperlinkDecorator(TEXT("md"), FSlateHyperlinkRun::FOnClick::CreateSP(this, &SInventoryDocumentation::HandleLinkClicked));
}

FString SInventoryDocumentation::MakeMarkup(const TArray<FInlineRun>& Runs, int32 Scale, bool bBold)
{
	using namespace InventoryDocumentation;

	FString Markup;
	for (const FInlineRun& Run : Runs)
	{
		const EInlineStyle Style = bBold ? (Run.Style | EInlineStyle::Bold) : Run.Style;
		const bool bLink = !Run.Link.IsEmpty();
		const int32 LinkIndex = bLink ? Links.Add(Run.Link) : INDEX_NONE;
		const FString Name = StyleName(Scale, Style, bLink).ToString();
		const FString Text = Run.bImage ? FString::Printf(TEXT("[%s]"), Run.Text.IsEmpty() ? TEXT("image") : *Run.Text) : Run.Text;

		// Markup can't span lines, so each line of a run is tagged on its own.
		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			if (LineIndex > 0)
			{
				Markup += TEXT("\n");
			}
			if (Lines[LineIndex].IsEmpty())
			{
				continue;
			}
			const FString Escaped = EscapeMarkup(Lines[LineIndex]);
			if (bLink)
			{
				Markup += FString::Printf(TEXT("<a id=\"md\" style=\"%s\" link=\"%d\">%s</>"), *Name, LinkIndex, *Escaped);
			}
			else if (Style != EInlineStyle::None)
			{
				Markup += FString::Printf(TEXT("<%s>%s</>"), *Name, *Escaped);
			}
			else
			{
				Markup += Escaped;
			}
		}
	}
	return Markup;
}

// The sidebar
void SInventoryDocumentation::RebuildSidebar()
{
	if (!SidebarArea.IsValid())
	{
		return;
	}

	if (!SearchWords.IsEmpty())
	{
		SidebarArea->SetContent(MakeSearchResults());
		return;
	}

	const TSharedRef<SVerticalBox> List = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Contents", "CONTENTS"))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];

	// Every chapter, and the sections of the one that's open.
	for (int32 PageIndex = 0; PageIndex < Pages.Num(); ++PageIndex)
	{
		List->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 1.0f)
			[
				MakeChapterEntry(PageIndex)
			];

		if (PageIndex == CurrentPage)
		{
			for (const FSection& Section : Sections)
			{
				if (Section.Page == PageIndex && Section.Level == 3)
				{
					List->AddSlot()
						.AutoHeight()
						[
							MakeSectionEntry(Section)
						];
				}
			}
		}
	}
	SidebarArea->SetContent(List);
}

TSharedRef<SWidget> SInventoryDocumentation::MakeChapterEntry(int32 PageIndex)
{
	using namespace InventoryDocumentation;

	const FPage& Page = Pages[PageIndex];
	const bool bCurrent = PageIndex == CurrentPage;
	return SNew(SButton)
		.ButtonStyle(&FlatButtonStyle())
		.ContentPadding(FMargin(0.0f))
		.ToolTipText(FText::FromString(Page.Title))
		.OnClicked(this, &SInventoryDocumentation::HandlePageClicked, PageIndex)
		[
			SNew(SBorder)
			.BorderImage(bCurrent ? RoundedBrush() : NoBrush())
			.BorderBackgroundColor(FStyleColors::Header)
			.Padding(0.0f)
			[
				SNew(SHorizontalBox)
				// The open chapter is marked with a bar in the accent color.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(3.0f)
					[
						SNew(SBorder)
						.BorderImage(bCurrent ? RoundedBrush() : NoBrush())
						.BorderBackgroundColor(FStyleColors::AccentBlue)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 5.0f, 0.0f, 5.0f)
				[
					SNew(SBox)
					.MinDesiredWidth(20.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Page.Number))
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
						.ColorAndOpacity(bCurrent ? FSlateColor(FStyleColors::AccentBlue) : FSlateColor::UseSubduedForeground())
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 5.0f, 8.0f, 5.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Page.Name))
					.Font(FCoreStyle::GetDefaultFontStyle(bCurrent ? TEXT("Bold") : TEXT("Regular"), 10))
				]
			]
		];
}

TSharedRef<SWidget> SInventoryDocumentation::MakeSectionEntry(const FSection& Section)
{
	FString Number;
	FString Name;
	InventoryDocumentation::SplitNumber(Section.Title, Number, Name);
	return SNew(SButton)
		.ButtonStyle(&InventoryDocumentation::FlatButtonStyle())
		.ContentPadding(FMargin(35.0f, 3.0f, 8.0f, 3.0f))
		.ToolTipText(FText::FromString(Section.Title))
		.OnClicked(this, &SInventoryDocumentation::HandleSectionClicked, Section.Page, Section.Anchor)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Name))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SInventoryDocumentation::MakeSearchResults()
{
	// Chapters and sections with every word searched for, those with the words in their titles first.
	TArray<int32> Found;
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		const FSection& Section = Sections[Index];
		const bool bAll = Algo::AllOf(SearchWords, [&Section](const FString& Word) { return Section.SearchText.Contains(Word); });
		if (bAll)
		{
			Found.Add(Index);
		}
	}
	Found.StableSort([this](int32 A, int32 B)
	{
		auto InTitle = [this](int32 Index)
		{
			const FString Title = Sections[Index].Title.ToLower();
			return Algo::AllOf(SearchWords, [&Title](const FString& Word) { return Title.Contains(Word); });
		};
		return InTitle(A) && !InTitle(B);
	});

	const TSharedRef<SVerticalBox> List = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text(Found.IsEmpty()
				? FText::Format(LOCTEXT("NoResults", "Nothing mentions \"{0}\"."), SearchText)
				: FText::Format(LOCTEXT("Results", "{0} {0}|plural(one=RESULT,other=RESULTS)"), Found.Num()))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];

	constexpr int32 MaxResults = 60;
	for (int32 Shown = 0; Shown < Found.Num() && Shown < MaxResults; ++Shown)
	{
		const FSection& Section = Sections[Found[Shown]];
		const FPage& Page = Pages[Section.Page];
		const FText Where = Section.Level == 3 ? FText::FromString(Page.Title) : FText::GetEmpty();

		List->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 1.0f)
			[
				SNew(SButton)
				.ButtonStyle(&InventoryDocumentation::FlatButtonStyle())
				.ContentPadding(FMargin(8.0f, 6.0f))
				.OnClicked(this, &SInventoryDocumentation::HandleSectionClicked, Section.Page, Section.Anchor)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Section.Title))
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
						.HighlightText(this, &SInventoryDocumentation::GetHighlightText)
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(Where)
						.Visibility(Where.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
						.ColorAndOpacity(FStyleColors::AccentBlue)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(MakeSnippet(Section, SearchWords)))
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.HighlightText(this, &SInventoryDocumentation::GetHighlightText)
						.AutoWrapText(true)
					]
				]
			];
	}
	return List;
}

FString SInventoryDocumentation::MakeSnippet(const FSection& Section, const TArray<FString>& Words) const
{
	constexpr int32 Before = 40;
	constexpr int32 After = 110;

	const FString& Text = Section.Text;
	int32 Found = INDEX_NONE;
	for (const FString& Word : Words)
	{
		Found = Text.Find(Word, ESearchCase::IgnoreCase);
		if (Found != INDEX_NONE)
		{
			break;
		}
	}
	if (Found == INDEX_NONE)
	{
		Found = 0;
	}

	// From the start of a word to the end of one, so no word is cut in two.
	int32 Start = FMath::Max(0, Found - Before);
	while (Start > 0 && !FChar::IsWhitespace(Text[Start - 1]))
	{
		--Start;
	}
	int32 End = FMath::Min(Text.Len(), Found + After);
	while (End < Text.Len() && !FChar::IsWhitespace(Text[End]))
	{
		++End;
	}

	FString Snippet = Text.Mid(Start, End - Start).TrimStartAndEnd();
	if (Start > 0)
	{
		Snippet = TEXT("\u2026") + Snippet;
	}
	if (End < Text.Len())
	{
		Snippet += TEXT("\u2026");
	}
	return Snippet;
}

// Images
TSharedPtr<FInventoryDocumentationImage> SInventoryDocumentation::FindImage(const FString& Source)
{
	const FString Path = ResolvePath(Source);
	if (Path.IsEmpty())
	{
		return nullptr;
	}

	const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*Path);
	if (const TSharedPtr<FInventoryDocumentationImage>* Found = Images.Find(Path))
	{
		if ((*Found)->Timestamp == Timestamp)
		{
			return *Found;
		}
	}

	// Kept even when it can't be loaded, so it's tried again once the file appears or changes.
	const TSharedPtr<FInventoryDocumentationImage> Image = MakeShared<FInventoryDocumentationImage>();
	Image->Timestamp = Timestamp;
	Images.Add(Path, Image);

	::FImage Loaded;
	if (!FImageUtils::LoadImage(*Path, Loaded))
	{
		return Image;
	}
	::FImage Pixels;
	Loaded.CopyTo(Pixels, ERawImageFormat::BGRA8, EGammaSpace::sRGB);

	// With mips, so a large diagram shrinks to the column cleanly.
	UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);
	Texture->Source.Init(Pixels.SizeX, Pixels.SizeY, 1, 1, TSF_BGRA8, Pixels.RawData.GetData());
	Texture->CompressionSettings = TC_EditorIcon;
	Texture->MipGenSettings = TMGS_SimpleAverage;
	Texture->LODGroup = TEXTUREGROUP_UI;
	Texture->SRGB = true;
	Texture->PostEditChange();

	Image->Texture.Reset(Texture);
	Image->Size = FIntPoint(Pixels.SizeX, Pixels.SizeY);
	Image->Brush.SetResourceObject(Texture);
	Image->Brush.ImageSize = FVector2D(Image->Size);
	return Image;
}

FString SInventoryDocumentation::ResolvePath(const FString& Link) const
{
	const FString Guide = GetGuidePath();
	if (Link.IsEmpty() || InventoryDocumentation::IsWebLink(Link) || Guide.IsEmpty())
	{
		return FString();
	}

	FString FullPath = FPaths::GetPath(Guide) / Link.Replace(TEXT("%20"), TEXT(" "));
	FPaths::CollapseRelativeDirectories(FullPath);
	return FullPath;
}

// Responding
void SInventoryDocumentation::HandleLinkClicked(const TMap<FString, FString>& Metadata)
{
	const FString* Index = Metadata.Find(TEXT("link"));
	const int32 LinkIndex = Index ? FCString::Atoi(**Index) : INDEX_NONE;
	if (Links.IsValidIndex(LinkIndex))
	{
		FollowLink(Links[LinkIndex]);
	}
}

void SInventoryDocumentation::FollowLink(const FString& Link)
{
	// Another heading of the guide, on this page or another. Showing another page waits for the next frame, as it rebuilds the link.
	if (Link.StartsWith(TEXT("#")))
	{
		const int32 Page = FindPageOfAnchor(Link);
		if (Page != INDEX_NONE)
		{
			RequestPage(Page, Link);
		}
		return;
	}
	if (InventoryDocumentation::IsWebLink(Link))
	{
		FPlatformProcess::LaunchURL(*Link, nullptr, nullptr);
		return;
	}

	// A file next to the guide, such as a diagram, opens in its own application.
	FString Path = Link;
	int32 Hash = INDEX_NONE;
	if (Path.FindChar(TEXT('#'), Hash))
	{
		Path = Path.Left(Hash);
	}
	const FString FullPath = ResolvePath(Path);
	if (FullPath.IsEmpty() || (!FPaths::FileExists(FullPath) && !FPaths::DirectoryExists(FullPath)))
	{
		InventoryEditor::Notify(FText::Format(LOCTEXT("LinkMissing", "{0} isn't there."), FText::FromString(Path)));
		return;
	}
	if (FPaths::DirectoryExists(FullPath))
	{
		FPlatformProcess::ExploreFolder(*FullPath);
	}
	else
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*FullPath);
	}
}

FReply SInventoryDocumentation::HandlePageClicked(int32 PageIndex)
{
	RequestPage(PageIndex, FString());
	return FReply::Handled();
}

FReply SInventoryDocumentation::HandleSectionClicked(int32 PageIndex, FString Anchor)
{
	RequestPage(PageIndex, Anchor);
	return FReply::Handled();
}

FReply SInventoryDocumentation::HandleCopyClicked(FString Code)
{
	FPlatformApplicationMisc::ClipboardCopy(*Code);
	InventoryEditor::Notify(LOCTEXT("Copied", "Copied to the clipboard."));
	return FReply::Handled();
}

FReply SInventoryDocumentation::HandleBackClicked()
{
	GoBack();
	return FReply::Handled();
}

bool SInventoryDocumentation::CanGoBack() const
{
	return !History.IsEmpty();
}

void SInventoryDocumentation::GoBack()
{
	if (History.IsEmpty())
	{
		return;
	}

	const FVisit Visit = History.Pop();
	ShowPage(Visit.Page, FString(), Visit.ScrollOffset);
}

FReply SInventoryDocumentation::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// The mouse's back button, as in a web browser.
	if (MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton && CanGoBack())
	{
		GoBack();
		return FReply::Handled();
	}
	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

void SInventoryDocumentation::HandleSearchChanged(const FText& Text)
{
	SearchText = Text;
	SearchWords.Reset();
	Text.ToString().ToLower().ParseIntoArrayWS(SearchWords);
	RebuildSidebar();
	if (SidebarScroll.IsValid())
	{
		SidebarScroll->ScrollToStart();
	}
}

void SInventoryDocumentation::HandleSearchCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter || SearchWords.IsEmpty())
	{
		return;
	}

	// The first result, as the results list it.
	for (const FSection& Section : Sections)
	{
		const bool bAll = Algo::AllOf(SearchWords, [&Section](const FString& Word) { return Section.SearchText.Contains(Word); });
		if (bAll)
		{
			RequestPage(Section.Page, Section.Anchor);
			return;
		}
	}
}

FText SInventoryDocumentation::GetHighlightText() const
{
	return SearchText;
}

FText SInventoryDocumentation::GetVersionText() const
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ModularInventory"));
	const FString Version = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString();
	return Version.IsEmpty() ? LOCTEXT("Documentation", "Documentation") : FText::Format(LOCTEXT("DocumentationVersion", "Documentation, version {0}"), FText::FromString(Version));
}

EActiveTimerReturnType SInventoryDocumentation::CheckForChanges(double InCurrentTime, float InDeltaTime)
{
	const FString Guide = GetGuidePath();
	if (Guide.IsEmpty())
	{
		return EActiveTimerReturnType::Continue;
	}

	IFileManager& FileManager = IFileManager::Get();
	bool bChanged = FileManager.GetTimeStamp(*Guide) != GuideTimestamp;
	for (const TPair<FString, TSharedPtr<FInventoryDocumentationImage>>& Entry : Images)
	{
		bChanged |= Entry.Value.IsValid() && FileManager.GetTimeStamp(*Entry.Key) != Entry.Value->Timestamp;
	}
	if (bChanged)
	{
		Reload();
	}
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
