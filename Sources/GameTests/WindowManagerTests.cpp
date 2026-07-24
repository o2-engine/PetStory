#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Scene/SceneTestHelpers.h"
#include "GameLib/Windows/GameWindow.h"
#include "GameLib/Windows/WindowManager.h"
#include "o2/Scene/Scene.h"

using namespace o2;

// Headless windows load stub roots: the lifecycle and the action flow are the
// same as with prototypes, only the visuals are missing
TEST(WindowManagerTests, ShowLoadsAndHideKeepsLoaded)
{
	SceneCleanGuard sceneGuard;

	auto window = mmake<GameWindow>("Test", "");
	EXPECT_FALSE(window->IsLoaded());
	EXPECT_FALSE(window->IsShown());

	window->Show();
	TickFrame();

	ASSERT_TRUE(window->IsLoaded());
	EXPECT_TRUE(window->IsShown());
	ASSERT_TRUE(window->GetRoot());
	EXPECT_EQ(window->GetRoot()->GetName(), "Test");
	EXPECT_TRUE(window->GetRoot()->IsEnabled());

	window->Hide();

	EXPECT_TRUE(window->IsLoaded());
	EXPECT_FALSE(window->IsShown());
	EXPECT_FALSE(window->GetRoot()->IsEnabled());

	// A loaded window shows again without reloading
	auto root = window->GetRoot();
	window->Show();
	EXPECT_EQ(window->GetRoot(), root);
	EXPECT_TRUE(window->IsShown());
}

TEST(WindowManagerTests, UnloadDestroysRoot)
{
	SceneCleanGuard sceneGuard;

	auto window = mmake<GameWindow>("Test", "");
	window->Show();
	TickFrame();

	window->Unload();

	EXPECT_FALSE(window->IsLoaded());
	EXPECT_FALSE(window->IsShown());
	EXPECT_EQ(window->GetRoot(), nullptr);

	o2Scene.UpdateDestroyingEntities();
}

TEST(WindowManagerTests, EmitActionFiresCallback)
{
	SceneCleanGuard sceneGuard;

	auto window = mmake<GameWindow>("Test", "");

	Vector<String> actions;
	window->onAction = [&](const String& action) { actions.Add(action); };

	window->Show();
	window->EmitAction("next");
	window->EmitAction("close");

	EXPECT_EQ(actions, Vector<String>({ "next", "close" }));
}

TEST(WindowManagerTests, ManagerShowsAndHidesByName)
{
	SceneCleanGuard sceneGuard;

	auto manager = mmake<WindowManager>();
	EXPECT_EQ(WindowManager::Instance(), manager.Get());

	manager->AddWindow(mmake<GameWindow>("A", ""));
	manager->AddWindow(mmake<GameWindow>("B", ""));

	ASSERT_TRUE(manager->GetWindow("A"));
	ASSERT_TRUE(manager->GetWindow("B"));
	EXPECT_EQ(manager->GetWindow("C"), nullptr);
	EXPECT_EQ(manager->ShowWindow("C"), nullptr);

	auto shown = manager->ShowWindow("A");
	ASSERT_TRUE(shown);
	EXPECT_TRUE(shown->IsShown());
	EXPECT_FALSE(manager->GetWindow("B")->IsShown());

	manager->ShowWindow("B");
	manager->HideWindow("A");
	EXPECT_FALSE(manager->GetWindow("A")->IsShown());
	EXPECT_TRUE(manager->GetWindow("B")->IsShown());

	manager->HideAll();
	EXPECT_FALSE(manager->GetWindow("B")->IsShown());
	EXPECT_TRUE(manager->GetWindow("A")->IsLoaded());

	manager->UnloadAll();
	EXPECT_FALSE(manager->GetWindow("A")->IsLoaded());
	EXPECT_FALSE(manager->GetWindow("B")->IsLoaded());

	o2Scene.UpdateDestroyingEntities();
}

TEST(WindowManagerTests, DuplicateNameIsRejected)
{
	SceneCleanGuard sceneGuard;

	auto manager = mmake<WindowManager>();

	auto first = mmake<GameWindow>("A", "");
	manager->AddWindow(first);
	manager->AddWindow(mmake<GameWindow>("A", "other"));

	EXPECT_EQ(manager->GetWindow("A"), first);
}

TEST(WindowManagerTests, InstanceClearsWithManager)
{
	{
		auto manager = mmake<WindowManager>();
		EXPECT_EQ(WindowManager::Instance(), manager.Get());
	}

	EXPECT_EQ(WindowManager::Instance(), nullptr);
}
