#include "Markdown/SMarkdownReviewer.h"

#include "Brushes/SlateColorBrush.h"
#include "CoreGlobals.h"
#include "DesktopPlatformModule.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "IDesktopPlatform.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "SourceCodeNavigation.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#include "ModularInventoryEditor.h"

#define LOCTEXT_NAMESPACE "MarkdownReviewer"

/** An image shown in the document, kept while it's on screen. */
struct FMarkdownReviewerImage
{
	TStrongObjectPtr<UTexture2D> Texture;
	FSlateBrush Brush;
	FIntPoint Size = FIntPoint::ZeroValue;

	/** The file's time when it was loaded, to load it again once it's saved. */
	FDateTime Timestamp;
};

namespace MarkdownView
{
	using namespace MarkdownReview;

	const TCHAR* const ConfigSection = TEXT("MarkdownReviewer");

	FName TabName()
	{
		return FName(TEXT("MarkdownReviewer"));
	}

	TWeakPtr<SMarkdownReviewer> OpenReviewer;
	TSharedPtr<FSlateStyleSet> Styles;

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
	{
		const TSharedRef<SMarkdownReviewer> Reviewer = SNew(SMarkdownReviewer);
		OpenReviewer = Reviewer;
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				Reviewer
			];
	}

	const FSlateBrush* WhiteBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	/** Point sizes of body text and headings 1 to 6. */
	constexpr int32 ScaleCount = 7;
	constexpr int32 TextSizes[ScaleCount] = { 10, 20, 16, 13, 11, 10, 10 };

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

	/** Code blocks: monospaced, in the text's own color. */
	FName CodeBlockStyleName()
	{
		return FName(TEXT("mdcodeblock"));
	}

	const FSlateStyleSet& GetStyles()
	{
		if (Styles.IsValid())
		{
			return *Styles;
		}

		Styles = MakeShared<FSlateStyleSet>(TEXT("MarkdownReviewer"));
		const FTextBlockStyle& NormalText = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
		const FHyperlinkStyle& Hyperlink = FCoreStyle::Get().GetWidgetStyle<FHyperlinkStyle>(TEXT("Hyperlink"));
		const FSlateColor CodeColor(FLinearColor(FColor(230, 180, 140)));

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
						Color = CodeColor;
					}
					else if (EnumHasAnyFlags(Style, EInlineStyle::Strikethrough) || Scale == 6)
					{
						Color = FSlateColor::UseSubduedForeground();
					}

					const FTextBlockStyle TextStyle = FTextBlockStyle(NormalText)
						.SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, Size))
						.SetColorAndOpacity(Color);
					if (bLink)
					{
						const FHyperlinkStyle LinkStyle = FHyperlinkStyle(Hyperlink).SetTextStyle(TextStyle);
						Styles->Set(StyleName(Scale, Style, true), LinkStyle);
					}
					else
					{
						Styles->Set(StyleName(Scale, Style, false), TextStyle);
					}
				}
			}
		}

		const FTextBlockStyle CodeBlock = FTextBlockStyle(NormalText).SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 9));
		Styles->Set(CodeBlockStyleName(), CodeBlock);
		return *Styles;
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

	FString ProjectDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	/** A path as the file list shows it: relative to the project. */
	FString DisplayPath(const FString& Path)
	{
		FString Relative = Path;
		return FPaths::MakePathRelativeTo(Relative, *ProjectDirectory()) ? Relative : Path;
	}

	/** This plugin's guide, shown until another file is chosen. */
	FString PluginGuide()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ModularInventory"));
		return Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("README.md")) : FString();
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

	/** An image drawn as large as it is, or narrower to fit, keeping its shape. */
	class SMarkdownImage : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMarkdownImage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<FMarkdownReviewerImage>& InImage)
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
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform()), &Image->Brush);
			return LayerId;
		}

	private:
		TSharedPtr<FMarkdownReviewerImage> Image;
		double AvailableWidth = 0.0;
	};
}

