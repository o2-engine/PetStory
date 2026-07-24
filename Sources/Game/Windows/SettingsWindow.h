#pragma once

#include "GameLib/Windows/GameWindow.h"

// Settings popup: sound and music toggles, policy links; UI logic in
// Assets/Scripts/UI/SettingsWindow.js inside the prototype
class SettingsWindow: public GameWindow
{
public:
	static constexpr auto kName = "Settings";

	SettingsWindow();
};
