#include "Icons/SInventoryIconStudio.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "InputCoreTypes.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Rendering/DrawElements.h"
#include "TextureResource.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#include "Data/InventoryItemDefinition.h"
#include "Icons/InventoryIconGenerator.h"
#include "Icons/InventoryIconRenderer.h"
#include "InventoryEditorSettings.h"
#include "ModularInventoryEditor.h"

#define LOCTEXT_NAMESPACE "InventoryIconStudio"

namespace
{
	DECLARE_DELEGATE_TwoParams(FOnIconDragged, FVector2D /*Delta*/, bool /*bMove*/);
	DECLARE_DELEGATE_OneParam(FOnIconZoomed, float /*WheelDelta*/);

	/** Draws the icon as large as it fits. Drags are reported as fractions of the icon's size, so they feel the same at any zoom. */
	class SIconPreview : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SIconPreview) {}
			SLATE_ARGUMENT(const FSlateBrush*, Brush)
			SLATE_EVENT(FOnIconDragged, OnDragged)
			SLATE_EVENT(FOnIconZoomed, OnZoomed)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Brush = InArgs._Brush;
			OnDragged = InArgs._OnDragged;
			OnZoomed = InArgs._OnZoomed;
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			return FVector2D(256.0, 256.0);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			static const FSlateColorBrush Backdrop(FLinearColor::White);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &Backdrop, ESlateDrawEffect::None, FLinearColor(0.015f, 0.015f, 0.015f));

			FVector2D Position;
			FVector2D Size;
			if (!GetIconRect(AllottedGeometry, Position, Size))
			{
				return LayerId;
			}

			// A mid grey behind the icon itself shows both dark and light edges.
			const FPaintGeometry IconGeometry = AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, IconGeometry, &Backdrop, ESlateDrawEffect::None, FLinearColor(0.1f, 0.1f, 0.1f));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, IconGeometry, Brush);
			return LayerId + 2;
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			const FKey Button = MouseEvent.GetEffectingButton();
			if (Button == EKeys::LeftMouseButton || Button == EKeys::RightMouseButton)
			{
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
			return FReply::Unhandled();
		}

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			return HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
		}

		virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			FVector2D Position;
			FVector2D Size;
			if (!HasMouseCapture() || !GetIconRect(MyGeometry, Position, Size))
			{
				return FReply::Unhandled();
			}

			const FVector2D Delta = FVector2D(MouseEvent.GetCursorDelta()) / MyGeometry.Scale / Size;
			OnDragged.ExecuteIfBound(Delta, MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton));
			return FReply::Handled();
		}

		virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			OnZoomed.ExecuteIfBound(MouseEvent.GetWheelDelta());
			return FReply::Handled();
		}

		virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
		{
			return FCursorReply::Cursor(HasMouseCapture() ? EMouseCursor::GrabHandClosed : EMouseCursor::GrabHand);
		}

	private:
		/** Centered, as large as fits. False until there's an icon to show. */
		bool GetIconRect(const FGeometry& Geometry, FVector2D& OutPosition, FVector2D& OutSize) const
		{
			if (!Brush || !Brush->GetResourceObject())
			{
				return false;
			}

			const FVector2D ImageSize(Brush->GetImageSize());
			const FVector2D Area(Geometry.GetLocalSize());
			if (ImageSize.X <= 0.0 || ImageSize.Y <= 0.0)
			{
				return false;
			}

			OutSize = ImageSize * FMath::Min(Area.X / ImageSize.X, Area.Y / ImageSize.Y);
			OutPosition = (Area - OutSize) * 0.5;
			return true;
		}

		const FSlateBrush* Brush = nullptr;
		FOnIconDragged OnDragged;
		FOnIconZoomed OnZoomed;
	};
}

void SInventoryIconStudio::Open(UInventoryItemDefinition& Item)
{
	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::Format(LOCTEXT("Title", "Icon Studio: {0}"), FText::FromString(Item.GetName())))
		.ClientSize(FVector2D(480.0, 720.0))
		[
			SNew(SInventoryIconStudio, &Item)
		];
	FSlateApplication::Get().AddWindow(Window);
}

