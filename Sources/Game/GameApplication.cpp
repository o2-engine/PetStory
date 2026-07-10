#include "o2/stdafx.h"
#include "GameApplication.h"

#include "o2/Assets/Assets.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Application/Input.h"
#include "o2/Utils/Debug/Debug.h"

#include "PipelineDemoScene.h"

GameApplication::GameApplication(RefCounter* refCounter):
	Application(refCounter)
{}

void GameApplication::OnStarted()
{
	o2Application.SetWindowSize(Vec2I(1280, 1024));

	BuildPipelineDemoScene();

	// Settle one frame so components lay themselves out,
	// then persist the generated scene so it can be opened in the editor.
	o2Scene.Update(0.0f);
	o2Scene.UpdateTransforms();
	o2Scene.Save(o2Assets.GetAssetsPath() + String("PipelineDemo.scn"));
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("PetStory 3D Pipeline Demo") +
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
