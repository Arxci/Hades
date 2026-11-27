#include "Hades.h"

#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"

DEFINE_LOG_CATEGORY(LogHadesAbility);
DEFINE_LOG_CATEGORY(LogHadesInput);
DEFINE_LOG_CATEGORY(LogHadesInteraction);
DEFINE_LOG_CATEGORY(LogHadesUI);
DEFINE_LOG_CATEGORY(LogHadesInventory);
DEFINE_LOG_CATEGORY(LogHadesFlashlight);
DEFINE_LOG_CATEGORY(LogHadesCamera);

namespace
{
	void AddMovedClass(TArray<FCoreRedirect>& Redirects, const TCHAR* OldName, const TCHAR* NewModule, const TCHAR* NewName)
	{
		Redirects.Emplace(ECoreRedirectFlags::Type_Class, FString::Printf(TEXT("/Script/Hades.%s"), OldName), FString::Printf(TEXT("/Script/%s.%s"), NewModule, NewName));
	}

	/** An enum that moved with the same values, so each value saved with the old enum's name finds its place in the new one. */
	void AddMovedEnum(TArray<FCoreRedirect>& Redirects, const TCHAR* OldName, const TCHAR* NewModule, const TCHAR* NewName, std::initializer_list<const TCHAR*> Values)
	{
		FCoreRedirect& Redirect = Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Enum, FString::Printf(TEXT("/Script/Hades.%s"), OldName),
			FString::Printf(TEXT("/Script/%s.%s"), NewModule, NewName));
		for (const TCHAR* Value : Values)
		{
			Redirect.ValueChanges.Add(FString::Printf(TEXT("%s::%s"), OldName, Value), FString::Printf(TEXT("%s::%s"), NewName, Value));
		}
	}
}

/** The game module. Widgets and types that moved into the Modular UI and Modular Inventory plugins load as their plugin versions. */
class FHadesModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		TArray<FCoreRedirect> Redirects;

		// Foundation widgets, messages and world markers, now Modular UI's.
		AddMovedClass(Redirects, TEXT("HadesActivatableWidget"), TEXT("ModularUI"), TEXT("ModularActivatableWidget"));
		AddMovedClass(Redirects, TEXT("HadesInputKeyWidget"), TEXT("ModularUI"), TEXT("ModularInputKeyWidget"));
		AddMovedClass(Redirects, TEXT("HadesMenuButton"), TEXT("ModularUI"), TEXT("ModularButton"));
		AddMovedClass(Redirects, TEXT("HadesTabButton"), TEXT("ModularUI"), TEXT("ModularTabButton"));
		AddMovedClass(Redirects, TEXT("HadesTabListWidget"), TEXT("ModularUI"), TEXT("ModularTabListWidget"));
		AddMovedClass(Redirects, TEXT("HadesMessageWidget"), TEXT("ModularUI"), TEXT("ModularMessageFeed"));
		AddMovedClass(Redirects, TEXT("HadesUIUtility"), TEXT("ModularUI"), TEXT("ModularUIFunctionLibrary"));
		AddMovedClass(Redirects, TEXT("HadesWorldMarker"), TEXT("ModularUI"), TEXT("ModularWorldMarker"));
		AddMovedClass(Redirects, TEXT("HadesWorldMarkerLayer"), TEXT("ModularUI"), TEXT("ModularWorldMarkerLayer"));
		AddMovedClass(Redirects, TEXT("HadesWorldMarkerManagerComponent"), TEXT("ModularUI"), TEXT("ModularWorldMarkerManagerComponent"));
		AddMovedClass(Redirects, TEXT("HadesWorldMarkerWidget"), TEXT("ModularUI"), TEXT("ModularWorldMarkerWidget"));
		Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/Hades.HadesTabDescriptor"), TEXT("/Script/ModularUI.ModularTabDescriptor"));
		AddMovedEnum(Redirects, TEXT("EHadesWidgetInputMode"), TEXT("ModularUI"), TEXT("EModularUIInputMode"), { TEXT("Default"), TEXT("GameAndMenu"), TEXT("Game"), TEXT("Menu") });
		AddMovedEnum(Redirects, TEXT("EHadesMenuButtonJustify"), TEXT("ModularUI"), TEXT("EModularButtonJustify"), { TEXT("Left"), TEXT("Center"), TEXT("Right") });
		AddMovedEnum(Redirects, TEXT("EHadesWorldMarkerEdgeMode"), TEXT("ModularUI"), TEXT("EModularWorldMarkerEdgeMode"), { TEXT("None"), TEXT("Fade"), TEXT("Clamp") });

		// Item use, dropping, shortcuts and the item actions for them, now Modular Inventory's.
		Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/Hades.HadesItemUse"), TEXT("/Script/ModularInventory.InventoryItemUse"));
		AddMovedClass(Redirects, TEXT("HadesItemAction_Use"), TEXT("ModularInventoryUI"), TEXT("InventoryItemAction_Use"));
		AddMovedClass(Redirects, TEXT("HadesItemAction_Discard"), TEXT("ModularInventoryUI"), TEXT("InventoryItemAction_Drop"));
		AddMovedClass(Redirects, TEXT("HadesItemAction_AssignQuickSlot"), TEXT("ModularInventoryUI"), TEXT("InventoryItemAction_AssignShortcut"));
		AddMovedClass(Redirects, TEXT("HadesQuickSlotMenuEntry"), TEXT("ModularInventoryUI"), TEXT("InventoryShortcutMenuEntry"));
		Redirects.Emplace(ECoreRedirectFlags::Type_Property, TEXT("/Script/Hades.HadesInventoryComponent.DroppedPickupClass"), TEXT("DroppedItemClass"));
		Redirects.Emplace(ECoreRedirectFlags::Type_Property, TEXT("/Script/Hades.HadesInventoryScreen.OrganizeAction"), TEXT("SortAction"));
		Redirects.Emplace(ECoreRedirectFlags::Type_Property, TEXT("/Script/Hades.HadesQuickSlotMenu.EntryClass"), TEXT("SlotEntryClass"));

		FCoreRedirects::AddRedirectList(Redirects, TEXT("Hades"));

		FDefaultGameModuleImpl::StartupModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FHadesModule, Hades, "Hades");
