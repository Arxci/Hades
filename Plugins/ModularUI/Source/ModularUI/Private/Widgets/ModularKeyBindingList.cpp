#include "Widgets/ModularKeyBindingList.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameplayTagContainer.h"

#include "ModularUI.h"
#include "Widgets/ModularKeyBindingSlot.h"
#include "Widgets/ModularKeyCapture.h"

namespace
{
	const FKeyMappingRow* FindMappingRow(const UEnhancedPlayerMappableKeyProfile& Profile, FName MappingName)
	{
		return Profile.FindKeyMappingRow(MappingName);
	}

	/** The Display Name of the action's mappings, for players. */
	FText GetMappingDisplayName(const UEnhancedPlayerMappableKeyProfile& Profile, FName MappingName)
	{
		const FKeyMappingRow* MappingRow = FindMappingRow(Profile, MappingName);
		return MappingRow && MappingRow->HasAnyMappings() ? MappingRow->Mappings.begin()->GetDisplayName() : FText::FromName(MappingName);
	}

	/** The Display Category of the action's mappings. */
	FText GetMappingCategory(const UEnhancedPlayerMappableKeyProfile& Profile, FName MappingName)
	{
		const FKeyMappingRow* MappingRow = FindMappingRow(Profile, MappingName);
		return MappingRow && MappingRow->HasAnyMappings() ? MappingRow->Mappings.begin()->GetDisplayCategory() : FText::GetEmpty();
	}

	/** Puts the key on the action's key slot, through Enhanced Input's user settings. False, logging why, when it couldn't. */
	bool MapPlayerKey(UEnhancedInputUserSettings& Settings, FName MappingName, EPlayerMappableKeySlot KeySlot, const FKey& NewKey, const UObject* Context)
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = MappingName;
		Args.Slot = KeySlot;
		Args.NewKey = NewKey;

		FGameplayTagContainer FailureReason;
		Settings.MapPlayerKey(Args, FailureReason);
		if (!FailureReason.IsEmpty())
		{
			UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s couldn't put %s on %s: %s"), *GetPathNameSafe(Context), *NewKey.ToString(), *MappingName.ToString(),
				*FailureReason.ToStringSimple());
			return false;
		}
		return true;
	}
}

// Header
void UModularKeyBindingHeader::SetCategory(const FText& InCategory)
{
	ShownCategory = InCategory;
	if (CategoryText)
	{
		CategoryText->SetText(InCategory);
	}
	OnCategoryChanged(InCategory);
}

// Row
void UModularKeyBindingRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UModularKeyBindingSlot* KeySlot = Cast<UModularKeyBindingSlot>(Widget))
			{
				KeySlot->SetRow(*this);
				KeySlots.Add(KeySlot);
			}
		});
	}
	if (ResetButton)
	{
		ResetButton->OnClicked().AddUObject(this, &ThisClass::HandleResetClicked);
	}
}

void UModularKeyBindingRow::SetMapping(UModularKeyBindingList& InList, FName InMappingName)
{
	OwningList = &InList;
	ActionMappingName = InMappingName;
	Refresh();
}

