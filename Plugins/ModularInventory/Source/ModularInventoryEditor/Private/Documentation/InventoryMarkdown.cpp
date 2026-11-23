#include "Documentation/InventoryMarkdown.h"

#include "Misc/Paths.h"

namespace InventoryMarkdown
{
	namespace
	{
		/** How deep lists and quotes nest, and emphasis and links inside each other, before the rest is read as text. */
		constexpr int32 MaxBlockDepth = 32;
		constexpr int32 MaxInlineDepth = 24;

		struct FLine
		{
			FString Text;
			int32 Number = 0;
		};

		/** Where each line of a paragraph starts in its joined text. */
		struct FLineStart
		{
			int32 Offset = 0;
			int32 Number = 0;
		};

		/** Link reference definitions, [label]: destination, by their normalized label. */
		using FDefinitions = TMap<FString, FString>;

		bool IsAsciiPunctuation(TCHAR Char)
		{
			return (Char >= TEXT('!') && Char <= TEXT('/')) || (Char >= TEXT(':') && Char <= TEXT('@'))
				|| (Char >= TEXT('[') && Char <= TEXT('`')) || (Char >= TEXT('{') && Char <= TEXT('~'));
		}

		bool IsPunctuation(TCHAR Char)
		{
			return IsAsciiPunctuation(Char) || (Char > 127 && FChar::IsPunct(Char));
		}

		int32 IndentOf(const FString& Text)
		{
			int32 Indent = 0;
			while (Indent < Text.Len() && Text[Indent] == TEXT(' '))
			{
				++Indent;
			}
			return Indent;
		}

		bool IsBlank(const FString& Text)
		{
			for (int32 Index = 0; Index < Text.Len(); ++Index)
			{
				if (!FChar::IsWhitespace(Text[Index]))
				{
					return false;
				}
			}
			return true;
		}

		/** Drops up to Columns leading spaces. */
		FString Unindent(const FString& Text, int32 Columns)
		{
			return Text.RightChop(FMath::Min(IndentOf(Text), Columns));
		}

		/** Tabs become spaces up to the next multiple of four, as Markdown counts them. */
		FString ExpandTabs(const FString& Text)
		{
			FString Result;
			Result.Reserve(Text.Len());
			for (int32 Index = 0; Index < Text.Len(); ++Index)
			{
				if (Text[Index] != TEXT('\t'))
				{
					Result.AppendChar(Text[Index]);
					continue;
				}
				do
				{
					Result.AppendChar(TEXT(' '));
				}
				while (Result.Len() % 4 != 0);
			}
			return Result;
		}

		TArray<FLine> SplitLines(const FString& Text)
		{
			TArray<FLine> Lines;
			int32 LineBegin = 0;
			int32 Number = 1;
			for (int32 Index = 0; Index <= Text.Len(); ++Index)
			{
				const bool bEnd = Index == Text.Len();
				if (!bEnd && Text[Index] != TEXT('\n') && Text[Index] != TEXT('\r'))
				{
					continue;
				}
				FLine& Line = Lines.AddDefaulted_GetRef();
				Line.Text = ExpandTabs(Text.Mid(LineBegin, Index - LineBegin));
				Line.Number = Number++;
				if (!bEnd && Text[Index] == TEXT('\r') && Index + 1 < Text.Len() && Text[Index + 1] == TEXT('\n'))
				{
					++Index;
				}
				LineBegin = Index + 1;
			}
			return Lines;
		}

		int32 HexValue(TCHAR Char)
		{
			if (Char >= TEXT('0') && Char <= TEXT('9'))
			{
				return Char - TEXT('0');
			}
			if (Char >= TEXT('a') && Char <= TEXT('f'))
			{
				return Char - TEXT('a') + 10;
			}
			if (Char >= TEXT('A') && Char <= TEXT('F'))
			{
				return Char - TEXT('A') + 10;
			}
			return INDEX_NONE;
		}

		/** %20 and the like, as in links. Only plain ASCII is decoded; anything else is left as written. */
		FString PercentDecode(const FString& Text)
		{
			FString Result;
			Result.Reserve(Text.Len());
			for (int32 Index = 0; Index < Text.Len(); ++Index)
			{
				if (Text[Index] == TEXT('%') && Index + 2 < Text.Len())
				{
					const int32 High = HexValue(Text[Index + 1]);
					const int32 Low = HexValue(Text[Index + 2]);
					if (High != INDEX_NONE && Low != INDEX_NONE && High < 8)
					{
						Result.AppendChar(static_cast<TCHAR>(High * 16 + Low));
						Index += 2;
						continue;
					}
				}
				Result.AppendChar(Text[Index]);
			}
			return Result;
		}

		/** A label as links match it: case and runs of spaces don't matter. */
		FString NormalizeLabel(const FString& Label)
		{
			FString Result;
			bool bPendingSpace = false;
			for (int32 Index = 0; Index < Label.Len(); ++Index)
			{
				const TCHAR Char = Label[Index];
				if (FChar::IsWhitespace(Char))
				{
					bPendingSpace = !Result.IsEmpty();
					continue;
				}
				if (bPendingSpace)
				{
					Result.AppendChar(TEXT(' '));
					bPendingSpace = false;
				}
				Result.AppendChar(FChar::ToLower(Char));
			}
			return Result;
		}

		//~ Block starts

		struct FFence
		{
			TCHAR Char = 0;
			int32 Length = 0;
			int32 Indent = 0;
			FString Info;
		};

		bool ParseFenceOpen(const FString& Text, FFence& OutFence)
		{
			const int32 Indent = IndentOf(Text);
			if (Indent > 3 || Indent >= Text.Len() || (Text[Indent] != TEXT('`') && Text[Indent] != TEXT('~')))
			{
				return false;
			}
			const TCHAR Char = Text[Indent];
			int32 RunEnd = Indent;
			while (RunEnd < Text.Len() && Text[RunEnd] == Char)
			{
				++RunEnd;
			}
			if (RunEnd - Indent < 3)
			{
				return false;
			}
			const FString Info = Text.Mid(RunEnd).TrimStartAndEnd();
			if (Char == TEXT('`') && Info.Contains(TEXT("`")))
			{
				return false;
			}
			OutFence.Char = Char;
			OutFence.Length = RunEnd - Indent;
			OutFence.Indent = Indent;

			// The language is the first word.
			int32 Space = INDEX_NONE;
			OutFence.Info = Info.FindChar(TEXT(' '), Space) ? Info.Left(Space) : Info;
			return true;
		}

		bool IsFenceClose(const FString& Text, TCHAR Char, int32 Length)
		{
			const int32 Indent = IndentOf(Text);
			if (Indent > 3)
			{
				return false;
			}
			int32 RunEnd = Indent;
			while (RunEnd < Text.Len() && Text[RunEnd] == Char)
			{
				++RunEnd;
			}
			return RunEnd - Indent >= Length && IsBlank(Text.Mid(RunEnd));
		}

		/** # Heading, with an optional closing run of #s. */
		bool ParseAtxHeading(const FString& Text, int32& OutLevel, FString& OutContent)
		{
			const int32 Indent = IndentOf(Text);
			if (Indent > 3)
			{
				return false;
			}
			int32 Index = Indent;
			while (Index < Text.Len() && Text[Index] == TEXT('#'))
			{
				++Index;
			}
			const int32 Level = Index - Indent;
			if (Level < 1 || Level > 6 || (Index < Text.Len() && Text[Index] != TEXT(' ')))
			{
				return false;
			}

			FString Content = Text.Mid(Index).TrimStartAndEnd();
			int32 ContentEnd = Content.Len();
			while (ContentEnd > 0 && Content[ContentEnd - 1] == TEXT('#'))
			{
				--ContentEnd;
			}
			if (ContentEnd == 0)
			{
				Content.Reset();
			}
			else if (ContentEnd < Content.Len() && Content[ContentEnd - 1] == TEXT(' '))
			{
				Content = Content.Left(ContentEnd).TrimEnd();
			}
			OutLevel = Level;
			OutContent = Content;
			return true;
		}

		/** ---, *** or ___, spaces allowed between. */
		bool IsThematicBreak(const FString& Text)
		{
			if (IndentOf(Text) > 3)
			{
				return false;
			}
			TCHAR Marker = 0;
			int32 Count = 0;
			for (int32 Index = 0; Index < Text.Len(); ++Index)
			{
				const TCHAR Char = Text[Index];
				if (Char == TEXT(' '))
				{
					continue;
				}
				if ((Char != TEXT('-') && Char != TEXT('*') && Char != TEXT('_')) || (Marker != 0 && Char != Marker))
				{
					return false;
				}
				Marker = Char;
				++Count;
			}
			return Count >= 3;
		}

		/** === or --- under a paragraph makes it a heading: 1 or 2, or 0 when the line isn't one. */
		int32 SetextLevel(const FString& Text)
		{
			if (IndentOf(Text) > 3)
			{
				return 0;
			}
			const FString Trimmed = Text.TrimStartAndEnd();
			if (Trimmed.IsEmpty() || (Trimmed[0] != TEXT('=') && Trimmed[0] != TEXT('-')))
			{
				return 0;
			}
			for (int32 Index = 1; Index < Trimmed.Len(); ++Index)
			{
				if (Trimmed[Index] != Trimmed[0])
				{
					return 0;
				}
			}
			return Trimmed[0] == TEXT('=') ? 1 : 2;
		}

		bool IsQuoteStart(const FString& Text)
		{
			const int32 Indent = IndentOf(Text);
			return Indent <= 3 && Indent < Text.Len() && Text[Indent] == TEXT('>');
		}

		struct FListMarker
		{
			bool bOrdered = false;

			/** The bullet, - * or +, or what follows the number, . or ). Lists change when it does. */
			TCHAR Delimiter = 0;

			int32 Start = 1;

			/** Where the item's content starts: lines indented this far belong to it. */
			int32 ContentColumn = 0;

			/** Nothing follows the marker on its line. */
			bool bEmpty = false;
		};

		bool ParseListMarker(const FString& Text, FListMarker& OutMarker)
		{
			const int32 Indent = IndentOf(Text);
			if (Indent > 3 || Indent >= Text.Len())
			{
				return false;
			}

			int32 Index = Indent;
			const TCHAR First = Text[Index];
			if (First == TEXT('-') || First == TEXT('*') || First == TEXT('+'))
			{
				OutMarker.bOrdered = false;
				OutMarker.Delimiter = First;
				++Index;
			}
			else if (FChar::IsDigit(First))
			{
				int32 Number = 0;
				while (Index < Text.Len() && FChar::IsDigit(Text[Index]) && Index - Indent < 9)
				{
					Number = Number * 10 + (Text[Index] - TEXT('0'));
					++Index;
				}
				if (Index >= Text.Len() || (Text[Index] != TEXT('.') && Text[Index] != TEXT(')')))
				{
					return false;
				}
				OutMarker.bOrdered = true;
				OutMarker.Delimiter = Text[Index];
				OutMarker.Start = Number;
				++Index;
			}
			else
			{
				return false;
			}

			const int32 MarkerEnd = Index;
			if (Index < Text.Len() && Text[Index] != TEXT(' '))
			{
				return false;
			}
			int32 Spaces = 0;
			while (Index < Text.Len() && Text[Index] == TEXT(' '))
			{
				++Spaces;
				++Index;
			}
			OutMarker.bEmpty = Index >= Text.Len();

			// Five or more spaces start indented code inside the item, which then starts one space after the marker.
			OutMarker.ContentColumn = (OutMarker.bEmpty || Spaces > 4) ? MarkerEnd + 1 : MarkerEnd + Spaces;
			return true;
		}

