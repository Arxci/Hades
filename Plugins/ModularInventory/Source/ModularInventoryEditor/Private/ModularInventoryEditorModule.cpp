#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ToolMenus.h"

#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryEditorActions.h"
#include "InventoryIconThumbnailRenderer.h"
#include "InventoryItemDefinitionCustomization.h"

class FModularInventoryEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UThumbnailManager::Get().RegisterCustomRenderer(UInventoryItemDefinition::StaticClass(), UInventoryIconThumbnailRenderer::StaticClass());
		UThumbnailManager::Get().RegisterCustomRenderer(UInventoryItemCosmetics::StaticClass(), UInventoryIconThumbnailRenderer::StaticClass());

		ItemDefinitionClassName = UInventoryItemDefinition::StaticClass()->GetFName();
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditor.RegisterCustomClassLayout(ItemDefinitionClassName, FOnGetDetailCustomizationInstance::CreateStatic(&FInventoryItemDefinitionCustomization::MakeInstance));
		PropertyEditor.NotifyCustomizationModuleChanged();

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FModularInventoryEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);

		if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyEditor->UnregisterCustomClassLayout(ItemDefinitionClassName);
		}

		if (UObjectInitialized())
		{
			UThumbnailManager::Get().UnregisterCustomRenderer(UInventoryItemDefinition::StaticClass());
			UThumbnailManager::Get().UnregisterCustomRenderer(UInventoryItemCosmetics::StaticClass());
		}
	}

private:
	void RegisterMenus()
	{
		// Owning the entries lets ShutdownModule remove them all at once.
		FToolMenuOwnerScoped OwnerScoped(this);
		InventoryEditorActions::RegisterMenus();
	}

	FName ItemDefinitionClassName;
};

IMPLEMENT_MODULE(FModularInventoryEditorModule, ModularInventoryEditor)
