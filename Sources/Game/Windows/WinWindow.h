#pragma once

#include "GameLib/Windows/GameWindow.h"

// Level completed popup: result stars and the next button; UI logic in
// Assets/Scripts/UI/WinWindow.js inside the prototype
class WinWindow: public GameWindow
{
public:
	static constexpr auto kName = "Win";

	WinWindow();
};
