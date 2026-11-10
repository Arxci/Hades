//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSModule.h"
#include "EMSTypes.h"
#include "EMSDebug.h"
#include "EMSPluginSettings.h"

void FEasyMultiSaveModule::StartupModule()
{
	UEMSPluginSettings* PluginSettings = GetMutableDefault<UEMSPluginSettings>();
	if (PluginSettings->SlotInfoSaveGameClass.IsNull())
	{
		PluginSettings->SlotInfoSaveGameClass = FSoftClassPath(TEXT("/Script/EasyMultiSave.EMSInfoSaveGame"));
	}
}

void FEasyMultiSaveModule::ShutdownModule()
{
#if EMS_BUILD_DEVELOPER
	FSaveReport::ClearReportCache();
#endif
}
	
IMPLEMENT_MODULE(FEasyMultiSaveModule, EasyMultiSave)