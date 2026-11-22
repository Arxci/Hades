#pragma once

#include "CoreMinimal.h"

#include "Misc/EnumClassFlags.h"
#include "Templates/Function.h"

/**
 * Markdown as GitHub shows it, as far as READMEs use it: headings, paragraphs, emphasis, code, links and images, nested and task lists,
 * tables, fenced code, quotes and alerts, and rules. Parsed into blocks for the Markdown Reviewer to draw, and checked for what a reader
 * would trip over, such as a link to a heading that doesn't exist.
 */
namespace MarkdownReview
{
	enum class EInlineStyle : uint8
	{
		None = 0,
		Bold = 1 << 0,
		Italic = 1 << 1,
		Code = 1 << 2,
		Strikethrough = 1 << 3
	};
	ENUM_CLASS_FLAGS(EInlineStyle)

	/** A stretch of text that looks one way. */
	struct FInlineRun
	{
		FString Text;
		EInlineStyle Style = EInlineStyle::None;

		/** Where it links to, as written, or empty. For an image, its source. */
		FString Link;

		/** An image in the text: Text is its alt text. */
		bool bImage = false;

		/** The line of the file it starts on, from 1. */
		int32 Line = 0;
	};

	enum class EBlockType : uint8
	{
		Paragraph,
		Heading,
		List,
		ListItem,
		Table,
		Code,
		Quote,
		Rule,
		/** A paragraph that's only an image, drawn as large as it fits. */
		Image
	};

	enum class EColumnAlignment : uint8
	{
		Left,
		Center,
		Right
	};

	/** One cell of a table: its text. */
	using FTableCell = TArray<FInlineRun>;

	struct FBlock
	{
		EBlockType Type = EBlockType::Paragraph;

		/** The line of the file it starts on, from 1. */
		int32 Line = 0;

		/** Heading: 1 to 6. */
		int32 Level = 0;

		/** Heading: what links to it put after the #, as GitHub makes it. */
		FString Anchor;

		/** Paragraph and Heading: the text. */
		TArray<FInlineRun> Runs;

		/** Code: the code. Image: its source, as written. */
		FString Text;

		/** Code: the language named after the opening fence. Image: its alt text. Quote: an alert's kind, such as Note or Warning. */
		FString Info;

		/** List: numbered, from Start. */
		bool bOrdered = false;
		int32 Start = 1;

		/** List Item: a task list's checkbox, and whether it's ticked. */
		bool bTask = false;
		bool bChecked = false;

		/** List: its items. List Item and Quote: what's inside them. */
		TArray<FBlock> Children;

		/** Table: the header row, how each column is aligned, and the rows, each with a cell per column. */
		TArray<FTableCell> Header;
		TArray<EColumnAlignment> Alignments;
		TArray<TArray<FTableCell>> Rows;
	};

	/** Something a reader would trip over, such as a link to a heading that doesn't exist. */
	struct FProblem
	{
		int32 Line = 0;
		FString Message;
	};

	struct FDocument
	{
		TArray<FBlock> Blocks;

		/** Every heading's anchor. */
		TSet<FString> Anchors;

		/** Parse finds code blocks left open and broken tables; Review adds links and images that lead nowhere. In line order. */
		TArray<FProblem> Problems;
	};

	/** Parses Markdown text. Never fails: whatever it doesn't recognise is text. */
	FDocument Parse(const FString& Text);

	/** Parses the text of one paragraph, heading or table cell. */
	void ParseInline(const FString& Text, TArray<FInlineRun>& OutRuns);

	/** GitHub's anchor for a heading's text: lower case, punctuation dropped, each space a hyphen. */
	FString MakeAnchor(const FString& HeadingText);

	/** The text of runs without their formatting. Images are left out, as they are from anchors. */
	FString ToPlainText(const TArray<FInlineRun>& Runs);

	/**
	 * Adds the problems with the document's links and images: anchors no heading has, and files that don't exist. Base Directory is the
	 * Markdown file's folder, which relative paths start from. Web links aren't checked. Call it once per parsed document.
	 */
	void Review(FDocument& Document, const FString& BaseDirectory);

	/** Calls Visitor for every block, nested ones included, in the order they're read. */
	void ForEachBlock(const TArray<FBlock>& Blocks, TFunctionRef<void(const FBlock&)> Visitor);
}
