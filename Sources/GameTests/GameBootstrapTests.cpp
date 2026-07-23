#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/LevelController.h"
#include "Progress/GameProgress.h"
#include "Screens/GameBootstrap.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MetaScreen.h"
#include "o2/Assets/Assets.h"
#include "Scene/SceneTestHelpers.h"
#include "o2/Scene/Scene.h"

using namespace o2;

namespace
{
	struct BootGuard
	{
		SceneCleanGuard sceneGuard;

		~BootGuard()
		{
			GameProgress::Reset();
		}
	};

	Ref<GameBootstrapComponent> MakeBootActor()
	{
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName("GameBootstrap");
		return actor->AddComponent<GameBootstrapComponent>();
	}
}

// The whole game boots headless: visuals are guarded out, logic remains
TEST(GameBootstrapTests, StartsGameOnFirstSceneUpdate)
{
	BootGuard guard;

	auto bootstrap = MakeBootActor();
	EXPECT_EQ(ScreenManager::Instance(), nullptr);

	TickFrame();

	ASSERT_TRUE(bootstrap->GetScreens());
	EXPECT_EQ(ScreenManager::Instance(), bootstrap->GetScreens().Get());
	EXPECT_EQ(GameProgress::GetLevelsCount(), 10);

	auto current = bootstrap->GetScreens()->GetCurrentScreen();
	ASSERT_TRUE(current);
	EXPECT_EQ(current->GetName(), MetaScreen::kName);
	EXPECT_TRUE(DynamicCast<MetaScreen>(current)->GetRoot());
}

TEST(GameBootstrapTests, FullLevelCycleReturnsToMeta)
{
	BootGuard guard;

	auto bootstrap = MakeBootActor();
	TickFrame();

	auto manager = bootstrap->GetScreens();
	manager->ShowScreen(GameplayScreen::kName);
	TickFrame();

	auto gameplay = DynamicCast<GameplayScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	int levelBefore = GameProgress::GetCurrentLevel();

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);
	for (auto& goal : controller->GetGoals())
		controller->OnChipsPopped(goal.chipType, goal.count);

	EXPECT_TRUE(controller->IsCompleted());

	TickFrames(90, 1.0f/30.0f); // wait out the completion delay and the deferred switch

	EXPECT_EQ(manager->GetCurrentScreen()->GetName(), MetaScreen::kName);
	EXPECT_EQ(GameProgress::GetCurrentLevel(), levelBefore + 1);
}

TEST(GameBootstrapTests, SceneClearTearsGameDown)
{
	BootGuard guard;

	MakeBootActor();
	TickFrame();
	ASSERT_NE(ScreenManager::Instance(), nullptr);

	o2Scene.Clear(true);
	o2Scene.UpdateDestroyingEntities();

	EXPECT_EQ(ScreenManager::Instance(), nullptr);
}

TEST(GameBootstrapTests, BootSceneAssetStartsGame)
{
	BootGuard guard;

	o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Boot.scn"));

	auto actor = o2Scene.FindActor("GameBootstrap");
	ASSERT_TRUE(actor);
	auto bootstrap = actor->GetComponent<GameBootstrapComponent>();
	ASSERT_TRUE(bootstrap);

	TickFrame();

	ASSERT_TRUE(bootstrap->GetScreens());
	EXPECT_EQ(bootstrap->GetScreens()->GetCurrentScreen()->GetName(), MetaScreen::kName);
}