void SMarkdownReviewer::RegisterTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MarkdownView::TabName(), FOnSpawnTab::CreateStatic(&MarkdownView::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Markdown Reviewer"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Shows a Markdown file as GitHub would, with its broken links and images listed."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void SMarkdownReviewer::UnregisterTab()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MarkdownView::TabName());
	}
	MarkdownView::Styles.Reset();
}

void SMarkdownReviewer::Open(const FString& File)
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(MarkdownView::TabName()));
	const TSharedPtr<SMarkdownReviewer> Reviewer = MarkdownView::OpenReviewer.Pin();
	if (Reviewer.IsValid() && !File.IsEmpty())
	{
		Reviewer->ShowFile(File);
	}
}

void SMarkdownReviewer::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(FileCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&FileOptions)
				.OnComboBoxOpening(this, &SMarkdownReviewer::FindMarkdownFiles)
				.OnGenerateWidget(this, &SMarkdownReviewer::MakeFileOption)
				.OnSelectionChanged(this, &SMarkdownReviewer::HandleFileChosen)
				.ToolTipText(LOCTEXT("FileTooltip", "The Markdown files in the project's root, Docs, Source and Plugins folders. Pick one to review it."))
				[
					SNew(STextBlock)
					.Text(this, &SMarkdownReviewer::GetFileLabel)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Browse", "Browse..."))
				.ToolTipText(LOCTEXT("BrowseTooltip", "Choose any Markdown file on disk."))
				.OnClicked(this, &SMarkdownReviewer::HandleBrowseClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Reload", "Reload"))
				.ToolTipText(LOCTEXT("ReloadTooltip", "Reads the file again. It also reloads by itself whenever the file or one of its images is saved."))
				.OnClicked(this, &SMarkdownReviewer::HandleReloadClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Edit", "Edit"))
				.ToolTipText(LOCTEXT("EditTooltip", "Opens the file in your code editor, chosen in Editor Preferences > General > Source Code."))
				.OnClicked(this, &SMarkdownReviewer::HandleEditClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SMarkdownReviewer::GetSummary)
				.ColorAndOpacity(this, &SMarkdownReviewer::GetSummaryColor)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 6.0f)
		[
			SAssignNew(ProblemsArea, SBox)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.22f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				.Padding(8.0f)
				[
					SAssignNew(ContentsArea, SBox)
				]
			]
			+ SSplitter::Slot()
			.Value(0.78f)
			[
				SNew(SBorder)
				.BorderImage(MarkdownView::WhiteBrush())
				.BorderBackgroundColor(FStyleColors::Recessed)
				.Padding(0.0f)
				[
					SAssignNew(DocumentScroll, SScrollBox)
					+ SScrollBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(24.0f, 16.0f)
					[
						// About as wide as GitHub shows a README, narrower when the tab is.
						SAssignNew(DocumentArea, SBox)
						.WidthOverride(980.0f)
					]
				]
			]
		]
	];

	RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SMarkdownReviewer::CheckForChanges));
	FindMarkdownFiles();

	FString File;
	if (!GConfig->GetString(MarkdownView::ConfigSection, TEXT("File"), File, GEditorPerProjectIni) || !FPaths::FileExists(File))
	{
		File = MarkdownView::PluginGuide();
	}
	if (!File.IsEmpty())
	{
		ShowFile(File);
	}
}

void SMarkdownReviewer::ShowFile(const FString& File, const FString& Anchor)
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(File);
	const bool bSameFile = FullPath.Equals(CurrentFile, ESearchCase::IgnoreCase);
	if (!bSameFile)
	{
		Images.Reset();
	}
	CurrentFile = FullPath;
	GConfig->SetString(MarkdownView::ConfigSection, TEXT("File"), *CurrentFile, GEditorPerProjectIni);

	Load();
	Rebuild();
	if (!Anchor.IsEmpty())
	{
		PendingAnchor = Anchor;
		ScrollWhenLaidOut();
	}
	else if (!bSameFile && DocumentScroll.IsValid())
	{
		DocumentScroll->ScrollToStart();
	}
}

