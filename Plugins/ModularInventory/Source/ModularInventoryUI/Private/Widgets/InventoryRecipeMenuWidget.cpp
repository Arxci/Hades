#include "Widgets/InventoryRecipeMenuWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"
#include "InventorySubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "ModularInventory.h"
#include "Widgets/InventoryItemWidget.h"

#define LOCTEXT_NAMESPACE "InventoryRecipeMenuWidget"

namespace
{
	void ShowText(UTextBlock* TextBlock, const FText& Text)
	{
		if (TextBlock)
		{
			TextBlock->SetText(Text);
			TextBlock->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}

	/** Shows a definition in an item widget, as a preview of Count of it, or hides the widget without one. */
	void ShowPreviewIn(UInventoryItemWidget* ItemWidget, UInventoryItemDefinition* Definition, int32 Count)
	{
		if (!ItemWidget)
		{
			return;
		}
		if (!Definition)
		{
			// Hidden, not collapsed, so a Size Box around it keeps its size.
			ItemWidget->SetVisibility(ESlateVisibility::Hidden);
			return;
		}

		FInventoryItemPreview Preview;
		Preview.Definition = Definition;
		Preview.Count = FMath::Max(1, Count);
		ItemWidget->ShowPreview(Preview);
		ItemWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

// Ingredient
void UInventoryRecipeIngredientWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CountText)
	{
		DefaultCountColor = CountText->GetColorAndOpacity();
	}
}

void UInventoryRecipeIngredientWidget::NativeDestruct()
{
	InventoryWidgets::CancelLoad(IconHandle);

	Super::NativeDestruct();
}

void UInventoryRecipeIngredientWidget::SetIngredient(const FInventoryRecipeIngredientView& InIngredient)
{
	Ingredient = InIngredient;

	ShowPreviewIn(ItemWidget, Ingredient.Definition, Ingredient.Count);
	InventoryWidgets::ShowIcon(Icon, Ingredient.Definition, IconHandle);
	ShowText(NameText, Ingredient.Definition ? Ingredient.Definition->DisplayName : FText::GetEmpty());
	if (CountText)
	{
		CountText->SetText(FText::FormatNamed(CountFormat, TEXT("Count"), FText::AsNumber(Ingredient.Count), TEXT("Held"), FText::AsNumber(Ingredient.Held)));
		CountText->SetColorAndOpacity(bTintWhenMissing && !Ingredient.bEnough ? FSlateColor(MissingColor) : DefaultCountColor);
	}
	if (ToolMarker)
	{
		ToolMarker->SetVisibility(Ingredient.bConsumed ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	OnIngredientChanged(Ingredient);
}

// Entry
void UInventoryRecipeMenuEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	AvailableStyle = GetStyle() ? GetStyle()->GetClass() : nullptr;
}

void UInventoryRecipeMenuEntry::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyTextStyle();
}

void UInventoryRecipeMenuEntry::NativeDestruct()
{
	InventoryWidgets::CancelLoad(IconHandle);

	Super::NativeDestruct();
}

void UInventoryRecipeMenuEntry::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	ApplyTextStyle();
}

void UInventoryRecipeMenuEntry::ApplyTextStyle()
{
	// A button Style without text styles leaves the name as it is in the designer.
	UCommonTextBlock* CommonName = Cast<UCommonTextBlock>(NameText);
	const TSubclassOf<UCommonTextStyle> TextStyle = GetCurrentTextStyleClass();
	if (CommonName && TextStyle)
	{
		CommonName->SetStyle(TextStyle);
	}
}