void SInventoryIconStudio::Construct(const FArguments& InArgs, UInventoryItemDefinition* InItem)
{
	Item = InItem;
	Renderer = MakeUnique<FInventoryIconRenderer>();

	Recipe.Reset(NewObject<UInventoryIconRecipe>(GetTransientPackage(), NAME_None, RF_Transient));
	Recipe->Framing = InItem ? InventoryIcons::GetFraming(*InItem) : FInventoryIconFraming();

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	const TSharedRef<IDetailsView> FramingDetails = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsArgs);
	FramingDetails->SetObject(Recipe.Get());

	SettingsChangedHandle = GetMutableDefault<UInventoryEditorSettings>()->OnSettingChanged().AddSP(this, &SInventoryIconStudio::HandleSettingsChanged);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f)
		[
			SNew(SIconPreview)
			.Brush(&PreviewBrush)
			.OnDragged(this, &SInventoryIconStudio::HandleDragged)
			.OnZoomed(this, &SInventoryIconStudio::HandleZoomed)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Controls", "Drag to turn the mesh, right-drag to move it, scroll to zoom."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			FramingDetails
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SInventoryIconStudio::GetStatusText)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Reset", "Reset"))
				.ToolTipText(LOCTEXT("ResetTooltip", "Goes back to the default framing from the project settings."))
				.OnClicked(this, &SInventoryIconStudio::HandleResetClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("QuarterTurn", "Quarter Turn"))
				.ToolTipText(LOCTEXT("QuarterTurnTooltip", "Turns the mesh a quarter in the picture, to stand a long item up or lay it down."))
				.OnClicked(this, &SInventoryIconStudio::HandleQuarterTurnClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Lighting", "Lighting..."))
				.ToolTipText(LOCTEXT("LightingTooltip", "Opens the icon settings, shared by every icon in the project. The preview updates as you change them."))
				.OnClicked_Lambda([]()
				{
					UInventoryEditorSettings::Open();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Save", "Save Icon"))
				.IsEnabled(this, &SInventoryIconStudio::CanSave)
				.OnClicked(this, &SInventoryIconStudio::HandleSaveClicked)
			]
		]
	];
}

SInventoryIconStudio::~SInventoryIconStudio()
{
	if (UObjectInitialized())
	{
		GetMutableDefault<UInventoryEditorSettings>()->OnSettingChanged().Remove(SettingsChangedHandle);
	}
}

void SInventoryIconStudio::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const UInventoryItemDefinition* Definition = Item.Get();
	UStaticMesh* Mesh = Definition ? InventoryIcons::LoadWorldMesh(*Definition) : nullptr;
	const FIntPoint Size = Definition ? InventoryIcons::GetIconSize(*Definition) : FIntPoint::ZeroValue;
	if (bSettingsChanged || Mesh != PreviewMesh.Get() || Size != PreviewSize || !(Recipe->Framing == PreviewFraming))
	{
		RenderPreview(Mesh, Size);
	}
}

void SInventoryIconStudio::RenderPreview(UStaticMesh* Mesh, FIntPoint Size)
{
	PreviewMesh = Mesh;
	PreviewSize = Size;
	PreviewFraming = Recipe->Framing;
	bSettingsChanged = false;

	if (Mesh && Renderer->GetMesh() != Mesh)
	{
		Renderer->SetMesh(Mesh);
	}

	const TArray<FColor> Pixels = Mesh ? Renderer->Render(PreviewFraming, Size) : TArray<FColor>();
	if (Pixels.IsEmpty())
	{
		PreviewBrush.SetResourceObject(nullptr);
		return;
	}

	if (!PreviewTexture || PreviewTexture->GetSizeX() != Size.X || PreviewTexture->GetSizeY() != Size.Y)
	{
		PreviewTexture.Reset(UTexture2D::CreateTransient(Size.X, Size.Y, PF_B8G8R8A8));
		PreviewBrush.ImageSize = FVector2D(Size);
	}

	FTexture2DMipMap& Mip = PreviewTexture->GetPlatformData()->Mips[0];
	FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	PreviewTexture->UpdateResource();
	PreviewBrush.SetResourceObject(PreviewTexture.Get());
}

