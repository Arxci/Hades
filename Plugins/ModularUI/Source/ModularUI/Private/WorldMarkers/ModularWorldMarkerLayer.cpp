#include "WorldMarkers/ModularWorldMarkerLayer.h"

#include "Engine/AssetManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "Widgets/Layout/SDPIScaler.h"

#include "WorldMarkers/ModularWorldMarker.h"
#include "WorldMarkers/ModularWorldMarkerManagerComponent.h"
#include "WorldMarkers/ModularWorldMarkerWidget.h"

/** Calls OnUpdate every frame until StopUpdates. */
class SModularWorldMarkerCanvas : public SConstraintCanvas
{
public:
	SLATE_BEGIN_ARGS(SModularWorldMarkerCanvas) {}
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
	bool PlaceMarker(const UModularWorldMarker& Marker, UModularWorldMarkerWidget& Widget, SConstraintCanvas::FSlot& Slot,
		const FSceneViewProjectionData& ProjectionData, const FVector2f& CanvasSize)
	{
		const FVector AnchorLocation = Marker.GetAnchorLocation();

		FVector2D Position;
		const bool bInFront = ULocalPlayer::GetPixelPoint(ProjectionData, AnchorLocation, Position, &CanvasSize);

		const FVector2D HalfSize = FVector2D(CanvasSize) * 0.5;
		FVector2D FromCenter = Position - HalfSize;

		switch (Widget.GetEdgeMode())
		{
		case EModularWorldMarkerEdgeMode::None:
			if (!bInFront)
			{
				return false;
			}
			break;

		case EModularWorldMarkerEdgeMode::Fade:
			if (!bInFront)
			{
				return false;
			}
			Widget.UpdateEdgeFade(FromCenter / HalfSize);
			break;

		case EModularWorldMarkerEdgeMode::Clamp:
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

			const FMargin Padding = Widget.GetEdgePadding() * Widget.GetMarkerScale();
			const FVector2D Bounds = (HalfSize - FVector2D(FromCenter.X > 0.0 ? Padding.Right : Padding.Left, FromCenter.Y > 0.0 ? Padding.Bottom : Padding.Top)).ComponentMax(FVector2D(1.0));
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

		const FVector2f SlotPosition(Position + Widget.GetScreenOffset() * Widget.GetMarkerScale());
		const float Distance = static_cast<float>(FVector::Dist(ProjectionData.ViewOrigin, AnchorLocation));

		Slot.SetOffset(FMargin(SlotPosition.X, SlotPosition.Y, 0.0f, 0.0f));
		Slot.SetZOrder(Marker.GetPriority() * PriorityZOrderStep - Distance);
		return true;
	}
}

UModularWorldMarkerLayer::UModularWorldMarkerLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, WidgetPool(*this)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UModularWorldMarkerLayer::RebuildWidget()
{
	FWidgetActiveTimerDelegate OnUpdate;
	if (!IsDesignTime())
	{
		OnUpdate = FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::UpdateMarkers);
	}

	Canvas = SNew(SModularWorldMarkerCanvas).OnUpdate(OnUpdate);
	return Canvas.ToSharedRef();
}

void UModularWorldMarkerLayer::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	for (const TSharedPtr<FStreamableHandle>& Handle : LoadHandles)
	{
		if (Handle.IsValid() && Handle->IsLoadingInProgress())
		{
			Handle->CancelHandle();
		}
	}
	LoadHandles.Reset();

	if (UModularWorldMarkerManagerComponent* MarkerManager = Manager.Get())
	{
		MarkerManager->OnMarkerAdded.RemoveAll(this);
		MarkerManager->OnMarkerRemoved.RemoveAll(this);
	}
	Manager.Reset();

	for (const FModularWorldMarkerLayerEntry& Entry : Entries)
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

EActiveTimerReturnType UModularWorldMarkerLayer::UpdateMarkers(double InCurrentTime, float InDeltaTime)
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
		const FModularWorldMarkerLayerEntry Entry = Entries[Index];
		if (!Entry.Marker->GetAnchor())
		{
			RemoveEntry(Index);
			continue;
		}

		const bool bShowing = Entry.Widget->UpdateShowHide(!Entry.bRemoving && Entry.Marker->IsVisible(), InDeltaTime);
		const bool bPlaced = bShowing && bCanProject && PlaceMarker(*Entry.Marker, *Entry.Widget, *Entry.Slot, ProjectionData, CanvasSize);
		if (Entry.bRemoving && !bPlaced)
		{
			RemoveEntry(Index);
			continue;
		}

		const ESlateVisibility DesiredVisibility = bPlaced ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (Entry.Widget->GetVisibility() != DesiredVisibility)
		{
			Entry.Widget->SetVisibility(DesiredVisibility);
		}
	}

	return EActiveTimerReturnType::Continue;
}

