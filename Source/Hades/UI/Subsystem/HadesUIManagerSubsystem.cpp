#include "HadesUIManagerSubsystem.h"

#include "CommonLocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameUIPolicy.h"
#include "PrimaryGameLayout.h"

namespace
{
	/** By default Space presses the focused button like Enter, which would beat any UI action bound to it. */
	class FHadesNavigationConfig : public FNavigationConfig
	{
	public:
		virtual EUINavigationAction GetNavigationActionFromKey(const FKeyEvent& InKeyEvent) const override
		{
			return InKeyEvent.GetKey() == EKeys::SpaceBar ? EUINavigationAction::Invalid : FNavigationConfig::GetNavigationActionFromKey(InKeyEvent);
		}
	};
}

void UHadesUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 0.0f);

	if (FSlateApplication::IsInitialized())
	{
		PreviousNavigationConfig = FSlateApplication::Get().GetNavigationConfig();
		FSlateApplication::Get().SetNavigationConfig(MakeShared<FHadesNavigationConfig>());
	}
}

void UHadesUIManagerSubsystem::Deinitialize()
{
	Super::Deinitialize();

	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);

	if (FSlateApplication::IsInitialized() && PreviousNavigationConfig)
	{
		FSlateApplication::Get().SetNavigationConfig(PreviousNavigationConfig.ToSharedRef());
	}
	PreviousNavigationConfig.Reset();
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
