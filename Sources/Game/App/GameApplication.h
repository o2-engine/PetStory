#pragma once

#include "o2/Application/Application.h"

using namespace o2;

class GameApplication: public Application
{
public:
	// Default constructor
	GameApplication(RefCounter* refCounter);

protected:
	// Calls when application is starting
	void OnStarted() override;

	// Called on updating
	void OnUpdate(float dt) override;

	// Called on drawing
	void OnDraw() override;

	// Draws scene
	void DrawScene() override;
};