void UModularKeyBindingRow::Refresh()
{
	const UModularKeyBindingList* List = OwningList.Get();
	if (!List)
	{
		return;
	}

	const TArray<FPlayerKeyMapping> Mappings = List->GetDeviceMappings(ActionMappingName);
	ActionDisplayName = Mappings.IsEmpty() ? FText::FromName(ActionMappingName) : Mappings[0].GetDisplayName();
	bKeysCustomized = Mappings.ContainsByPredicate([](const FPlayerKeyMapping& Mapping) { return Mapping.IsCustomized(); });

	if (DisplayNameText)
	{
		DisplayNameText->SetText(ActionDisplayName);
	}
	if (ResetButton)
	{
		ResetButton->SetVisibility(bKeysCustomized ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	// Each slot changes one of the keys the game mapped for the device. One past them is hidden, keeping its place, as there's no key to
	// change.
	const ECommonInputType InputType = List->GetInputType();
	const FName GamepadName = List->GetGamepadName();
	for (UModularKeyBindingSlot* KeySlot : KeySlots)
	{
		if (KeySlot)
		{
			const int32 KeyIndex = KeySlot->GetKeyIndex();
			const bool bHasMapping = Mappings.IsValidIndex(KeyIndex);
			KeySlot->SetVisibility(bHasMapping ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
			KeySlot->ShowKey(bHasMapping ? Mappings[KeyIndex].GetCurrentKey() : FKey(), InputType, GamepadName);
		}
	}
	OnRowRefreshed(ActionDisplayName, bKeysCustomized);
}

void UModularKeyBindingRow::ResetToDefault()
{
	if (UModularKeyBindingList* List = OwningList.Get())
	{
		List->ResetMapping(ActionMappingName);
	}
}

void UModularKeyBindingRow::HandleResetClicked()
{
	ResetToDefault();
}

UModularKeyBindingSlot* UModularKeyBindingRow::FindFirstSlot() const
{
	UModularKeyBindingSlot* First = nullptr;
	for (UModularKeyBindingSlot* KeySlot : KeySlots)
	{
		if (KeySlot && KeySlot->IsVisible() && (!First || KeySlot->GetKeyIndex() < First->GetKeyIndex()))
		{
			First = KeySlot;
		}
	}
	return First;
}

// List
UModularKeyBindingList::UModularKeyBindingList(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CancelKeys = { EKeys::Escape, EKeys::Gamepad_Special_Right };
}

void UModularKeyBindingList::NativePreConstruct()
{
	Super::NativePreConstruct();

#if WITH_EDITOR
	if (IsDesignTime())
	{
		RefreshDesignerPreview();
	}
#endif
}

void UModularKeyBindingList::NativeConstruct()
{
	Super::NativeConstruct();

	// The keys change with the player's rebinding, here or anywhere else, which rebuilds the control mappings, and their icons change with
	// the gamepad in use.
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetEnhancedInput())
	{
		EnhancedInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::HandleControlMappingsRebuilt);
	}
	if (UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		CommonInput->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	Refresh();
}

void UModularKeyBindingList::NativeDestruct()
{
	StopWaiting();
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetEnhancedInput())
	{
		EnhancedInput->ControlMappingsRebuiltDelegate.RemoveAll(this);
	}
	if (UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		CommonInput->OnInputMethodChangedNative.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UModularKeyBindingList::HandleControlMappingsRebuilt()
{
	Refresh();
}

void UModularKeyBindingList::HandleInputMethodChanged(ECommonInputType InputType)
{
	Refresh();
}

UEnhancedInputLocalPlayerSubsystem* UModularKeyBindingList::GetEnhancedInput() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

UEnhancedInputUserSettings* UModularKeyBindingList::GetUserSettings() const
{
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetEnhancedInput();
	return EnhancedInput ? EnhancedInput->GetUserSettings() : nullptr;
}

UEnhancedPlayerMappableKeyProfile* UModularKeyBindingList::GetKeyProfile() const
{
	UEnhancedInputUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetActiveKeyProfile() : nullptr;
}

ECommonInputType UModularKeyBindingList::GetInputType() const
{
	return Device == EModularKeyBindingDevice::Gamepad ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard;
}

FName UModularKeyBindingList::GetGamepadName() const
{
	const UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	return CommonInput ? CommonInput->GetCurrentGamepadName() : NAME_None;
}

bool UModularKeyBindingList::IsDeviceMapping(const FPlayerKeyMapping& Mapping) const
{
	const FKey DefiningKey = Mapping.GetDefaultKey().IsValid() ? Mapping.GetDefaultKey() : Mapping.GetCurrentKey();
	return DefiningKey.IsValid() && !DefiningKey.IsTouch() && DefiningKey.IsGamepadKey() == (Device == EModularKeyBindingDevice::Gamepad);
}

TArray<FPlayerKeyMapping> UModularKeyBindingList::GetDeviceMappings(FName MappingName) const
{
	TArray<FPlayerKeyMapping> Mappings;
	const UEnhancedPlayerMappableKeyProfile* Profile = GetKeyProfile();
	const FKeyMappingRow* MappingRow = Profile ? FindMappingRow(*Profile, MappingName) : nullptr;
	if (!MappingRow)
	{
		return Mappings;
	}

	for (const FPlayerKeyMapping& Mapping : MappingRow->Mappings)
	{
		if (IsDeviceMapping(Mapping))
		{
			Mappings.Add(Mapping);
		}
	}
	Mappings.Sort([](const FPlayerKeyMapping& A, const FPlayerKeyMapping& B) { return A.GetSlot() < B.GetSlot(); });
	return Mappings;
}

void UModularKeyBindingList::Refresh()
{
	if (!RowPanel || IsDesignTime())
	{
		return;
	}

	// What to list: each category's actions, under its heading, the categories in the order their first action came, and the actions in the
	// order Enhanced Input registered them.
	TArray<FString> CategoryKeys;
	TArray<FText> CategoryNames;
	TArray<TArray<FName>> CategoryActions;
	if (UEnhancedPlayerMappableKeyProfile* Profile = GetKeyProfile())
	{
		for (const TPair<FName, FKeyMappingRow>& RowPair : Profile->GetPlayerMappingRows())
		{
			const TArray<FPlayerKeyMapping> Mappings = GetDeviceMappings(RowPair.Key);
			if (Mappings.IsEmpty())
			{
				continue;
			}

			const FText RowCategory = Mappings[0].GetDisplayCategory();
			int32 CategoryIndex = CategoryKeys.Find(RowCategory.ToString());
			if (CategoryIndex == INDEX_NONE)
			{
				CategoryIndex = CategoryKeys.Add(RowCategory.ToString());
				CategoryNames.Add(RowCategory);
				CategoryActions.AddDefaulted();
			}
			CategoryActions[CategoryIndex].Add(RowPair.Key);
		}
	}

	TArray<FString> Layout;
	for (int32 CategoryIndex = 0; CategoryIndex < CategoryKeys.Num(); ++CategoryIndex)
	{
		if (HeaderClass)
		{
			Layout.Add(TEXT("Header:") + CategoryKeys[CategoryIndex]);
		}
		for (const FName& MappingName : CategoryActions[CategoryIndex])
		{
			Layout.Add(TEXT("Row:") + MappingName.ToString());
		}
	}

	// The same actions in the same order keep their rows, so focus stays on the slot it's on, such as the one whose key just changed.
	if (Layout == EntryLayout && Entries.Num() == Layout.Num())
	{
		for (UUserWidget* Entry : Entries)
		{
			if (UModularKeyBindingRow* Row = Cast<UModularKeyBindingRow>(Entry))
			{
				Row->Refresh();
			}
		}
		return;
	}

	StopWaiting();
	RowPanel->ClearChildren();
	Entries.Reset();
	EntryLayout = Layout;
	if (!RowClass)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s has no Row Class, so it lists no keys."), *GetPathNameSafe(this));
		return;
	}

	for (int32 CategoryIndex = 0; CategoryIndex < CategoryKeys.Num(); ++CategoryIndex)
	{
		if (HeaderClass)
		{
			if (UModularKeyBindingHeader* Header = CreateWidget<UModularKeyBindingHeader>(this, HeaderClass))
			{
				RowPanel->AddChild(Header);
				Header->SetCategory(CategoryNames[CategoryIndex]);
				Entries.Add(Header);
			}
		}
		for (const FName& MappingName : CategoryActions[CategoryIndex])
		{
			if (UModularKeyBindingRow* Row = CreateWidget<UModularKeyBindingRow>(this, RowClass))
			{
				RowPanel->AddChild(Row);
				Row->SetMapping(*this, MappingName);
				Entries.Add(Row);
			}
		}
	}
}

UModularKeyBindingSlot* UModularKeyBindingList::GetFirstSlot() const
{
	for (UUserWidget* Entry : Entries)
	{
		const UModularKeyBindingRow* Row = Cast<UModularKeyBindingRow>(Entry);
		if (UModularKeyBindingSlot* First = Row ? Row->FindFirstSlot() : nullptr)
		{
			return First;
		}
	}
	return nullptr;
}

bool UModularKeyBindingList::IsWaitingForKey() const
{
	return Capture.IsValid();
}

void UModularKeyBindingList::WaitForKey(UModularKeyBindingSlot& KeySlot)
{
	StopWaiting();

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer || !FSlateApplication::IsInitialized())
	{
		return;
	}
	if (!GetUserSettings())
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s can't change keys: Enhanced Input has no user settings. Turn on Enable User Settings in Project Settings > Engine > Enhanced Input."),
			*GetPathNameSafe(this));
		return;
	}

	WaitingSlot = &KeySlot;
	KeySlot.SetWaitingForKey(true);

	// First, so nothing else sees the key, such as Back, or a button under the cursor.
	const TWeakObjectPtr<ThisClass> WeakThis(this);
	Capture = MakeShared<FModularKeyCapture>(*LocalPlayer, [WeakThis](const FKey& PressedKey)
	{
		ThisClass* List = WeakThis.Get();
		return !List || List->HandleCapturedKey(PressedKey);
	});
	FSlateApplication::Get().RegisterInputPreProcessor(Capture, 0);
	OnWaitingChanged.Broadcast(true);
}

