#pragma once

#include "CoreMinimal.h"

#include "InputCoreTypes.h"


/**
 * Times one hold at a time, from the key presses and releases it's told about, at the times it's told, so it works from any clock. A hold is
 * timed from the earliest press of its keys still down, completes once one has been held for its hold time, and is cancelled once every one
 * is let go first. Each press holds once: starting again during the same press does nothing, even after its hold ended.
 */
class FModularUIHoldTimer
{
public:
	/** Where a hold is, as it's moved on. */
	enum class EStep : uint8
	{
		/** No hold is in progress. */
		None,
		/** Still held, short of its hold time. */
		Holding,
		/** Held for its hold time. The hold is over. */
		Completed,
		/** Every key was let go first. The hold is over. */
		Cancelled
	};

	/** A key went down at Time, in seconds. A key already down stays timed from its first press. */
	void Press(const FKey& Key, double Time);

	/** A key came up at Time, in seconds. */
	void Release(const FKey& Key, double Time);

	/** Every key down came up at Time, such as when the application lost focus, as keys let go meanwhile never say so. */
	void ReleaseAll(double Time);

	/** Whether any key is down. */
	bool HasKeysDown() const { return !PressedAt.IsEmpty(); }

	/**
	 * Starts holding the keys for Hold Time seconds, timed from the earliest of them still down, and true. False, changing nothing, when none
	 * is down, or the press it would be timed from is holding or has held already. A hold of another press, in progress, is cancelled.
	 */
	bool Start(const TArray<FKey>& InKeys, float InHoldTime);

	/** Ends the hold in progress without completing it. Its press can't hold again. */
	void Cancel();

	/**
	 * Moves the hold on to Time, in seconds, and how far it is, 0 to 1: Completed at 1 once it was held long enough, even if let go since,
	 * Cancelled at 0 once let go short of it, and otherwise Holding.
	 */
	EStep Advance(double Time, float& OutProgress);

	bool IsHolding() const { return bHolding; }

private:
	bool IsAnyHeldKeyDown() const;
	void EndHold();

	/** When each key down now went down. */
	TMap<FKey, double> PressedAt;

	// The hold in progress: whether there is one, whether every key of it was let go, and when; its keys, how long it takes, and when the
	// press it's timed from went down.
	bool bHolding = false;
	bool bLetGo = false;
	double LetGoTime = 0.0;
	TArray<FKey> Keys;
	float HoldTime = 0.0f;
	double StartTime = 0.0;

	/** When the press the last hold was timed from went down, so that press can't hold again. */
	TOptional<double> LastHeldPress;
};
