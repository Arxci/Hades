#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "Framework/Text/TextLayout.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

#include "Documentation/InventoryMarkdown.h"

class SBox;
class SOverlay;
class SScrollBox;
class SSearchBox;
class SVerticalBox;
struct FInventoryDocumentationImage;


/**
 * The Modular Inventory documentation window: the plugin's guide, its README.md, one chapter at a time, with the chapters and the open
 * chapter's sections beside it and a search through all of them. Links between chapters, the guide's diagrams and its code samples all
 * work here, and the window reads the guide again whenever it's saved. Opened from the Modular Inventory menu on the Level Editor toolbar.
 */
class SInventoryDocumentation : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInventoryDocumentation) {}
	SLATE_END_ARGS()

	/** Adds the window's tab to the editor, and takes it away. The editor module does both. */
	static void RegisterTab();
	static void UnregisterTab();

	/** Opens the window, at the chapter or section whose heading has Anchor, such as 3-quick-start. Empty opens it where it was. */
	static void Open(const FString& Anchor = FString());

	void Construct(const FArguments& InArgs);
	virtual ~SInventoryDocumentation() override;

	/** Shows the chapter with the heading Anchor names, scrolled to that heading. */
	void ShowAnchor(const FString& Anchor);

	//~ Begin SWidget Interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget Interface

