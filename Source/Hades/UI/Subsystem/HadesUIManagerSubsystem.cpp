#include "HadesUIManagerSubsystem.h"

#include "CommonLocalPlayer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameUIPolicy.h"
#include "PrimaryGameLayout.h"

void UHadesUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 0.0f);
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
