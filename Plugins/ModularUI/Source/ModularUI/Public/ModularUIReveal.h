#pragma once

#include "CoreMinimal.h"

#include "ModularUITransition.h"

#include "ModularUIReveal.generated.h"

class UUserWidget;
class UWidget;


/** One part of a reveal: a widget, and how it comes in. */
USTRUCT(BlueprintType)
struct MODULARUI_API FModularUIRevealStep
{
	GENERATED_BODY()

	/** The widget's name in the designer, such as ItemGrid. Everything inside it comes in with it. None is the whole widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reveal")
	FName Widget;

	/** How it comes in. Give the parts different Delays so they arrive one after another. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reveal")
	FModularUITransition Transition;
};


/**
 * Brings a widget's parts in one after another, such as a panel, then its title, then its text, each with its own transition and delay,
 * and sends them back out together. Like Modular UI Transition Player, it only moves on as Advance is called, and adds to the render opacity
 * and transform the parts have when they're added, so leave those to it.
 */
struct MODULARUI_API FModularUIReveal
{
	/** Forgets every part, and adds a part for each step's widget in Owner. Steps naming a widget Owner doesn't have are left out, with a warning. */
	void Init(UUserWidget& Owner, TConstArrayView<FModularUIRevealStep> InSteps);

	/** Adds a part: Target comes in with Transition. A widget that's already a part stays as it was added. */
	void Add(UWidget& Target, const FModularUITransition& Transition);

	void Reset();

	/** Every part comes in, each once its delay has passed. */
	void PlayIn();

	/** Every part goes back the way it came, all at once, from wherever it got to. */
	void ReverseIn();

	/** Every part goes out with Transition, all at once, from however it looks now. */
	void PlayOut(const FModularUITransition& Transition);

	/** Every part stops where it is. */
	void Stop();

	bool IsPlaying() const;

	/** Moves every part on by Delta Time, in seconds. True while any is still playing. */
	bool Advance(float DeltaTime);

private:
	struct FPart
	{
		TWeakObjectPtr<UWidget> Widget;
		FModularUITransition Transition;
		FModularUITransitionPlayer Player;
	};

	TArray<FPart> Parts;
};
