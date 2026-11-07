#include "InventoryEditorSettings.h"

#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"

UInventoryEditorSettings::UInventoryEditorSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("ModularInventory");

	ItemPrefix = TEXT("DA_");
	CosmeticsPrefix = TEXT("DA_");
	CosmeticsSuffix = TEXT("_Cosmetics");
	IconPrefix = TEXT("T_");
	IconSuffix = TEXT("_Icon");
	SourcePrefixes = { TEXT("SM_"), TEXT("SK_"), TEXT("T_"), TEXT("TX_"), TEXT("UI_") };

	// A three-quarter view of a mesh modelled facing X: its front turned to the right, its top tipped toward the camera.
	DefaultRotation = FRotator(0.0, 120.0, 20.0);

	auto AddLight = [this](const FRotator& Direction, float Intensity, const FLinearColor& Color)
	{
		FInventoryIconLight& Light = Lights.AddDefaulted_GetRef();
		Light.Direction = Direction;
		Light.Intensity = Intensity;
		Light.Color = Color;
	};
	// Warm key from the upper left, cool fill from the right, rim from behind.
	AddLight(FRotator(-40.0, 35.0, 0.0), 2.5f, FLinearColor(1.0f, 0.95f, 0.9f));
	AddLight(FRotator(-10.0, -50.0, 0.0), 0.8f, FLinearColor(0.85f, 0.9f, 1.0f));
	AddLight(FRotator(-25.0, 160.0, 0.0), 1.5f, FLinearColor::White);
}

void UInventoryEditorSettings::Open()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		const UInventoryEditorSettings* Settings = GetDefault<UInventoryEditorSettings>();
		SettingsModule->ShowViewer(Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
	}
}

FString UInventoryEditorSettings::GetBaseName(const FString& AssetName) const
{
	FString BaseName = AssetName;

	TArray<FString> Prefixes = { ItemPrefix, CosmeticsPrefix, IconPrefix };
	Prefixes.Append(SourcePrefixes);
	for (const FString& Prefix : Prefixes)
	{
		if (!Prefix.IsEmpty() && BaseName.RemoveFromStart(Prefix))
		{
			break;
		}
	}

	for (const FString& Suffix : { CosmeticsSuffix, IconSuffix })
	{
		if (!Suffix.IsEmpty() && BaseName.RemoveFromEnd(Suffix))
		{
			break;
		}
	}

	return BaseName.IsEmpty() ? AssetName : BaseName;
}
