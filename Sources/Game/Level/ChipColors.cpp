#include "o2/stdafx.h"
#include "Level/ChipColors.h"

namespace ChipColors
{
	static const char* kColors[] = { "Blue", "Green", "Orange", "Red", "Violet", "Yellow" };

	bool IsKnownColor(const String& color)
	{
		for (auto known : kColors)
		{
			if (color == known)
				return true;
		}

		return false;
	}

	String GetPrototypePath(const String& color)
	{
		if (!IsKnownColor(color))
			return String();

		return String("Prefabs/Chip") + color + ".proto";
	}

	String GetIconPath(const String& color)
	{
		if (!IsKnownColor(color))
			return String();

		return String("Game field/Objects/Main/") + color.ToLowerCase() + ".png";
	}
}
