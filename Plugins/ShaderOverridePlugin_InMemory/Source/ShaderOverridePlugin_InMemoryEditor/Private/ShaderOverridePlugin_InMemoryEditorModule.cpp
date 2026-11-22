#include "Modules/ModuleManager.h"
#include "TonemapOverrideSettings.h"

class FShaderOverridePlugin_InMemoryEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Keep the generated shader config in sync with Project Settings, e.g. after pulling a
		// DefaultGame.ini change. Only rewrites and recompiles when the file is out of date.
		if (const UTonemapOverrideSettings* Settings = GetDefault<UTonemapOverrideSettings>())
		{
			Settings->ApplyToShaders(/*bRecompile=*/ true);
		}
	}
};

IMPLEMENT_MODULE(FShaderOverridePlugin_InMemoryEditorModule, ShaderOverridePlugin_InMemoryEditor)
