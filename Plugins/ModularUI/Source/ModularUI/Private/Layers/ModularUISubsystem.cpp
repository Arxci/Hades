#include "Layers/ModularUISubsystem.h"

#include "CommonActivatableWidget.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

#include "Layers/ModularRootLayout.h"
#include "ModularUI.h"
#include "ModularUISettings.h"

UModularUISubsystem* UModularUISubsystem::Get(const ULocalPlayer* LocalPlayer)
{
	return LocalPlayer ? LocalPlayer->GetSubsystem<UModularUISubsystem>() : nullptr;
}

UModularUISubsystem* UModularUISubsystem::GetForPlayer(const APlayerController* PlayerController)
{
	return PlayerController ? Get(PlayerController->GetLocalPlayer()) : nullptr;
}

FModularUIFindLayerDelegate& UModularUISubsystem::OnFindLayer()
{
	static FModularUIFindLayerDelegate Delegate;
	return Delegate;
}

void UModularUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Only a project that has Modular UI make its root layouts needs to watch for the player's controller.
	if (!GetDefault<UModularUISettings>()->RootLayoutClass.IsNull())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
	}
}

void UModularUISubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle = FTSTicker::FDelegateHandle();

	// Loads still going are dropped, and the input they held back comes back.
	TArray<FPendingPush> Pending = MoveTemp(PendingPushes);
	PendingPushes.Reset();
	for (FPendingPush& Push : Pending)
	{
		if (Push.Handle.IsValid() && Push.Handle->IsLoadingInProgress())
		{
			Push.Handle->CancelHandle();
		}
		ResumeInput(Push.SuspendToken);
	}

	if (RootLayout)
	{
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
	}
	Layers.Reset();

	Super::Deinitialize();
}

bool UModularUISubsystem::Tick(float DeltaTime)
{
	RefreshRootLayout();
	return true;
}

void UModularUISubsystem::RefreshRootLayout()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
	if (!Controller)
	{
		return;
	}

	// A new controller, such as after travel, gets a root layout of its own: the old one went with its world's screen.
	if (RootLayout && RootLayout->GetOwningPlayer() != Controller)
	{
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
	}

	const UModularUISettings* Settings = GetDefault<UModularUISettings>();
	if (!RootLayout)
	{
		const TSubclassOf<UModularRootLayout> LayoutClass = Settings->RootLayoutClass.LoadSynchronous();
		if (!LayoutClass)
		{
			UE_CLOG(!bWarnedNoRootLayoutClass, LogModularUI, Warning, TEXT("[Modular UI] Root Layout Class %s in Project Settings > Plugins > Modular UI can't be loaded."),
				*Settings->RootLayoutClass.ToString());
			bWarnedNoRootLayoutClass = true;
			return;
		}
		RootLayout = CreateWidget<UModularRootLayout>(Controller, LayoutClass);
	}
	if (RootLayout && !RootLayout->IsInViewport())
	{
		RootLayout->AddToPlayerScreen(Settings->RootLayoutZOrder);
	}

	// Hidden along with the HUD, such as for a cinematic or with the ShowHUD command.
	if (RootLayout && Settings->bHideRootLayoutWithHUD)
	{
		const AHUD* HUD = Controller->GetHUD();
		const ESlateVisibility Wanted = HUD && !HUD->bShowHUD ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;
		if (RootLayout->GetVisibility() != Wanted)
		{
			RootLayout->SetVisibility(Wanted);
		}
	}
}

void UModularUISubsystem::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* Layer)
{
	if (LayerTag.IsValid() && Layer)
	{
		Layers.Add(LayerTag, Layer);
	}
}

void UModularUISubsystem::UnregisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* Layer)
{
	const TWeakObjectPtr<UCommonActivatableWidgetContainerBase>* Registered = Layers.Find(LayerTag);
	if (Registered && Registered->Get() == Layer)
	{
		Layers.Remove(LayerTag);
	}
}

UCommonActivatableWidgetContainerBase* UModularUISubsystem::GetLayer(FGameplayTag LayerTag) const
{
	if (const TWeakObjectPtr<UCommonActivatableWidgetContainerBase>* Registered = Layers.Find(LayerTag))
	{
		if (UCommonActivatableWidgetContainerBase* Layer = Registered->Get())
		{
			return Layer;
		}
	}

	const FModularUIFindLayerDelegate& FindLayer = OnFindLayer();
	return LayerTag.IsValid() && FindLayer.IsBound() ? FindLayer.Execute(GetLocalPlayer(), LayerTag) : nullptr;
}