		/** <div>, </details>, <!-- and the like at the start of a line. Only some can interrupt a paragraph. */
		bool IsHtmlStart(const FString& Text, bool bInterruptingParagraph)
		{
			const int32 Indent = IndentOf(Text);
			if (Indent > 3 || Indent >= Text.Len() || Text[Indent] != TEXT('<'))
			{
				return false;
			}
			const FString Rest = Text.RightChop(Indent + 1);
			if (Rest.StartsWith(TEXT("!--")))
			{
				return true;
			}

			const int32 NameBegin = Rest.StartsWith(TEXT("/")) ? 1 : 0;
			int32 NameEnd = NameBegin;
			while (NameEnd < Rest.Len() && FChar::IsAlnum(Rest[NameEnd]))
			{
				++NameEnd;
			}
			if (NameEnd == NameBegin || (NameEnd < Rest.Len() && Rest[NameEnd] != TEXT(' ') && Rest[NameEnd] != TEXT('>') && Rest[NameEnd] != TEXT('/')))
			{
				return false;
			}

			static const TCHAR* const BlockTags[] =
			{
				TEXT("address"), TEXT("article"), TEXT("aside"), TEXT("blockquote"), TEXT("body"), TEXT("caption"), TEXT("center"),
				TEXT("col"), TEXT("colgroup"), TEXT("dd"), TEXT("details"), TEXT("dialog"), TEXT("div"), TEXT("dl"), TEXT("dt"),
				TEXT("fieldset"), TEXT("figcaption"), TEXT("figure"), TEXT("footer"), TEXT("form"), TEXT("h1"), TEXT("h2"), TEXT("h3"),
				TEXT("h4"), TEXT("h5"), TEXT("h6"), TEXT("header"), TEXT("hr"), TEXT("html"), TEXT("iframe"), TEXT("legend"), TEXT("li"),
				TEXT("main"), TEXT("menu"), TEXT("nav"), TEXT("ol"), TEXT("p"), TEXT("picture"), TEXT("section"), TEXT("summary"),
				TEXT("table"), TEXT("tbody"), TEXT("td"), TEXT("tfoot"), TEXT("th"), TEXT("thead"), TEXT("tr"), TEXT("ul")
			};
			const FString Name = Rest.Mid(NameBegin, NameEnd - NameBegin);
			for (const TCHAR* Tag : BlockTags)
			{
				if (Name.Equals(Tag, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}

			// Any other tag alone on its line, such as <img src="logo.png">, starts a block too, but can't interrupt a paragraph.
			const FString Trimmed = Text.TrimStartAndEnd();
			int32 FirstClose = INDEX_NONE;
			return !bInterruptingParagraph && Trimmed.FindChar(TEXT('>'), FirstClose) && FirstClose == Trimmed.Len() - 1;
		}

		/** A line that would start a block of its own rather than continue a paragraph lazily, such as in a quote or list item. */
		bool StartsBlock(const FString& Text)
		{
			if (IsBlank(Text))
			{
				return true;
			}
			if (IndentOf(Text) >= 4)
			{
				return false;
			}
			FFence Fence;
			int32 Level = 0;
			FString Content;
			FListMarker Marker;
			return ParseFenceOpen(Text, Fence) || ParseAtxHeading(Text, Level, Content) || IsThematicBreak(Text) || IsQuoteStart(Text)
				|| ParseListMarker(Text, Marker) || IsHtmlStart(Text, true);
		}

		/** [label]: destination "optional title", alone on its line. */
		bool ParseDefinition(const FString& Text, FString& OutLabel, FString& OutDestination)
		{
			const int32 Indent = IndentOf(Text);
			if (Indent > 3 || Indent >= Text.Len() || Text[Indent] != TEXT('['))
			{
				return false;
			}
			int32 Close = INDEX_NONE;
			for (int32 Index = Indent + 1; Index < Text.Len(); ++Index)
			{
				if (Text[Index] == TEXT('\\'))
				{
					++Index;
				}
				else if (Text[Index] == TEXT('['))
				{
					return false;
				}
				else if (Text[Index] == TEXT(']'))
				{
					Close = Index;
					break;
				}
			}
			if (Close == INDEX_NONE || Close + 1 >= Text.Len() || Text[Close + 1] != TEXT(':'))
			{
				return false;
			}

			// Footnotes, [^1]: text, aren't links.
			OutLabel = NormalizeLabel(Text.Mid(Indent + 1, Close - Indent - 1));
			if (OutLabel.IsEmpty() || OutLabel.StartsWith(TEXT("^")))
			{
				return false;
			}

			FString Rest = Text.Mid(Close + 2).TrimStartAndEnd();
			if (Rest.StartsWith(TEXT("<")))
			{
				int32 Closing = INDEX_NONE;
				if (!Rest.FindChar(TEXT('>'), Closing))
				{
					return false;
				}
				OutDestination = Rest.Mid(1, Closing - 1);
				Rest = Rest.Mid(Closing + 1).TrimStart();
			}
			else
			{
				int32 DestinationEnd = 0;
				while (DestinationEnd < Rest.Len() && !FChar::IsWhitespace(Rest[DestinationEnd]))
				{
					++DestinationEnd;
				}
				OutDestination = Rest.Left(DestinationEnd);
				Rest = Rest.Mid(DestinationEnd).TrimStart();
			}
			if (OutDestination.IsEmpty())
			{
				return false;
			}
			if (Rest.IsEmpty())
			{
				return true;
			}

			// Anything after the destination has to be a title.
			const TCHAR Open = Rest[0];
			const TCHAR Last = Rest[Rest.Len() - 1];
			return Rest.Len() >= 2 && ((Open == TEXT('"') && Last == TEXT('"')) || (Open == TEXT('\'') && Last == TEXT('\'')) || (Open == TEXT('(') && Last == TEXT(')')));
		}

		/** Splits a table row at its pipes, leaving out the outer ones. A pipe written \| is part of its cell. */
		TArray<FString> SplitTableRow(const FString& Text)
		{
			FString Row = Text.TrimStartAndEnd();
			if (Row.StartsWith(TEXT("|")))
			{
				Row = Row.RightChop(1);
			}
			if (Row.EndsWith(TEXT("|")) && !Row.EndsWith(TEXT("\\|")))
			{
				Row = Row.LeftChop(1);
			}

			TArray<FString> Cells;
			FString Cell;
			for (int32 Index = 0; Index < Row.Len(); ++Index)
			{
				const TCHAR Char = Row[Index];
				if (Char == TEXT('\\') && Index + 1 < Row.Len() && Row[Index + 1] == TEXT('|'))
				{
					Cell.AppendChar(TEXT('|'));
					++Index;
				}
				else if (Char == TEXT('|'))
				{
					Cells.Add(Cell.TrimStartAndEnd());
					Cell.Reset();
				}
				else
				{
					Cell.AppendChar(Char);
				}
			}
			Cells.Add(Cell.TrimStartAndEnd());
			return Cells;
		}

		/** The line under a table's header: a cell of dashes per column, with colons for alignment. */
		bool ParseDelimiterRow(const FString& Text, TArray<EColumnAlignment>& OutAlignments)
		{
			OutAlignments.Reset();
			if (IndentOf(Text) > 3 || IsBlank(Text))
			{
				return false;
			}
			for (const FString& Cell : SplitTableRow(Text))
			{
				const bool bLeftColon = Cell.StartsWith(TEXT(":"));
				const bool bRightColon = Cell.EndsWith(TEXT(":"));
				const int32 DashBegin = bLeftColon ? 1 : 0;
				const int32 DashEnd = Cell.Len() - (bRightColon ? 1 : 0);
				if (DashEnd <= DashBegin)
				{
					return false;
				}
				for (int32 Index = DashBegin; Index < DashEnd; ++Index)
				{
					if (Cell[Index] != TEXT('-'))
					{
						return false;
					}
				}
				OutAlignments.Add((bLeftColon && bRightColon) ? EColumnAlignment::Center : (bRightColon ? EColumnAlignment::Right : EColumnAlignment::Left));
			}
			return !OutAlignments.IsEmpty();
		}

		/** A table header at Index: the line under it is a delimiter row with as many cells. */
		bool IsTableStart(const TArray<FLine>& Lines, int32 Index)
		{
			if (Index + 1 >= Lines.Num() || IndentOf(Lines[Index].Text) > 3 || SetextLevel(Lines[Index + 1].Text) > 0)
			{
				return false;
			}
			TArray<EColumnAlignment> Alignments;
			return ParseDelimiterRow(Lines[Index + 1].Text, Alignments) && SplitTableRow(Lines[Index].Text).Num() == Alignments.Num();
		}

		/** Whether the line at Index ends the paragraph before it, rather than continuing it. */
		bool InterruptsParagraph(const TArray<FLine>& Lines, int32 Index)
		{
			const FString& Text = Lines[Index].Text;
			if (IsBlank(Text))
			{
				return true;
			}
			if (IndentOf(Text) >= 4)
			{
				return false;
			}
			FFence Fence;
			int32 Level = 0;
			FString Content;
			if (ParseFenceOpen(Text, Fence) || ParseAtxHeading(Text, Level, Content) || IsThematicBreak(Text) || IsQuoteStart(Text) || IsHtmlStart(Text, true))
			{
				return true;
			}
			// A list interrupts a paragraph only if its item has text, and a numbered one only if it starts at 1.
			FListMarker Marker;
			if (ParseListMarker(Text, Marker) && !Marker.bEmpty && (!Marker.bOrdered || Marker.Start == 1))
			{
				return true;
			}
			return IsTableStart(Lines, Index);
		}

		/** The lines of a paragraph as one text. A line break is a space, or a line break where the line ends in a backslash or two spaces. */
		FString JoinLines(const TArray<FLine>& Lines, int32 Begin, int32 End, TArray<FLineStart>& OutLineStarts)
		{
			FString Joined;
			for (int32 Index = Begin; Index < End; ++Index)
			{
				FString Text = Lines[Index].Text.TrimStart();
				bool bHardBreak = false;
				if (Index + 1 < End)
				{
					if (Text.EndsWith(TEXT("  ")))
					{
						bHardBreak = true;
					}
					else if (Text.EndsWith(TEXT("\\")) && !Text.EndsWith(TEXT("\\\\")))
					{
						bHardBreak = true;
						Text = Text.LeftChop(1);
					}
				}
				Text.TrimEndInline();

				FLineStart& Start = OutLineStarts.AddDefaulted_GetRef();
				Start.Offset = Joined.Len();
				Start.Number = Lines[Index].Number;
				Joined += Text;
				if (Index + 1 < End)
				{
					Joined.AppendChar(bHardBreak ? TEXT('\n') : TEXT(' '));
				}
			}
			return Joined;
		}

		//~ Inline text

		struct FLinkParts
		{
			int32 TextBegin = 0;
			int32 TextEnd = 0;
			int32 End = 0;
			FString Destination;
		};

		/** Reads the text of one paragraph, heading or cell into runs, following CommonMark's rules closely enough for READMEs. */
		class FInlineParser
		{
		public:
			FInlineParser(const FString& InSource, TArray<FLineStart> InLineStarts, const FDefinitions* InDefinitions)
				: Source(InSource)
				, LineStarts(MoveTemp(InLineStarts))
				, Definitions(InDefinitions)
			{
				FindCodeSpanClosers();
				FindBracketClosers();
			}

			void Parse(TArray<FInlineRun>& OutRuns)
			{
				ParseRange(0, Source.Len(), EInlineStyle::None, FString(), 0, OutRuns);
			}

		private:
			int32 LineAt(int32 Offset) const
			{
				int32 Number = LineStarts.IsEmpty() ? 0 : LineStarts[0].Number;
				for (const FLineStart& Start : LineStarts)
				{
					if (Start.Offset > Offset)
					{
						break;
					}
					Number = Start.Number;
				}
				return Number;
			}

			/** The characters around a run, with the edges of the text counting as spaces. */
			TCHAR CharBefore(int32 Index) const
			{
				return Index > 0 ? Source[Index - 1] : TEXT(' ');
			}

			TCHAR CharAt(int32 Index) const
			{
				return Index < Source.Len() ? Source[Index] : TEXT(' ');
			}

			int32 SkipRun(int32 Index, int32 End, TCHAR Char) const
			{
				while (Index < End && Source[Index] == Char)
				{
					++Index;
				}
				return Index;
			}

			FInlineRun& AddRun(TArray<FInlineRun>& OutRuns, const FString& Text, EInlineStyle Style, const FString& Link, bool bImage, int32 Offset) const
			{
				if (!bImage && OutRuns.Num() > 0)
				{
					FInlineRun& Previous = OutRuns.Last();
					if (!Previous.bImage && Previous.Style == Style && Previous.Link.Equals(Link, ESearchCase::CaseSensitive))
					{
						Previous.Text += Text;
						return Previous;
					}
				}
				FInlineRun& Run = OutRuns.AddDefaulted_GetRef();
				Run.Text = Text;
				Run.Style = Style;
				Run.Link = Link;
				Run.bImage = bImage;
				Run.Line = LineAt(Offset);
				return Run;
			}

			/** For each run of backticks, the next run as long after it, which closes a code span the first opens. Found once, from the end. */
			void FindCodeSpanClosers()
			{
				CodeSpanClosers.Init(INDEX_NONE, Source.Len());
				TMap<int32, int32> NextRunOfLength;
				int32 RunEnd = Source.Len();
				while (RunEnd > 0)
				{
					if (Source[RunEnd - 1] != TEXT('`'))
					{
						--RunEnd;
						continue;
					}
					int32 RunBegin = RunEnd - 1;
					while (RunBegin > 0 && Source[RunBegin - 1] == TEXT('`'))
					{
						--RunBegin;
					}
					const int32 Length = RunEnd - RunBegin;
					if (const int32* Next = NextRunOfLength.Find(Length))
					{
						CodeSpanClosers[RunBegin] = *Next;
					}
					NextRunOfLength.Add(Length, RunBegin);
					RunEnd = RunBegin;
				}
			}

			/** For each [, the ] that closes it. Brackets nest; escaped ones and those in code don't count. Found once, front to back. */
			void FindBracketClosers()
			{
				BracketClosers.Init(INDEX_NONE, Source.Len());
				TArray<int32> Opened;
				int32 Scan = 0;
				while (Scan < Source.Len())
				{
					const TCHAR Current = Source[Scan];
					if (Current == TEXT('\\'))
					{
						Scan += 2;
						continue;
					}
					if (Current == TEXT('`'))
					{
						Scan = SkipBackticks(Scan, Source.Len());
						continue;
					}
					if (Current == TEXT('['))
					{
						Opened.Add(Scan);
					}
					else if (Current == TEXT(']') && !Opened.IsEmpty())
					{
						BracketClosers[Opened.Last()] = Scan;
						Opened.Pop();
					}
					++Scan;
				}
			}

			/** A code span opening at Index: where its content is and where it ends. False when no run of as many backticks closes it. */
			bool FindCodeSpan(int32 Index, int32 End, int32& OutContentBegin, int32& OutContentEnd, int32& OutAfter) const
			{
				const int32 OpenEnd = SkipRun(Index, End, TEXT('`'));
				const int32 Length = OpenEnd - Index;
				int32 Close = INDEX_NONE;
				if (Index == 0 || Source[Index - 1] != TEXT('`'))
				{
					Close = CodeSpanClosers[Index];
				}
				else
				{
					// After an escaped backtick the run starts part way along, so it's searched for.
					for (int32 Scan = OpenEnd; Scan < End && Close == INDEX_NONE;)
					{
						const int32 ScanEnd = SkipRun(Scan, End, TEXT('`'));
						if (ScanEnd > Scan && ScanEnd - Scan == Length)
						{
							Close = Scan;
						}
						Scan = (ScanEnd > Scan) ? ScanEnd : Scan + 1;
					}
				}
				if (Close == INDEX_NONE || Close + Length > End)
				{
					return false;
				}
				OutContentBegin = OpenEnd;
				OutContentEnd = Close;
				OutAfter = Close + Length;
				return true;
			}

			/** Past the code span or the run of backticks at Index. */
			int32 SkipBackticks(int32 Index, int32 End) const
			{
				int32 ContentBegin = 0;
				int32 ContentEnd = 0;
				int32 After = 0;
				return FindCodeSpan(Index, End, ContentBegin, ContentEnd, After) ? After : SkipRun(Index, End, TEXT('`'));
			}

			FString CodeText(int32 Begin, int32 End) const
			{
				FString Code = Source.Mid(Begin, End - Begin).Replace(TEXT("\n"), TEXT(" "));
				if (Code.Len() >= 2 && Code.StartsWith(TEXT(" ")) && Code.EndsWith(TEXT(" ")) && !IsBlank(Code))
				{
					Code = Code.Mid(1, Code.Len() - 2);
				}
				return Code;
			}

			bool IsLeftFlanking(int32 RunBegin, int32 RunEnd) const
			{
				const TCHAR Before = CharBefore(RunBegin);
				const TCHAR After = CharAt(RunEnd);
				return !FChar::IsWhitespace(After) && (!IsPunctuation(After) || FChar::IsWhitespace(Before) || IsPunctuation(Before));
			}

			bool IsRightFlanking(int32 RunBegin, int32 RunEnd) const
			{
				const TCHAR Before = CharBefore(RunBegin);
				const TCHAR After = CharAt(RunEnd);
				return !FChar::IsWhitespace(Before) && (!IsPunctuation(Before) || FChar::IsWhitespace(After) || IsPunctuation(After));
			}

			/** Underscores inside words, as in snake_case, don't open or close emphasis. */
			bool CanOpen(int32 RunBegin, int32 RunEnd) const
			{
				const bool bLeft = IsLeftFlanking(RunBegin, RunEnd);
				if (Source[RunBegin] != TEXT('_'))
				{
					return bLeft;
				}
				return bLeft && (!IsRightFlanking(RunBegin, RunEnd) || IsPunctuation(CharBefore(RunBegin)));
			}

			bool CanClose(int32 RunBegin, int32 RunEnd) const
			{
				const bool bRight = IsRightFlanking(RunBegin, RunEnd);
				if (Source[RunBegin] != TEXT('_'))
				{
					return bRight;
				}
				return bRight && (!IsLeftFlanking(RunBegin, RunEnd) || IsPunctuation(CharAt(RunEnd)));
			}

			static EInlineStyle EmphasisStyle(TCHAR Char, int32 Length)
			{
				if (Char == TEXT('~'))
				{
					return EInlineStyle::Strikethrough;
				}
				return Length == 1 ? EInlineStyle::Italic : (Length == 2 ? EInlineStyle::Bold : (EInlineStyle::Bold | EInlineStyle::Italic));
			}

			/** Where the run that closes emphasis opened with Length of Char starts, or INDEX_NONE. */
			int32 FindCloser(int32 From, int32 End, TCHAR Char, int32 Length)
			{
				const uint64 Key = (static_cast<uint64>(From) << 36) | (static_cast<uint64>(End) << 8) | (static_cast<uint64>(Char & 0x3F) << 2) | static_cast<uint64>(Length & 3);
				if (const int32* Cached = CloserCache.Find(Key))
				{
					return *Cached;
				}
				if (ScanDepth >= MaxInlineDepth)
				{
					return INDEX_NONE;
				}
				++ScanDepth;
				const int32 Closer = ScanForCloser(From, End, Char, Length);
				--ScanDepth;
				CloserCache.Add(Key, Closer);
				return Closer;
			}

			int32 ScanForCloser(int32 From, int32 End, TCHAR Char, int32 Length)
			{
				int32 Scan = From;
				while (Scan < End)
				{
					// Text built to defeat this, with thousands of unmatched *, gets its remaining emphasis read as text rather than freezing the editor.
					if (--WorkLeft <= 0)
					{
						return INDEX_NONE;
					}
					const TCHAR Current = Source[Scan];
					if (Current == TEXT('\\'))
					{
						Scan += 2;
						continue;
					}
					if (Current == TEXT('`'))
					{
						Scan = SkipBackticks(Scan, End);
						continue;
					}
					if (Current == TEXT('['))
					{
						// Links bind tighter than emphasis.
						FLinkParts Parts;
						Scan = ParseLink(Scan, End, Parts) ? Parts.End : Scan + 1;
						continue;
					}
					if (Current != TEXT('*') && Current != TEXT('_') && Current != TEXT('~'))
					{
						++Scan;
						continue;
					}

					const int32 RunEnd = SkipRun(Scan, End, Current);
					const int32 RunLength = RunEnd - Scan;
					const bool bLengthFits = (Char == TEXT('~')) ? RunLength == Length : RunLength >= Length;
					if (Current == Char && bLengthFits && CanClose(Scan, RunEnd))
					{
						return Scan;
					}

					// A pair inside this one is skipped whole, so its closer isn't taken for this one's.
					int32 InnerEnd = INDEX_NONE;
					if (CanOpen(Scan, RunEnd) && (Current != TEXT('~') || RunLength == 2))
					{
						for (int32 InnerLength = FMath::Min(RunLength, 3); InnerLength > 0 && InnerEnd == INDEX_NONE; --InnerLength)
						{
							if (Current == TEXT('~') && InnerLength != 2)
							{
								continue;
							}
							const int32 InnerCloser = FindCloser(RunEnd, End, Current, InnerLength);
							if (InnerCloser != INDEX_NONE)
							{
								InnerEnd = InnerCloser + InnerLength;
							}
						}
					}
					Scan = (InnerEnd != INDEX_NONE) ? InnerEnd : RunEnd;
				}
				return INDEX_NONE;
			}

			/** The ] that closes the [ at Open, before End. */
			int32 FindBracketClose(int32 Open, int32 End) const
			{
				const int32 Close = BracketClosers[Open];
				return (Close != INDEX_NONE && Close < End) ? Close : INDEX_NONE;
			}

			/** The (destination "title") after a link's text, from just inside the (. */
			bool ParseDestination(int32 Begin, int32 End, FString& OutDestination, int32& OutEnd) const
			{
				int32 Scan = Begin;
				while (Scan < End && FChar::IsWhitespace(Source[Scan]))
				{
					++Scan;
				}

				FString Destination;
				if (Scan < End && Source[Scan] == TEXT('<'))
				{
					++Scan;
					while (Scan < End && Source[Scan] != TEXT('>'))
					{
						if (Source[Scan] == TEXT('\n') || Source[Scan] == TEXT('<'))
						{
							return false;
						}
						Destination.AppendChar(Source[Scan]);
						++Scan;
					}
					if (Scan >= End)
					{
						return false;
					}
					++Scan;
				}
				else
				{
					int32 Parentheses = 0;
					while (Scan < End)
					{
						const TCHAR Current = Source[Scan];
						if (Current == TEXT('\\') && Scan + 1 < End && IsAsciiPunctuation(Source[Scan + 1]))
						{
							Destination.AppendChar(Source[Scan + 1]);
							Scan += 2;
							continue;
						}
						if (FChar::IsWhitespace(Current))
						{
							break;
						}
						if (Current == TEXT('('))
						{
							++Parentheses;
						}
						else if (Current == TEXT(')'))
						{
							if (Parentheses == 0)
							{
								break;
							}
							--Parentheses;
						}
						Destination.AppendChar(Current);
						++Scan;
					}
				}

				while (Scan < End && FChar::IsWhitespace(Source[Scan]))
				{
					++Scan;
				}
				if (Scan < End && (Source[Scan] == TEXT('"') || Source[Scan] == TEXT('\'') || Source[Scan] == TEXT('(')))
				{
					const TCHAR Closing = (Source[Scan] == TEXT('(')) ? TEXT(')') : Source[Scan];
					++Scan;
					while (Scan < End && Source[Scan] != Closing)
					{
						Scan += (Source[Scan] == TEXT('\\')) ? 2 : 1;
					}
					if (Scan >= End)
					{
						return false;
					}
					++Scan;
					while (Scan < End && FChar::IsWhitespace(Source[Scan]))
					{
						++Scan;
					}
				}
				if (Scan >= End || Source[Scan] != TEXT(')'))
				{
					return false;
				}
				OutDestination = Destination;
				OutEnd = Scan + 1;
				return true;
			}

			/** A link or image from its [: [text](destination), or [text][label], [label][] or [label] with a definition. */
			bool ParseLink(int32 Open, int32 End, FLinkParts& OutParts) const
			{
				const int32 Close = FindBracketClose(Open, End);
				if (Close == INDEX_NONE)
				{
					return false;
				}
				OutParts.TextBegin = Open + 1;
				OutParts.TextEnd = Close;
				if (Close + 1 < End && Source[Close + 1] == TEXT('(') && ParseDestination(Close + 2, End, OutParts.Destination, OutParts.End))
				{
					return true;
				}
				if (!Definitions)
				{
					return false;
				}

				FString Label = Source.Mid(Open + 1, Close - Open - 1);
				int32 After = Close + 1;
				if (Close + 1 < End && Source[Close + 1] == TEXT('['))
				{
					const int32 LabelClose = FindBracketClose(Close + 1, End);
					if (LabelClose != INDEX_NONE)
					{
						if (LabelClose > Close + 2)
						{
							Label = Source.Mid(Close + 2, LabelClose - Close - 2);
						}
						After = LabelClose + 1;
					}
				}
				const FString* Defined = Definitions->Find(NormalizeLabel(Label));
				if (!Defined)
				{
					return false;
				}
				OutParts.Destination = *Defined;
				OutParts.End = After;
				return true;
			}

			/** <https://example.com> or <name@example.com>. */
			bool ParseAutolink(int32 Open, int32 End, FString& OutLink, FString& OutText, int32& OutEnd) const
			{
				int32 Scan = Open + 1;
				while (Scan < End && Source[Scan] != TEXT('>'))
				{
					if (FChar::IsWhitespace(Source[Scan]) || Source[Scan] == TEXT('<'))
					{
						return false;
					}
					++Scan;
				}
				if (Scan >= End)
				{
					return false;
				}

				const FString Inside = Source.Mid(Open + 1, Scan - Open - 1);
				int32 Colon = INDEX_NONE;
				bool bScheme = Inside.FindChar(TEXT(':'), Colon) && Colon >= 2 && Colon <= 32 && FChar::IsAlpha(Inside[0]);
				for (int32 Index = 0; bScheme && Index < Colon; ++Index)
				{
					const TCHAR Char = Inside[Index];
					bScheme = FChar::IsAlnum(Char) || Char == TEXT('+') || Char == TEXT('.') || Char == TEXT('-');
				}
				int32 At = INDEX_NONE;
				const bool bEmail = !bScheme && Inside.FindChar(TEXT('@'), At) && At > 0 && At < Inside.Len() - 1;
				if (!bScheme && !bEmail)
				{
					return false;
				}
				OutText = Inside;
				OutLink = bEmail ? (TEXT("mailto:") + Inside) : Inside;
				OutEnd = Scan + 1;
				return true;
			}

			/** <br> as a line break, and comments, which aren't shown. Where it ends, or INDEX_NONE for any other HTML, which shows as written. */
			int32 ParseInlineHtml(int32 Open, int32 End, bool& bOutLineBreak) const
			{
				bOutLineBreak = false;
				if (Source.Mid(Open, 4) == TEXT("<!--"))
				{
					const int32 Close = Source.Find(TEXT("-->"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 4);
					return (Close != INDEX_NONE && Close + 3 <= End) ? Close + 3 : INDEX_NONE;
				}

				int32 Scan = Open + 1;
				FString Tag;
				while (Scan < End && Source[Scan] != TEXT('>') && Scan - Open <= 8)
				{
					if (Source[Scan] != TEXT(' ') && Source[Scan] != TEXT('/'))
					{
						Tag.AppendChar(Source[Scan]);
					}
					++Scan;
				}
				if (Scan < End && Source[Scan] == TEXT('>') && Tag.Equals(TEXT("br"), ESearchCase::IgnoreCase))
				{
					bOutLineBreak = true;
					return Scan + 1;
				}
				return INDEX_NONE;
			}

			/** &amp; and the like, and numbered ones such as &#169;. Where it ends, or INDEX_NONE when it isn't one. */
			int32 ParseEntity(int32 Open, int32 End, FString& OutText) const
			{
				int32 Semicolon = INDEX_NONE;
				for (int32 Scan = Open + 1; Scan < End && Scan - Open <= 10; ++Scan)
				{
					if (Source[Scan] == TEXT(';'))
					{
						Semicolon = Scan;
						break;
					}
					if (!FChar::IsAlnum(Source[Scan]) && Source[Scan] != TEXT('#'))
					{
						break;
					}
				}
				if (Semicolon == INDEX_NONE || Semicolon == Open + 1)
				{
					return INDEX_NONE;
				}

				const FString Name = Source.Mid(Open + 1, Semicolon - Open - 1);
				if (Name[0] == TEXT('#'))
				{
					const bool bHex = Name.Len() > 1 && (Name[1] == TEXT('x') || Name[1] == TEXT('X'));
					const FString Digits = Name.RightChop(bHex ? 2 : 1);
					if (Digits.IsEmpty() || Digits.Len() > 6)
					{
						return INDEX_NONE;
					}
					int32 Code = 0;
					for (int32 Index = 0; Index < Digits.Len(); ++Index)
					{
						const int32 Value = HexValue(Digits[Index]);
						if (Value == INDEX_NONE || (!bHex && Value > 9))
						{
							return INDEX_NONE;
						}
						Code = Code * (bHex ? 16 : 10) + Value;
					}
					// Beyond the basic plane it's left as written.
					if (Code <= 0 || Code > 0xFFFF)
					{
						return INDEX_NONE;
					}
					OutText = FString::Chr(static_cast<TCHAR>(Code));
					return Semicolon + 1;
				}

				static const TCHAR* const Entities[][2] =
				{
					{ TEXT("amp"), TEXT("&") }, { TEXT("lt"), TEXT("<") }, { TEXT("gt"), TEXT(">") }, { TEXT("quot"), TEXT("\"") },
					{ TEXT("apos"), TEXT("'") }, { TEXT("nbsp"), TEXT("\u00A0") }, { TEXT("copy"), TEXT("\u00A9") }, { TEXT("reg"), TEXT("\u00AE") },
					{ TEXT("trade"), TEXT("\u2122") }, { TEXT("mdash"), TEXT("\u2014") }, { TEXT("ndash"), TEXT("\u2013") },
					{ TEXT("hellip"), TEXT("\u2026") }, { TEXT("larr"), TEXT("\u2190") }, { TEXT("rarr"), TEXT("\u2192") },
					{ TEXT("uarr"), TEXT("\u2191") }, { TEXT("darr"), TEXT("\u2193") }, { TEXT("times"), TEXT("\u00D7") },
					{ TEXT("middot"), TEXT("\u00B7") }, { TEXT("bull"), TEXT("\u2022") }, { TEXT("deg"), TEXT("\u00B0") },
					{ TEXT("plusmn"), TEXT("\u00B1") }, { TEXT("laquo"), TEXT("\u00AB") }, { TEXT("raquo"), TEXT("\u00BB") }
				};
				for (const auto& Entity : Entities)
				{
					if (Name.Equals(Entity[0], ESearchCase::CaseSensitive))
					{
						OutText = Entity[1];
						return Semicolon + 1;
					}
				}
				return INDEX_NONE;
			}

			/** A bare web address, which GitHub links: www.example.com or https://example.com. Where it ends, or INDEX_NONE. */
			int32 ParseBareLink(int32 Begin, int32 End, FString& OutLink) const
			{
				const TCHAR Before = CharBefore(Begin);
				if (!FChar::IsWhitespace(Before) && Before != TEXT('*') && Before != TEXT('_') && Before != TEXT('~') && Before != TEXT('('))
				{
					return INDEX_NONE;
				}
				const FString Start = Source.Mid(Begin, FMath::Min(End - Begin, 8));
				const bool bWww = Start.StartsWith(TEXT("www."), ESearchCase::IgnoreCase);
				if (!bWww && !Start.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) && !Start.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
				{
					return INDEX_NONE;
				}

				int32 Scan = Begin;
				while (Scan < End && !FChar::IsWhitespace(Source[Scan]) && Source[Scan] != TEXT('<'))
				{
					++Scan;
				}
				// Punctuation at the end belongs to the sentence, as does a ) without its (.
				while (Scan > Begin)
				{
					const TCHAR Last = Source[Scan - 1];
					if (Last == TEXT('?') || Last == TEXT('!') || Last == TEXT('.') || Last == TEXT(',') || Last == TEXT(':') || Last == TEXT('*')
						|| Last == TEXT('_') || Last == TEXT('~') || Last == TEXT('\'') || Last == TEXT('"'))
					{
						--Scan;
						continue;
					}
					if (Last == TEXT(')'))
					{
						int32 Balance = 0;
						for (int32 Check = Begin; Check < Scan; ++Check)
						{
							Balance += (Source[Check] == TEXT('(')) ? 1 : ((Source[Check] == TEXT(')')) ? -1 : 0);
						}
						if (Balance < 0)
						{
							--Scan;
							continue;
						}
					}
					break;
				}

				const FString Address = Source.Mid(Begin, Scan - Begin);
				if (Address.Len() <= (bWww ? 4 : 8))
				{
					return INDEX_NONE;
				}
				OutLink = bWww ? (TEXT("http://") + Address) : Address;
				return Scan;
			}

			void ParseRange(int32 Begin, int32 End, EInlineStyle Style, const FString& Link, int32 Depth, TArray<FInlineRun>& OutRuns)
			{
				if (Depth > MaxInlineDepth)
				{
					AddRun(OutRuns, Source.Mid(Begin, End - Begin), Style, Link, false, Begin);
					return;
				}

				FString Pending;
				int32 PendingOffset = Begin;
				auto AddText = [&Pending, &PendingOffset](const FString& Piece, int32 At)
				{
					if (Pending.IsEmpty())
					{
						PendingOffset = At;
					}
					Pending += Piece;
				};
				auto AddChar = [&Pending, &PendingOffset](TCHAR Piece, int32 At)
				{
					if (Pending.IsEmpty())
					{
						PendingOffset = At;
					}
					Pending.AppendChar(Piece);
				};
				auto Flush = [this, &Pending, &PendingOffset, &OutRuns, Style, &Link]()
				{
					if (!Pending.IsEmpty())
					{
						AddRun(OutRuns, Pending, Style, Link, false, PendingOffset);
						Pending.Reset();
					}
				};

				int32 Index = Begin;
				while (Index < End)
				{
					const TCHAR Char = Source[Index];

					if (Char == TEXT('\\') && Index + 1 < End && IsAsciiPunctuation(Source[Index + 1]))
					{
						AddChar(Source[Index + 1], Index);
						Index += 2;
						continue;
					}

					if (Char == TEXT('`'))
					{
						int32 ContentBegin = 0;
						int32 ContentEnd = 0;
						int32 After = 0;
						if (FindCodeSpan(Index, End, ContentBegin, ContentEnd, After))
						{
							Flush();
							AddRun(OutRuns, CodeText(ContentBegin, ContentEnd), Style | EInlineStyle::Code, Link, false, Index);
							Index = After;
							continue;
						}
						// Backticks nothing closes are text, all of them.
						const int32 RunEnd = SkipRun(Index, End, TEXT('`'));
						AddText(Source.Mid(Index, RunEnd - Index), Index);
						Index = RunEnd;
						continue;
					}

					if (Char == TEXT('!') && Index + 1 < End && Source[Index + 1] == TEXT('['))
					{
						FLinkParts Parts;
						if (ParseLink(Index + 1, End, Parts))
						{
							Flush();
							AddRun(OutRuns, Source.Mid(Parts.TextBegin, Parts.TextEnd - Parts.TextBegin), Style, Parts.Destination, true, Index);
							Index = Parts.End;
							continue;
						}
					}

					// Links can't hold links.
					if (Char == TEXT('[') && Link.IsEmpty())
					{
						FLinkParts Parts;
						if (ParseLink(Index, End, Parts))
						{
							Flush();
							ParseRange(Parts.TextBegin, Parts.TextEnd, Style, Parts.Destination, Depth + 1, OutRuns);
							Index = Parts.End;
							continue;
						}
					}

					if (Char == TEXT('<'))
					{
						FString AutoLink;
						FString AutoText;
						int32 After = INDEX_NONE;
						if (Link.IsEmpty() && ParseAutolink(Index, End, AutoLink, AutoText, After))
						{
							Flush();
							AddRun(OutRuns, AutoText, Style, AutoLink, false, Index);
							Index = After;
							continue;
						}
						bool bLineBreak = false;
						After = ParseInlineHtml(Index, End, bLineBreak);
						if (After != INDEX_NONE)
						{
							if (bLineBreak)
							{
								AddChar(TEXT('\n'), Index);
							}
							Index = After;
							continue;
						}
					}

					if (Char == TEXT('&'))
					{
						FString Decoded;
						const int32 After = ParseEntity(Index, End, Decoded);
						if (After != INDEX_NONE)
						{
							AddText(Decoded, Index);
							Index = After;
							continue;
						}
					}

					if (Link.IsEmpty() && (Char == TEXT('h') || Char == TEXT('H') || Char == TEXT('w') || Char == TEXT('W')))
					{
						FString BareLink;
						const int32 After = ParseBareLink(Index, End, BareLink);
						if (After != INDEX_NONE)
						{
							Flush();
							AddRun(OutRuns, Source.Mid(Index, After - Index), Style, BareLink, false, Index);
							Index = After;
							continue;
						}
					}

					if (Char == TEXT('*') || Char == TEXT('_') || Char == TEXT('~'))
					{
						const int32 RunEnd = SkipRun(Index, End, Char);
						const int32 RunLength = RunEnd - Index;
						bool bMatched = false;
						if (CanOpen(Index, RunEnd) && (Char != TEXT('~') || RunLength == 2))
						{
							for (int32 Length = FMath::Min(RunLength, 3); Length > 0 && !bMatched; --Length)
							{
								if (Char == TEXT('~') && Length != 2)
								{
									continue;
								}
								const int32 Closer = FindCloser(RunEnd, End, Char, Length);
								if (Closer == INDEX_NONE)
								{
									continue;
								}
								// Opening characters beyond what the closer matches are text, before the emphasis.
								if (RunLength > Length)
								{
									AddText(Source.Mid(Index, RunLength - Length), Index);
								}
								Flush();
								ParseRange(RunEnd, Closer, Style | EmphasisStyle(Char, Length), Link, Depth + 1, OutRuns);
								Index = Closer + Length;
								bMatched = true;
							}
						}
						if (!bMatched)
						{
							AddText(Source.Mid(Index, RunLength), Index);
							Index = RunEnd;
						}
						continue;
					}

					AddChar(Char, Index);
					++Index;
				}
				Flush();
			}

			FString Source;
			TArray<FLineStart> LineStarts;
			const FDefinitions* Definitions = nullptr;

			TArray<int32> CodeSpanClosers;
			TArray<int32> BracketClosers;

			/** Where each emphasis closer search ended, so text with many unmatched * stays quick. */
			TMap<uint64, int32> CloserCache;
			int32 ScanDepth = 0;
			int32 WorkLeft = 4000000;
		};

		/** A paragraph that's one image and nothing else but spaces. */
		const FInlineRun* FindLoneImage(const TArray<FInlineRun>& Runs)
		{
			const FInlineRun* Image = nullptr;
			for (const FInlineRun& Run : Runs)
			{
				if (Run.bImage)
				{
					if (Image)
					{
						return nullptr;
					}
					Image = &Run;
				}
				else if (!IsBlank(Run.Text))
				{
					return nullptr;
				}
			}
			return Image;
		}

		//~ Blocks

		class FBlockParser
		{
		public:
			FBlockParser(const FDefinitions& InDefinitions, TArray<FProblem>& InProblems)
				: Definitions(InDefinitions)
				, Problems(InProblems)
			{
			}

			void Parse(const TArray<FLine>& Lines, TArray<FBlock>& OutBlocks, int32 Depth)
			{
				int32 Index = 0;
				while (Index < Lines.Num())
				{
					const FString& Text = Lines[Index].Text;
					FFence Fence;
					int32 Level = 0;
					FString Content;
					FListMarker Marker;

					if (IsBlank(Text))
					{
						++Index;
					}
					else if (IndentOf(Text) >= 4)
					{
						Index = ParseIndentedCode(Lines, Index, OutBlocks);
					}
					else if (ParseFenceOpen(Text, Fence))
					{
						Index = ParseFencedCode(Lines, Index, Fence, Depth, OutBlocks);
					}
					else if (ParseAtxHeading(Text, Level, Content))
					{
						AddHeading(Level, Content, Lines[Index].Number, OutBlocks);
						++Index;
					}
					else if (IsThematicBreak(Text))
					{
						FBlock& Rule = OutBlocks.AddDefaulted_GetRef();
						Rule.Type = EBlockType::Rule;
						Rule.Line = Lines[Index].Number;
						++Index;
					}
					else if (Depth < MaxBlockDepth && IsQuoteStart(Text))
					{
						Index = ParseQuote(Lines, Index, Depth, OutBlocks);
					}
					else if (Depth < MaxBlockDepth && ParseListMarker(Text, Marker))
					{
						Index = ParseList(Lines, Index, Depth, OutBlocks);
					}
					else if (IsHtmlStart(Text, false))
					{
						Index = ParseHtml(Lines, Index, OutBlocks);
					}
					else if (IsTableStart(Lines, Index))
					{
						Index = ParseTable(Lines, Index, OutBlocks);
					}
					else
					{
						CheckBrokenTable(Lines, Index);
						Index = ParseParagraph(Lines, Index, OutBlocks);
					}
				}
			}

		private:
			TArray<FInlineRun> ParseText(const FString& Text, TArray<FLineStart> LineStarts) const
			{
				TArray<FInlineRun> Runs;
				FInlineParser Parser(Text, MoveTemp(LineStarts), &Definitions);
				Parser.Parse(Runs);
				return Runs;
			}

			TArray<FInlineRun> ParseLine(const FString& Text, int32 LineNumber) const
			{
				TArray<FLineStart> LineStarts;
				FLineStart& Start = LineStarts.AddDefaulted_GetRef();
				Start.Number = LineNumber;
				return ParseText(Text, MoveTemp(LineStarts));
			}

			void AddHeading(int32 Level, const FString& Content, int32 LineNumber, TArray<FBlock>& OutBlocks) const
			{
				FBlock& Heading = OutBlocks.AddDefaulted_GetRef();
				Heading.Type = EBlockType::Heading;
				Heading.Level = Level;
				Heading.Line = LineNumber;
				Heading.Runs = ParseLine(Content, LineNumber);
			}

			int32 ParseParagraph(const TArray<FLine>& Lines, int32 Begin, TArray<FBlock>& OutBlocks)
			{
				int32 End = Begin + 1;
				int32 HeadingLevel = 0;
				while (End < Lines.Num())
				{
					HeadingLevel = SetextLevel(Lines[End].Text);
					if (HeadingLevel > 0 || InterruptsParagraph(Lines, End))
					{
						break;
					}
					CheckBrokenTable(Lines, End);
					++End;
				}

				// Link reference definitions at its start aren't shown. With nothing else, an underline after them is read on its own.
				int32 First = Begin;
				FString Label;
				FString Destination;
				while (First < End && ParseDefinition(Lines[First].Text, Label, Destination))
				{
					++First;
				}
				if (First == End)
				{
					return End;
				}

				TArray<FLineStart> LineStarts;
				const FString Joined = JoinLines(Lines, First, End, LineStarts);
				TArray<FInlineRun> Runs = ParseText(Joined, MoveTemp(LineStarts));

				FBlock& Block = OutBlocks.AddDefaulted_GetRef();
				Block.Line = Lines[First].Number;
				if (HeadingLevel > 0)
				{
					Block.Type = EBlockType::Heading;
					Block.Level = HeadingLevel;
					Block.Runs = MoveTemp(Runs);
					return End + 1;
				}
				if (const FInlineRun* Image = FindLoneImage(Runs))
				{
					Block.Type = EBlockType::Image;
					Block.Text = Image->Link;
					Block.Info = Image->Text;
					Block.Line = Image->Line;
					return End;
				}
				Block.Type = EBlockType::Paragraph;
				Block.Runs = MoveTemp(Runs);
				return End;
			}

			int32 ParseFencedCode(const TArray<FLine>& Lines, int32 Begin, const FFence& Fence, int32 Depth, TArray<FBlock>& OutBlocks)
			{
				FBlock& Code = OutBlocks.AddDefaulted_GetRef();
				Code.Type = EBlockType::Code;
				Code.Line = Lines[Begin].Number;
				Code.Info = Fence.Info;

				TArray<FString> CodeLines;
				bool bClosed = false;
				int32 Index = Begin + 1;
				while (Index < Lines.Num())
				{
					if (IsFenceClose(Lines[Index].Text, Fence.Char, Fence.Length))
					{
						bClosed = true;
						++Index;
						break;
					}
					CodeLines.Add(Unindent(Lines[Index].Text, Fence.Indent));
					++Index;
				}
				Code.Text = FString::Join(CodeLines, TEXT("\n"));

				if (!bClosed)
				{
					FProblem& Problem = Problems.AddDefaulted_GetRef();
					Problem.Line = Code.Line;
					Problem.Message = (Depth == 0)
						? FString(TEXT("This code block is never closed, so everything after it shows as code."))
						: FString(TEXT("This code block is never closed, so the rest of its list item or quote shows as code."));
				}
				return Index;
			}

			int32 ParseIndentedCode(const TArray<FLine>& Lines, int32 Begin, TArray<FBlock>& OutBlocks)
			{
				TArray<FString> CodeLines;
				int32 LastCode = Begin;
				for (int32 Index = Begin; Index < Lines.Num(); ++Index)
				{
					const FString& Text = Lines[Index].Text;
					if (!IsBlank(Text) && IndentOf(Text) < 4)
					{
						break;
					}
					CodeLines.Add(Unindent(Text, 4));
					if (!IsBlank(Text))
					{
						LastCode = Index;
					}
				}
				// Blank lines at the end aren't part of it.
				CodeLines.SetNum(LastCode - Begin + 1);

				FBlock& Code = OutBlocks.AddDefaulted_GetRef();
				Code.Type = EBlockType::Code;
				Code.Line = Lines[Begin].Number;
				Code.Text = FString::Join(CodeLines, TEXT("\n"));
				return LastCode + 1;
			}

			/** HTML is shown as written, as code; comments aren't shown at all, as on GitHub. */
			int32 ParseHtml(const TArray<FLine>& Lines, int32 Begin, TArray<FBlock>& OutBlocks)
			{
				const bool bComment = Lines[Begin].Text.TrimStart().StartsWith(TEXT("<!--"));
				TArray<FString> HtmlLines;
				int32 Index = Begin;
				while (Index < Lines.Num())
				{
					const FString& Text = Lines[Index].Text;
					if (bComment)
					{
						++Index;
						if (Text.Contains(TEXT("-->")))
						{
							break;
						}
						continue;
					}
					if (IsBlank(Text))
					{
						break;
					}
					HtmlLines.Add(Text);
					++Index;
				}

				if (!bComment)
				{
					FBlock& Html = OutBlocks.AddDefaulted_GetRef();
					Html.Type = EBlockType::Code;
					Html.Line = Lines[Begin].Number;
					Html.Info = TEXT("html");
					Html.Text = FString::Join(HtmlLines, TEXT("\n"));
				}
				return Index;
			}

			int32 ParseQuote(const TArray<FLine>& Lines, int32 Begin, int32 Depth, TArray<FBlock>& OutBlocks)
			{
				TArray<FLine> Inner;
				bool bLazyAllowed = false;
				int32 Index = Begin;
				while (Index < Lines.Num())
				{
					const FString& Text = Lines[Index].Text;
					if (IsQuoteStart(Text))
					{
						FString Rest = Text.RightChop(IndentOf(Text) + 1);
						if (Rest.StartsWith(TEXT(" ")))
						{
							Rest = Rest.RightChop(1);
						}
						bLazyAllowed = !IsBlank(Rest);
						FLine& Line = Inner.AddDefaulted_GetRef();
						Line.Text = Rest;
						Line.Number = Lines[Index].Number;
						++Index;
					}
					else if (bLazyAllowed && !StartsBlock(Text))
					{
						FLine& Line = Inner.AddDefaulted_GetRef();
						Line.Text = Text.TrimStart();
						Line.Number = Lines[Index].Number;
						++Index;
					}
					else
					{
						break;
					}
				}

				FBlock& Quote = OutBlocks.AddDefaulted_GetRef();
				Quote.Type = EBlockType::Quote;
				Quote.Line = Lines[Begin].Number;

				// GitHub's alerts: a quote starting with [!NOTE] alone on its line.
				static const TCHAR* const Alerts[][2] =
				{
					{ TEXT("[!NOTE]"), TEXT("Note") }, { TEXT("[!TIP]"), TEXT("Tip") }, { TEXT("[!IMPORTANT]"), TEXT("Important") },
					{ TEXT("[!WARNING]"), TEXT("Warning") }, { TEXT("[!CAUTION]"), TEXT("Caution") }
				};
				if (!Inner.IsEmpty())
				{
					const FString FirstLine = Inner[0].Text.TrimStartAndEnd();
					for (const auto& Alert : Alerts)
					{
						if (FirstLine.Equals(Alert[0], ESearchCase::IgnoreCase))
						{
							Quote.Info = Alert[1];
							Inner.RemoveAt(0);
							break;
						}
					}
				}

				Parse(Inner, Quote.Children, Depth + 1);
				return Index;
			}

			int32 ParseList(const TArray<FLine>& Lines, int32 Begin, int32 Depth, TArray<FBlock>& OutBlocks)
			{
				FListMarker First;
				ParseListMarker(Lines[Begin].Text, First);

				FBlock& List = OutBlocks.AddDefaulted_GetRef();
				List.Type = EBlockType::List;
				List.Line = Lines[Begin].Number;
				List.bOrdered = First.bOrdered;
				List.Start = First.Start;

				int32 Index = Begin;
				while (Index < Lines.Num())
				{
					FListMarker Marker;
					const FString& MarkerText = Lines[Index].Text;
					if (!ParseListMarker(MarkerText, Marker) || Marker.bOrdered != First.bOrdered || Marker.Delimiter != First.Delimiter || IsThematicBreak(MarkerText))
					{
						break;
					}

					// The item: the rest of the marker's line, then the lines indented to its content or continuing its paragraph.
					TArray<FLine> ItemLines;
					FLine& FirstLine = ItemLines.AddDefaulted_GetRef();
					FirstLine.Text = Marker.bEmpty ? FString() : MarkerText.RightChop(Marker.ContentColumn);
					FirstLine.Number = Lines[Index].Number;

					bool bLazyAllowed = !Marker.bEmpty;
					int32 Next = Index + 1;
					while (Next < Lines.Num())
					{
						const FString& Text = Lines[Next].Text;
						if (IsBlank(Text))
						{
							// A blank line stays in the item only when the item goes on after it. An item starts with one blank line at most.
							int32 After = Next + 1;
							while (After < Lines.Num() && IsBlank(Lines[After].Text))
							{
								++After;
							}
							const bool bEmptyStart = Marker.bEmpty && ItemLines.Num() == 1;
							if (bEmptyStart || After >= Lines.Num() || IndentOf(Lines[After].Text) < Marker.ContentColumn)
							{
								break;
							}
							for (; Next < After; ++Next)
							{
								FLine& Blank = ItemLines.AddDefaulted_GetRef();
								Blank.Number = Lines[Next].Number;
							}
							bLazyAllowed = false;
						}
						else if (IndentOf(Text) >= Marker.ContentColumn)
						{
							FLine& Line = ItemLines.AddDefaulted_GetRef();
							Line.Text = Text.RightChop(Marker.ContentColumn);
							Line.Number = Lines[Next].Number;
							bLazyAllowed = true;
							++Next;
						}
						else if (bLazyAllowed && !StartsBlock(Text))
						{
							FLine& Line = ItemLines.AddDefaulted_GetRef();
							Line.Text = Text.TrimStart();
							Line.Number = Lines[Next].Number;
							++Next;
						}
						else
						{
							break;
						}
					}

					FBlock& Item = List.Children.AddDefaulted_GetRef();
					Item.Type = EBlockType::ListItem;
					Item.Line = Lines[Index].Number;
					TakeTaskMarker(ItemLines[0].Text, Item);
					Parse(ItemLines, Item.Children, Depth + 1);

					// The next item can come after blank lines.
					Index = Next;
					while (Index < Lines.Num() && IsBlank(Lines[Index].Text))
					{
						++Index;
					}
				}
				return Index;
			}

			/** A task list item's [ ] or [x], taken off the start of its text. */
			static void TakeTaskMarker(FString& Text, FBlock& Item)
			{
				if (Text.Len() < 3 || Text[0] != TEXT('[') || Text[2] != TEXT(']') || (Text.Len() > 3 && Text[3] != TEXT(' ')))
				{
					return;
				}
				const TCHAR Mark = Text[1];
				if (Mark == TEXT(' ') || Mark == TEXT('x') || Mark == TEXT('X'))
				{
					Item.bTask = true;
					Item.bChecked = Mark != TEXT(' ');
					Text = Text.RightChop(FMath::Min(4, Text.Len()));
				}
			}

			int32 ParseTable(const TArray<FLine>& Lines, int32 Begin, TArray<FBlock>& OutBlocks)
			{
				FBlock& Table = OutBlocks.AddDefaulted_GetRef();
				Table.Type = EBlockType::Table;
				Table.Line = Lines[Begin].Number;
				ParseDelimiterRow(Lines[Begin + 1].Text, Table.Alignments);
				const int32 Columns = Table.Alignments.Num();

				for (const FString& Cell : SplitTableRow(Lines[Begin].Text))
				{
					Table.Header.Add(ParseLine(Cell, Lines[Begin].Number));
				}

				// Rows go on to a blank line or another block. A line without pipes is a row of one cell.
				int32 Index = Begin + 2;
				for (; Index < Lines.Num(); ++Index)
				{
					const FString& Text = Lines[Index].Text;
					if (StartsBlock(Text))
					{
						break;
					}
					const TArray<FString> Cells = SplitTableRow(Text);
					if (Cells.Num() > Columns)
					{
						FProblem& Problem = Problems.AddDefaulted_GetRef();
						Problem.Line = Lines[Index].Number;
						Problem.Message = FString::Printf(TEXT("This row has %d cells, but the table has %d columns. GitHub leaves out the extra cells."), Cells.Num(), Columns);
					}
					TArray<FTableCell>& Row = Table.Rows.AddDefaulted_GetRef();
					for (int32 Column = 0; Column < Columns; ++Column)
					{
						Row.Add(Column < Cells.Num() ? ParseLine(Cells[Column], Lines[Index].Number) : FTableCell());
					}
				}
				return Index;
			}

			/** A line that looks like a table header, but whose delimiter row has a different number of cells: GitHub shows it as text. */
			void CheckBrokenTable(const TArray<FLine>& Lines, int32 Index)
			{
				if (Index + 1 >= Lines.Num() || !Lines[Index].Text.Contains(TEXT("|")) || !Lines[Index + 1].Text.Contains(TEXT("|")))
				{
					return;
				}
				TArray<EColumnAlignment> Alignments;
				if (!ParseDelimiterRow(Lines[Index + 1].Text, Alignments))
				{
					return;
				}
				const int32 HeaderCells = SplitTableRow(Lines[Index].Text).Num();
				if (HeaderCells != Alignments.Num())
				{
					FProblem& Problem = Problems.AddDefaulted_GetRef();
					Problem.Line = Lines[Index].Number;
					Problem.Message = FString::Printf(TEXT("This table's header has %d cells, but the line under it has %d, so GitHub shows it as text instead of a table."), HeaderCells, Alignments.Num());
				}
			}

			const FDefinitions& Definitions;
			TArray<FProblem>& Problems;
		};

		/** Every [label]: destination in the file, outside code blocks, so links can use them before they're defined. */
		FDefinitions CollectDefinitions(const TArray<FLine>& Lines)
		{
			FDefinitions Found;
			TCHAR FenceChar = 0;
			int32 FenceLength = 0;
			for (const FLine& Line : Lines)
			{
				const FString Trimmed = Line.Text.TrimStart();
				if (FenceLength > 0)
				{
					if (IsFenceClose(Trimmed, FenceChar, FenceLength))
					{
						FenceLength = 0;
					}
					continue;
				}
				FFence Fence;
				if (ParseFenceOpen(Trimmed, Fence))
				{
					FenceChar = Fence.Char;
					FenceLength = Fence.Length;
					continue;
				}
				FString Label;
				FString Destination;
				if (ParseDefinition(Trimmed, Label, Destination) && !Found.Contains(Label))
				{
					Found.Add(Label, Destination);
				}
			}
			return Found;
		}

		/** Anchors in the order GitHub gives them: a repeated one gets -1, -2 and so on. */
		void AssignAnchors(TArray<FBlock>& Blocks, TMap<FString, int32>& Occurrences, TSet<FString>& OutAnchors)
		{
			for (FBlock& Block : Blocks)
			{
				if (Block.Type == EBlockType::Heading)
				{
					const FString Original = MakeAnchor(ToPlainText(Block.Runs));
					FString Anchor = Original;
					while (Occurrences.Contains(Anchor))
					{
						int32& Count = Occurrences.FindChecked(Original);
						++Count;
						Anchor = FString::Printf(TEXT("%s-%d"), *Original, Count);
					}
					Occurrences.Add(Anchor, 0);
					Block.Anchor = Anchor;
					OutAnchors.Add(Anchor);
				}
				AssignAnchors(Block.Children, Occurrences, OutAnchors);
			}
		}

		/** A web address or another scheme, such as mailto:, which the review doesn't check. */
		bool IsExternalLink(const FString& Link)
		{
			if (Link.StartsWith(TEXT("//")))
			{
				return true;
			}
			for (int32 Index = 0; Index < Link.Len(); ++Index)
			{
				const TCHAR Char = Link[Index];
				if (Char == TEXT(':'))
				{
					// One letter is a Windows drive.
					return Index > 1;
				}
				if (!FChar::IsAlnum(Char) && Char != TEXT('+') && Char != TEXT('.') && Char != TEXT('-'))
				{
					return false;
				}
			}
			return false;
		}

		/** The folder holding .git above Directory, which paths starting with / are relative to on GitHub. Empty when there's none. */
		FString FindRepositoryRoot(const FString& Directory)
		{
			FString Current = Directory;
			FPaths::NormalizeDirectoryName(Current);
			for (int32 Level = 0; Level < 64 && !Current.IsEmpty(); ++Level)
			{
				const FString GitPath = Current / TEXT(".git");
				if (FPaths::DirectoryExists(GitPath) || FPaths::FileExists(GitPath))
				{
					return Current;
				}
				const FString Parent = FPaths::GetPath(Current);
				if (Parent.IsEmpty() || Parent.Equals(Current))
				{
					break;
				}
				Current = Parent;
			}
			return FString();
		}

		/** Section numbers come and go as a document grows: 124-item-details and 125-item-details are the same section. */
		FString WithoutNumbering(const FString& Anchor)
		{
			int32 Index = 0;
			while (Index < Anchor.Len() && (FChar::IsDigit(Anchor[Index]) || Anchor[Index] == TEXT('-')))
			{
				++Index;
			}
			return Anchor.RightChop(Index);
		}

		/** The one anchor that differs from Missing only in its numbering, or empty. */
		FString SuggestAnchor(const TSet<FString>& Anchors, const FString& Missing)
		{
			const FString Wanted = WithoutNumbering(Missing);
			if (Wanted.IsEmpty())
			{
				return FString();
			}
			FString Suggestion;
			for (const FString& Anchor : Anchors)
			{
				if (WithoutNumbering(Anchor).Equals(Wanted, ESearchCase::IgnoreCase))
				{
					if (!Suggestion.IsEmpty())
					{
						return FString();
					}
					Suggestion = Anchor;
				}
			}
			return Suggestion;
		}
	}

	FDocument Parse(const FString& Text)
	{
		FDocument Document;
		const TArray<FLine> Lines = SplitLines(Text);
		const FDefinitions Definitions = CollectDefinitions(Lines);

		FBlockParser Parser(Definitions, Document.Problems);
		Parser.Parse(Lines, Document.Blocks, 0);

		TMap<FString, int32> Occurrences;
		AssignAnchors(Document.Blocks, Occurrences, Document.Anchors);
		return Document;
	}

	void ParseInline(const FString& Text, TArray<FInlineRun>& OutRuns)
	{
		FInlineParser Parser(Text, TArray<FLineStart>(), nullptr);
		Parser.Parse(OutRuns);
	}

	FString MakeAnchor(const FString& HeadingText)
	{
		FString Anchor;
		Anchor.Reserve(HeadingText.Len());
		for (int32 Index = 0; Index < HeadingText.Len(); ++Index)
		{
			const TCHAR Char = FChar::ToLower(HeadingText[Index]);
			if (Char == TEXT(' '))
			{
				Anchor.AppendChar(TEXT('-'));
			}
			else if (Char == TEXT('-') || Char == TEXT('_') || FChar::IsAlnum(Char))
			{
				Anchor.AppendChar(Char);
			}
		}
		return Anchor;
	}

	FString ToPlainText(const TArray<FInlineRun>& Runs)
	{
		FString Text;
		for (const FInlineRun& Run : Runs)
		{
			if (!Run.bImage)
			{
				Text += Run.Text;
			}
		}
		return Text;
	}

	void Review(FDocument& Document, const FString& BaseDirectory)
	{
		const FString RepositoryRoot = FindRepositoryRoot(BaseDirectory);
		TArray<FProblem> Found;

		auto CheckLink = [&Document, &Found, &BaseDirectory, &RepositoryRoot](int32 Line, const FString& Link, bool bImage)
		{
			if (Link.IsEmpty() || IsExternalLink(Link))
			{
				return;
			}

			if (Link.StartsWith(TEXT("#")))
			{
				const FString Anchor = PercentDecode(Link.RightChop(1)).ToLower();
				if (Anchor.IsEmpty() || Document.Anchors.Contains(Anchor))
				{
					return;
				}
				FProblem& Problem = Found.AddDefaulted_GetRef();
				Problem.Line = Line;
				const FString Suggestion = SuggestAnchor(Document.Anchors, Anchor);
				Problem.Message = Suggestion.IsEmpty()
					? FString::Printf(TEXT("No heading has the anchor #%s."), *Anchor)
					: FString::Printf(TEXT("No heading has the anchor #%s. Did you mean #%s?"), *Anchor, *Suggestion);
				return;
			}

			// A file in the repository. An anchor after it isn't checked.
			FString Path = Link;
			int32 Cut = INDEX_NONE;
			if (Path.FindChar(TEXT('#'), Cut))
			{
				Path = Path.Left(Cut);
			}
			if (Path.FindChar(TEXT('?'), Cut))
			{
				Path = Path.Left(Cut);
			}
			Path = PercentDecode(Path);
			if (Path.IsEmpty())
			{
				return;
			}

			FString FullPath;
			if (Path.StartsWith(TEXT("/")))
			{
				if (RepositoryRoot.IsEmpty())
				{
					return;
				}
				FullPath = RepositoryRoot / Path.RightChop(1);
			}
			else
			{
				FullPath = BaseDirectory / Path;
			}
			FPaths::CollapseRelativeDirectories(FullPath);
			if (FPaths::FileExists(FullPath) || FPaths::DirectoryExists(FullPath))
			{
				return;
			}
			FProblem& Problem = Found.AddDefaulted_GetRef();
			Problem.Line = Line;
			if (bImage)
			{
				Problem.Message = FString::Printf(TEXT("The image %s isn't there."), *Path);
			}
			else
			{
				Problem.Message = FString::Printf(TEXT("Links to %s, which isn't there."), *Path);
			}
		};

		auto CheckRuns = [&CheckLink](const TArray<FInlineRun>& Runs)
		{
			for (const FInlineRun& Run : Runs)
			{
				CheckLink(Run.Line, Run.Link, Run.bImage);
			}
		};

		ForEachBlock(Document.Blocks, [&CheckLink, &CheckRuns](const FBlock& Block)
		{
			CheckRuns(Block.Runs);
			for (const FTableCell& HeaderCell : Block.Header)
			{
				CheckRuns(HeaderCell);
			}
			for (const TArray<FTableCell>& Row : Block.Rows)
			{
				for (const FTableCell& Cell : Row)
				{
					CheckRuns(Cell);
				}
			}
			if (Block.Type == EBlockType::Image)
			{
				CheckLink(Block.Line, Block.Text, true);
			}
		});

		Document.Problems.Append(Found);
		Document.Problems.StableSort([](const FProblem& A, const FProblem& B)
		{
			return A.Line < B.Line;
		});
	}

	void ForEachBlock(const TArray<FBlock>& Blocks, TFunctionRef<void(const FBlock&)> Visitor)
	{
		for (const FBlock& Block : Blocks)
		{
			Visitor(Block);
			ForEachBlock(Block.Children, Visitor);
		}
	}

	// Code
	namespace
	{
		enum class ECodeLanguage : uint8
		{
			Plain,
			CLike,
			Ini
		};

		const TCHAR* const CppKeywordList[] = {
			TEXT("alignas"), TEXT("alignof"), TEXT("asm"), TEXT("auto"), TEXT("bool"), TEXT("break"), TEXT("case"), TEXT("catch"), TEXT("char"),
			TEXT("char16_t"), TEXT("char32_t"), TEXT("char8_t"), TEXT("class"), TEXT("co_await"), TEXT("co_return"), TEXT("co_yield"), TEXT("concept"),
			TEXT("const"), TEXT("const_cast"), TEXT("consteval"), TEXT("constexpr"), TEXT("constinit"), TEXT("continue"), TEXT("decltype"), TEXT("default"),
			TEXT("delete"), TEXT("do"), TEXT("double"), TEXT("dynamic_cast"), TEXT("else"), TEXT("enum"), TEXT("explicit"), TEXT("export"), TEXT("extern"),
			TEXT("false"), TEXT("final"), TEXT("float"), TEXT("for"), TEXT("friend"), TEXT("goto"), TEXT("if"), TEXT("inline"), TEXT("int"), TEXT("long"),
			TEXT("mutable"), TEXT("namespace"), TEXT("new"), TEXT("noexcept"), TEXT("nullptr"), TEXT("operator"), TEXT("override"), TEXT("private"),
			TEXT("protected"), TEXT("public"), TEXT("register"), TEXT("reinterpret_cast"), TEXT("requires"), TEXT("return"), TEXT("short"), TEXT("signed"),
			TEXT("sizeof"), TEXT("static"), TEXT("static_assert"), TEXT("static_cast"), TEXT("struct"), TEXT("switch"), TEXT("template"), TEXT("this"),
			TEXT("thread_local"), TEXT("throw"), TEXT("true"), TEXT("try"), TEXT("typedef"), TEXT("typeid"), TEXT("typename"), TEXT("union"),
			TEXT("unsigned"), TEXT("using"), TEXT("virtual"), TEXT("void"), TEXT("volatile"), TEXT("wchar_t"), TEXT("while")
		};

		const TCHAR* const CSharpKeywordList[] = {
			TEXT("abstract"), TEXT("as"), TEXT("base"), TEXT("bool"), TEXT("break"), TEXT("byte"), TEXT("case"), TEXT("catch"), TEXT("char"),
			TEXT("checked"), TEXT("class"), TEXT("const"), TEXT("continue"), TEXT("decimal"), TEXT("default"), TEXT("delegate"), TEXT("do"),
			TEXT("double"), TEXT("else"), TEXT("enum"), TEXT("event"), TEXT("explicit"), TEXT("extern"), TEXT("false"), TEXT("finally"), TEXT("fixed"),
			TEXT("float"), TEXT("for"), TEXT("foreach"), TEXT("get"), TEXT("goto"), TEXT("if"), TEXT("implicit"), TEXT("in"), TEXT("int"),
			TEXT("interface"), TEXT("internal"), TEXT("is"), TEXT("lock"), TEXT("long"), TEXT("namespace"), TEXT("new"), TEXT("null"), TEXT("object"),
			TEXT("operator"), TEXT("out"), TEXT("override"), TEXT("params"), TEXT("private"), TEXT("protected"), TEXT("public"), TEXT("readonly"),
			TEXT("ref"), TEXT("return"), TEXT("sbyte"), TEXT("sealed"), TEXT("set"), TEXT("short"), TEXT("sizeof"), TEXT("stackalloc"), TEXT("static"),
			TEXT("string"), TEXT("struct"), TEXT("switch"), TEXT("this"), TEXT("throw"), TEXT("true"), TEXT("try"), TEXT("typeof"), TEXT("uint"),
			TEXT("ulong"), TEXT("unchecked"), TEXT("unsafe"), TEXT("ushort"), TEXT("using"), TEXT("var"), TEXT("virtual"), TEXT("void"),
			TEXT("volatile"), TEXT("while")
		};

		/** JavaScript and TypeScript, and the few words JSON has. */
		const TCHAR* const ScriptKeywordList[] = {
			TEXT("async"), TEXT("await"), TEXT("break"), TEXT("case"), TEXT("catch"), TEXT("class"), TEXT("const"), TEXT("continue"), TEXT("default"),
			TEXT("delete"), TEXT("do"), TEXT("else"), TEXT("export"), TEXT("extends"), TEXT("false"), TEXT("finally"), TEXT("for"), TEXT("from"),
			TEXT("function"), TEXT("if"), TEXT("import"), TEXT("in"), TEXT("instanceof"), TEXT("let"), TEXT("new"), TEXT("null"), TEXT("of"),
			TEXT("return"), TEXT("switch"), TEXT("this"), TEXT("throw"), TEXT("true"), TEXT("try"), TEXT("typeof"), TEXT("undefined"), TEXT("var"),
			TEXT("void"), TEXT("while"), TEXT("yield")
		};

		/** Unreal's names for the basic types, colored as types like its classes are. */
		const TCHAR* const UnrealTypeNameList[] = {
			TEXT("int8"), TEXT("int16"), TEXT("int32"), TEXT("int64"), TEXT("uint8"), TEXT("uint16"), TEXT("uint32"), TEXT("uint64"), TEXT("TCHAR"),
			TEXT("ANSICHAR"), TEXT("WIDECHAR"), TEXT("UTF8CHAR"), TEXT("SIZE_T"), TEXT("SSIZE_T"), TEXT("PTRINT"), TEXT("UPTRINT"), TEXT("size_t")
		};

		template <int32 Count>
		bool IsCodeWordIn(const FString& Word, const TCHAR* const (&List)[Count])
		{
			for (const TCHAR* Entry : List)
			{
				if (Word.Equals(Entry, ESearchCase::CaseSensitive))
				{
					return true;
				}
			}
			return false;
		}

		ECodeLanguage FindCodeLanguage(const FString& LowerName)
		{
			static const TCHAR* const CLikeNames[] = {
				TEXT("c"), TEXT("cc"), TEXT("cpp"), TEXT("cxx"), TEXT("c++"), TEXT("h"), TEXT("hpp"), TEXT("hxx"), TEXT("hlsl"), TEXT("usf"), TEXT("ush"),
				TEXT("cs"), TEXT("csharp"), TEXT("c#"), TEXT("java"), TEXT("js"), TEXT("javascript"), TEXT("ts"), TEXT("typescript"), TEXT("json")
			};
			static const TCHAR* const IniNames[] = { TEXT("ini"), TEXT("cfg") };
			if (IsCodeWordIn(LowerName, CLikeNames))
			{
				return ECodeLanguage::CLike;
			}
			return IsCodeWordIn(LowerName, IniNames) ? ECodeLanguage::Ini : ECodeLanguage::Plain;
		}

		bool IsCodeIdentifierStart(TCHAR Char)
		{
			return FChar::IsAlpha(Char) || Char == TEXT('_');
		}

		bool IsCodeIdentifierChar(TCHAR Char)
		{
			return FChar::IsAlnum(Char) || Char == TEXT('_');
		}

		/** An Unreal class or struct by its prefix and capitals, such as UObject, AActor, FInventoryRegistry, EError, TArray, IInterface or SWidget. */
		bool LooksLikeUnrealType(const FString& Word)
		{
			if (Word.Len() < 3 || FCString::Strchr(TEXT("UAFETIS"), Word[0]) == nullptr || !FChar::IsUpper(Word[1]))
			{
				return false;
			}
			for (const TCHAR Char : Word)
			{
				if (FChar::IsLower(Char))
				{
					return true;
				}
			}
			return false;
		}

		/** A macro, as they're written in capitals, such as TEXT, INDEX_NONE or UPROPERTY. */
		bool IsMacroName(const FString& Word)
		{
			if (Word.Len() < 2)
			{
				return false;
			}
			bool bHasLetter = false;
			for (const TCHAR Char : Word)
			{
				if (FChar::IsLower(Char))
				{
					return false;
				}
				bHasLetter |= FChar::IsAlpha(Char);
			}
			return bHasLetter;
		}

		/** Walks code once, from start to end, adding a span for each piece. */
		class FCodeHighlighter
		{
		public:
			FCodeHighlighter(const FString& InCode, const FString& LowerLanguage)
				: Code(InCode)
				, Language(FindCodeLanguage(LowerLanguage))
			{
				const bool bCSharp = LowerLanguage == TEXT("cs") || LowerLanguage == TEXT("csharp") || LowerLanguage == TEXT("c#");
				const bool bScript = LowerLanguage == TEXT("js") || LowerLanguage == TEXT("javascript") || LowerLanguage == TEXT("ts")
					|| LowerLanguage == TEXT("typescript") || LowerLanguage == TEXT("json");
				KeywordSet = bCSharp ? EKeywordSet::CSharp : (bScript ? EKeywordSet::Script : EKeywordSet::Cpp);
				bPreprocessorLines = !bScript && LowerLanguage != TEXT("java");
				bVerbatimStrings = bCSharp;
			}

			TArray<FCodeSpan> Run()
			{
				switch (Language)
				{
				case ECodeLanguage::CLike:
					ReadCLike();
					break;
				case ECodeLanguage::Ini:
					ReadIni();
					break;
				default:
					Add(0, Code.Len(), ECodeToken::Plain);
					break;
				}
				return MoveTemp(Spans);
			}

		private:
			enum class EKeywordSet : uint8
			{
				Cpp,
				CSharp,
				Script
			};

			void Add(int32 Start, int32 End, ECodeToken Token)
			{
				if (End <= Start)
				{
					return;
				}
				if (Spans.Num() > 0 && Spans.Last().Token == Token)
				{
					Spans.Last().Text += Code.Mid(Start, End - Start);
					return;
				}
				FCodeSpan& Span = Spans.AddDefaulted_GetRef();
				Span.Text = Code.Mid(Start, End - Start);
				Span.Token = Token;
			}

			TCHAR At(int32 Index) const
			{
				return Code.IsValidIndex(Index) ? Code[Index] : TEXT('\0');
			}

			int32 EndOfLine(int32 Index) const
			{
				while (Index < Code.Len() && Code[Index] != TEXT('\n'))
				{
					++Index;
				}
				return Index;
			}

			/** Past the quote closing the one at Start, or the end of the line when it isn't closed. Verbatim strings double a quote to hold one. */
			int32 SkipQuoted(int32 Start, bool bEscapes) const
			{
				const TCHAR Quote = Code[Start];
				int32 Index = Start + 1;
				while (Index < Code.Len() && Code[Index] != TEXT('\n'))
				{
					if (bEscapes && Code[Index] == TEXT('\\'))
					{
						Index += 2;
						continue;
					}
					if (Code[Index] == Quote)
					{
						if (!bEscapes && At(Index + 1) == Quote)
						{
							Index += 2;
							continue;
						}
						return Index + 1;
					}
					++Index;
				}
				return FMath::Min(Index, Code.Len());
			}

			/** Past a C++ raw string, R"delimiter(...)delimiter", whose quote is at Quote; it may span lines. */
			int32 SkipRawString(int32 Quote) const
			{
				const int32 Open = Code.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromStart, Quote);
				const int32 LineEnd = EndOfLine(Quote);
				if (Open == INDEX_NONE || Open > LineEnd)
				{
					return SkipQuoted(Quote, true);
				}
				const FString Closing = TEXT(")") + Code.Mid(Quote + 1, Open - Quote - 1) + TEXT("\"");
				const int32 Close = Code.Find(Closing, ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
				return Close == INDEX_NONE ? Code.Len() : Close + Closing.Len();
			}

			bool IsKeyword(const FString& Word) const
			{
				switch (KeywordSet)
				{
				case EKeywordSet::CSharp:
					return IsCodeWordIn(Word, CSharpKeywordList);
				case EKeywordSet::Script:
					return IsCodeWordIn(Word, ScriptKeywordList);
				default:
					return IsCodeWordIn(Word, CppKeywordList);
				}
			}

			/** Whether the next thing after Index, past spaces, is an opening bracket, as a call's is. */
			bool IsFollowedByCall(int32 Index) const
			{
				while (At(Index) == TEXT(' ') || At(Index) == TEXT('\t'))
				{
					++Index;
				}
				return At(Index) == TEXT('(');
			}

			void ReadCLike()
			{
				bool bLineStart = true;
				bool bIncludeLine = false;
				int32 Index = 0;
				while (Index < Code.Len())
				{
					const int32 Start = Index;
					const TCHAR Char = Code[Index];
					const TCHAR Next = At(Index + 1);

					if (Char == TEXT('\n'))
					{
						Add(Start, Index + 1, ECodeToken::Plain);
						Index = Start + 1;
						bLineStart = true;
						bIncludeLine = false;
						continue;
					}
					if (FChar::IsWhitespace(Char))
					{
						Add(Start, Index + 1, ECodeToken::Plain);
						++Index;
						continue;
					}

					const bool bWasLineStart = bLineStart;
					bLineStart = false;

					if (Char == TEXT('/') && Next == TEXT('/'))
					{
						Index = EndOfLine(Index);
						Add(Start, Index, ECodeToken::Comment);
					}
					else if (Char == TEXT('/') && Next == TEXT('*'))
					{
						const int32 Close = Code.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 2);
						Index = Close == INDEX_NONE ? Code.Len() : Close + 2;
						Add(Start, Index, ECodeToken::Comment);
					}
					else if (Char == TEXT('#') && bWasLineStart && bPreprocessorLines)
					{
						// The directive, such as #include; what follows is read as code, but an include's <file> is a string.
						Index = Start + 1;
						while (At(Index) == TEXT(' ') || At(Index) == TEXT('\t'))
						{
							++Index;
						}
						const int32 WordStart = Index;
						while (IsCodeIdentifierChar(At(Index)))
						{
							++Index;
						}
						const FString Directive = Code.Mid(WordStart, Index - WordStart);
						bIncludeLine = Directive == TEXT("include") || Directive == TEXT("import");
						Add(Start, Index, ECodeToken::Preprocessor);
					}
					else if (Char == TEXT('<') && bIncludeLine)
					{
						const int32 Close = Code.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
						const int32 LineEnd = EndOfLine(Index);
						Index = (Close == INDEX_NONE || Close > LineEnd) ? LineEnd : Close + 1;
						Add(Start, Index, ECodeToken::String);
					}
					else if (Char == TEXT('"') || Char == TEXT('\''))
					{
						Index = SkipQuoted(Index, true);
						Add(Start, Index, ECodeToken::String);
					}
					else if ((Char == TEXT('@') || Char == TEXT('$')) && Next == TEXT('"') && bVerbatimStrings)
					{
						Index = SkipQuoted(Index + 1, Char == TEXT('$'));
						Add(Start, Index, ECodeToken::String);
					}
					else if (FChar::IsDigit(Char) || (Char == TEXT('.') && FChar::IsDigit(Next)))
					{
						++Index;
						while (Index < Code.Len())
						{
							const TCHAR Current = Code[Index];
							const TCHAR Previous = Code[Index - 1];
							const bool bExponentSign = (Current == TEXT('+') || Current == TEXT('-'))
								&& (Previous == TEXT('e') || Previous == TEXT('E') || Previous == TEXT('p') || Previous == TEXT('P'));
							if (!IsCodeIdentifierChar(Current) && Current != TEXT('.') && Current != TEXT('\'') && !bExponentSign)
							{
								break;
							}
							++Index;
						}
						Add(Start, Index, ECodeToken::Number);
					}
					else if (IsCodeIdentifierStart(Char))
					{
						while (IsCodeIdentifierChar(At(Index)))
						{
							++Index;
						}
						const FString Word = Code.Mid(Start, Index - Start);

						// A prefixed string, such as L"wide" or R"(raw)".
						if (At(Index) == TEXT('"') && KeywordSet == EKeywordSet::Cpp
							&& (Word == TEXT("L") || Word == TEXT("u") || Word == TEXT("U") || Word == TEXT("u8") || Word.EndsWith(TEXT("R"), ESearchCase::CaseSensitive)))
						{
							const bool bRaw = Word.EndsWith(TEXT("R"), ESearchCase::CaseSensitive)
								&& (Word == TEXT("R") || Word == TEXT("LR") || Word == TEXT("uR") || Word == TEXT("UR") || Word == TEXT("u8R"));
							if (bRaw || Word.Len() <= 2)
							{
								Index = bRaw ? SkipRawString(Index) : SkipQuoted(Index, true);
								Add(Start, Index, ECodeToken::String);
								continue;
							}
						}

						ECodeToken Token = ECodeToken::Plain;
						if (IsKeyword(Word))
						{
							Token = ECodeToken::Keyword;
						}
						else if (IsCodeWordIn(Word, UnrealTypeNameList) || LooksLikeUnrealType(Word))
						{
							Token = ECodeToken::Type;
						}
						else if (IsMacroName(Word))
						{
							Token = ECodeToken::Preprocessor;
						}
						else if (IsFollowedByCall(Index))
						{
							Token = ECodeToken::Function;
						}
						Add(Start, Index, Token);
					}
					else
					{
						Add(Start, Index + 1, ECodeToken::Plain);
						++Index;
					}
				}
			}

			void ReadIni()
			{
				int32 LineStart = 0;
				while (LineStart < Code.Len())
				{
					const int32 LineEnd = EndOfLine(LineStart);
					int32 First = LineStart;
					while (First < LineEnd && FChar::IsWhitespace(Code[First]))
					{
						++First;
					}
					Add(LineStart, First, ECodeToken::Plain);

					const TCHAR Char = At(First);
					if (First >= LineEnd)
					{
						// An empty line.
					}
					else if (Char == TEXT(';') || Char == TEXT('#'))
					{
						Add(First, LineEnd, ECodeToken::Comment);
					}
					else if (Char == TEXT('['))
					{
						const int32 Close = Code.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, First);
						const int32 SectionEnd = (Close == INDEX_NONE || Close >= LineEnd) ? LineEnd : Close + 1;
						Add(First, SectionEnd, ECodeToken::Keyword);
						Add(SectionEnd, LineEnd, ECodeToken::Plain);
					}
					else
					{
						// A key, such as +Paths, then its value, whose quoted strings stand out.
						const int32 Equals = Code.Find(TEXT("="), ESearchCase::CaseSensitive, ESearchDir::FromStart, First);
						const int32 ValueStart = (Equals == INDEX_NONE || Equals >= LineEnd) ? First : Equals + 1;
						if (ValueStart > First)
						{
							Add(First, Equals, ECodeToken::Type);
							Add(Equals, ValueStart, ECodeToken::Plain);
						}
						int32 Index = ValueStart;
						while (Index < LineEnd)
						{
							if (Code[Index] == TEXT('"'))
							{
								const int32 Close = SkipQuoted(Index, true);
								Add(Index, Close, ECodeToken::String);
								Index = Close;
							}
							else
							{
								Add(Index, Index + 1, ECodeToken::Plain);
								++Index;
							}
						}
					}

					Add(LineEnd, FMath::Min(LineEnd + 1, Code.Len()), ECodeToken::Plain);
					LineStart = LineEnd + 1;
				}
			}

			const FString& Code;
			ECodeLanguage Language = ECodeLanguage::Plain;
			EKeywordSet KeywordSet = EKeywordSet::Cpp;
			bool bPreprocessorLines = true;
			bool bVerbatimStrings = false;
			TArray<FCodeSpan> Spans;
		};
	}

