#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "ModularUISubsystem.generated.h"

class APlayerController;
class UCommonActivatableWidget;
class UModularRootLayout;
struct FStreamableHandle;


/** Finds a layer Modular UI doesn't know of, such as one of another plugin's root layout. See Modular UI Subsystem's On Find Layer. */
DECLARE_DELEGATE_RetVal_TwoParams(UCommonActivatableWidgetContainerBase*, FModularUIFindLayerDelegate, const ULocalPlayer* /*LocalPlayer*/, FGameplayTag /*LayerTag*/);


/**
 * A local player's UI: the layers widgets are pushed onto, each a Common UI stack or queue known by a gameplay tag, such as UI.Layer.Game
 * for the HUD and UI.Layer.Modal for popups. Anything can push a widget onto a layer by its tag, without knowing where the layer is.
 * A Modular Root Layout registers the stacks its Layers name as it's made, and any other stack or queue can be registered with Register
 * Layer, such as one inside a screen. With a Root Layout Class in Project Settings > Plugins > Modular UI, the subsystem also makes the
 * player's root layout, holding the layers, and keeps it on their screen. On Find Layer reaches layers of other UI frameworks, such as
 * Common Game's Primary Game Layout.
 */
UCLASS()
class MODULARUI_API UModularUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UModularUISubsystem* Get(const ULocalPlayer* LocalPlayer);

	/** The subsystem of the local player the player controller belongs to. */
	static UModularUISubsystem* GetForPlayer(const APlayerController* PlayerController);

	/**
	 * Asked for a layer no root layout or Register Layer gave, such as to find layers in another plugin's root layout. Bind it once,
	 * such as as the game instance starts, and unbind it as it ends.
	 */
	static FModularUIFindLayerDelegate& OnFindLayer();

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	/** Widgets pushed onto Layer Tag go onto Layer from now on, until it's unregistered or destroyed. A later one with the same tag replaces it. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Layers")
	void RegisterLayer(UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* Layer);

	/** Forgets Layer for its tag, if it's still the one registered for it. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Layers")
	void UnregisterLayer(UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* Layer);

	/** The layer with the tag: one registered, or else one On Find Layer finds. Null when there's none. */
	UFUNCTION(BlueprintPure, Category = "Modular UI|Layers")
	UCommonActivatableWidgetContainerBase* GetLayer(UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag) const;

	/** Pushes a widget of the class onto the layer, and returns it. Null when there's no such layer. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Layers", meta = (DisplayName = "Push Widget To Layer", DeterminesOutputType = "WidgetClass"))
	UCommonActivatableWidget* K2_PushWidgetToLayer(UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag, UPARAM(meta = (AllowAbstract = false)) TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/** Pushes a widget onto the layer, after Init Func sets it up, before it shows. Null when there's no such layer. */
	template <typename WidgetT = UCommonActivatableWidget>
	WidgetT* PushWidgetToLayer(FGameplayTag LayerTag, UClass* WidgetClass, TFunctionRef<void(WidgetT&)> InitFunc)
	{
		static_assert(TIsDerivedFrom<WidgetT, UCommonActivatableWidget>::IsDerived, "Only Common Activatable Widgets can be pushed onto layers.");
		UCommonActivatableWidgetContainerBase* Layer = FindLayerOrWarn(LayerTag, WidgetClass);
		return Layer && WidgetClass ? Layer->AddWidget<WidgetT>(WidgetClass, InitFunc) : nullptr;
	}

	/**
	 * Loads the class in the background, then pushes a widget of it onto the layer, after Init Func sets it up, before it shows, such as the
	 * page a screen opens on. On Pushed is called with it then, or with null when it couldn't be pushed. With Suspend Input, the player's
	 * input waits meanwhile, so nothing is pressed twice while it loads.
	 */
	void PushStreamedWidgetToLayer(FGameplayTag LayerTag, TSoftClassPtr<UCommonActivatableWidget> WidgetClass, bool bSuspendInput = true,
		TFunction<void(UCommonActivatableWidget&)>&& InitFunc = TFunction<void(UCommonActivatableWidget&)>(),
		TFunction<void(UCommonActivatableWidget*)>&& OnPushed = TFunction<void(UCommonActivatableWidget*)>());

	/** Loads the class in the background, then pushes a widget of it onto the layer. The player's input waits meanwhile. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Layers", meta = (DisplayName = "Push Streamed Widget To Layer"))
	void K2_PushStreamedWidgetToLayer(UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag, TSoftClassPtr<UCommonActivatableWidget> WidgetClass);

	/** Removes the widget from whichever layer has it, at once. Prefer its own Close Widget, so its close plays. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Layers")
	void RemoveWidgetFromLayers(UCommonActivatableWidget* Widget);

	/** The root layout the subsystem made from Project Settings' Root Layout Class, or null. */
	UFUNCTION(BlueprintPure, Category = "Modular UI|Layers")
	UModularRootLayout* GetRootLayout() const { return RootLayout; }

	/** Stops all the player's input until Resume Input with the token it returns, such as while a screen loads. */
	FName SuspendInput(FName Reason);
	void ResumeInput(FName Token);

private:
	UCommonActivatableWidgetContainerBase* FindLayerOrWarn(FGameplayTag LayerTag, const UClass* WidgetClass) const;

	/**
	 * Makes the root layout once the player has a player controller, adds it to their screen, and keeps it there, as travel clears the
	 * screen, and hides it with the HUD. From the core ticker, as it waits on things that happen at no set time.
	 */
	bool Tick(float DeltaTime);
	void RefreshRootLayout();

	TMap<FGameplayTag, TWeakObjectPtr<UCommonActivatableWidgetContainerBase>> Layers;

	UPROPERTY(Transient)
	TObjectPtr<UModularRootLayout> RootLayout;

	/** Widget classes loading to be pushed, and the input suspension each holds. */
	struct FPendingPush
	{
		TSharedPtr<FStreamableHandle> Handle;
		FName SuspendToken;
	};
	TArray<FPendingPush> PendingPushes;

	FTSTicker::FDelegateHandle TickHandle;
	bool bWarnedNoRootLayoutClass = false;

	/**
	 * Set as the subsystem deinitializes. From then on it leaves Common Input alone: the player's subsystems go in no set order, and
	 * Common Input's input filters can't be changed once it has gone. They go with it, so the input a load held back needs no giving back.
	 */
	bool bDeinitialized = false;
};
