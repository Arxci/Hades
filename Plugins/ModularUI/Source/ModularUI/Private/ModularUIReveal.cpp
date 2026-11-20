#include "ModularUIReveal.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

#include "ModularUI.h"

void FModularUIReveal::Init(UUserWidget& Owner, TConstArrayView<FModularUIRevealStep> InSteps)
{
	Reset();
	for (const FModularUIRevealStep& Step : InSteps)
	{
		UWidget* Target = &Owner;
		if (!Step.Widget.IsNone())
		{
			Target = Owner.WidgetTree ? Owner.WidgetTree->FindWidget(Step.Widget) : nullptr;
		}

		if (Target)
		{
			Add(*Target, Step.Transition);
		}
		else
		{
			UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s: no widget named %s to reveal."), *GetPathNameSafe(&Owner), *Step.Widget.ToString());
		}
	}
}

void FModularUIReveal::Add(UWidget& Target, const FModularUITransition& Transition)
{
	// Two parts moving one widget would fight over its opacity and transform.
	const bool bAdded = Parts.ContainsByPredicate([&Target](const FPart& Part) { return Part.Widget.Get() == &Target; });
	if (bAdded)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s is revealed more than once. Only its first step is used."), *GetPathNameSafe(&Target));
		return;
	}

	FPart& Part = Parts.AddDefaulted_GetRef();
	Part.Widget = &Target;
	Part.Transition = Transition;
	Part.Player.Init(&Target, &Target);
}

void FModularUIReveal::Reset()
{
	Parts.Reset();
}

void FModularUIReveal::PlayIn()
{
	for (FPart& Part : Parts)
	{
		Part.Player.PlayIn(Part.Transition);
	}
}

void FModularUIReveal::ReverseIn()
{
	for (FPart& Part : Parts)
	{
		Part.Player.ReverseIn();
	}
}

void FModularUIReveal::PlayOut(const FModularUITransition& Transition)
{
	for (FPart& Part : Parts)
	{
		Part.Player.PlayOut(Transition);
	}
}

void FModularUIReveal::Stop()
{
	for (FPart& Part : Parts)
	{
		Part.Player.Stop();
	}
}

bool FModularUIReveal::IsPlaying() const
{
	return Parts.ContainsByPredicate([](const FPart& Part) { return Part.Player.IsPlaying(); });
}

bool FModularUIReveal::Advance(float DeltaTime)
{
	bool bPlaying = false;
	for (FPart& Part : Parts)
	{
		bPlaying |= Part.Player.Advance(DeltaTime);
	}
	return bPlaying;
}
