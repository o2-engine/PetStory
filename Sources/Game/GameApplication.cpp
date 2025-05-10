#include "o2/stdafx.h"
#include "GameApplication.h"

#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Application/Input.h"
#include "o2/Utils/Debug/Debug.h"
#include "o2/Scripts/ScriptEngine.h"
#include "o2/Utils/FileSystem/FileSystem.h"

GameApplication::GameApplication(RefCounter* refCounter):
	Application(refCounter)
{}

void GameApplication::OnStarted()
{
	o2Scene.Load(GetBuiltAssetsPath() + String("test.scn"));
	o2Application.SetWindowSize(Vec2I(750, 1334));

	o2Scripts.Run(o2Scripts.Parse(o2FileSystem.ReadFile(GetAssetsPath() + String("test.js"))));
	o2Scripts.Run(o2Scripts.Parse(o2FileSystem.ReadFile(GetAssetsPath() + String("testUpdate.js"))));
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("Pet story") +
		"; FPS: " + (String)((int)o2Time.GetFPS()) +
		" Cursor: " + (String)o2Input.GetCursorPos() +
		" JS: " + (String)(o2Scripts.GetUsedMemory() / 1024) + "kb";

	if (o2Input.IsKeyPressed('J'))
		o2Scripts.Run(o2Scripts.Parse(o2FileSystem.ReadFile(GetAssetsPath() + String("testUpdate.js"))));

	//o2Debug.DrawCircle(o2Input.GetCursorPos(), 20);
}

void GameApplication::OnDraw()
{
	o2Render.camera = Camera::Default();
}

void GameApplication::DrawScene()
{
	Application::DrawScene();

	o2Scripts.GetGlobal().GetProperty("updateAndDraw").Invoke<void, float>(o2Time.GetDeltaTime());
}

