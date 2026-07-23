#include "o2/stdafx.h"
#include "GameApplication.h"

#include "Progress/GameProgress.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MetaScreen.h"
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

	GameProgress::LoadChain();

	mScreens = mmake<ScreenManager>();
	mScreens->AddScreen(mmake<MetaScreen>());
	mScreens->AddScreen(mmake<GameplayScreen>());
	mScreens->ShowScreen(MetaScreen::kName);
}

void GameApplication::OnUpdate(float dt)
{
	if (mScreens)
		mScreens->Update(dt);

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
