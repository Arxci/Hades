#include "Documentation/InventoryMarkdown.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

BEGIN_DEFINE_SPEC(FInventoryMarkdownSpec, "ModularInventory.Editor.Documentation.Markdown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** Runs as [text] when plain, else {flags->link:text}, with B, I, C and S for bold, italic, code and struck, and IMG for an image. */
	static FString RunsToText(const TArray<InventoryMarkdown::FInlineRun>& Runs)
	{
		FString Result;
		for (const InventoryMarkdown::FInlineRun& Run : Runs)
		{
			FString Flags;
			Flags += EnumHasAnyFlags(Run.Style, InventoryMarkdown::EInlineStyle::Bold) ? TEXT("B") : TEXT("");
			Flags += EnumHasAnyFlags(Run.Style, InventoryMarkdown::EInlineStyle::Italic) ? TEXT("I") : TEXT("");
			Flags += EnumHasAnyFlags(Run.Style, InventoryMarkdown::EInlineStyle::Code) ? TEXT("C") : TEXT("");
			Flags += EnumHasAnyFlags(Run.Style, InventoryMarkdown::EInlineStyle::Strikethrough) ? TEXT("S") : TEXT("");
			Flags += Run.bImage ? TEXT("IMG") : TEXT("");
			if (!Run.Link.IsEmpty())
			{
				Flags += TEXT("->") + Run.Link;
			}
			Result += Flags.IsEmpty() ? FString::Printf(TEXT("[%s]"), *Run.Text) : FString::Printf(TEXT("{%s:%s}"), *Flags, *Run.Text);
		}
		return Result;
	}

	static FString InlineToText(const TCHAR* Text)
	{
		TArray<InventoryMarkdown::FInlineRun> Runs;
		InventoryMarkdown::ParseInline(Text, Runs);
		return RunsToText(Runs);
	}

	/** The block types in reading order, nested ones indented by a space per level. */
	static FString Outline(const TArray<InventoryMarkdown::FBlock>& Blocks, int32 Depth = 0)
	{
		static const TCHAR* const Names[] = { TEXT("Paragraph"), TEXT("Heading"), TEXT("List"), TEXT("Item"), TEXT("Table"), TEXT("Code"), TEXT("Quote"), TEXT("Rule"), TEXT("Image") };
		FString Result;
		for (const InventoryMarkdown::FBlock& Block : Blocks)
		{
			Result += FString::ChrN(Depth, TEXT(' ')) + Names[static_cast<int32>(Block.Type)] + TEXT("\n");
			Result += Outline(Block.Children, Depth + 1);
		}
		return Result;
	}

	FString Folder;

END_DEFINE_SPEC(FInventoryMarkdownSpec)

void FInventoryMarkdownSpec::Define()
{
	using namespace InventoryMarkdown;

	Describe("Anchors", [this]()
	{
		It("are made as GitHub makes them", [this]()
		{
			TestEqual(TEXT("Numbered"), MakeAnchor(TEXT("6.3 Inventory Shortcut Component")), FString(TEXT("63-inventory-shortcut-component")));
			TestEqual(TEXT("Punctuation dropped, each space a hyphen"), MakeAnchor(TEXT("Value Displays & Groups")), FString(TEXT("value-displays--groups")));
			TestEqual(TEXT("Underscores and hyphens kept"), MakeAnchor(TEXT("snake_case - and hyphen")), FString(TEXT("snake_case---and-hyphen")));
		});

		It("number repeated headings and leave formatting out", [this]()
		{
			const FDocument Document = Parse(TEXT("# Intro\n\n## Setup\n\n## Setup\n\n### `Code` *Heading*\n"));
			TestTrue(TEXT("First"), Document.Anchors.Contains(TEXT("setup")));
			TestTrue(TEXT("Repeated"), Document.Anchors.Contains(TEXT("setup-1")));
			TestTrue(TEXT("Formatting left out"), Document.Anchors.Contains(TEXT("code-heading")));
		});
	});

	Describe("Inline text", [this]()
	{
		It("reads emphasis and code, and leaves snake_case and lone stars alone", [this]()
		{
			TestEqual(TEXT("Emphasis"), InlineToText(TEXT("a **bold** and *italic* and ***both*** and `co*de*` and snake_case_name and 2 * 3 * 4")),
				FString(TEXT("[a ]{B:bold}[ and ]{I:italic}[ and ]{BI:both}[ and ]{C:co*de*}[ and snake_case_name and 2 * 3 * 4]")));
			TestEqual(TEXT("Nested"), InlineToText(TEXT("*a **b** c* then ~~gone~~")), FString(TEXT("{I:a }{BI:b}{I: c}[ then ]{S:gone}")));
			TestEqual(TEXT("Unclosed"), InlineToText(TEXT("**unclosed and *also")), FString(TEXT("[**unclosed and *also]")));
		});

		It("reads links, images, web addresses and escapes", [this]()
		{
			TestEqual(TEXT("Links and images"), InlineToText(TEXT("See [the **guide**](#11-building-the-ui) and ![alt](Docs/a.png).")),
				FString(TEXT("[See ]{->#11-building-the-ui:the }{B->#11-building-the-ui:guide}[ and ]{IMG->Docs/a.png:alt}[.]")));
			TestEqual(TEXT("Addresses and escapes"), InlineToText(TEXT("Or <https://a.b/c> or www.site.org, \\*not italic\\* &amp;")),
				FString(TEXT("[Or ]{->https://a.b/c:https://a.b/c}[ or ]{->http://www.site.org:www.site.org}[, *not italic* &]")));
		});

		It("uses link definitions from anywhere in the file", [this]()
		{
			const FDocument Document = Parse(TEXT("Read [the docs][docs] and [docs].\n\n[docs]: https://example.com/docs\n"));
			TestEqual(TEXT("The definition isn't shown"), Document.Blocks.Num(), 1);
			if (Document.Blocks.Num() == 1)
			{
				TestEqual(TEXT("Both links"), RunsToText(Document.Blocks[0].Runs), FString(TEXT("[Read ]{->https://example.com/docs:the docs}[ and ]{->https://example.com/docs:docs}[.]")));
			}
		});
	});

	Describe("Blocks", [this]()
	{
		It("nest lists, tables and code inside list items", [this]()
		{
			const FDocument Document = Parse(TEXT("1. First\n   - sub\n\n   | A | B |\n   | --- | :-: |\n   | 1 | 2 |\n\n   ```cpp\n   int X;\n   ```\n2. Second\n\n- [x] done\n"));
			TestEqual(TEXT("Outline"), Outline(Document.Blocks),
				FString(TEXT("List\n Item\n  Paragraph\n  List\n   Item\n    Paragraph\n  Table\n  Code\n Item\n  Paragraph\nList\n Item\n  Paragraph\n")));
			const bool bShaped = Document.Blocks.Num() == 2 && Document.Blocks[0].Children.Num() == 2 && Document.Blocks[0].Children[0].Children.Num() == 4
				&& Document.Blocks[1].Children.Num() == 1;
			if (bShaped)
			{
				const FBlock& Table = Document.Blocks[0].Children[0].Children[2];
				TestTrue(TEXT("Centered column"), Table.Alignments.Num() == 2 && Table.Alignments[1] == EColumnAlignment::Center);
				TestTrue(TEXT("Ticked task"), Document.Blocks[1].Children[0].bTask && Document.Blocks[1].Children[0].bChecked);
			}
		});

		It("read underlined headings, rules, quotes and alerts", [this]()
		{
			const FDocument Document = Parse(TEXT("Title\n=====\n\n---\n\n> quoted\nlazy line\n\n> [!WARNING]\n> Careful.\n"));
			TestEqual(TEXT("Outline"), Outline(Document.Blocks), FString(TEXT("Heading\nRule\nQuote\n Paragraph\nQuote\n Paragraph\n")));
			if (Document.Blocks.Num() == 4 && Document.Blocks[2].Children.Num() == 1)
			{
				TestEqual(TEXT("Lazy line joins the quote"), RunsToText(Document.Blocks[2].Children[0].Runs), FString(TEXT("[quoted lazy line]")));
				TestEqual(TEXT("Alert"), Document.Blocks[3].Info, FString(TEXT("Warning")));
			}
		});

		It("hide comments and show other HTML as code", [this]()
		{
			const FDocument Document = Parse(TEXT("<!-- hidden\nstill hidden -->\n\n<div align=\"center\">\n</div>\n\nText<br>more\n"));
			TestEqual(TEXT("Outline"), Outline(Document.Blocks), FString(TEXT("Code\nParagraph\n")));
		});

		It("stay quick on text full of unmatched markers", [this]()
		{
			FString Text;
			for (int32 Index = 0; Index < 3000; ++Index)
			{
				Text += TEXT("*a _b [c ");
			}
			TestEqual(TEXT("One paragraph"), Parse(Text).Blocks.Num(), 1);
		});
	});

	Describe("Problems", [this]()
	{
		BeforeEach([this]()
		{
			Folder = FPaths::ConvertRelativePathToFull(FPaths::AutomationTransientDir() / TEXT("InventoryDocumentation"));
			IFileManager::Get().MakeDirectory(*Folder, true);
			FFileHelper::SaveStringToFile(TEXT("int Main;"), *(Folder / TEXT("main.cpp")));
		});

		AfterEach([this]()
		{
			IFileManager::Get().DeleteDirectory(*Folder, false, true);
		});

		It("list broken tables and code blocks left open", [this]()
		{
			const FDocument Document = Parse(TEXT("| A | B |\n|---|---|\n| 1 | 2 | 3 |\n\n| X | Y |\n|---|\n\n```\nopen\n"));
			TestEqual(TEXT("Three problems"), Document.Problems.Num(), 3);
			if (Document.Problems.Num() == 3)
			{
				TestEqual(TEXT("The extra cell"), Document.Problems[0].Line, 3);
				TestEqual(TEXT("The broken table"), Document.Problems[1].Line, 5);
				TestEqual(TEXT("The open code block"), Document.Problems[2].Line, 8);
			}
		});

		It("list links and images that lead nowhere, and suggest renumbered sections", [this]()
		{
			FDocument Document = Parse(TEXT("# Top\n\n## 12.5 Item Details\n\nSee [a](#top), [b](#missing), [c](#124-item-details), [d](main.cpp), [e](nope.md#x), [f](https://x.y).\n\n![pic](gone.png)\n"));
			Review(Document, Folder);
			TestEqual(TEXT("Four problems"), Document.Problems.Num(), 4);
			if (Document.Problems.Num() == 4)
			{
				TestEqual(TEXT("Missing anchor"), Document.Problems[0].Message, FString(TEXT("No heading has the anchor #missing.")));
				TestEqual(TEXT("Renumbered"), Document.Problems[1].Message, FString(TEXT("No heading has the anchor #124-item-details. Did you mean #125-item-details?")));
				TestEqual(TEXT("Missing file"), Document.Problems[2].Message, FString(TEXT("Links to nope.md, which isn't there.")));
				TestEqual(TEXT("Missing image on its line"), Document.Problems[3].Line, 7);
			}
		});
	});

	// The documentation window follows the guide's links between chapters, so a broken one is a dead end for readers there too.
	Describe("The plugin's guide", [this]()
	{
		It("has no broken links, images, tables or code blocks", [this]()
		{
			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ModularInventory"));
			if (!TestTrue(TEXT("The plugin is found"), Plugin.IsValid()))
			{
				return;
			}

			const FString Guide = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("README.md"));
			FString Text;
			if (!TestTrue(TEXT("The guide can be read"), FFileHelper::LoadFileToString(Text, *Guide)))
			{
				return;
			}

			FDocument Document = Parse(Text);
			Review(Document, FPaths::GetPath(Guide));
			for (const FProblem& Problem : Document.Problems)
			{
				AddError(FString::Printf(TEXT("README.md line %d: %s"), Problem.Line, *Problem.Message));
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
