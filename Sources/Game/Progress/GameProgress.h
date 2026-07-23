#pragma once

#include "o2/Utils/Types/Containers/Vector.h"
#include "o2/Utils/Types/String.h"

using namespace o2;

// ------------------------------------------------------------------
// In-memory game progress: position in the sequential level chain.
// The chain is the list of level asset paths from "Levels/Chain.json"
// ({ "levels": ["Levels/Level01.json", ...] }); levels play in order
// and wrap around after the last one.
// ------------------------------------------------------------------
namespace GameProgress
{
	// Loads the chain from the data asset; false when missing or empty
	bool LoadChain(const String& chainAssetPath = "Levels/Chain.json");

	// Replaces the chain directly (used by tests)
	void SetChain(const Vector<String>& levelPaths);

	int GetLevelsCount();

	// Current level index, 0-based; clamped into the chain
	int GetCurrentLevel();
	void SetCurrentLevel(int index);

	// Returns level asset path for the current level, empty when no chain
	String GetCurrentLevelPath();

	// Advances to the next level, wrapping to the first after the last
	void AdvanceLevel();

	// Resets progress and forgets the chain
	void Reset();
}
