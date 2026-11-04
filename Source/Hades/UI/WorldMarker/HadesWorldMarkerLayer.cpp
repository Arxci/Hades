#include "HadesWorldMarkerLayer.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"

#include "Hades/UI/WorldMarker/HadesWorldMarker.h"
#include "Hades/UI/WorldMarker/HadesWorldMarkerManagerComponent.h"
#include "Hades/UI/WorldMarker/HadesWorldMarkerWidget.h"

/** Calls OnUpdate every frame until StopUpdates. Like Lyra's SActorCanvas, the timer is registered from inside the Slate widget. */
class SHadesWorldMarkerCanvas : public SConstraintCanvas
{
public:
	SLATE_BEGIN_ARGS(SHadesWorldMarkerCanvas) {}
		SLATE_EVENT(FWidgetActiveTimerDelegate, OnUpdate)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SConstraintCanvas::Construct(SConstraintCanvas::FArguments());

		if (InArgs._OnUpdate.IsBound())
		{
			UpdateTimer = RegisterActiveTimer(0.0f, InArgs._OnUpdate);
		}
	}

	void StopUpdates()
	{
		if (UpdateTimer)
		{
			UnRegisterActiveTimer(UpdateTimer.ToSharedRef());
			UpdateTimer.Reset();
		}
	}

private:
	TSharedPtr<FActiveTimerHandle> UpdateTimer;
};

namespace
{
	/** Each priority draws above lower priorities up to this many centimeters away. */
	constexpr float PriorityZOrderStep = 1000000.0f;

	/** Positions the marker for its widget's edge mode. Returns false when it shouldn't be drawn this frame. */
	bool PlaceMarker(const UHadesWorldMarker& Marker, UHadesWorldMarkerWidget& Widget, SConstraintCanvas::FSlot& Slot,
		const FSceneViewProjectionData& ProjectionData, const FVector2f& CanvasSize)
	{
		const FVector AnchorLocation = Marker.GetAnchorLocation();

		// As in Lyra, relies on locations behind the camera still landing on the side of the screen they are on.
		FVector2D Position;
		const bool bInFront = ULocalPlayer::GetPixelPoint(ProjectionData, AnchorLocation, Position, &CanvasSize);

		const FVector2D HalfSize = FVector2D(CanvasSize) * 0.5;
		FVector2D FromCenter = Position - HalfSize;

		switch (Widget.GetEdgeMode())
		{
		case EHadesWorldMarkerEdgeMode::None:
			if (!bInFront)
			{
				return false;
			}
			break;

		case EHadesWorldMarkerEdgeMode::Fade:
			if (!bInFront)
			{
				return false;
			}
			Widget.UpdateEdgeFade(FromCenter / HalfSize);
			break;

		case EHadesWorldMarkerEdgeMode::Clamp:
		{
			if (!bInFront)
			{
				// Behind the camera only the direction is meaningful, so push it off screen to clamp it to the edge.
				FVector2D Direction = FromCenter.GetSafeNormal();
				if (Direction.IsZero())
				{
					// Directly behind: bottom edge.
					Direction = FVector2D(0.0, 1.0);
				}
				FromCenter = Direction * FVector2D(CanvasSize).Size();
			}

			const FVector2D Padding = Widget.GetDesiredSize() * 0.5 + FVector2D(Widget.GetEdgePadding());
			const FVector2D Bounds = (HalfSize - Padding).ComponentMax(FVector2D(1.0));
			const double Overshoot = FMath::Max(FMath::Abs(FromCenter.X) / Bounds.X, FMath::Abs(FromCenter.Y) / Bounds.Y);
			const bool bClamped = Overshoot > 1.0;

			Widget.SetClampedToEdge(bClamped, static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(FromCenter.Y, FromCenter.X))));

			if (bClamped)
			{
				// Pulled back toward the center until inside the padded screen.
				FromCenter /= Overshoot;
			}
			Position = HalfSize + FromCenter;
			break;
		}
		}

		const FVector2f SlotPosition(Position);
		const float Distance = static_cast<float>(FVector::Dist(ProjectionData.ViewOrigin, AnchorLocation));

		Slot.SetOffset(FMargin(SlotPosition.X, SlotPosition.Y, 0.0f, 0.0f));
		Slot.SetZOrder(Marker.GetPriority() * PriorityZOrderStep - Distance);
		return true;
	}
}

UHadesWorldMarkerLayer::UHadesWorldMarkerLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, WidgetPool(*this)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UHadesWorldMarkerLayer::RebuildWidget()
{
	FWidgetActiveTimerDelegate OnUpdate;
	if (!IsDesignTime())
	{
		OnUpdate = FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::UpdateMarkers);
	}

	Canvas = SNew(SHadesWorldMarkerCanvas).OnUpdate(OnUpdate);
	return Canvas.ToSharedRef();
}

void UHadesWorldMarkerLayer::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	CancelAsyncLoading();

	if (UHadesWorldMarkerManagerComponent* MarkerManager = Manager.Get())
	{
		MarkerManager->OnMarkerAdded.RemoveAll(this);
		MarkerManager->OnMarkerRemoved.RemoveAll(this);
	}
	Manager.Reset();

	for (const FHadesWorldMarkerLayerEntry& Entry : Entries)
	{
		Entry.Marker->Widget = nullptr;
	}
	Entries.Reset();
	WidgetPool.ResetPool();

	if (Canvas)
	{
		Canvas->StopUpdates();
	}
	Canvas.Reset();
}

