#pragma once

#include "CoreMinimal.h"

#include "Framework/Text/TextLayout.h"
#include "Input/Reply.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

#include "Markdown/MarkdownDocument.h"

class FActiveTimerHandle;
class SBox;
class SScrollBox;
class SVerticalBox;
struct FMarkdownReviewerImage;


/**
 * Shows a Markdown file the way GitHub shows it, with what a reader would trip over listed above it: links to headings or files that
 * don't exist, images that aren't there, code blocks left open and broken tables. It reloads whenever the file or its images are saved,
 * so a README can be written in any text editor and checked here as it's written. Tools > Modular Inventory > Markdown Reviewer.
 */
class SMarkdownReviewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMarkdownReviewer) {}
	SLATE_END_ARGS()

	/** Adds the reviewer's tab to the editor, and takes it away. The module does both. */
	static void RegisterTab();
	static void UnregisterTab();

	/** Opens the reviewer's tab and shows File in it. Without a file it shows the one it showed last. */
	static void Open(const FString& File = FString());

	void Construct(const FArguments& InArgs);

	/** Shows a Markdown file, scrolled to the heading with Anchor when one is given. */
	void ShowFile(const FString& File, const FString& Anchor = FString());

private:
	using FBlock = MarkdownReview::FBlock;
	using FInlineRun = MarkdownReview::FInlineRun;
	using FTableCell = MarkdownReview::FTableCell;
	using FProblem = MarkdownReview::FProblem;
	using EBlockType = MarkdownReview::EBlockType;
	using EInlineStyle = MarkdownReview::EInlineStyle;
	using EColumnAlignment = MarkdownReview::EColumnAlignment;

	void Load();
	void Rebuild();
	void RebuildContents();
	void RebuildProblems();

	/** Reads the file again, keeping the place in it. */
	void Reload();

	void AddBlocks(const TSharedRef<SVerticalBox>& Box, const TArray<MarkdownReview::FBlock>& Blocks, int32 Depth, float Spacing);
	TSharedRef<SWidget> MakeBlock(const MarkdownReview::FBlock& Block, int32 Depth);
	TSharedRef<SWidget> MakeHeading(const MarkdownReview::FBlock& Heading);
	TSharedRef<SWidget> MakeList(const MarkdownReview::FBlock& List, int32 Depth);
	TSharedRef<SWidget> MakeTable(const MarkdownReview::FBlock& Table);
	TSharedRef<SWidget> MakeCode(const MarkdownReview::FBlock& Code);
	TSharedRef<SWidget> MakeQuote(const MarkdownReview::FBlock& Quote, int32 Depth);
	TSharedRef<SWidget> MakeImage(const MarkdownReview::FBlock& Image);

	/** Text with its formatting and links. Scale is 0 for body text, or a heading's level. */
	TSharedRef<SWidget> MakeText(const TArray<MarkdownReview::FInlineRun>& Runs, int32 Scale, bool bBold = false, ETextJustify::Type Justification = ETextJustify::Left);
	FString MakeMarkup(const TArray<MarkdownReview::FInlineRun>& Runs, int32 Scale, bool bBold);

	/** An image next to the file, loaded once and again when it's saved. Null for web images, which aren't fetched. */
	TSharedPtr<FMarkdownReviewerImage> FindImage(const FString& Source);

	/** A link's file on disk, relative to the shown file. Empty for web links. */
	FString ResolvePath(const FString& Link) const;

	void HandleLinkClicked(const TMap<FString, FString>& Metadata);
	EActiveTimerReturnType FollowPendingLink(double InCurrentTime, float InDeltaTime);
	void FollowLink(const FString& Link);
	void ScrollToAnchor(FString Anchor);
	void ScrollToLine(int32 Line);
	void EditAtLine(int32 Line) const;

	/** Scrolls again once the rebuilt document has been laid out, as wrapped text only settles its height after a frame or two. */
	void ScrollWhenLaidOut();
	EActiveTimerReturnType ApplyPendingScroll(double InCurrentTime, float InDeltaTime);
	EActiveTimerReturnType CheckForChanges(double InCurrentTime, float InDeltaTime);

	void FindMarkdownFiles();
	TSharedRef<SWidget> MakeFileOption(TSharedPtr<FString> Option) const;
	void HandleFileChosen(TSharedPtr<FString> Choice, ESelectInfo::Type SelectInfo);
	FText GetFileLabel() const;
	FText GetSummary() const;
	FSlateColor GetSummaryColor() const;
	FReply HandleBrowseClicked();
	FReply HandleReloadClicked();
	FReply HandleEditClicked();

	FString CurrentFile;
	FDateTime FileTimestamp;
	bool bFileUnreadable = false;
	MarkdownReview::FDocument Document;

	/** Where each link in the rich text goes, by the index it carries. */
	TArray<FString> Links;

	TMap<FString, TSharedRef<SWidget>> AnchorWidgets;

	/** Each block's widget with the line it starts on, to scroll to a line. */
	TArray<TPair<int32, TSharedRef<SWidget>>> LineWidgets;

	/** Loaded images by their full path. */
	TMap<FString, TSharedPtr<FMarkdownReviewerImage>> Images;

	TArray<TSharedPtr<FString>> FileOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> FileCombo;
	TSharedPtr<SScrollBox> DocumentScroll;
	TSharedPtr<SBox> DocumentArea;
	TSharedPtr<SBox> ContentsArea;
	TSharedPtr<SBox> ProblemsArea;

	FString PendingLink;
	float PendingScrollOffset = -1.0f;
	FString PendingAnchor;
	TSharedPtr<FActiveTimerHandle> PendingScrollTimer;
};