void SMarkdownReviewer::Load()
{
	FString Text;
	bFileUnreadable = !FFileHelper::LoadFileToString(Text, *CurrentFile);
	FileTimestamp = IFileManager::Get().GetTimeStamp(*CurrentFile);
	Document = MarkdownReview::Parse(Text);
	MarkdownReview::Review(Document, FPaths::GetPath(CurrentFile));
}

void SMarkdownReviewer::Reload()
{
	const float Offset = DocumentScroll.IsValid() ? DocumentScroll->GetScrollOffset() : 0.0f;
	Load();
	Rebuild();
	if (DocumentScroll.IsValid())
	{
		DocumentScroll->SetScrollOffset(Offset);
	}
	PendingScrollOffset = Offset;
	ScrollWhenLaidOut();
}

void SMarkdownReviewer::Rebuild()
{
	Links.Reset();
	AnchorWidgets.Reset();
	LineWidgets.Reset();

	const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	AddBlocks(Body, Document.Blocks, 0, 8.0f);
	DocumentArea->SetContent(Body);

	RebuildContents();
	RebuildProblems();
}

void SMarkdownReviewer::RebuildContents()
{
	const TSharedRef<SVerticalBox> Contents = SNew(SVerticalBox);
	const FTextBlockStyle* EntryStyle = &MarkdownView::GetStyles().GetWidgetStyle<FTextBlockStyle>(MarkdownView::StyleName(0, EInlineStyle::None, false));
	MarkdownReview::ForEachBlock(Document.Blocks, [this, &Contents, EntryStyle](const FBlock& Block)
	{
		if (Block.Type != EBlockType::Heading)
		{
			return;
		}
		Contents->AddSlot()
			.AutoHeight()
			.Padding(static_cast<float>(FMath::Max(Block.Level - 1, 0)) * 12.0f, 2.0f, 0.0f, 2.0f)
			[
				SNew(SHyperlink)
				.Text(FText::FromString(MarkdownReview::ToPlainText(Block.Runs)))
				.TextStyle(EntryStyle)
				.ToolTipText(FText::FromString(TEXT("#") + Block.Anchor))
				.OnNavigate(FSimpleDelegate::CreateSP(this, &SMarkdownReviewer::ScrollToAnchor, Block.Anchor))
			];
	});
	ContentsArea->SetContent(Contents);
}

void SMarkdownReviewer::RebuildProblems()
{
	if (Document.Problems.IsEmpty())
	{
		ProblemsArea->SetContent(SNullWidget::NullWidget);
		return;
	}

	const TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FProblem& Problem : Document.Problems)
	{
		List->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SHyperlink)
					.Text(FText::Format(LOCTEXT("ProblemLine", "Line {0}"), Problem.Line))
					.ToolTipText(LOCTEXT("ProblemLineTooltip", "Scrolls the preview to it."))
					.OnNavigate(FSimpleDelegate::CreateSP(this, &SMarkdownReviewer::ScrollToLine, Problem.Line))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Problem.Message))
					.AutoWrapText(true)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SHyperlink)
					.Text(LOCTEXT("EditLine", "Edit"))
					.ToolTipText(LOCTEXT("EditLineTooltip", "Opens the file at this line in your code editor."))
					.OnNavigate(FSimpleDelegate::CreateSP(this, &SMarkdownReviewer::EditAtLine, Problem.Line))
				]
			];
	}

	ProblemsArea->SetContent(
		SNew(SBorder)
		.BorderImage(MarkdownView::WhiteBrush())
		.BorderBackgroundColor(FStyleColors::Header)
		.Padding(8.0f)
		[
			SNew(SBox)
			.MaxDesiredHeight(160.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					List
				]
			]
		]);
}

