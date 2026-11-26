#include "Layers/ModularRootLayout.h"

#include "Blueprint/WidgetTree.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "Layers/ModularUISubsystem.h"
#include "ModularUI.h"

void UModularRootLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	for (const TPair<FGameplayTag, FName>& Layer : Layers)
	{
		UCommonActivatableWidgetContainerBase* Container = WidgetTree ? Cast<UCommonActivatableWidgetContainerBase>(WidgetTree->FindWidget(Layer.Value)) : nullptr;
		if (Container)
		{
			RegisterLayer(Layer.Key, Container);
		}
		else
		{
			UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s: no Common Activatable Widget Stack or Queue named %s for the layer %s."),
				*GetPathNameSafe(this), *Layer.Value.ToString(), *Layer.Key.ToString());
		}
	}
}

void UModularRootLayout::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* Layer)
{
	if (UModularUISubsystem* UI = UModularUISubsystem::Get(GetOwningLocalPlayer()))
	{
		UI->RegisterLayer(LayerTag, Layer);
	}
}