void UModularKeyBindingList::StopWaiting()
{
	const bool bWasWaiting = Capture.IsValid();
	if (UModularKeyBindingSlot* KeySlot = WaitingSlot.Get())
	{
		KeySlot->SetWaitingForKey(false);
	}
	WaitingSlot.Reset();
	RetireCapture();
	if (bWasWaiting)
	{
		OnWaitingChanged.Broadcast(false);
	}
}

void UModularKeyBindingList::RetireCapture()
{
	if (!Capture)
	{
		return;
	}

	// It goes from Slate on a later frame, never from inside its own handling of a press, and only once the keys it took are let go, so
	// nothing sees their releases either.
	Capture->Stop();
	const TSharedPtr<FModularKeyCapture> Retired = MoveTemp(Capture);
	Capture.Reset();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Retired](float)
	{
		if (!Retired->IsDone())
		{
			return true;
		}
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Retired);
		}
		return false;
	}));
}

bool UModularKeyBindingList::HandleCapturedKey(const FKey& Key)
{
	switch (ModularUI::ClassifyCapturedKey(Key, Device == EModularKeyBindingDevice::Gamepad, CancelKeys, ReservedKeys, bAllowMouseButtons))
	{
	case EModularKeyCaptureResult::Ignore:
		return false;
	case EModularKeyCaptureResult::Cancel:
		StopWaiting();
		return true;
	case EModularKeyCaptureResult::Bind:
		break;
	}

	UModularKeyBindingSlot* KeySlot = WaitingSlot.Get();
	StopWaiting();
	if (KeySlot)
	{
		BindKey(*KeySlot, Key);
	}
	return true;
}

