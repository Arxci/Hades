#include "ModularUIHoldTimer.h"

void FModularUIHoldTimer::Press(const FKey& Key, double Time)
{
	if (Key.IsValid() && !PressedAt.Contains(Key))
	{
		PressedAt.Add(Key, Time);
	}
}

void FModularUIHoldTimer::Release(const FKey& Key, double Time)
{
	PressedAt.Remove(Key);
	if (bHolding && !bLetGo && Keys.Contains(Key) && !IsAnyHeldKeyDown())
	{
		bLetGo = true;
		LetGoTime = Time;
	}
}

void FModularUIHoldTimer::ReleaseAll(double Time)
{
	TArray<FKey> Down;
	PressedAt.GetKeys(Down);
	for (const FKey& Key : Down)
	{
		Release(Key, Time);
	}
}

bool FModularUIHoldTimer::Start(const TArray<FKey>& InKeys, float InHoldTime)
{
	TOptional<double> PressTime;
	for (const FKey& Key : InKeys)
	{
		if (const double* Found = PressedAt.Find(Key))
		{
			PressTime = PressTime.IsSet() ? FMath::Min(PressTime.GetValue(), *Found) : *Found;
		}
	}

	// Nothing holds it without one of its keys down, such as for a click on its action bar button. A press holds once.
	const bool bPressHolding = PressTime.IsSet() && bHolding && StartTime == PressTime.GetValue();
	const bool bPressHeld = PressTime.IsSet() && LastHeldPress.IsSet() && LastHeldPress.GetValue() == PressTime.GetValue();
	if (!PressTime.IsSet() || bPressHolding || bPressHeld)
	{
		return false;
	}

	Cancel();
	bHolding = true;
	bLetGo = false;
	Keys = InKeys;
	HoldTime = FMath::Max(InHoldTime, 0.0f);
	StartTime = PressTime.GetValue();
	return true;
}

void FModularUIHoldTimer::Cancel()
{
	if (bHolding)
	{
		EndHold();
	}
}

FModularUIHoldTimer::EStep FModularUIHoldTimer::Advance(double Time, float& OutProgress)
{
	OutProgress = 0.0f;
	if (!bHolding)
	{
		return EStep::None;
	}

	// Timed up to when the last key came up, so a key let go just short of the time, between two updates, never completes it.
	const double HeldUntil = bLetGo ? LetGoTime : Time;
	const float Progress = HoldTime > 0.0f ? FMath::Clamp(static_cast<float>((HeldUntil - StartTime) / HoldTime), 0.0f, 1.0f) : 1.0f;
	if (Progress >= 1.0f)
	{
		EndHold();
		OutProgress = 1.0f;
		return EStep::Completed;
	}
	if (bLetGo)
	{
		EndHold();
		return EStep::Cancelled;
	}
	OutProgress = Progress;
	return EStep::Holding;
}

bool FModularUIHoldTimer::IsAnyHeldKeyDown() const
{
	return Keys.ContainsByPredicate([this](const FKey& Key) { return PressedAt.Contains(Key); });
}

void FModularUIHoldTimer::EndHold()
{
	LastHeldPress = StartTime;
	bHolding = false;
	bLetGo = false;
	Keys.Reset();
}