void UInventoryRecipeMenuEntry::SetRecipe(const FInventoryRecipeView& InView)
{
	const bool bSameIngredients = View.Recipe == InView.Recipe && View.Ingredients.Num() == InView.Ingredients.Num();
	View = InView;

	ShowPreviewIn(OutputItem, View.Output, View.OutputCount);
	InventoryWidgets::ShowIcon(OutputIcon, View.Output, IconHandle);
	if (NameText)
	{
		NameText->SetText(View.Name);
	}
	ShowText(OutputCountText, View.OutputCount > 1 ? FText::FormatNamed(OutputCountFormat, TEXT("Count"), FText::AsNumber(View.OutputCount)) : FText::GetEmpty());
	if (HeldText)
	{
		HeldText->SetText(FText::FormatNamed(HeldFormat, TEXT("Count"), FText::AsNumber(View.Held)));
	}
	if (CraftableText)
	{
		CraftableText->SetText(FText::FormatNamed(CraftableFormat, TEXT("Count"), FText::AsNumber(View.CraftableTimes)));
	}
	ShowText(ReasonText, View.CanCraft() ? FText::GetEmpty() : View.Reason);

	// The same recipe again keeps its ingredient widgets and only updates them, as when what the player holds changes.
	if (bSameIngredients && IngredientWidgets.Num() == View.Ingredients.Num())
	{
		for (int32 Index = 0; Index < IngredientWidgets.Num(); ++Index)
		{
			if (IngredientWidgets[Index])
			{
				IngredientWidgets[Index]->SetIngredient(View.Ingredients[Index]);
			}
		}
	}
	else
	{
		RefreshIngredients();
	}

	const TSubclassOf<UCommonButtonStyle> Wanted = !View.CanCraft() && UnavailableStyle ? UnavailableStyle : AvailableStyle;
	const UCommonButtonStyle* Current = GetStyle();
	if (Wanted && (!Current || Current->GetClass() != Wanted))
	{
		SetStyle(Wanted);
	}
	OnRecipeChanged(View);
}

void UInventoryRecipeMenuEntry::RefreshIngredients()
{
	IngredientWidgets.Reset();
	if (!IngredientPanel)
	{
		return;
	}

	IngredientPanel->ClearChildren();
	for (int32 Index = 0; IngredientClass && Index < View.Ingredients.Num(); ++Index)
	{
		if (Index > 0 && IngredientSeparatorClass)
		{
			if (UUserWidget* Separator = CreateWidget<UUserWidget>(this, IngredientSeparatorClass))
			{
				IngredientPanel->AddChild(Separator);
			}
		}

		UInventoryRecipeIngredientWidget* IngredientWidget = CreateWidget<UInventoryRecipeIngredientWidget>(this, IngredientClass);
		if (!IngredientWidget)
		{
			continue;
		}
		IngredientPanel->AddChild(IngredientWidget);
		IngredientWidget->SetIngredient(View.Ingredients[Index]);
		IngredientWidgets.Add(IngredientWidget);
	}
}

// Menu
UInventoryRecipeMenuWidget::UInventoryRecipeMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// What picking the recipe does. What it makes is the output's to show, in Output Description Text.
	RecipeDescription = LOCTEXT("RecipeDescription", "Craft {Name}.");
	UncraftableReasons.Add(EInventoryError::MissingIngredients, LOCTEXT("MissingIngredients", "Not enough materials."));
	UncraftableReasons.Add(EInventoryError::NoSpace, LOCTEXT("NoSpace", "Not enough space."));
	UncraftableReasons.Add(EInventoryError::RequirementsNotMet, LOCTEXT("RequirementsNotMet", "That can't be made here."));
	UncraftableReason = LOCTEXT("Uncraftable", "That can't be made now.");
	CraftPromptText = LOCTEXT("CraftPrompt", "Make how many?");
}

void UInventoryRecipeMenuWidget::SetRecipeSource(const UInventoryRecipeBook* Book, const FGameplayTagContainer& InContextTags)
{
	SourceBook = Book;
	SourceContextTags = InContextTags;
	if (GetItem().IsSet())
	{
		RefreshRecipes();
	}
}

const UInventoryRecipeBook* UInventoryRecipeMenuWidget::GetBook() const
{
	const UInventoryRecipeBook* Given = SourceBook.Get();
	return Given ? Given : RecipeBook.Get();
}

FGameplayTagContainer UInventoryRecipeMenuWidget::GetContextTags() const
{
	FGameplayTagContainer Tags = ContextTags;
	Tags.AppendTags(SourceContextTags);
	return Tags;
}

UInventoryComponent* UInventoryRecipeMenuWidget::GetCrafter() const
{
	return UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn());
}

bool UInventoryRecipeMenuWidget::CanOpenFor(const FInventoryItemActionContext& Context) const
{
	// Asked of the class defaults, so the player comes from the context. Without a book of its own, whoever opens it gives one, and decides.
	if (!RecipeBook)
	{
		return true;
	}
	const APawn* Pawn = Context.Player ? Context.Player->GetPawn() : nullptr;
	const UInventoryComponent* Crafter = UInventoryComponent::FindInventoryComponent(Pawn);
	if (Crafter && !Crafter->GetRecipesUsing(RecipeBook, Context.Item).IsEmpty())
	{
		return true;
	}

	TArray<UInventoryItemAction*> Gathered;
	GatherActions(Gathered);
	return !Gathered.IsEmpty() && Super::CanOpenFor(Context);
}

void UInventoryRecipeMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	RecipeListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRecipeRegistryChanged));
}

void UInventoryRecipeMenuWidget::NativeOnDeactivated()
{
	RecipeListener.Unbind();
	InventoryWidgets::CancelLoad(OutputIconHandle);

	// The stack reuses menus: whoever opens it next gives its own recipes.
	SourceBook.Reset();
	SourceContextTags.Reset();

	Super::NativeOnDeactivated();
}

void UInventoryRecipeMenuWidget::NativeOnItemChanged()
{
	Super::NativeOnItemChanged();

	RefreshRecipes();
}

void UInventoryRecipeMenuWidget::HandleRecipeRegistryChanged(const FInventoryChangeSet& Changes)
{
	// Counts change as the player's items do, such as after a craft. A menu going, such as for its item used up, is left as it is.
	if (IsActivated() && !IsClosing())
	{
		RefreshRecipes();
	}
}

FText UInventoryRecipeMenuWidget::GetReason(EInventoryError Error) const
{
	if (Error == EInventoryError::None)
	{
		return FText::GetEmpty();
	}
	const FText* Reason = UncraftableReasons.Find(Error);
	return Reason ? *Reason : UncraftableReason;
}

FInventoryRecipeView UInventoryRecipeMenuWidget::MakeView(UInventoryRecipe* Recipe) const
{
	FInventoryRecipeView View;
	View.Recipe = Recipe;
	const UInventoryComponent* Crafter = GetCrafter();
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	if (!Recipe || !Crafter || !Inventory)
	{
		View.Error = EInventoryError::InvalidDefinition;
		View.Reason = GetReason(View.Error);
		return View;
	}

	// What it makes, by its first output, and how many of that the player has.
	const FInventoryItemSpec* FirstOutput = Recipe->Outputs.IsEmpty() ? nullptr : &Recipe->Outputs[0];
	View.Output = FirstOutput ? FirstOutput->Definition : nullptr;
	View.OutputCount = FirstOutput ? FirstOutput->Count : 0;
	View.Name = !Recipe->DisplayName.IsEmpty() ? Recipe->DisplayName : View.Output ? View.Output->DisplayName : FText::GetEmpty();
	if (View.Output)
	{
		FInventoryItemQuery OutputQuery;
		OutputQuery.Definitions.Add(View.Output);
		View.Held = Crafter->CountItems(OutputQuery);
	}

	// What it takes, from what's in storage, as crafting takes it.
	const TArray<FInventoryContainerHandle> Storage = Crafter->GetStorageContainers();
	for (const FInventoryRecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		FInventoryRecipeIngredientView& Shown = View.Ingredients.AddDefaulted_GetRef();
		Shown.Count = Ingredient.Count;
		Shown.bConsumed = Ingredient.bConsume;
		Shown.Held = Inventory->CountItems(Storage, Ingredient.Query, true);
		Shown.bEnough = Shown.Held >= Ingredient.Count;
		Shown.Definition = Ingredient.DisplayItem;
		if (!Shown.Definition && !Ingredient.Query.Definitions.IsEmpty())
		{
			Shown.Definition = Ingredient.Query.Definitions[0];
		}
		if (!Shown.Definition)
		{
			const TArray<FInventoryItemHandle> Matching = Inventory->FindItems(Storage, Ingredient.Query, true);
			Shown.Definition = !Matching.IsEmpty() ? const_cast<UInventoryItemDefinition*>(Inventory->GetItemDefinition(Matching[0])) : nullptr;
		}
	}

	const FGameplayTagContainer Tags = GetContextTags();
	View.Error = Crafter->GetCraftError(Recipe, Tags, 1);
	View.CraftableTimes = View.Error == EInventoryError::None ? Crafter->GetCraftableTimes(Recipe, Tags, MaxCraftTimes) : 0;
	View.Reason = GetReason(View.Error);
	return View;
}

TArray<FInventoryRecipeView> UInventoryRecipeMenuWidget::GetRecipeViews() const
{
	TArray<FInventoryRecipeView> Views;
	for (const UInventoryRecipeMenuEntry* Entry : RecipeEntries)
	{
		if (Entry)
		{
			Views.Add(Entry->GetRecipeView());
		}
	}
	return Views;
}

