#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"

#include "Actions/InventoryItemAction.h"
#include "InventoryTypes.h"
#include "InventoryUITypes.h"
#include "Widgets/InventoryItemMenuWidget.h"

#include "InventoryRecipeMenuWidget.generated.h"

class UImage;
class UInventoryComponent;
class UInventoryItemDefinition;
class UInventoryItemWidget;
class UInventoryRecipe;
class UInventoryRecipeBook;
class UPanelWidget;
class UTextBlock;
struct FInventoryChangeSet;


/** One ingredient of a recipe, as a crafting menu shows it. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryRecipeIngredientView
{
	GENERATED_BODY()

	/** What it shows as: the ingredient's Display Item, else the first of its query's Definitions, else the first held item that matches. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	TObjectPtr<UInventoryItemDefinition> Definition;

	/** How many one craft takes, or needs, for a tool. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	int32 Count = 1;

	/** How many the crafter holds that would do, in their storage. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	int32 Held = 0;

	/** Off for a tool: needed, but kept. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	bool bConsumed = true;

	/** Whether the crafter holds enough of it for one craft. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	bool bEnough = false;
};


/** A recipe, as a crafting menu shows it: what it makes, what it takes, and whether the player can make it now. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryRecipeView
{
	GENERATED_BODY()

	bool CanCraft() const { return CraftableTimes > 0; }

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	TObjectPtr<UInventoryRecipe> Recipe;

	/** The recipe's Display Name, or else the name of what it makes. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	FText Name;

	/** What it makes: its first output. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	TObjectPtr<UInventoryItemDefinition> Output;

	/** How many of Output one craft makes. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	int32 OutputCount = 0;

	/** How many of Output the player holds now. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	int32 Held = 0;

	/** How many times it can be made at once now. 0 when it can't be. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	int32 CraftableTimes = 0;

	/** Why it can't be made now, such as Missing Ingredients or No Space. None when it can. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	EInventoryError Error = EInventoryError::None;

	/** What Error means for the player, from the menu's Uncraftable Reasons. Empty when it can be made. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	FText Reason;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe")
	TArray<FInventoryRecipeIngredientView> Ingredients;
};


/** One ingredient in a recipe menu's entry: its icon, how many one craft takes, and how many the player has. */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryRecipeIngredientWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows an ingredient. Recipe entries call it for those they make. */
	void SetIngredient(const FInventoryRecipeIngredientView& InIngredient);

	UFUNCTION(BlueprintPure, Category = "Inventory|Recipe")
	FInventoryRecipeIngredientView GetIngredient() const { return Ingredient; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** The ingredient shown changed. Show anything else about it here, such as marking one the player hasn't enough of. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Recipe")
	void OnIngredientChanged(const FInventoryRecipeIngredientView& NewIngredient);

	/** The ingredient drawn as an item: its icon, with Count as a preview's count. Put it in a Scale Box to fit it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The ingredient's icon, at its texture's size. Put it in a Scale Box to fit it. Collapsed without one. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** In Count Format. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** Shown only for a tool, which is needed but kept, such as a small mark beside it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ToolMarker;

	/** What Count Text says: {Count} is how many one craft takes, and {Held} how many the player has, such as {Count}, or {Held}/{Count}. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe")
	FText CountFormat = INVTEXT("{Count}");

	/** Count Text takes Missing Color while the player hasn't enough for one craft. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe")
	bool bTintWhenMissing = true;

	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe", meta = (EditCondition = "bTintWhenMissing"))
	FLinearColor MissingColor = FLinearColor(FColor(217, 83, 79));

private:
	FInventoryRecipeIngredientView Ingredient;
	FSlateColor DefaultCountColor;
	TSharedPtr<FStreamableHandle> IconHandle;
};


/**
 * One recipe in a recipe menu: what it makes, how many of that the player has, how many times it can be made now, and what it takes, each in
 * a named widget, each optional. A Common UI button: picking it crafts the recipe. A recipe that can't be made now takes Unavailable Style,
 * and can still be highlighted, so the player sees why.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryRecipeMenuEntry : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Shows a recipe. The menu calls it as it lists recipes, and again as what the player holds changes. */
	void SetRecipe(const FInventoryRecipeView& InView);

	const FInventoryRecipeView& GetRecipeView() const { return View; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Recipe", meta = (DisplayName = "Get Recipe View"))
	FInventoryRecipeView K2_GetRecipeView() const { return View; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Recipe")
	UInventoryRecipe* GetRecipe() const { return View.Recipe; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonButtonBase Interface
	virtual void NativeOnCurrentTextStyleChanged() override;
	//~ End UCommonButtonBase Interface

	/** The recipe shown changed, or what the player holds did. Show anything else about it here, such as greying out one that can't be made. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Recipe")
	void OnRecipeChanged(const FInventoryRecipeView& NewView);

	/** What it makes, drawn as an item, with the count one craft makes. Put it in a Scale Box to fit it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> OutputItem;

	/** What it makes, as an icon at its texture's size. Put it in a Scale Box to fit it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UImage> OutputIcon;

	/** The recipe's name, in the button Style's text style for the button's state. Make it a Common Text, which text styles need. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** How many one craft makes, in Output Count Format. Collapsed when it makes one. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OutputCountText;

	/** How many of what it makes the player holds now, in Held Format, such as the ammo already carried. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeldText;

	/** How many times it can be made now, in Craftable Format. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CraftableText;

	/** Why it can't be made now, shown only while it can't. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReasonText;

	/** Gets an Ingredient Class widget for each ingredient, in order, with an Ingredient Separator Class widget between them, such as a Horizontal Box. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Recipe", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> IngredientPanel;

	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe")
	TSubclassOf<UInventoryRecipeIngredientWidget> IngredientClass;

	/** Goes between ingredients in Ingredient Panel, such as a plus sign. None puts nothing between them. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe")
	TSubclassOf<UUserWidget> IngredientSeparatorClass;

	/** What Output Count Text says. {Count} is how many one craft makes, such as x{Count}. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe")
	FText OutputCountFormat = INVTEXT("x{Count}");

	/** What Held Text says. {Count} is how many the player holds. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe")
	FText HeldFormat = INVTEXT("{Count}");

	/** What Craftable Text says. {Count} is how many times it can be made now. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe")
	FText CraftableFormat = INVTEXT("{Count}");

	/** Used instead of Style while the recipe can't be made now, such as a greyed-out look. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Recipe")
	TSubclassOf<UCommonButtonStyle> UnavailableStyle;

private:
	void ApplyTextStyle();
	void RefreshIngredients();

	FInventoryRecipeView View;
	TSubclassOf<UCommonButtonStyle> AvailableStyle;
	TSharedPtr<FStreamableHandle> IconHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryRecipeIngredientWidget>> IngredientWidgets;
};


/**
 * What an item can be crafted into, as some games' Combine shows it: each recipe of its recipe book that takes the item, with what it makes,
 * how many of that the player has, how many times it can be made, and what it takes. Picking one crafts it from the player's storage, taking
 * from the item the menu is for first, as many times as Craft Prompt asks.
 * The Craft item action opens it, where the screen opens item menus, replacing the item menu it was picked from, and gives it the recipes.
 * It's an item menu: it opens beside the item, or as a picker in the middle of the screen with Placement As Designed, and can list item
 * actions under the recipes. Description Text, the menu's and the screen's, says what picking the highlighted recipe does, or why it can't
 * be made; the Output widgets show what it makes.
 * Lay Recipe Panel out inside Menu Panel. The menu stays open after a craft, showing the new counts, until Close On Craft, or until the item
 * is used up.
 */
UCLASS(Abstract, Blueprintable)
class MODULARINVENTORYUI_API UInventoryRecipeMenuWidget : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

public:
	UInventoryRecipeMenuWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Lists Book's recipes that take the item, checked with Context Tags on top of the menu's own, such as a workbench's. The Craft action
	 * calls it as it opens the menu, before Set Item. None lists the menu's own Recipe Book.
	 */
	void SetRecipeSource(const UInventoryRecipeBook* Book, const FGameplayTagContainer& InContextTags);

	/** The recipes the menu lists without a Set Recipe Source, for whoever opens it to ask. */
	const UInventoryRecipeBook* GetDefaultRecipeBook() const { return RecipeBook; }

	/** Whether the item takes part in a recipe of Recipe Book, or the menu has an action for it. Without a Recipe Book, whoever opens it decides. */
	virtual bool CanOpenFor(const FInventoryItemActionContext& Context) const override;

	/** The recipes listed now, as shown. */
	UFUNCTION(BlueprintPure, Category = "Recipe Menu")
	TArray<FInventoryRecipeView> GetRecipeViews() const;

	/** Lists the recipes again, such as after something the menu doesn't see changed what can be made. Changes to the player's items do it. */
	UFUNCTION(BlueprintCallable, Category = "Recipe Menu")
	void RefreshRecipes();

	/**
	 * Crafts the recipe Times times from the player's storage, taking from the menu's item first, as picking its entry does once Craft
	 * Prompt is answered. False, changing nothing, when it can't, and On Craft Failed says why.
	 */
	UFUNCTION(BlueprintCallable, Category = "Recipe Menu")
	bool CraftRecipe(UInventoryRecipe* Recipe, int32 Times = 1);

protected:
	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UModularContextMenuWidget Interface
	virtual FText GetEntryDescription(UCommonButtonBase* Entry) const override;
	virtual void NativeOnEntryHighlighted(UCommonButtonBase* Entry, const FText& Description) override;
	//~ End UModularContextMenuWidget Interface

	//~ Begin UInventoryItemMenuWidget Interface
	virtual void NativeOnItemChanged() override;
	//~ End UInventoryItemMenuWidget Interface

	/** A recipe was highlighted, by the mouse or focus. Show anything else about it here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Recipe Menu")
	void OnRecipeHighlighted(const FInventoryRecipeView& View);

	/** A recipe was crafted Times times, making Made, such as to play a sound. View is how it stood before. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Recipe Menu")
	void OnRecipeCrafted(const FInventoryRecipeView& View, int32 Times, const TArray<FInventoryItemHandle>& Made);

	/** A recipe picked couldn't be crafted, for Error, after Description Text says why. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Recipe Menu")
	void OnCraftFailed(const FInventoryRecipeView& View, EInventoryError Error);

	/** The player's Inventory Component, which crafts. */
	UInventoryComponent* GetCrafter() const;

	// Recipes
	/** The recipes the menu lists, when whoever opens it gives none, such as a Craft action without a Recipe Book of its own. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Recipes")
	TObjectPtr<UInventoryRecipeBook> RecipeBook;

	/** Context tags the recipes are checked and crafted with, such as the station the menu is for. Added to those whoever opens it gives. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Recipes")
	FGameplayTagContainer ContextTags;

	/** Made in Recipe Panel for each recipe. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Recipes")
	TSubclassOf<UInventoryRecipeMenuEntry> RecipeEntryClass;

	/** Holds the recipes, such as a Vertical Box, inside Menu Panel. It's emptied each time the menu lists recipes, so put nothing of your own in it. */
	UPROPERTY(BlueprintReadOnly, Category = "Recipe Menu|Recipes", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RecipePanel;

	/** Whether a recipe that can't be made now is listed disabled, with why, so the player sees what the item could make, or left out. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Recipes")
	EInventoryActionUnavailable WhenUncraftable = EInventoryActionUnavailable::ShowDisabled;

	/** The most times one recipe is counted as craftable, and crafted at once. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Recipes", meta = (ClampMin = "1"))
	int32 MaxCraftTimes = 99;

	// Crafting
	/**
	 * What picking a recipe asks first: nothing, crafting it once; to confirm; or how many times, from one to as many as can be made. The
	 * question is asked where Prompt Presentation says.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Crafting")
	EInventoryActionPrompt CraftPrompt = EInventoryActionPrompt::None;

	/** What it asks. {Item} is the recipe's name, {Count} the count chosen so far, and {Max} the most that can be made. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Crafting", meta = (EditCondition = "CraftPrompt != EInventoryActionPrompt::None"))
	FText CraftPromptText;

	/** The prompt's confirm button's label, such as Craft, on buttons that are Modular Buttons. Empty keeps the button's own. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Crafting", meta = (EditCondition = "CraftPrompt != EInventoryActionPrompt::None"))
	FText CraftPromptConfirmLabel;

	/** The menu closes once a recipe is crafted. Off, it stays open with the new counts, to craft again. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Crafting")
	bool bCloseOnCraft = false;

	// Description
	/**
	 * What Description Text says while a recipe that can be made is highlighted: what picking it does, such as "Craft {Name}.". {Name} is
	 * what it makes, {Count} how many one craft makes, {Held} how many the player has, {Craftable} how many times it can be made, and
	 * {Description} the description of what it makes, though that belongs in Output Description Text. Empty says nothing.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Description", meta = (MultiLine = true))
	FText RecipeDescription;

	/** Why a recipe can't be made, by the reason, such as "Not enough materials." for Missing Ingredients, said in its place. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Description")
	TMap<EInventoryError, FText> UncraftableReasons;

	/** Why a recipe can't be made, for reasons not in Uncraftable Reasons. */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe Menu|Description")
	FText UncraftableReason;

	// What the highlighted recipe makes, each optional, such as a details panel beside the list.
	UPROPERTY(BlueprintReadOnly, Category = "Recipe Menu|Output", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> OutputItem;

	UPROPERTY(BlueprintReadOnly, Category = "Recipe Menu|Output", meta = (BindWidgetOptional))
	TObjectPtr<UImage> OutputIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Recipe Menu|Output", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OutputNameText;

	UPROPERTY(BlueprintReadOnly, Category = "Recipe Menu|Output", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OutputDescriptionText;

private:
	/** The book listed: from Set Recipe Source, or else Recipe Book. */
	const UInventoryRecipeBook* GetBook() const;
	FGameplayTagContainer GetContextTags() const;

	FInventoryRecipeView MakeView(UInventoryRecipe* Recipe) const;
	FText GetReason(EInventoryError Error) const;

	void HandleRecipeEntryClicked(UInventoryRecipeMenuEntry* Entry);
	void HandleRecipeRegistryChanged(const FInventoryChangeSet& Changes);

	/** Fills in Output Item, Output Icon, Output Name Text and Output Description Text for the recipe, or empties them. */
	void ShowOutput(const FInventoryRecipeView* View);

	void ClearRecipeEntries();

	/** The recipe book and context tags from Set Recipe Source. */
	TWeakObjectPtr<const UInventoryRecipeBook> SourceBook;
	FGameplayTagContainer SourceContextTags;

	/** Hears changes to the player's items, while the menu is open. */
	FInventoryRegistryListener RecipeListener;

	TSharedPtr<FStreamableHandle> OutputIconHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryRecipeMenuEntry>> RecipeEntries;
};
