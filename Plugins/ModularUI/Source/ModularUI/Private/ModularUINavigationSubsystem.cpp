#include "ModularUINavigationSubsystem.h"

#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

#include "ModularUISettings.h"

namespace
{
	/** Unreal's navigation, less what Project Settings turned off. */
	class FModularUINavigationConfig : public FNavigationConfig
	{
	public:
		explicit FModularUINavigationConfig(const UModularUISettings& Settings)
			: bSpaceBarAccepts(Settings.bSpaceBarAccepts)
		{
			bTabNavigation = Settings.bTabNavigation;
			if (!Settings.bArrowKeyNavigation)
			{
				for (const FKey& ArrowKey : { EKeys::Left, EKeys::Right, EKeys::Up, EKeys::Down })
				{
					KeyEventRules.Remove(ArrowKey);
				}
			}
		}

		virtual EUINavigationAction GetNavigationActionFromKey(const FKeyEvent& InKeyEvent) const override
		{
			// Space presses the focused button like Enter, which beats any UI action bound to it.
			if (!bSpaceBarAccepts && InKeyEvent.GetKey() == EKeys::SpaceBar)
			{
				return EUINavigationAction::Invalid;
			}
			return FNavigationConfig::GetNavigationActionFromKey(InKeyEvent);
		}

	private:
		bool bSpaceBarAccepts = true;
	};
}

void UModularUINavigationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UModularUISettings* Settings = GetDefault<UModularUISettings>();
	if (Settings->ChangesNavigation() && FSlateApplication::IsInitialized())
	{
		PreviousNavigationConfig = FSlateApplication::Get().GetNavigationConfig();
		FSlateApplication::Get().SetNavigationConfig(MakeShared<FModularUINavigationConfig>(*Settings));
	}
}

void UModularUINavigationSubsystem::Deinitialize()
{
	if (FSlateApplication::IsInitialized() && PreviousNavigationConfig)
	{
		FSlateApplication::Get().SetNavigationConfig(PreviousNavigationConfig.ToSharedRef());
	}
	PreviousNavigationConfig.Reset();

	Super::Deinitialize();
}
