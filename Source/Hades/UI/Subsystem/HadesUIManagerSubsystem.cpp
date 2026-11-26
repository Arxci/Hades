#include "HadesUIManagerSubsystem.h"

#include "CommonLocalPlayer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameUIPolicy.h"
#include "PrimaryGameLayout.h"

#include "Layers/ModularUISubsystem.h"

namespace
{
	/** The layer of CommonGame's root layout with the tag, for Modular UI, for any game instance's player. */
	UCommonActivatableWidgetContainerBase* FindRootLayoutLayer(const ULocalPlayer* LocalPlayer, FGameplayTag LayerTag)
	{
		const UCommonLocalPlayer* CommonPlayer = Cast<UCommonLocalPlayer>(LocalPlayer);
		const UGameInstance* GameInstance = CommonPlayer ? CommonPlayer->GetGameInstance() : nullptr;
		const UGameUIManagerSubsystem* Manager = GameInstance ? GameInstance->GetSubsystem<UGameUIManagerSubsystem>() : nullptr;
		const UGameUIPolicy* Policy = Manager ? Manager->GetCurrentUIPolicy() : nullptr;
		UPrimaryGameLayout* Layout = Policy ? Policy->GetRootLayout(CommonPlayer) : nullptr;
		return Layout ? Layout->GetLayerWidget(LayerTag) : nullptr;
	}
}

void UHadesUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 0.0f);

	// It finds each player's own root layout, so it's bound once and serves every game instance, such as several in Play In Editor.
	if (!UModularUISubsystem::OnFindLayer().IsBound())
	{
		UModularUISubsystem::OnFindLayer().BindStatic(&FindRootLayoutLayer);
	}
}

void UHadesUIManagerSubsystem::Deinitialize()
{
	Super::Deinitialize();

	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
}

bool UHadesUIManagerSubsystem::Tick(float DeltaTime)
{
	SyncRootLayoutVisibilityToShowHUD();

	return true;
}

void UHadesUIManagerSubsystem::SyncRootLayoutVisibilityToShowHUD()
{
	const UGameUIPolicy* Policy = GetCurrentUIPolicy();
	if (!Policy)
	{
		return;
	}

	for (const ULocalPlayer* LocalPlayer : GetGameInstance()->GetLocalPlayers())
	{
		bool bShouldShowUI = true;

		if (const APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld()))
		{
			const AHUD* HUD = PlayerController->GetHUD();
			if (HUD && !HUD->bShowHUD)
			{
				bShouldShowUI = false;
			}
		}

		if (UPrimaryGameLayout* RootLayout = Policy->GetRootLayout(CastChecked<UCommonLocalPlayer>(LocalPlayer)))
		{
			const ESlateVisibility DesiredVisibility = bShouldShowUI ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
			if (DesiredVisibility != RootLayout->GetVisibility())
			{
				RootLayout->SetVisibility(DesiredVisibility);
			}
		}
	}
}