UCommonActivatableWidgetContainerBase* UModularUISubsystem::FindLayerOrWarn(FGameplayTag LayerTag, const UClass* WidgetClass) const
{
	UCommonActivatableWidgetContainerBase* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] No layer %s to push %s onto. Name it in your root layout's Layers, or register it with Register Layer."),
			*LayerTag.ToString(), *GetNameSafe(WidgetClass));
	}
	return Layer;
}

UCommonActivatableWidget* UModularUISubsystem::K2_PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	return PushWidgetToLayer<UCommonActivatableWidget>(LayerTag, WidgetClass.Get(), [](UCommonActivatableWidget&) {});
}

void UModularUISubsystem::K2_PushStreamedWidgetToLayer(FGameplayTag LayerTag, TSoftClassPtr<UCommonActivatableWidget> WidgetClass)
{
	PushStreamedWidgetToLayer(LayerTag, WidgetClass, /*bSuspendInput*/ true);
}

void UModularUISubsystem::PushStreamedWidgetToLayer(FGameplayTag LayerTag, TSoftClassPtr<UCommonActivatableWidget> WidgetClass, bool bSuspendInput,
	TFunction<void(UCommonActivatableWidget*)>&& OnPushed)
{
	if (WidgetClass.IsNull())
	{
		if (OnPushed)
		{
			OnPushed(nullptr);
		}
		return;
	}

	// Already loaded, it goes up at once.
	if (UClass* Loaded = WidgetClass.Get())
	{
		UCommonActivatableWidget* Pushed = PushWidgetToLayer<UCommonActivatableWidget>(LayerTag, Loaded, [](UCommonActivatableWidget&) {});
		if (OnPushed)
		{
			OnPushed(Pushed);
		}
		return;
	}

	static const FName SuspendReason(TEXT("ModularUIPushingWidget"));
	const FName SuspendToken = bSuspendInput ? SuspendInput(SuspendReason) : NAME_None;
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(WidgetClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, LayerTag, WidgetClass, SuspendToken, OnPushed]()
		{
			ResumeInput(SuspendToken);
			UCommonActivatableWidget* Pushed = PushWidgetToLayer<UCommonActivatableWidget>(LayerTag, WidgetClass.Get(), [](UCommonActivatableWidget&) {});
			if (OnPushed)
			{
				OnPushed(Pushed);
			}
		}));

	if (Handle.IsValid())
	{
		// A load cut short still gives the player their input back.
		Handle->BindCancelDelegate(FStreamableDelegate::CreateWeakLambda(this, [this, SuspendToken]()
		{
			ResumeInput(SuspendToken);
		}));

		PendingPushes.RemoveAll([](const FPendingPush& Push) { return !Push.Handle.IsValid() || !Push.Handle->IsLoadingInProgress(); });
		FPendingPush& Push = PendingPushes.AddDefaulted_GetRef();
		Push.Handle = Handle;
		Push.SuspendToken = SuspendToken;
	}
}

void UModularUISubsystem::RemoveWidgetFromLayers(UCommonActivatableWidget* Widget)
{
	if (!Widget)
	{
		return;
	}
	for (const TPair<FGameplayTag, TWeakObjectPtr<UCommonActivatableWidgetContainerBase>>& Layer : Layers)
	{
		if (UCommonActivatableWidgetContainerBase* Container = Layer.Value.Get())
		{
			Container->RemoveWidget(*Widget);
		}
	}
}

FName UModularUISubsystem::SuspendInput(FName Reason)
{
	UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetLocalPlayer());
	if (!Input)
	{
		return NAME_None;
	}

	// Each suspension has a token of its own, so several can wait at once and each ends only its own.
	static int32 Suspensions = 0;
	FName Token = Reason;
	Token.SetNumber(++Suspensions);
	Input->SetInputTypeFilter(ECommonInputType::MouseAndKeyboard, Token, true);
	Input->SetInputTypeFilter(ECommonInputType::Gamepad, Token, true);
	Input->SetInputTypeFilter(ECommonInputType::Touch, Token, true);
	return Token;
}

void UModularUISubsystem::ResumeInput(FName Token)
{
	UCommonInputSubsystem* Input = Token.IsNone() ? nullptr : UCommonInputSubsystem::Get(GetLocalPlayer());
	if (Input)
	{
		Input->SetInputTypeFilter(ECommonInputType::MouseAndKeyboard, Token, false);
		Input->SetInputTypeFilter(ECommonInputType::Gamepad, Token, false);
		Input->SetInputTypeFilter(ECommonInputType::Touch, Token, false);
	}
}