private:
	using FBlock = InventoryMarkdown::FBlock;
	using FInlineRun = InventoryMarkdown::FInlineRun;
	using FTableCell = InventoryMarkdown::FTableCell;

	/** A chapter: a second-level heading and what follows it, up to the next. The guide's title and introduction come first. */
	struct FPage
	{
		/** As the heading says, such as "3. Quick start". */
		FString Title;

		/** The heading's number without the title, such as 3, or empty. */
		FString Number;

		/** The heading's title without its number, such as Quick start. */
		FString Name;

		/** The heading's anchor; empty for the introduction. */
		FString Anchor;

		/** The guide's top-level blocks it's made of: from First Block up to, but not including, End Block. */
		int32 FirstBlock = 0;
		int32 EndBlock = 0;
	};

	/** A chapter, or a section of one, with all its text, for the contents beside the guide and for search. */
	struct FSection
	{
		int32 Page = 0;

		/** 2 for a chapter, 3 for a section of one. */
		int32 Level = 2;

		FString Title;
		FString Anchor;

		/** Its text as written, for search results to quote, and in lower case, to search. */
		FString Text;
		FString SearchText;
	};

	/** A page to go back to: which one, and how far down it was. */
	struct FVisit
	{
		int32 Page = 0;
		float ScrollOffset = 0.0f;
	};

	// Reading the guide
	FString GetGuidePath() const;
	void Load();
	void BuildPages();
	void BuildSections();

	/** Reads the guide again after it was saved, keeping the page and the place on it. */
	void Reload();

	// Showing a page
	/** Shows a page on the next frame, scrolled to Anchor or else to the top. Clicks call it, as showing a page rebuilds what was clicked. */
	void RequestPage(int32 PageIndex, const FString& Anchor, bool bRemember = true);

	/**
	 * Shows a page, scrolled to Anchor, else to Scroll Offset when it's 0 or more, else to the top. The page on screen only scrolls, unless
	 * Rebuild asks for it to be built again. Another page is built unseen behind the one on screen, and takes its place a couple of frames
	 * later, once its wrapped text and its images have their final size, so nothing on screen jumps.
	 */
	void ShowPage(int32 PageIndex, const FString& Anchor, float ScrollOffset = -1.0f, bool bRebuild = false);
	int32 FindPageOfAnchor(const FString& Anchor) const;

	/** Builds the page unseen, behind the one on screen. */
	void RebuildPage();
	void RebuildSidebar();

	/** Runs once a frame, before the window is laid out, while a page is asked for or a new page is settling. */
	bool TickPages(float DeltaTime);
	void StartTickingPages();

	/** The settled page takes the place of the one on screen, and the window scrolls where it was asked to. */
	void FinishPageChange();

	/** Scrolls to Pending Anchor, else to Pending Scroll Offset, else to the top. */
	void ScrollToPending(bool bAnimate);

	// Building the page
	void AddBlocks(const TSharedRef<SVerticalBox>& Box, const FBlock* Blocks, int32 Count, int32 Depth);
	TSharedRef<SWidget> MakeBlock(const FBlock& Block, int32 Depth);
	TSharedRef<SWidget> MakeHeading(const FBlock& Heading);
	TSharedRef<SWidget> MakePageTitle(const FBlock& Heading, const FPage& Page);
	TSharedRef<SWidget> MakeList(const FBlock& List, int32 Depth);
	TSharedRef<SWidget> MakeTable(const FBlock& Table);
	TSharedRef<SWidget> MakeCode(const FBlock& Code);
	TSharedRef<SWidget> MakeQuote(const FBlock& Quote, int32 Depth);
	TSharedRef<SWidget> MakeImage(const FBlock& Image);
	TSharedRef<SWidget> MakeChapterCards();
	TSharedRef<SWidget> MakePageFooter();
	TSharedRef<SWidget> MakeNeighbourCard(int32 PageIndex, bool bNext);

	/** Text with its formatting and links. Scale is 0 for body text, or a heading's level. */
	TSharedRef<SWidget> MakeText(const TArray<FInlineRun>& Runs, int32 Scale, bool bBold = false, ETextJustify::Type Justification = ETextJustify::Left);
	FString MakeMarkup(const TArray<FInlineRun>& Runs, int32 Scale, bool bBold);

	// Building the sidebar
	TSharedRef<SWidget> MakeChapterEntry(int32 PageIndex);
	TSharedRef<SWidget> MakeSectionEntry(const FSection& Section);
	TSharedRef<SWidget> MakeSearchResults();

	/** A short piece of the section's text around the first word searched for. */
	FString MakeSnippet(const FSection& Section, const TArray<FString>& Words) const;

	// Images
	/** An image of the guide's, loaded once and again when it's saved. Null for web images, which aren't fetched. */
	TSharedPtr<FInventoryDocumentationImage> FindImage(const FString& Source);

	/** A link's file on disk, relative to the guide. Empty for web links. */
	FString ResolvePath(const FString& Link) const;

	// Responding
	void HandleLinkClicked(const TMap<FString, FString>& Metadata);
	void FollowLink(const FString& Link);
	FReply HandlePageClicked(int32 PageIndex);
	FReply HandleSectionClicked(int32 PageIndex, FString Anchor);
	FReply HandleCopyClicked(FString Code);
	FReply HandleBackClicked();
	bool CanGoBack() const;
	void GoBack();
	void HandleSearchChanged(const FText& Text);
	void HandleSearchCommitted(const FText& Text, ETextCommit::Type CommitType);
	FText GetHighlightText() const;
	FText GetVersionText() const;

	EActiveTimerReturnType CheckForChanges(double InCurrentTime, float InDeltaTime);

	FDateTime GuideTimestamp;
	bool bGuideUnreadable = false;
	InventoryMarkdown::FDocument Document;

	TArray<FPage> Pages;
	TArray<FSection> Sections;
	int32 CurrentPage = 0;

	/** Pages to go back to, the last visited last. */
	TArray<FVisit> History;

	/** What the search box holds, and its words in lower case. */
	FText SearchText;
	TArray<FString> SearchWords;

	/** Where each link in the shown page's rich text goes, by the index it carries, and the same for the page settling unseen. */
	TArray<FString> Links;
	TArray<FString> IncomingLinks;

	/** The shown page's headings, by anchor, to scroll to, and the same for the page settling unseen. */
	TMap<FString, TSharedRef<SWidget>> AnchorWidgets;
	TMap<FString, TSharedRef<SWidget>> IncomingAnchorWidgets;

	/** Loaded images by their full path. */
	TMap<FString, TSharedPtr<FInventoryDocumentationImage>> Images;

	TSharedPtr<SScrollBox> DocumentScroll;
	TSharedPtr<SBox> SidebarArea;
	TSharedPtr<SScrollBox> SidebarScroll;
	TSharedPtr<SSearchBox> SearchBox;

	/** Holds the page on screen, and the next one while it settles unseen behind it. */
	TSharedPtr<SOverlay> PageStack;
	TSharedPtr<SWidget> ShownPage;
	TSharedPtr<SWidget> IncomingPage;

	/** Frames left before the incoming page has been drawn often enough to have its final size. */
	int32 IncomingFramesLeft = 0;

	// A page asked for, shown on the next frame.
	int32 RequestedPage = INDEX_NONE;
	FString RequestedAnchor;
	bool bRememberRequest = true;

	/** Where to scroll once the page asked for is on screen. */
	FString PendingAnchor;
	float PendingScrollOffset = -1.0f;

	FTSTicker::FDelegateHandle PagesTicker;
};
