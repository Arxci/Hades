#include "InventoryEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FInventoryEditorStyle::StyleInstance = nullptr;

void FInventoryEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FInventoryEditorStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

const ISlateStyle& FInventoryEditorStyle::Get()
{
	check(StyleInstance.IsValid());
	return *StyleInstance;
}

FName FInventoryEditorStyle::GetStyleSetName()
{
	static const FName StyleSetName(TEXT("ModularInventoryEditorStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FInventoryEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ModularInventory"));
	if (Plugin.IsValid())
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	const FVector2D Icon16x16(16.0, 16.0);
	const FVector2D Icon20x20(20.0, 20.0);
	const FVector2D Icon32x32(32.0, 32.0);

	auto Svg = [&Style](const TCHAR* RelativePath, const FVector2D& Size)
	{
		return new FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), Size);
	};

	// The plugin: the toolbar's menu button, the documentation tab, and the documentation window's header.
	Style->Set("ModularInventory.Toolbar", Svg(TEXT("Icons/ModularInventory_20"), Icon20x20));
	Style->Set("ModularInventory.Icon", Svg(TEXT("Icons/ModularInventory_16"), Icon16x16));
	Style->Set("ModularInventory.Logo", Svg(TEXT("Icons/ModularInventory_20"), Icon32x32));

	// The tools in the toolbar menu.
	Style->Set("ModularInventory.Documentation", Svg(TEXT("Icons/Documentation_16"), Icon16x16));
	Style->Set("ModularInventory.GenerateIcons", Svg(TEXT("Icons/GenerateIcons_16"), Icon16x16));
	Style->Set("ModularInventory.MessageLog", Svg(TEXT("Icons/MessageLog_16"), Icon16x16));
	Style->Set("ModularInventory.Settings", Svg(TEXT("Icons/Settings_16"), Icon16x16));

	return Style;
}