FText SInventoryIconStudio::GetStatusText() const
{
	const UInventoryItemDefinition* Definition = Item.Get();
	if (!Definition)
	{
		return LOCTEXT("NoItem", "The item no longer exists.");
	}
	if (!InventoryIcons::LoadWorldMesh(*Definition))
	{
		return LOCTEXT("NoMesh", "The item has no World Mesh to render. Set one on its cosmetics, and the preview appears.");
	}

	const FIntPoint Size = InventoryIcons::GetIconSize(*Definition);
	UTexture2D* Icon = InventoryIcons::LoadIcon(*Definition);
	if (!Icon)
	{
		return FText::Format(LOCTEXT("NewIcon", "Saving creates a {0} x {1} icon texture."), Size.X, Size.Y);
	}
	if (!InventoryIcons::FindRecipe(Icon))
	{
		return FText::Format(LOCTEXT("HandMadeIcon", "{0} is hand-made. Saving creates a new {1} x {2} texture for the item and leaves {0} as it is."),
			FText::FromString(Icon->GetName()), Size.X, Size.Y);
	}
	return FText::Format(LOCTEXT("UpdateIcon", "Saving updates {0} ({1} x {2})."), FText::FromString(Icon->GetName()), Size.X, Size.Y);
}

bool SInventoryIconStudio::CanSave() const
{
	const UInventoryItemDefinition* Definition = Item.Get();
	return Definition && InventoryIcons::LoadWorldMesh(*Definition);
}

void SInventoryIconStudio::HandleDragged(FVector2D Delta, bool bMove)
{
	FInventoryIconFraming& Framing = Recipe->Framing;
	if (bMove)
	{
		Framing.Offset += Delta;
		return;
	}

	// The mesh follows the mouse: a drag across the whole icon turns it half way round.
	const FQuat Yaw(FVector::UpVector, -Delta.X * UE_PI);
	const FQuat Pitch(FVector::RightVector, -Delta.Y * UE_PI);
	Framing.Rotation = (Yaw * Pitch * FQuat(Framing.Rotation)).Rotator();
}

void SInventoryIconStudio::HandleZoomed(float WheelDelta)
{
	FInventoryIconFraming& Framing = Recipe->Framing;
	Framing.Zoom = FMath::Clamp(Framing.Zoom * FMath::Pow(1.1f, WheelDelta), 0.1f, 10.0f);
}

void SInventoryIconStudio::HandleSettingsChanged(UObject* Settings, FPropertyChangedEvent& Event)
{
	bSettingsChanged = true;
}

FReply SInventoryIconStudio::HandleResetClicked()
{
	const UInventoryItemDefinition* Definition = Item.Get();
	const UStaticMesh* Mesh = Definition ? InventoryIcons::LoadWorldMesh(*Definition) : nullptr;
	Recipe->Framing = Mesh ? InventoryIcons::MakeDefaultFraming(*Mesh, Definition->Size.ComponentMax(FIntPoint(1, 1))) : FInventoryIconFraming();
	return FReply::Handled();
}

FReply SInventoryIconStudio::HandleQuarterTurnClicked()
{
	FInventoryIconFraming& Framing = Recipe->Framing;
	Framing.Rotation = (FQuat(FVector::ForwardVector, UE_HALF_PI) * FQuat(Framing.Rotation)).Rotator();
	return FReply::Handled();
}

FReply SInventoryIconStudio::HandleSaveClicked()
{
	UInventoryItemDefinition* Definition = Item.Get();
	if (!Definition)
	{
		return FReply::Handled();
	}

	FText Error;
	if (InventoryIcons::SaveIcon(*Definition, Recipe->Framing, *Renderer, Error))
	{
		const UTexture2D* Icon = InventoryIcons::LoadIcon(*Definition);
		InventoryEditor::Notify(FText::Format(LOCTEXT("Saved", "Saved {0}. Save the changed assets to keep it."), FText::FromString(GetNameSafe(Icon))));
	}
	else
	{
		InventoryEditor::Notify(FText::Format(LOCTEXT("SaveFailed", "{0} {1}"), FText::FromString(Definition->GetName()), Error));
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
