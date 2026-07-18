#pragma once

#include "o2/Application/Application.h"

using namespace o2;

class GameApplication: public Application
{
public:
	// Default constructor
	GameApplication(RefCounter* refCounter);

protected:
	String mExportMode;      // TTT_EXPORT env value: "anims" or "scene", empty in normal runs
	int    mExportFrame = 0; // Frames passed in export mode; the export fires after scene warm-up

	// Calls when application is starting
	void OnStarted() override;

	// Called on updating
	void OnUpdate(float dt) override;

	// Called on drawing
	void OnDraw() override;

	// Draws scene
	void DrawScene() override;

	// Creates the root actor with the game scripts; the scene is then built by JS
	void BootstrapFromCode();
};