TArray<FName> UModularKeyBindingList::FindActionsWithKey(const UEnhancedPlayerMappableKeyProfile& Profile, FName MappingName, const FKey& Key) const
{
	TArray<FName> WithKey;
	Profile.GetMappingNamesForKey(Key, WithKey);

	const FString ActionCategory = GetMappingCategory(Profile, MappingName).ToString();
	TArray<FName> Others;
	for (const FName& Other : WithKey)
	{
		const bool bCounts = Other != MappingName && (!bConflictsWithinCategory || GetMappingCategory(Profile, Other).ToString().Equals(ActionCategory));
		if (bCounts)
		{
			Others.AddUnique(Other);
		}
	}
	return Others;
}

void UModularKeyBindingList::BindKey(UModularKeyBindingSlot& KeySlot, const FKey& NewKey)
{
	UModularKeyBindingRow* Row = KeySlot.GetRow();
	UEnhancedInputUserSettings* Settings = GetUserSettings();
	const UEnhancedPlayerMappableKeyProfile* Profile = GetKeyProfile();
	if (!Row || !Settings || !Profile)
	{
		return;
	}

	// Only a key the game mapped for the device changes: its slots are the only ones shown.
	const FName MappingName = Row->GetMappingName();
	const TArray<FPlayerKeyMapping> Mappings = GetDeviceMappings(MappingName);
	const int32 KeyIndex = KeySlot.GetKeyIndex();
	if (!Mappings.IsValidIndex(KeyIndex))
	{
		return;
	}
	const FPlayerKeyMapping& Current = Mappings[KeyIndex];
	const FKey OldKey = Current.GetCurrentKey();
	if (OldKey == NewKey)
	{
		return;
	}

	const TArray<FName> Others = FindActionsWithKey(*Profile, MappingName, NewKey);
	TArray<FText> OtherNames;
	for (const FName& Other : Others)
	{
		OtherNames.Add(GetMappingDisplayName(*Profile, Other));
	}
	const FText ActionName = Row->GetActionDisplayName();
	if (!Others.IsEmpty() && KeyConflict == EModularKeyConflict::Refuse)
	{
		OnKeyConflict.Broadcast(ActionName, NewKey, OtherNames, /*bBound*/ false);
		return;
	}

	if (!MapPlayerKey(*Settings, MappingName, Current.GetSlot(), NewKey, this))
	{
		return;
	}

	// Swapped, each other action takes the key this one had. Where it had none, they keep the key too, as with Allow.
	if (KeyConflict == EModularKeyConflict::Swap && OldKey.IsValid())
	{
		for (const FName& Other : Others)
		{
			for (const FPlayerKeyMapping& OtherMapping : GetDeviceMappings(Other))
			{
				if (OtherMapping.GetCurrentKey() == NewKey)
				{
					MapPlayerKey(*Settings, Other, OtherMapping.GetSlot(), OldKey, this);
				}
			}
		}
	}

	ApplyKeys();
	if (!Others.IsEmpty())
	{
		OnKeyConflict.Broadcast(ActionName, NewKey, OtherNames, /*bBound*/ true);
	}
	OnKeysChanged.Broadcast(MappingName);
}