void SMarkdownReviewer::AddBlocks(const TSharedRef<SVerticalBox>& Box, const TArray<FBlock>& Blocks, int32 Depth, float Spacing)
{
	for (const FBlock& Block : Blocks)
	{
		const TSharedRef<SWidget> Widget = MakeBlock(Block, Depth);
		LineWidgets.Emplace(Block.Line, Widget);

		// Headings get more room above them than below, so they sit with their section.
		const float Above = (Block.Type == EBlockType::Heading) ? Spacing * 2.0f : 0.0f;
		Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, Above, 0.0f, Spacing)
			[
				Widget
			];
	}
}

TSharedRef<SWidget> SMarkdownReviewer::MakeBlock(const FBlock& Block, int32 Depth)
{
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
		return SNew(SSeparator).Orientation(Orient_Horizontal);
	case EBlockType::Image:
		return MakeImage(Block);
	case EBlockType::Paragraph:
	case EBlockType::ListItem:
	default:
		return MakeText(Block.Runs, 0);
	}
}

TSharedRef<SWidget> SMarkdownReviewer::MakeHeading(const FBlock& Heading)
{
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeText(Heading.Runs, FMath::Clamp(Heading.Level, 1, 6))
		];
	if (Heading.Level <= 2)
	{
		Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			];
	}
	const TSharedRef<SWidget> Widget = Box;
	AnchorWidgets.Add(Heading.Anchor, Widget);
	return Widget;
}

TSharedRef<SWidget> SMarkdownReviewer::MakeList(const FBlock& List, int32 Depth)
{
	const TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
	const FTextBlockStyle* MarkerStyle = &MarkdownView::GetStyles().GetWidgetStyle<FTextBlockStyle>(MarkdownView::StyleName(0, EInlineStyle::None, false));
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
			const FString Label = List.bOrdered ? FString::Printf(TEXT("%d."), Number) : FString(TEXT("\u2022"));
			Marker = SNew(STextBlock)
				.Text(FText::FromString(Label))
				.TextStyle(MarkerStyle);
		}
		++Number;

		const TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
		AddBlocks(Content, Item.Children, Depth + 1, 4.0f);

		Items->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding((Depth == 0) ? 4.0f : 0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBox)
					.MinDesiredWidth(List.bOrdered ? 24.0f : 14.0f)
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

TSharedRef<SWidget> SMarkdownReviewer::MakeTable(const FBlock& Table)
{
	const int32 Columns = Table.Alignments.Num();
	const TSharedRef<SGridPanel> Grid = SNew(SGridPanel);
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		Grid->SetColumnFill(Column, MarkdownView::ColumnWeight(Table, Column));
	}

	// The gaps between cells show the border's color behind them as grid lines.
	auto AddCell = [this, &Grid, &Table](int32 CellColumn, int32 CellRow, const FTableCell& Cell)
	{
		const EColumnAlignment Alignment = Table.Alignments[CellColumn];
		const ETextJustify::Type Justification = (Alignment == EColumnAlignment::Center) ? ETextJustify::Center
			: ((Alignment == EColumnAlignment::Right) ? ETextJustify::Right : ETextJustify::Left);
		const bool bHeader = CellRow == 0;
		const FSlateColor Background = bHeader ? FSlateColor(FStyleColors::Header)
			: ((CellRow % 2 == 0) ? FSlateColor(FStyleColors::Panel) : FSlateColor(FStyleColors::Background));
		Grid->AddSlot(CellColumn, CellRow)
			.Padding(0.5f)
			[
				SNew(SBorder)
				.BorderImage(MarkdownView::WhiteBrush())
				.BorderBackgroundColor(Background)
				.Padding(FMargin(8.0f, 5.0f))
				[
					MakeText(Cell, 0, bHeader, Justification)
				]
			];
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
		.BorderImage(MarkdownView::WhiteBrush())
		.BorderBackgroundColor(FStyleColors::Hover)
		.Padding(0.5f)
		[
			Grid
		];
}

