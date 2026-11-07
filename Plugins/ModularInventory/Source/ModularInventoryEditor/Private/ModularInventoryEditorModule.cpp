#include "Framework/Notifications/NotificationManager.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Assets/InventoryIconThumbnailRenderer.h"
#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "Details/InventoryItemDefinitionCustomization.h"
#include "Details/InventoryLootTableCustomization.h"
#include "InventoryEditorMenus.h"
#include "Loot/InventoryLootTable.h"
#include "ModularInventoryEditor.h"

#define LOCTEXT_NAMESPACE "ModularInventoryEditor"

void InventoryEditor::Notify(const FText& Message)
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = 6.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

class FModularInventoryEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UThumbnailManager::Get().RegisterCustomRenderer(UInventoryItemDefinition::StaticClass(), UInventoryIconThumbnailRenderer::StaticClass());
		UThumbnailManager::Get().RegisterCustomRenderer(UInventoryItemCosmetics::StaticClass(), UInventoryIconThumbnailRenderer::StaticClass());

		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		RegisterLayout(PropertyEditor, UInventoryItemDefinition::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FInventoryItemDefinitionCustomization::MakeInstance));
		RegisterLayout(PropertyEditor, UInventoryLootTable::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FInventoryLootTableCustomization::MakeInstance));
		PropertyEditor.NotifyCustomizationModuleChanged();

		FMessageLogModule& MessageLog = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLog.RegisterLogListing(InventoryEditor::MessageLogName, LOCTEXT("MessageLogLabel", "Modular Inventory"));

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FModularInventoryEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);

		if (FMessageLogModule* MessageLog = FModuleManager::GetModulePtr<FMessageLogModule>("MessageLog"))
		{
			MessageLog->UnregisterLogListing(InventoryEditor::MessageLogName);
		}

		if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			for (const FName& ClassName : CustomizedClasses)
			{
				PropertyEditor->UnregisterCustomClassLayout(ClassName);
			}
		}

		if (UObjectInitialized())
		{
			UThumbnailManager::Get().UnregisterCustomRenderer(UInventoryItemDefinition::StaticClass());
			UThumbnailManager::Get().UnregisterCustomRenderer(UInventoryItemCosmetics::StaticClass());
		}
	}

private:
	void RegisterLayout(FPropertyEditorModule& PropertyEditor, const UClass* Class, const FOnGetDetailCustomizationInstance& MakeInstance)
	{
		CustomizedClasses.Add(Class->GetFName());
		PropertyEditor.RegisterCustomClassLayout(Class->GetFName(), MakeInstance);
	}

	void RegisterMenus()
	{
		// Owning the entries lets ShutdownModule remove them all at once.
		FToolMenuOwnerScoped OwnerScoped(this);
		InventoryEditorMenus::Register();
	}

	TArray<FName> CustomizedClasses;
};

IMPLEMENT_MODULE(FModularInventoryEditorModule, ModularInventoryEditor)

#undef LOCTEXT_NAMESPACE
