#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"

namespace
{
	/** An enum that moved to Modular UI with the same values, so each value saved with the old enum's name finds its place in the new one. */
	void AddMovedEnum(TArray<FCoreRedirect>& Redirects, const TCHAR* OldName, const TCHAR* NewName, std::initializer_list<const TCHAR*> Values)
	{
		FCoreRedirect& Redirect = Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Enum, FString::Printf(TEXT("/Script/ModularInventoryUI.%s"), OldName),
			FString::Printf(TEXT("/Script/ModularUI.%s"), NewName));
		for (const TCHAR* Value : Values)
		{
			Redirect.ValueChanges.Add(FString::Printf(TEXT("%s::%s"), OldName, Value), FString::Printf(TEXT("%s::%s"), NewName, Value));
		}
	}
}

class FModularInventoryUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		TArray<FCoreRedirect> Redirects;

		// Item menu transitions moved to Modular UI. Menus saved with the old types load with the new ones.
		Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/ModularInventoryUI.InventoryMenuTransition"), TEXT("/Script/ModularUI.ModularUITransition"));

		FCoreRedirect& Slide = Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Enum, TEXT("/Script/ModularInventoryUI.EInventoryMenuSlide"), TEXT("/Script/ModularUI.EModularUISlide"));
		Slide.ValueChanges.Add(TEXT("FromItem"), TEXT("FromOrigin"));
		Slide.ValueChanges.Add(TEXT("EInventoryMenuSlide::FromItem"), TEXT("EModularUISlide::FromOrigin"));

		FCoreRedirect& ScaleOrigin = Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Enum, TEXT("/Script/ModularInventoryUI.EInventoryMenuScaleOrigin"), TEXT("/Script/ModularUI.EModularUIScaleOrigin"));
		ScaleOrigin.ValueChanges.Add(TEXT("Item"), TEXT("Origin"));
		ScaleOrigin.ValueChanges.Add(TEXT("EInventoryMenuScaleOrigin::Item"), TEXT("EModularUIScaleOrigin::Origin"));

		// Item menus became Modular UI context menus: placement and outside clicks are the context menu's now. Right Press Outside, Close On
		// Right Press and Game Keeps Input are loaded as deprecated properties, under their own names, and carried over in PostLoad.
		AddMovedEnum(Redirects, TEXT("EInventoryMenuPlacement"), TEXT("EModularMenuPlacement"), { TEXT("TowardCenter"), TEXT("Right"), TEXT("Left"), TEXT("AsDesigned"), TEXT("AtCursor") });
		AddMovedEnum(Redirects, TEXT("EInventoryMenuOutsidePress"), TEXT("EModularMenuOutsideClick"), { TEXT("Close"), TEXT("PassThrough"), TEXT("Block") });
		Redirects.Emplace(ECoreRedirectFlags::Type_Property, TEXT("/Script/ModularInventoryUI.InventoryItemMenuWidget.LeftPressOutside"), TEXT("OutsideClick"));

		FCoreRedirects::AddRedirectList(Redirects, TEXT("ModularInventoryUI"));
	}
};

IMPLEMENT_MODULE(FModularInventoryUIModule, ModularInventoryUI)