// Manager
void UModularWorldMarkerLayer::BindToManager()
{
	if (Manager.IsValid())
	{
		return;
	}

	UModularWorldMarkerManagerComponent* MarkerManager = UModularWorldMarkerManagerComponent::Get(GetOwningPlayer());
	if (!MarkerManager)
	{
		return;
	}

	Manager = MarkerManager;
	MarkerManager->OnMarkerAdded.AddUObject(this, &ThisClass::HandleMarkerAdded);
	MarkerManager->OnMarkerRemoved.AddUObject(this, &ThisClass::HandleMarkerRemoved);

	// A copy: widget events can run Blueprint that adds markers to the manager.
	const TArray<TObjectPtr<UModularWorldMarker>> Markers = MarkerManager->GetMarkers();
	for (UModularWorldMarker* Marker : Markers)
	{
		HandleMarkerAdded(Marker);
	}
}

void UModularWorldMarkerLayer::HandleMarkerAdded(UModularWorldMarker* Marker)
{
	if (FModularWorldMarkerLayerEntry* Entry = FindEntry(Marker))
	{
		// Added back while fading out.
		Entry->bRemoving = false;
		return;
	}

	const TSoftClassPtr<UModularWorldMarkerWidget>& WidgetClass = Marker->GetWidgetClass();
	if (WidgetClass.IsNull())
	{
		return;
	}

	if (UClass* LoadedClass = WidgetClass.Get())
	{
		AddEntry(Marker, LoadedClass);
		return;
	}

	const TWeakObjectPtr<UModularWorldMarker> WeakMarker = Marker;
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(WidgetClass.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, WeakMarker]()
	{
		// The marker may have been removed, or added again, while its widget class loaded.
		UModularWorldMarker* LoadedMarker = WeakMarker.Get();
		if (LoadedMarker && Canvas && Manager.IsValid() && Manager->GetMarkers().Contains(LoadedMarker) && !FindEntry(LoadedMarker))
		{
			AddEntry(LoadedMarker, LoadedMarker->GetWidgetClass().Get());
		}
	}));

	LoadHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Loading) { return !Loading.IsValid() || !Loading->IsLoadingInProgress(); });
	if (Handle.IsValid())
	{
		LoadHandles.Add(Handle);
	}
}

void UModularWorldMarkerLayer::HandleMarkerRemoved(UModularWorldMarker* Marker)
{
	if (FModularWorldMarkerLayerEntry* Entry = FindEntry(Marker))
	{
		Entry->bRemoving = true;
	}
}

// Entries
FModularWorldMarkerLayerEntry* UModularWorldMarkerLayer::FindEntry(const UModularWorldMarker* Marker)
{
	return Entries.FindByPredicate([Marker](const FModularWorldMarkerLayerEntry& Entry) { return Entry.Marker == Marker; });
}

void UModularWorldMarkerLayer::AddEntry(UModularWorldMarker* Marker, TSubclassOf<UModularWorldMarkerWidget> WidgetClass)
{
	UModularWorldMarkerWidget* Widget = WidgetClass ? WidgetPool.GetOrCreateInstance(WidgetClass) : nullptr;
	if (!Widget)
	{
		return;
	}

	FModularWorldMarkerLayerEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Marker = Marker;
	Entry.Widget = Widget;

	Canvas->AddSlot()
		.Expose(Entry.Slot)
		.Anchors(FAnchors(0.0f))
		.Alignment(Widget->GetAnchorAlignment())
		.AutoSize(true)
		[
			SNew(SDPIScaler)
			.DPIScale(Widget->GetMarkerScale())
			[
				Widget->TakeWidget()
			]
		];

	Widget->SetVisibility(ESlateVisibility::Collapsed);
	Marker->Widget = Widget;
	Widget->SetMarker(Marker);
}

void UModularWorldMarkerLayer::RemoveEntry(int32 Index)
{
	FModularWorldMarkerLayerEntry& Entry = Entries[Index];

	Entry.Marker->Widget = nullptr;
	if (Canvas)
	{
		Canvas->RemoveSlot(Entry.Slot->GetWidget());
	}
	WidgetPool.Release(Entry.Widget.Get());

	Entries.RemoveAtSwap(Index);
}
