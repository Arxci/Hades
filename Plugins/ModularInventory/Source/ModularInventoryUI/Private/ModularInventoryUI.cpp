#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"

class FModularInventoryUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Item menu transitions moved to Modular UI. Menus saved with the old types load with the new ones.
		TArray<FCoreRedirect> Redirects;
		Redirects.Reserve(3);
		Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/ModularInventoryUI.InventoryMenuTransition"), TEXT("/Script/ModularUI.ModularUITransition"));

		FCoreRedirect& Slide = Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Enum, TEXT("/Script/ModularInventoryUI.EInventoryMenuSlide"), TEXT("/Script/ModularUI.EModularUISlide"));
		Slide.ValueChanges.Add(TEXT("FromItem"), TEXT("FromOrigin"));
		Slide.ValueChanges.Add(TEXT("EInventoryMenuSlide::FromItem"), TEXT("EModularUISlide::FromOrigin"));

		FCoreRedirect& ScaleOrigin = Redirects.Emplace_GetRef(ECoreRedirectFlags::Type_Enum, TEXT("/Script/ModularInventoryUI.EInventoryMenuScaleOrigin"), TEXT("/Script/ModularUI.EModularUIScaleOrigin"));
		ScaleOrigin.ValueChanges.Add(TEXT("Item"), TEXT("Origin"));
		ScaleOrigin.ValueChanges.Add(TEXT("EInventoryMenuScaleOrigin::Item"), TEXT("EModularUIScaleOrigin::Origin"));

		FCoreRedirects::AddRedirectList(Redirects, TEXT("ModularInventoryUI"));
	}
};

IMPLEMENT_MODULE(FModularInventoryUIModule, ModularInventoryUI)