EActiveTimerReturnType UHadesWorldMarkerLayer::UpdateMarkers(double InCurrentTime, float InDeltaTime)
{
	BindToManager();

	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const FVector2f CanvasSize(Canvas->GetTickSpaceGeometry().GetLocalSize());

	FSceneViewProjectionData ProjectionData;
	const bool bCanProject = LocalPlayer && LocalPlayer->ViewportClient && CanvasSize.X > 0.0f && CanvasSize.Y > 0.0f
		&& LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData);

	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		// A copy: widget events can run Blueprint that adds markers and reallocates Entries.
		const FHadesWorldMarkerLayerEntry Entry = Entries[Index];
		if (!Entry.Marker->GetAnchor())
		{
			RemoveEntry(Index);
			continue;
		}

		const bool bShowing = Entry.Widget->UpdateVisibilityFade(!Entry.bRemoving && Entry.Marker->IsVisible(), InDeltaTime);
		if (!bShowing && Entry.bRemoving)
		{
			RemoveEntry(Index);
			continue;
		}

		const bool bPlaced = bShowing && bCanProject && PlaceMarker(*Entry.Marker, *Entry.Widget, *Entry.Slot, ProjectionData, CanvasSize);
		const ESlateVisibility DesiredVisibility = bPlaced ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (Entry.Widget->GetVisibility() != DesiredVisibility)
		{
			Entry.Widget->SetVisibility(DesiredVisibility);
		}
	}

	return EActiveTimerReturnType::Continue;
}

//Manager
void UHadesWorldMarkerLayer::BindToManager()
{
	if (Manager.IsValid())
	{
		return;
	}

	UHadesWorldMarkerManagerComponent* MarkerManager = UHadesWorldMarkerManagerComponent::Get(GetOwningPlayer());
	if (!MarkerManager)
	{
		return;
	}

	Manager = MarkerManager;
	MarkerManager->OnMarkerAdded.AddUObject(this, &ThisClass::HandleMarkerAdded);
	MarkerManager->OnMarkerRemoved.AddUObject(this, &ThisClass::HandleMarkerRemoved);

	// A copy: widget events can run Blueprint that adds markers to the manager.
	const TArray<TObjectPtr<UHadesWorldMarker>> Markers = MarkerManager->GetMarkers();
	for (UHadesWorldMarker* Marker : Markers)
	{
		HandleMarkerAdded(Marker);
	}
}

void UHadesWorldMarkerLayer::HandleMarkerAdded(UHadesWorldMarker* Marker)
{
	if (FHadesWorldMarkerLayerEntry* Entry = FindEntry(Marker))
	{
		// Added back while fading out.
		Entry->bRemoving = false;
		return;
	}

	const TSoftClassPtr<UHadesWorldMarkerWidget>& WidgetClass = Marker->GetWidgetClass();
	if (WidgetClass.IsNull())
	{
		return;
	}

	if (UClass* LoadedClass = WidgetClass.Get())
	{
		AddEntry(Marker, LoadedClass);
		return;
	}

	TWeakObjectPtr<UHadesWorldMarker> WeakMarker = Marker;
	AsyncLoad(WidgetClass, [this, WeakMarker]()
	{
		// The marker may have been removed, or added again, while its widget class loaded.
		UHadesWorldMarker* LoadedMarker = WeakMarker.Get();
		if (LoadedMarker && Canvas && Manager.IsValid() && Manager->GetMarkers().Contains(LoadedMarker) && !FindEntry(LoadedMarker))
		{
			AddEntry(LoadedMarker, LoadedMarker->GetWidgetClass().Get());
		}
	});
	StartAsyncLoading();
}

void UHadesWorldMarkerLayer::HandleMarkerRemoved(UHadesWorldMarker* Marker)
{
	if (FHadesWorldMarkerLayerEntry* Entry = FindEntry(Marker))
	{
		Entry->bRemoving = true;
	}
}

//Entries
FHadesWorldMarkerLayerEntry* UHadesWorldMarkerLayer::FindEntry(const UHadesWorldMarker* Marker)
{
	return Entries.FindByPredicate([Marker](const FHadesWorldMarkerLayerEntry& Entry) { return Entry.Marker == Marker; });
}

void UHadesWorldMarkerLayer::AddEntry(UHadesWorldMarker* Marker, TSubclassOf<UHadesWorldMarkerWidget> WidgetClass)
{
	UHadesWorldMarkerWidget* Widget = WidgetClass ? WidgetPool.GetOrCreateInstance(WidgetClass) : nullptr;
	if (!Widget)
	{
		return;
	}

	FHadesWorldMarkerLayerEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Marker = Marker;
	Entry.Widget = Widget;

	Canvas->AddSlot()
		.Expose(Entry.Slot)
		.Anchors(FAnchors(0.0f))
		.Alignment(FVector2D(0.5, 0.5))
		.AutoSize(true)
		[
			Widget->TakeWidget()
		];

	Widget->SetVisibility(ESlateVisibility::Collapsed);
	Marker->Widget = Widget;
	Widget->SetMarker(Marker);
}

void UHadesWorldMarkerLayer::RemoveEntry(int32 Index)
{
	FHadesWorldMarkerLayerEntry& Entry = Entries[Index];

	Entry.Marker->Widget = nullptr;
	if (Canvas)
	{
		Canvas->RemoveSlot(Entry.Widget->TakeWidget());
	}
	WidgetPool.Release(Entry.Widget.Get());

	Entries.RemoveAtSwap(Index);
}