TSharedRef<SWidget> SMarkdownReviewer::MakeCode(const FBlock& Code)
{
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (!Code.Info.IsEmpty())
	{
		Box->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Code.Info))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}

	// Selectable, to copy from. Long lines scroll sideways, as on GitHub, without taking the mouse wheel from the page.
	Box->AddSlot()
		.AutoHeight()
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			.ConsumeMouseWheel(EConsumeMouseWheel::Never)
			+ SScrollBox::Slot()
			[
				SNew(SMultiLineEditableText)
				.Text(FText::FromString(Code.Text))
				.TextStyle(&MarkdownView::GetStyles().GetWidgetStyle<FTextBlockStyle>(MarkdownView::CodeBlockStyleName()))
				.IsReadOnly(true)
			]
		];

	return SNew(SBorder)
		.BorderImage(MarkdownView::WhiteBrush())
		.BorderBackgroundColor(FStyleColors::Background)
		.Padding(FMargin(12.0f, 8.0f))
		[
			Box
		];
}

TSharedRef<SWidget> SMarkdownReviewer::MakeQuote(const FBlock& Quote, int32 Depth)
{
	// Alerts are colored by their kind.
	FSlateColor BarColor = FSlateColor::UseSubduedForeground();
	if (Quote.Info == TEXT("Note"))
	{
		BarColor = FStyleColors::AccentBlue;
	}
	else if (Quote.Info == TEXT("Tip"))
	{
		BarColor = FStyleColors::AccentGreen;
	}
	else if (Quote.Info == TEXT("Important"))
	{
		BarColor = FStyleColors::AccentPurple;
	}
	else if (Quote.Info == TEXT("Warning"))
	{
		BarColor = FStyleColors::AccentYellow;
	}
	else if (Quote.Info == TEXT("Caution"))
	{
		BarColor = FStyleColors::AccentRed;
	}

	const TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (!Quote.Info.IsEmpty())
	{
		Content->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Quote.Info))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
				.ColorAndOpacity(BarColor)
			];
	}
	AddBlocks(Content, Quote.Children, Depth + 1, 6.0f);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(3.0f)
			[
				SNew(SBorder)
				.BorderImage(MarkdownView::WhiteBrush())
				.BorderBackgroundColor(BarColor)
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(12.0f, 2.0f, 0.0f, 2.0f)
		[
			Content
		];
}

TSharedRef<SWidget> SMarkdownReviewer::MakeImage(const FBlock& Image)
{
	const TSharedPtr<FMarkdownReviewerImage> Loaded = FindImage(Image.Text);
	if (Loaded.IsValid() && Loaded->Texture.IsValid())
	{
		return SNew(MarkdownView::SMarkdownImage, Loaded.ToSharedRef())
			.ToolTipText(FText::FromString(Image.Info));
	}

	// Web images aren't fetched, and missing ones are listed with the problems.
	return SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("NoImage", "[Image: {0}] {1}"), FText::FromString(Image.Info), FText::FromString(Image.Text)))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.AutoWrapText(true);
}

TSharedRef<SWidget> SMarkdownReviewer::MakeText(const TArray<FInlineRun>& Runs, int32 Scale, bool bBold, ETextJustify::Type Justification)
{
	const FSlateStyleSet& Style = MarkdownView::GetStyles();
	return SNew(SRichTextBlock)
		.Text(FText::FromString(MakeMarkup(Runs, Scale, bBold)))
		.TextStyle(&Style.GetWidgetStyle<FTextBlockStyle>(MarkdownView::StyleName(Scale, bBold ? EInlineStyle::Bold : EInlineStyle::None, false)))
		.DecoratorStyleSet(&Style)
		.AutoWrapText(true)
		.Justification(Justification)
		.LineHeightPercentage(1.1f)
		+ SRichTextBlock::HyperlinkDecorator(TEXT("md"), FSlateHyperlinkRun::FOnClick::CreateSP(this, &SMarkdownReviewer::HandleLinkClicked));
}

