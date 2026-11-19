#include "InventoryMenuTransition.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Border.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

BEGIN_DEFINE_SPEC(FInventoryMenuTransitionSpec, "ModularInventory.UI.MenuTransition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UBorder> Root;
	TStrongObjectPtr<UBorder> Panel;
	FInventoryMenuTransitionPlayer Player;

	/** Five frames at 60 fps, fading in as it slides 20 pixels out from the item at an even pace. */
	static FInventoryMenuTransition MakeSlide()
	{
		FInventoryMenuTransition Transition;
		Transition.Duration = 5.0f / 60.0f;
		Transition.Easing = EEasingFunc::Linear;
		Transition.bFade = true;
		Transition.FadeFrom = 0.0f;
		Transition.bSlide = true;
		Transition.SlideDistance = 20.0f;
		Transition.SlideFrom = EInventoryMenuSlide::FromItem;
		Transition.bScale = false;
		return Transition;
	}

	/** One frame at 60 fps. True while the transition still plays. */
	bool Step()
	{
		return Player.Advance(1.0f / 60.0f);
	}

	float GetOffsetX() const { return static_cast<float>(Panel->GetRenderTransform().Translation.X); }
	float GetOffsetY() const { return static_cast<float>(Panel->GetRenderTransform().Translation.Y); }
	float GetScaleX() const { return static_cast<float>(Panel->GetRenderTransform().Scale.X); }
	float GetPivotX() const { return static_cast<float>(Panel->GetRenderTransformPivot().X); }
	float GetOpacity() const { return Root->GetRenderOpacity(); }
END_DEFINE_SPEC(FInventoryMenuTransitionSpec)

void FInventoryMenuTransitionSpec::Define()
{
	BeforeEach([this]()
	{
		Root.Reset(NewObject<UBorder>(GetTransientPackage()));
		Panel.Reset(NewObject<UBorder>(GetTransientPackage()));
		Player = FInventoryMenuTransitionPlayer();
		Player.Init(Root.Get(), Panel.Get());

		// Opened on the item's right, level with its top.
		Player.SetOrigin(FVector2D(1.0, 0.0), FVector2D(0.0, 0.0));
	});

	AfterEach([this]()
	{
		Root.Reset();
		Panel.Reset();
	});

	Describe("Coming out of the item", [this]()
	{
		It("starts toward the item, unseen, and slides out at an even pace as it fades in", [this]()
		{
			Player.PlayIn(MakeSlide());
			TestEqual(TEXT("Starts toward the item"), GetOffsetX(), -20.0f, 0.01f);
			TestEqual(TEXT("Starts unseen"), GetOpacity(), 0.0f, 0.01f);

			const float Offsets[] = { -16.0f, -12.0f, -8.0f, -4.0f, 0.0f };
			for (int32 Frame = 0; Frame < 5; ++Frame)
			{
				const bool bPlaying = Step();
				TestEqual(*FString::Printf(TEXT("Offset on frame %d"), Frame + 1), GetOffsetX(), Offsets[Frame], 0.01f);
				TestEqual(*FString::Printf(TEXT("Opacity on frame %d"), Frame + 1), GetOpacity(), (Frame + 1) / 5.0f, 0.01f);
				TestTrue(*FString::Printf(TEXT("Plays until the last frame, after frame %d"), Frame + 1), bPlaying == (Frame < 4));
			}
			TestEqual(TEXT("Only moves sideways"), GetOffsetY(), 0.0f, 0.01f);
		});

		It("closes by going back the way it came", [this]()
		{
			Player.PlayIn(MakeSlide());
			while (Step())
			{
			}

			Player.ReverseIn();
			TestTrue(TEXT("Closing plays"), Player.IsPlaying());
			const float Offsets[] = { -4.0f, -8.0f, -12.0f, -16.0f, -20.0f };
			for (int32 Frame = 0; Frame < 5; ++Frame)
			{
				Step();
				TestEqual(*FString::Printf(TEXT("Offset on frame %d"), Frame + 1), GetOffsetX(), Offsets[Frame], 0.01f);
			}
			TestFalse(TEXT("Closing has ended"), Player.IsPlaying());
			TestEqual(TEXT("Ends unseen"), GetOpacity(), 0.0f, 0.01f);
		});

		It("goes back from wherever it got to when it closes while opening", [this]()
		{
			Player.PlayIn(MakeSlide());
			Step();
			Step();
			Player.ReverseIn();

			Step();
			TestEqual(TEXT("One frame back"), GetOffsetX(), -16.0f, 0.01f);
			TestFalse(TEXT("Back where it started a frame later"), Step());
			TestEqual(TEXT("Hidden again"), GetOffsetX(), -20.0f, 0.01f);
		});

		It("drops down from a cursor it opened below", [this]()
		{
			Player.SetOrigin(FVector2D(0.0, 1.0), FVector2D(0.0, 0.0));
			Player.PlayIn(MakeSlide());
			TestEqual(TEXT("Starts above, toward the cursor"), GetOffsetY(), -20.0f, 0.01f);
			TestEqual(TEXT("Doesn't start to one side"), GetOffsetX(), 0.0f, 0.01f);
		});
	});

	Describe("Close Transition", [this]()
	{
		It("goes from however the menu looks, keeps what it doesn't change, and is quicker the less shown the menu was", [this]()
		{
			Player.PlayIn(MakeSlide());
			Step();
			Step();
			Step();

			FInventoryMenuTransition FadeOut;
			FadeOut.Duration = 0.1f;
			FadeOut.Easing = EEasingFunc::Linear;
			FadeOut.bSlide = false;
			Player.PlayOut(FadeOut);

			int32 Frames = 1;
			while (Step())
			{
				++Frames;
				TestEqual(TEXT("The slide holds where it was"), GetOffsetX(), -8.0f, 0.01f);
			}
			TestEqual(TEXT("Frames to close, from 60% shown"), Frames, 4);
			TestEqual(TEXT("Ends unseen"), GetOpacity(), 0.0f, 0.01f);
		});
	});

	Describe("Without effect", [this]()
	{
		It("shows and hides at once", [this]()
		{
			FInventoryMenuTransition Instant;
			Instant.Duration = 0.0f;
			Player.PlayIn(Instant);
			TestFalse(TEXT("Nothing plays as it opens"), Player.IsPlaying());
			TestEqual(TEXT("Shown"), GetOpacity(), 1.0f, 0.01f);
			TestEqual(TEXT("In place"), GetOffsetX(), 0.0f, 0.01f);

			Player.ReverseIn();
			TestFalse(TEXT("Nothing plays as it closes"), Player.IsPlaying());
		});
	});

	Describe("Designer values", [this]()
	{
		It("adds to the opacity, offset and pivot set in the designer, growing from the corner nearest the item", [this]()
		{
			Root->SetRenderOpacity(0.8f);
			FWidgetTransform Designed;
			Designed.Translation = FVector2D(5.0, 7.0);
			Panel->SetRenderTransform(Designed);
			Panel->SetRenderTransformPivot(FVector2D(0.25, 0.25));
			Player.Init(Root.Get(), Panel.Get());

			// Opened on the item's left, in the lower half of the screen: its bottom right corner is nearest the item.
			Player.SetOrigin(FVector2D(-1.0, 0.0), FVector2D(1.0, 1.0));

			FInventoryMenuTransition Pop;
			Pop.Duration = 0.1f;
			Pop.bSlide = false;
			Pop.bScale = true;
			Pop.ScaleFrom = FVector2D(0.5, 0.5);
			Pop.ScaleOrigin = EInventoryMenuScaleOrigin::Item;
			Player.PlayIn(Pop);
			TestEqual(TEXT("Grows from the corner nearest the item"), GetPivotX(), 1.0f, 0.01f);
			TestEqual(TEXT("Starts at half size"), GetScaleX(), 0.5f, 0.01f);

			while (Step())
			{
			}
			TestEqual(TEXT("The designer's opacity"), GetOpacity(), 0.8f, 0.01f);
			TestEqual(TEXT("The designer's offset"), GetOffsetX(), 5.0f, 0.01f);
			TestEqual(TEXT("Full size"), GetScaleX(), 1.0f, 0.01f);
			TestEqual(TEXT("The designer's pivot once full size"), GetPivotX(), 0.25f, 0.01f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
