#include "HadesItemBox.h"

#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Interaction/HadesInteractableComponent.h"

#define LOCTEXT_NAMESPACE "HadesItemBox"

AHadesItemBox::AHadesItemBox()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Interactable = CreateDefaultSubobject<UHadesInteractableComponent>(TEXT("Interactable"));
	Interactable->SetInteractionText(LOCTEXT("OpenPrompt", "Open item box"));
}

void AHadesItemBox::BeginPlay()
{
	Super::BeginPlay();

	Interactable->OnInteractionCompleted.AddDynamic(this, &ThisClass::HandleInteractionCompleted);
}

void AHadesItemBox::HandleInteractionCompleted(AActor* Interactor)
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* Player = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	if (Player && !ScreenClass.IsNull())
	{
		UCommonUIExtensions::PushStreamedContentToLayer_ForPlayer(Player->GetLocalPlayer(), HadesGameplayTags::UI_Layer_GameMenu, ScreenClass);
	}
}

#undef LOCTEXT_NAMESPACE