	TArray<FCodeSpan> HighlightCode(const FString& Code, const FString& Language)
	{
		FCodeHighlighter Highlighter(Code, Language.ToLower());
		return Highlighter.Run();
	}

	FString GetLanguageName(const FString& Language)
	{
		struct FLanguageName
		{
			const TCHAR* Fence;
			const TCHAR* Name;
		};
		static const FLanguageName Names[] = {
			{ TEXT("cpp"), TEXT("C++") }, { TEXT("c++"), TEXT("C++") }, { TEXT("cc"), TEXT("C++") }, { TEXT("cxx"), TEXT("C++") },
			{ TEXT("h"), TEXT("C++") }, { TEXT("hpp"), TEXT("C++") }, { TEXT("hxx"), TEXT("C++") }, { TEXT("c"), TEXT("C") },
			{ TEXT("cs"), TEXT("C#") }, { TEXT("csharp"), TEXT("C#") }, { TEXT("c#"), TEXT("C#") }, { TEXT("hlsl"), TEXT("HLSL") },
			{ TEXT("usf"), TEXT("HLSL") }, { TEXT("ush"), TEXT("HLSL") }, { TEXT("java"), TEXT("Java") }, { TEXT("js"), TEXT("JavaScript") },
			{ TEXT("javascript"), TEXT("JavaScript") }, { TEXT("ts"), TEXT("TypeScript") }, { TEXT("typescript"), TEXT("TypeScript") },
			{ TEXT("json"), TEXT("JSON") }, { TEXT("ini"), TEXT("INI") }, { TEXT("cfg"), TEXT("INI") }, { TEXT("py"), TEXT("Python") },
			{ TEXT("python"), TEXT("Python") }, { TEXT("sh"), TEXT("Shell") }, { TEXT("bash"), TEXT("Shell") }, { TEXT("shell"), TEXT("Shell") },
			{ TEXT("console"), TEXT("Console") }, { TEXT("powershell"), TEXT("PowerShell") }, { TEXT("ps1"), TEXT("PowerShell") },
			{ TEXT("bat"), TEXT("Batch") }, { TEXT("cmd"), TEXT("Batch") }, { TEXT("text"), TEXT("Text") }, { TEXT("txt"), TEXT("Text") },
			{ TEXT("xml"), TEXT("XML") }, { TEXT("yaml"), TEXT("YAML") }, { TEXT("yml"), TEXT("YAML") }, { TEXT("md"), TEXT("Markdown") },
			{ TEXT("markdown"), TEXT("Markdown") }
		};
		for (const FLanguageName& Entry : Names)
		{
			if (Language.Equals(Entry.Fence, ESearchCase::IgnoreCase))
			{
				return Entry.Name;
			}
		}
		return Language;
	}
}