bool UModularKeyBindingList::ResetMappingKeys(FName MappingName)
{
	UEnhancedInputUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return false;
	}

	bool bChanged = false;
	for (const FPlayerKeyMapping& Mapping : GetDeviceMappings(MappingName))
	{
		if (Mapping.IsCustomized() && Mapping.GetDefaultKey().IsValid())
		{
			bChanged |= MapPlayerKey(*Settings, MappingName, Mapping.GetSlot(), Mapping.GetDefaultKey(), this);
		}
	}
	return bChanged;
}

void UModularKeyBindingList::ResetMapping(FName MappingName)
{
	StopWaiting();
	if (ResetMappingKeys(MappingName))
	{
		ApplyKeys();
		OnKeysChanged.Broadcast(MappingName);
	}
}

void UModularKeyBindingList::ResetAllToDefault()
{
	StopWaiting();

	TArray<FName> Changed;
	for (UUserWidget* Entry : Entries)
	{
		const UModularKeyBindingRow* Row = Cast<UModularKeyBindingRow>(Entry);
		if (Row && ResetMappingKeys(Row->GetMappingName()))
		{
			Changed.Add(Row->GetMappingName());
		}
	}
	if (!Changed.IsEmpty())
	{
		ApplyKeys();
		for (const FName& MappingName : Changed)
		{
			OnKeysChanged.Broadcast(MappingName);
		}
	}
}

void UModularKeyBindingList::ApplyKeys()
{
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetEnhancedInput())
	{
		EnhancedInput->RequestRebuildControlMappings();
	}
	if (bSaveOnChange)
	{
		SaveKeys();
	}
	Refresh();
}

void UModularKeyBindingList::SaveKeys()
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		Settings->AsyncSaveSettings();
	}
}

#if WITH_EDITOR
void UModularKeyBindingList::RefreshDesignerPreview()
{
	if (!RowPanel)
	{
		return;
	}

	// Built afresh each time, as the designer runs this again whenever the widget changes.
	RowPanel->ClearChildren();
	if (HeaderClass)
	{
		if (UModularKeyBindingHeader* Header = CreateWidget<UModularKeyBindingHeader>(this, HeaderClass))
		{
			Header->SetDesignerFlags(GetDesignerFlags());
			RowPanel->AddChild(Header);
		}
	}
	for (int32 Index = 0; RowClass && Index < NumDesignerPreviewRows; ++Index)
	{
		if (UModularKeyBindingRow* Row = CreateWidget<UModularKeyBindingRow>(this, RowClass))
		{
			Row->SetDesignerFlags(GetDesignerFlags());
			RowPanel->AddChild(Row);
		}
	}
}
#endif
