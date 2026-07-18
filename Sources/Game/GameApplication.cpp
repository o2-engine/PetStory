#include "o2/stdafx.h"
#include "GameApplication.h"

#include "o2/Assets/Assets.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Application/Input.h"
#include "o2/Utils/Debug/Debug.h"

#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Assets/Types/SceneAsset.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scripts/ScriptEngine.h"

#include "TicTacToeExport.h"

#include <cstdlib>

GameApplication::GameApplication(RefCounter* refCounter):
	Application(refCounter)
{}

void GameApplication::OnStarted()
{
	o2Application.SetWindowSize(Vec2I(1280, 1024));

	const char* exportMode = std::getenv("TTT_EXPORT");
	mExportMode = exportMode ? String(exportMode) : String();

	if (mExportMode == "anims")
	{
		TicTacToeExport::ExportAnimations();
		return;
	}

	if (mExportMode == "scene")
	{
		// The scripts skip starting gameplay, so the scene is saved in its pristine state
		o2Scripts.GetGlobal().SetProperty("tttExportMode", ScriptValue(true));
		BootstrapFromCode();
		return;
	}

	// Normal run: the scene asset is authored by the export flow; fall back to building
	// everything from scripts when it is not built yet
	if (o2Assets.GetAssetInfo("TicTacToe.scn").IsValid())
		AssetRef<SceneAsset>("TicTacToe.scn")->Load();
	else
		BootstrapFromCode();

	o2Scene.Update(0.0f);
	o2Scene.UpdateTransforms();
}

void GameApplication::BootstrapFromCode()
{
	auto root = mmake<Actor>(ActorCreateMode::InScene);
	root->SetName("TicTacToe");

	auto lib = mmake<ScriptableComponent>();
	root->AddComponent(lib);
	lib->SetScript(AssetRef<JavaScriptAsset>("Scripts/TicTacToe/TttLib.js"));

	auto game = mmake<ScriptableComponent>();
	root->AddComponent(game);
	game->SetScript(AssetRef<JavaScriptAsset>("Scripts/TicTacToe/TicTacToeGame.js"));

	o2Scene.Update(0.0f);
	o2Scene.UpdateTransforms();
}

void GameApplication::OnUpdate(float dt)
{
	if (!mExportMode.IsEmpty())
	{
		mExportFrame++;

		// A few frames let OnStart run and the script finish building the scene
		if (mExportMode == "anims" || mExportFrame >= 5)
		{
			if (mExportMode == "scene")
				TicTacToeExport::ExportScene();

			std::exit(0); // Application::Shutdown is a no-op on Mac
		}

		return;
	}

	o2Application.windowCaption = String("PetStory — Paws vs Bones") +
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