void UInventoryRecipeMenuWidget::RefreshRecipes()
{
	const UInventoryComponent* Crafter = GetCrafter();
	const UInventoryRecipeBook* Book = GetBook();
	TArray<FInventoryRecipeView> Views;
	if (Crafter && Book && GetItem().IsSet())
	{
		for (UInventoryRecipe* Recipe : Crafter->GetRecipesUsing(Book, GetItem()))
		{
			FInventoryRecipeView View = MakeView(Recipe);
			if (View.CanCraft() || WhenUncraftable == EInventoryActionUnavailable::ShowDisabled)
			{
				Views.Add(MoveTemp(View));
			}
		}
	}

	// The same recipes as shown: each entry only updates, so the highlight stays where it is.
	bool bSameRecipes = Views.Num() == RecipeEntries.Num();
	for (int32 Index = 0; bSameRecipes && Index < Views.Num(); ++Index)
	{
		bSameRecipes = RecipeEntries[Index] && RecipeEntries[Index]->GetRecipe() == Views[Index].Recipe;
	}
	if (bSameRecipes)
	{
		for (int32 Index = 0; Index < Views.Num(); ++Index)
		{
			RecipeEntries[Index]->SetRecipe(Views[Index]);
		}

		// What the highlighted recipe says may have changed, such as now being out of materials.
		UInventoryRecipeMenuEntry* Highlighted = Cast<UInventoryRecipeMenuEntry>(GetHighlightedEntry());
		if (Highlighted && RecipeEntries.Contains(Highlighted))
		{
			const FText Description = GetEntryDescription(Highlighted);
			DescribeHighlightedEntry(Description);
			ShowOutput(&Highlighted->GetRecipeView());
		}
		return;
	}

	ClearRecipeEntries();
	if (!RecipePanel || !RecipeEntryClass)
	{
		if (!Views.IsEmpty())
		{
			UE_LOG(LogInventory, Warning, TEXT("[Inventory UI] %s: nowhere to list recipes. Give the menu a Recipe Panel and a Recipe Entry Class."), *GetPathNameSafe(this));
		}
		return;
	}

	for (const FInventoryRecipeView& View : Views)
	{
		UInventoryRecipeMenuEntry* Entry = CreateWidget<UInventoryRecipeMenuEntry>(this, RecipeEntryClass);
		if (!Entry)
		{
			continue;
		}
		RecipePanel->AddChild(Entry);
		Entry->SetRecipe(View);
		const TWeakObjectPtr<UInventoryRecipeMenuEntry> WeakEntry = Entry;
		Entry->OnClicked().AddWeakLambda(this, [this, WeakEntry]()
		{
			HandleRecipeEntryClicked(WeakEntry.Get());
		});
		RecipeEntries.Add(Entry);
	}

	// Listed again while open: placed for its new size, with the first recipe highlighted. Opening does this itself.
	if (IsActivated() && !IsClosing())
	{
		PlaceMenu();
		HighlightEntry(Cast<UCommonButtonBase>(GetDesiredFocusTarget()), /*bFocus*/ false);
		FocusDesiredTarget();
	}
}

void UInventoryRecipeMenuWidget::ClearRecipeEntries()
{
	for (UInventoryRecipeMenuEntry* Entry : RecipeEntries)
	{
		if (Entry)
		{
			if (Entry == GetHighlightedEntry())
			{
				ClearHighlight();
			}
			Entry->OnClicked().RemoveAll(this);
			Entry->RemoveFromParent();
		}
	}
	RecipeEntries.Reset();
	if (RecipePanel)
	{
		RecipePanel->ClearChildren();
	}
}

FText UInventoryRecipeMenuWidget::GetEntryDescription(UCommonButtonBase* Entry) const
{
	const UInventoryRecipeMenuEntry* RecipeEntry = Cast<UInventoryRecipeMenuEntry>(Entry);
	if (!RecipeEntry)
	{
		return Super::GetEntryDescription(Entry);
	}

	// Why it can't be made, or else what it makes.
	const FInventoryRecipeView& View = RecipeEntry->GetRecipeView();
	if (!View.CanCraft())
	{
		return View.Reason;
	}
	const UInventoryItemDefinition* Output = View.Output;
	return FText::FormatNamed(RecipeDescription,
		TEXT("Name"), View.Name,
		TEXT("Description"), Output ? Output->Description : FText::GetEmpty(),
		TEXT("Count"), FText::AsNumber(View.OutputCount),
		TEXT("Held"), FText::AsNumber(View.Held),
		TEXT("Craftable"), FText::AsNumber(View.CraftableTimes));
}

