#include "o2/stdafx.h"
#include "GameApplication.h"

#include "o2/Assets/Assets.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Application/Input.h"
#include "o2/Utils/Debug/Debug.h"

#include "Localization.h"
#include "SlingPuckScene.h"
#include "YandexGames.h"

GameApplication::GameApplication(RefCounter* refCounter):
	Application(refCounter)
{}

void GameApplication::OnStarted()
{
	o2Application.SetWindowSize(Vec2I(1280, 1024));

	// The HTML shell initializes the Yandex SDK before main(), so the language is ready here;
	// without the SDK (desktop, local run) the code is empty and the default language stays
	Loc::SetLanguageFromCode(YandexGames::GetLanguage());

	BuildSlingPuckScene();

	// Settle one frame so components lay themselves out (rubber bands, transforms),
	// then persist the generated scene so it can be opened in the editor.
	o2Scene.Update(0.0f);
	o2Scene.UpdateTransforms();
	o2Scene.Save(o2Assets.GetAssetsPath() + String("SlingPuck.scn"));

	YandexGames::NotifyReady();
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("Sling Puck") +
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
