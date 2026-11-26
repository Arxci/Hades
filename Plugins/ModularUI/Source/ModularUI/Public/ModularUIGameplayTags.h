#pragma once

#include "NativeGameplayTags.h"

/**
 * Tags Modular UI suggests, so a project works without defining its own. Layers and message channels are only tags, so any other tags
 * work as well: these are the names Lyra and Common Game use for the usual layers, and the usual kinds of message.
 */
namespace ModularUIGameplayTags
{
	// Layers, bottom to top.
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Game);
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_GameMenu);
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Menu);
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Modal);

	// Message channels.
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Message);
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Message_Toast);
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Message_Banner);
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Message_Heading);
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Message_Hint);
	MODULARUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Message_Prompt);
}