void UInventoryRecipeMenuWidget::NativeOnEntryHighlighted(UCommonButtonBase* Entry, const FText& Description)
{
	Super::NativeOnEntryHighlighted(Entry, Description);

	if (const UInventoryRecipeMenuEntry* RecipeEntry = Cast<UInventoryRecipeMenuEntry>(Entry))
	{
		ShowOutput(&RecipeEntry->GetRecipeView());
		OnRecipeHighlighted(RecipeEntry->GetRecipeView());
	}
}

void UInventoryRecipeMenuWidget::ShowOutput(const FInventoryRecipeView* View)
{
	UInventoryItemDefinition* Output = View ? View->Output.Get() : nullptr;
	ShowPreviewIn(OutputItem, Output, View ? View->OutputCount : 0);
	InventoryWidgets::ShowIcon(OutputIcon, Output, OutputIconHandle);
	if (OutputNameText)
	{
		OutputNameText->SetText(View ? View->Name : FText::GetEmpty());
	}
	if (OutputDescriptionText)
	{
		OutputDescriptionText->SetText(Output ? Output->Description : FText::GetEmpty());
	}
}

void UInventoryRecipeMenuWidget::HandleRecipeEntryClicked(UInventoryRecipeMenuEntry* Entry)
{
	if (!Entry || IsClosing() || IsPrompting())
	{
		return;
	}

	// As things stand now, which may have changed since it was listed.
	const FInventoryRecipeView View = MakeView(Entry->GetRecipe());
	Entry->SetRecipe(View);
	if (!View.CanCraft())
	{
		DescribeHighlightedEntry(View.Reason);
		OnCraftFailed(View, View.Error);
		return;
	}
	if (CraftPrompt == EInventoryActionPrompt::None)
	{
		CraftRecipe(View.Recipe, 1);
		return;
	}

	// {Count} and {Max} stay as they are, for the prompt to fill in as the count changes.
	FModularPromptRequest Request;
	Request.Kind = CraftPrompt == EInventoryActionPrompt::Quantity ? EModularPromptKind::Quantity : EModularPromptKind::Confirm;
	Request.Title = View.Name;
	Request.Message = FText::FormatNamed(CraftPromptText, TEXT("Item"), View.Name);
	Request.ConfirmLabel = CraftPromptConfirmLabel;
	if (Request.Kind == EModularPromptKind::Quantity)
	{
		Request.MinCount = 1;
		Request.MaxCount = FMath::Max(1, View.CraftableTimes);
		Request.DefaultCount = 1;
	}

	const TWeakObjectPtr<ThisClass> WeakThis = this;
	const TWeakObjectPtr<UInventoryRecipe> WeakRecipe = View.Recipe;
	const bool bAsksHowMany = Request.Kind == EModularPromptKind::Quantity;
	ShowPrompt(Request, [WeakThis, WeakRecipe, bAsksHowMany](bool bConfirmed, int32 Count)
	{
		ThisClass* Menu = WeakThis.Get();
		UInventoryRecipe* Recipe = WeakRecipe.Get();
		if (Menu && Recipe && bConfirmed && Menu->IsActivated() && !Menu->IsClosing())
		{
			Menu->CraftRecipe(Recipe, bAsksHowMany ? FMath::Max(1, Count) : 1);
		}
	});
}

bool UInventoryRecipeMenuWidget::CraftRecipe(UInventoryRecipe* Recipe, int32 Times)
{
	UInventoryComponent* Crafter = GetCrafter();
	if (!Recipe || !Crafter)
	{
		return false;
	}

	// Taken from the menu's item first, so the stack the player picked is the one used.
	const FInventoryRecipeView Before = MakeView(Recipe);
	const TArray<FInventoryItemHandle> Preferred = { GetItem() };
	TArray<FInventoryItemHandle> Made;
	EInventoryError Error = EInventoryError::None;
	if (!Crafter->CraftPreferring(Recipe, Preferred, GetContextTags(), FMath::Clamp(Times, 1, MaxCraftTimes), Made, Error))
	{
		FInventoryRecipeView Failed = Before;
		Failed.Error = Error;
		Failed.Reason = GetReason(Error);
		DescribeHighlightedEntry(Failed.Reason);
		OnCraftFailed(Failed, Error);
		return false;
	}

	OnRecipeCrafted(Before, Times, Made);

	// Crafting may have used up the item, and the menu goes with it then. Otherwise it shows the new counts, unless it closes.
	if (IsActivated() && !IsClosing())
	{
		if (bCloseOnCraft)
		{
			CloseMenu();
		}
		else
		{
			RefreshRecipes();
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
