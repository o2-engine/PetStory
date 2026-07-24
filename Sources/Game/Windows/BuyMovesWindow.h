#pragma once

#include "GameLib/Windows/GameWindow.h"

// Out-of-moves popup: buy extra moves for coins or give up; UI logic in
// Assets/Scripts/UI/BuyMovesWindow.js inside the prototype
class BuyMovesWindow: public GameWindow
{
public:
	static constexpr auto kName = "BuyMoves";

	// Offer: kMoves extra moves for kPrice coins
	static constexpr int kMoves = 5;
	static constexpr int kPrice = 10;

	BuyMovesWindow();
};
