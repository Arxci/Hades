#include "ModularInventory.h"

#include "Modules/ModuleManager.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "Debug/GameplayDebuggerCategory_Inventory.h"
#endif

DEFINE_LOG_CATEGORY(LogInventory);

class FModularInventoryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER
		IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
		GameplayDebugger.RegisterCategory("Inventory",
			IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_Inventory::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
		GameplayDebugger.NotifyCategoriesChanged();
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER
		if (IGameplayDebugger::IsAvailable())
		{
			IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
			GameplayDebugger.UnregisterCategory("Inventory");
			GameplayDebugger.NotifyCategoriesChanged();
		}
#endif
	}
};

IMPLEMENT_MODULE(FModularInventoryModule, ModularInventory)
