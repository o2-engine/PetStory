#pragma once

#include "o2/Utils/Types/Containers/Vector.h"
#include "o2/Utils/Types/String.h"

using namespace o2;

// ------------------------------------------------------------------
// The sequential level chain: the list of level asset paths from
// "Levels/Chain.json" ({ "levels": ["Levels/Level01.json", ...] }).
// Which level the player is on lives in UserData.
// ------------------------------------------------------------------
namespace LevelChain
{
	// Loads the chain from the data asset; false when missing or empty
	bool Load(const String& chainAssetPath = "Levels/Chain.json");

	// Replaces the chain directly (used by tests)
	void Set(const Vector<String>& levelPaths);

	int Count();

	// Returns level asset path by index, empty when out of range
	String LevelPath(int index);

	// Forgets the chain
	void Reset();
}
