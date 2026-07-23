#pragma once

#include "o2/Utils/Types/String.h"

using namespace o2;

// Chip color helpers: map color names (Blue/Green/Orange/Red/Violet/Yellow)
// to prefab and UI icon asset paths
namespace ChipColors
{
	// Returns "Prefabs/Chip<Color>.proto" or empty string for unknown color
	String GetPrototypePath(const String& color);

	// Returns "Game field/Objects/Main/<color>.png" or empty string for unknown color
	String GetIconPath(const String& color);

	// Returns true when the color name is one of the known chip colors
	bool IsKnownColor(const String& color);
}
