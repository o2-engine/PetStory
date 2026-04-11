#include "o2/stdafx.h"
#include "GameApplication.h"

#include "o2/Scene/Scene.h"
#include "o2/Application/Input.h"
#include "o2/Scripts/ScriptEngine.h"
#include "o2/Utils/FileSystem/FileSystem.h"

namespace
{
	String GetBuiltScriptPath(const char* scriptName)
	{
		return GetBuiltAssetsPath() + String("Scripts/") + scriptName;
	}

	void RunScriptIfExists(const String& scriptPath)
	{
		if (!o2FileSystem.IsFileExist(scriptPath))
			return;

		auto scriptSource = o2FileSystem.ReadFile(scriptPath);
		if (!scriptSource.IsEmpty())
			o2Scripts.Run(o2Scripts.Parse(scriptSource));
	}
}

GameApplication::GameApplication(RefCounter* refCounter):
	Application(refCounter)
{}

void GameApplication::OnStarted()
{
	o2Application.SetWindowSize(Vec2I(1280, 1024));

	o2Scene.Load(GetBuiltAssetsPath() + String("test.scn"));
	RunScriptIfExists(GetBuiltScriptPath("test.js"));
	RunScriptIfExists(GetBuiltScriptPath("testUpdate.js"));
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("Pet story") +
		"; FPS: " + (String)((int)o2Time.GetFPS()) +
		" Cursor: " + (String)o2Input.GetCursorPos() +
		" JS: " + (String)(o2Scripts.GetUsedMemory() / 1024) + "kb";

	if (o2Input.IsKeyPressed('J'))
		RunScriptIfExists(GetBuiltScriptPath("testUpdate.js"));

	//o2Debug.DrawCircle(o2Input.GetCursorPos(), 20);
}

void GameApplication::DrawScene()
{
	Application::DrawScene();

	auto updateAndDraw = o2Scripts.GetGlobal().GetProperty("updateAndDraw");
	if (updateAndDraw.IsFunction())
		updateAndDraw.Invoke<void, float>(o2Time.GetDeltaTime());
}

