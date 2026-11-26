#include "ModularUIGameplayTags.h"

namespace ModularUIGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Game, "UI.Layer.Game", "The layer under the rest: the HUD and what the player plays through.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_GameMenu, "UI.Layer.GameMenu", "Menus that belong to play, such as an inventory or a map.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Menu, "UI.Layer.Menu", "Menus over the game, such as the pause menu or settings.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Modal, "UI.Layer.Modal", "The top layer: popups that must be answered, such as context menus and dialogs.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Message, "UI.Message", "Message channels. A message feed shows the channels it lists, and their children.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Message_Toast, "UI.Message.Toast", "Small notes that stack up and go by themselves, such as an item picked up.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Message_Banner, "UI.Message.Banner", "A strip across the screen, one at a time, such as a tutorial tip or a key hint.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Message_Heading, "UI.Message.Heading", "A large title, one at a time, such as a new objective or an area's name.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Message_Hint, "UI.Message.Hint", "A short line of feedback, such as \"It's locked.\"");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Message_Prompt, "UI.Message.Prompt", "What the player is looking at or can do now, shown until it's hidden, such as an item they can pick up.");
}
