#include "o2/stdafx.h"
#include "App/GameApplication.h"

#include "o2/Assets/Assets.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Application/Input.h"
#include "o2/Utils/Debug/Debug.h"

GameApplication::GameApplication(RefCounter* refCounter):
	Application(refCounter)
{}

void GameApplication::OnStarted()
{
	o2Application.SetWindowSize(Vec2I(720, 1280));

	// The boot scene carries the GameBootstrapComponent that starts the game;
	// the same scene played in the editor runs the game there
	o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Boot.scn"));
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("PetStory") +
		"; FPS: " + (String)((int)o2Time.GetFPS());
}

void GameApplication::OnDraw()
{
	o2Render.camera = Camera::Default();
}

void GameApplication::DrawScene()
{
	Application::DrawScene();
}