FString SMarkdownReviewer::MakeMarkup(const TArray<FInlineRun>& Runs, int32 Scale, bool bBold)
{
	FString Markup;
	for (const FInlineRun& Run : Runs)
	{
		const EInlineStyle Style = bBold ? (Run.Style | EInlineStyle::Bold) : Run.Style;
		const bool bLink = !Run.Link.IsEmpty();
		const int32 LinkIndex = bLink ? Links.Add(Run.Link) : INDEX_NONE;
		const FString Name = MarkdownView::StyleName(Scale, Style, bLink).ToString();
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
			const FString Escaped = MarkdownView::EscapeMarkup(Lines[LineIndex]);
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

TSharedPtr<FMarkdownReviewerImage> SMarkdownReviewer::FindImage(const FString& Source)
{
	const FString Path = ResolvePath(Source);
	if (Path.IsEmpty())
	{
		return nullptr;
	}

	const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*Path);
	if (const TSharedPtr<FMarkdownReviewerImage>* Found = Images.Find(Path))
	{
		if ((*Found)->Timestamp == Timestamp)
		{
			return *Found;
		}
	}

	// Kept even when it can't be loaded, so it's tried again once the file appears or changes.
	const TSharedPtr<FMarkdownReviewerImage> Image = MakeShared<FMarkdownReviewerImage>();
	Image->Timestamp = Timestamp;
	Images.Add(Path, Image);

	::FImage Loaded;
	if (!FImageUtils::LoadImage(*Path, Loaded))
	{
		return Image;
	}
	::FImage Pixels;
	Loaded.CopyTo(Pixels, ERawImageFormat::BGRA8, EGammaSpace::sRGB);

	// With mips, so a large screenshot shrinks to the column cleanly.
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

FString SMarkdownReviewer::ResolvePath(const FString& Link) const
{
	if (Link.IsEmpty() || MarkdownView::IsWebLink(Link) || CurrentFile.IsEmpty())
	{
		return FString();
	}
	const FString Path = Link.Replace(TEXT("%20"), TEXT(" "));

	// A path from the top of the repository, which for a project is usually its folder.
	FString FullPath = Path.StartsWith(TEXT("/")) ? MarkdownView::ProjectDirectory() / Path.RightChop(1) : FPaths::GetPath(CurrentFile) / Path;
	FPaths::CollapseRelativeDirectories(FullPath);
	return FullPath;
}

void SMarkdownReviewer::HandleLinkClicked(const TMap<FString, FString>& Metadata)
{
	const FString* Index = Metadata.Find(TEXT("link"));
	const int32 LinkIndex = Index ? FCString::Atoi(**Index) : INDEX_NONE;
	if (!Links.IsValidIndex(LinkIndex))
	{
		return;
	}

	// Followed on the next frame: opening another file rebuilds the text whose link was clicked.
	PendingLink = Links[LinkIndex];
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SMarkdownReviewer::FollowPendingLink));
}

EActiveTimerReturnType SMarkdownReviewer::FollowPendingLink(double InCurrentTime, float InDeltaTime)
{
	const FString Link = PendingLink;
	PendingLink.Reset();
	if (!Link.IsEmpty())
	{
		FollowLink(Link);
	}
	return EActiveTimerReturnType::Stop;
}

