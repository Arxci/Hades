#include "ModularUIFunctionLibrary.h"

#include "CommonActivatableWidget.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "Layers/ModularUISubsystem.h"
#include "Messages/ModularMessageSubsystem.h"

// Layers
UCommonActivatableWidget* UModularUIFunctionLibrary::PushWidgetToLayer(APlayerController* PlayerController, FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UModularUISubsystem* UI = UModularUISubsystem::GetForPlayer(PlayerController);
	return UI ? UI->K2_PushWidgetToLayer(LayerTag, WidgetClass) : nullptr;
}

void UModularUIFunctionLibrary::PushStreamedWidgetToLayer(APlayerController* PlayerController, FGameplayTag LayerTag, TSoftClassPtr<UCommonActivatableWidget> WidgetClass)
{
	if (UModularUISubsystem* UI = UModularUISubsystem::GetForPlayer(PlayerController))
	{
		UI->PushStreamedWidgetToLayer(LayerTag, WidgetClass, /*bSuspendInput*/ true);
	}
}

// Messages
void UModularUIFunctionLibrary::ShowMessage(APlayerController* PlayerController, const FModularMessage& Message)
{
	if (UModularMessageSubsystem* Messages = UModularMessageSubsystem::GetForPlayer(PlayerController))
	{
		Messages->ShowMessage(Message);
	}
}

void UModularUIFunctionLibrary::ShowMessageText(APlayerController* PlayerController, FGameplayTag Channel, const FText& Text, float Duration)
{
	if (UModularMessageSubsystem* Messages = UModularMessageSubsystem::GetForPlayer(PlayerController))
	{
		Messages->ShowText(Channel, Text, Duration);
	}
}

void UModularUIFunctionLibrary::HideMessage(APlayerController* PlayerController, FGameplayTag Channel, FName Key)
{
	if (UModularMessageSubsystem* Messages = UModularMessageSubsystem::GetForPlayer(PlayerController))
	{
		Messages->HideMessage(Channel, Key);
	}
}

// Widgets
void UModularUIFunctionLibrary::SetSizeBoxSize(USizeBox* Target, const FVector2D NewSize)
{
	if (IsValid(Target))
	{
		Target->SetWidthOverride(NewSize.X);
		Target->SetHeightOverride(NewSize.Y);
	}
}

void UModularUIFunctionLibrary::SetImageBrushSize(UImage* Target, const FVector2D NewSize)
{
	if (IsValid(Target))
	{
		FSlateBrush NewBrush = Target->GetBrush();
		NewBrush.ImageSize = FVector2D(NewSize.X, NewSize.Y);
		Target->SetBrush(NewBrush);
	}
}

void UModularUIFunctionLibrary::InterpolateWidgetRenderOpacity(UWidget* Target, const float TargetOpacity, const float InterpSpeed)
{
	const UWorld* World = IsValid(Target) ? Target->GetWorld() : nullptr;
	if (World)
	{
		const float NewOpacity = FMath::FInterpTo(Target->GetRenderOpacity(), TargetOpacity, World->GetDeltaSeconds(), InterpSpeed);
		Target->SetRenderOpacity(NewOpacity);
	}
}
