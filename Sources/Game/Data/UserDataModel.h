#pragma once

#include "Data/UserData.h"

// ------------------------------------------------------------------
// Change model over the global user data: every mutation goes
// through here, keeping the invariants (non-negative wallet, level
// index inside the chain).
// ------------------------------------------------------------------
namespace UserDataModel
{
	// Returns the global user data
	UserData& Get();

	// Resets the data to defaults
	void Reset();

	// Lives, clamped at zero
	void SetLives(int count);

	// Coins wallet, clamped at zero
	void SetCoins(int count);
	void AddCoins(int amount);

	// Spends coins; false and no change when there aren't enough
	bool TrySpendCoins(int amount);

	void SetSoundEnabled(bool enabled);
	void SetMusicEnabled(bool enabled);

	// Current level index, clamped into [0, levelsCount)
	void SetCurrentLevel(int index, int levelsCount);

	// Advances to the next level, wrapping to the first after the last
	void AdvanceLevel(int levelsCount);
}