void SMarkdownReviewer::FollowLink(const FString& Link)
{
	if (Link.StartsWith(TEXT("#")))
	{
		ScrollToAnchor(Link.RightChop(1));
		return;
	}
	if (MarkdownView::IsWebLink(Link))
	{
		FPlatformProcess::LaunchURL(*Link, nullptr, nullptr);
		return;
	}

	FString Path = Link;
	FString Anchor;
	int32 Hash = INDEX_NONE;
	if (Path.FindChar(TEXT('#'), Hash))
	{
		Anchor = Path.RightChop(Hash + 1);
		Path = Path.Left(Hash);
	}
	const FString FullPath = ResolvePath(Path);
	if (FullPath.IsEmpty() || (!FPaths::FileExists(FullPath) && !FPaths::DirectoryExists(FullPath)))
	{
		InventoryEditor::Notify(FText::Format(LOCTEXT("LinkMissing", "{0} isn't there."), FText::FromString(Path)));
		return;
	}

	// Other Markdown files open here; anything else opens in its own application.
	if (FPaths::GetExtension(FullPath).Equals(TEXT("md"), ESearchCase::IgnoreCase))
	{
		ShowFile(FullPath, Anchor);
	}
	else if (FPaths::DirectoryExists(FullPath))
	{
		FPlatformProcess::ExploreFolder(*FullPath);
	}
	else
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*FullPath);
	}
}

void SMarkdownReviewer::ScrollToAnchor(FString Anchor)
{
	const TSharedRef<SWidget>* Widget = AnchorWidgets.Find(Anchor.Replace(TEXT("%20"), TEXT(" ")).ToLower());
	if (Widget && DocumentScroll.IsValid())
	{
		DocumentScroll->ScrollDescendantIntoView(*Widget, true, EDescendantScrollDestination::TopOrLeft, 8.0f);
	}
}

void SMarkdownReviewer::ScrollToLine(int32 Line)
{
	// The block starting nearest before the line.
	int32 BestLine = INDEX_NONE;
	TSharedPtr<SWidget> Target;
	for (const TPair<int32, TSharedRef<SWidget>>& Entry : LineWidgets)
	{
		if (Entry.Key <= Line && Entry.Key >= BestLine)
		{
			BestLine = Entry.Key;
			Target = Entry.Value;
		}
	}
	if (Target.IsValid() && DocumentScroll.IsValid())
	{
		DocumentScroll->ScrollDescendantIntoView(Target, true, EDescendantScrollDestination::TopOrLeft, 8.0f);
	}
}

void SMarkdownReviewer::EditAtLine(int32 Line) const
{
	if (!CurrentFile.IsEmpty() && !FSourceCodeNavigation::OpenSourceFile(CurrentFile, Line))
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*CurrentFile);
	}
}

void SMarkdownReviewer::ScrollWhenLaidOut()
{
	if (!PendingScrollTimer.IsValid())
	{
		PendingScrollTimer = RegisterActiveTimer(0.15f, FWidgetActiveTimerDelegate::CreateSP(this, &SMarkdownReviewer::ApplyPendingScroll));
	}
}

EActiveTimerReturnType SMarkdownReviewer::ApplyPendingScroll(double InCurrentTime, float InDeltaTime)
{
	PendingScrollTimer.Reset();
	if (!PendingAnchor.IsEmpty())
	{
		ScrollToAnchor(PendingAnchor);
	}
	else if (PendingScrollOffset >= 0.0f && DocumentScroll.IsValid())
	{
		DocumentScroll->SetScrollOffset(PendingScrollOffset);
	}
	PendingAnchor.Reset();
	PendingScrollOffset = -1.0f;
	return EActiveTimerReturnType::Stop;
}

EActiveTimerReturnType SMarkdownReviewer::CheckForChanges(double InCurrentTime, float InDeltaTime)
{
	if (CurrentFile.IsEmpty())
	{
		return EActiveTimerReturnType::Continue;
	}
	IFileManager& FileManager = IFileManager::Get();
	bool bChanged = FileManager.GetTimeStamp(*CurrentFile) != FileTimestamp;
	for (const TPair<FString, TSharedPtr<FMarkdownReviewerImage>>& Entry : Images)
	{
		bChanged |= Entry.Value.IsValid() && FileManager.GetTimeStamp(*Entry.Key) != Entry.Value->Timestamp;
	}
	if (bChanged)
	{
		Reload();
	}
	return EActiveTimerReturnType::Continue;
}

void SMarkdownReviewer::FindMarkdownFiles()
{
	const FString ProjectDir = MarkdownView::ProjectDirectory();
	IFileManager& FileManager = IFileManager::Get();

	TArray<FString> Found;
	FileManager.FindFiles(Found, *(ProjectDir / TEXT("*.md")), true, false);
	for (FString& Name : Found)
	{
		Name = ProjectDir / Name;
	}
	static const TCHAR* const Folders[] = { TEXT("Docs"), TEXT("Documentation"), TEXT("Source"), TEXT("Plugins") };
	for (const TCHAR* Folder : Folders)
	{
		TArray<FString> InFolder;
		FileManager.FindFilesRecursive(InFolder, *(ProjectDir / Folder), TEXT("*.md"), true, false);
		Found.Append(InFolder);
	}
	Found.RemoveAll([](const FString& Path)
	{
		return Path.Contains(TEXT("/Intermediate/")) || Path.Contains(TEXT("/Binaries/")) || Path.Contains(TEXT("/Saved/")) || Path.Contains(TEXT("/node_modules/"));
	});
	Found.Sort();

	FileOptions.Reset();
	for (const FString& Path : Found)
	{
		FileOptions.Add(MakeShared<FString>(Path));
	}
	if (FileCombo.IsValid())
	{
		FileCombo->RefreshOptions();
	}
}

TSharedRef<SWidget> SMarkdownReviewer::MakeFileOption(TSharedPtr<FString> Option) const
{
	return SNew(STextBlock)
		.Text(FText::FromString(Option.IsValid() ? MarkdownView::DisplayPath(*Option) : FString()));
}

void SMarkdownReviewer::HandleFileChosen(TSharedPtr<FString> Choice, ESelectInfo::Type SelectInfo)
{
	if (Choice.IsValid())
	{
		ShowFile(*Choice);
	}
}

FText SMarkdownReviewer::GetFileLabel() const
{
	return CurrentFile.IsEmpty() ? LOCTEXT("ChooseFile", "Choose a Markdown file") : FText::FromString(MarkdownView::DisplayPath(CurrentFile));
}

FText SMarkdownReviewer::GetSummary() const
{
	if (CurrentFile.IsEmpty())
	{
		return FText::GetEmpty();
	}
	if (bFileUnreadable)
	{
		return LOCTEXT("Unreadable", "The file can't be read");
	}
	const int32 Count = Document.Problems.Num();
	return (Count == 0) ? LOCTEXT("NoProblems", "No problems found") : FText::Format(LOCTEXT("Problems", "{0} {0}|plural(one=problem,other=problems)"), Count);
}

FSlateColor SMarkdownReviewer::GetSummaryColor() const
{
	if (bFileUnreadable || !Document.Problems.IsEmpty())
	{
		return FStyleColors::Warning;
	}
	return FStyleColors::Success;
}

FReply SMarkdownReviewer::HandleBrowseClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	TArray<FString> Chosen;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	const FString DefaultPath = CurrentFile.IsEmpty() ? MarkdownView::ProjectDirectory() : FPaths::GetPath(CurrentFile);
	if (DesktopPlatform->OpenFileDialog(ParentWindow, LOCTEXT("BrowseTitle", "Choose a Markdown file").ToString(), DefaultPath, FString(),
		TEXT("Markdown files (*.md)|*.md|All files (*.*)|*.*"), EFileDialogFlags::None, Chosen) && !Chosen.IsEmpty())
	{
		ShowFile(Chosen[0]);
	}
	return FReply::Handled();
}

FReply SMarkdownReviewer::HandleReloadClicked()
{
	if (!CurrentFile.IsEmpty())
	{
		Reload();
	}
	return FReply::Handled();
}

FReply SMarkdownReviewer::HandleEditClicked()
{
	EditAtLine(1);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
