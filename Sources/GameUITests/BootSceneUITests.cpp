#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"
#include "Screens/GameBootstrap.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MetaScreen.h"
#include "o2/Assets/Assets.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";
}

class BootSceneUI: public ::testing::Test
{
protected:
	void SetUp() override
	{
		UserDataModel::Reset();
		LevelChain::Reset();
	}

	void TearDown() override
	{
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
		UserDataModel::Reset();
		LevelChain::Reset();
	}

	Ref<GameBootstrapComponent> FindBootstrap()
	{
		auto actor = o2Scene.FindActor("GameBootstrap");
		return actor ? actor->GetComponent<GameBootstrapComponent>() : nullptr;
	}
};

TEST_F(BootSceneUI, BootSceneStartsGameOnMeta)
{
	o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Boot.scn"));
	AppTestDriver::PumpFrames(5);

	auto bootstrap = FindBootstrap();
	ASSERT_TRUE(bootstrap);
	ASSERT_TRUE(bootstrap->GetScreens());
	EXPECT_EQ(ScreenManager::Instance(), bootstrap->GetScreens().Get());

	auto meta = DynamicCast<MetaScreen>(bootstrap->GetScreens()->GetCurrentScreen());
	ASSERT_TRUE(meta);
	ASSERT_TRUE(meta->GetRoot());
	EXPECT_TRUE(meta->GetRoot()->FindChild("Dog"));
	EXPECT_TRUE(meta->GetRoot()->FindChild("PlayButton"));

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "boot_scene_meta.png");
}

// The editor play button dumps the opened scene and reloads it before updating
// (EditorApplication::CheckPlayingSwitch); the same save-load-update sequence
// must boot the game, and restoring the dump must tear it down and boot again
TEST_F(BootSceneUI, EditorPlayStopPlayCycleWorks)
{
	o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Boot.scn"));

	// Dump the pristine scene like the editor does on play
	DataDocument sceneDump;
	o2Scene.Save(sceneDump);

	// Play: reload from dump and update
	o2Scene.Load(sceneDump);
	AppTestDriver::PumpFrames(5);

	auto bootstrap = FindBootstrap();
	ASSERT_TRUE(bootstrap);
	ASSERT_TRUE(bootstrap->GetScreens());
	EXPECT_EQ(bootstrap->GetScreens()->GetCurrentScreen()->GetName(), MetaScreen::kName);

	// Stop: restore the dump; the running game must tear down
	o2Scene.Load(sceneDump);
	AppTestDriver::PumpFrames(1);

	auto restored = FindBootstrap();
	ASSERT_TRUE(restored);
	EXPECT_NE(restored, bootstrap);

	// Play again: a fresh manager boots the game once more
	AppTestDriver::PumpFrames(5);
	ASSERT_TRUE(restored->GetScreens());
	EXPECT_EQ(ScreenManager::Instance(), restored->GetScreens().Get());
	EXPECT_EQ(restored->GetScreens()->GetCurrentScreen()->GetName(), MetaScreen::kName);
}

TEST_F(BootSceneUI, GameplayRunsFromBootScene)
{
	o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Boot.scn"));
	AppTestDriver::PumpFrames(5);

	auto bootstrap = FindBootstrap();
	ASSERT_TRUE(bootstrap);

	bootstrap->GetScreens()->ShowScreen(GameplayScreen::kName);
	AppTestDriver::PumpFrames(3);

	auto gameplay = DynamicCast<GameplayScreen>(bootstrap->GetScreens()->GetCurrentScreen());
	ASSERT_TRUE(gameplay);
	ASSERT_TRUE(gameplay->GetRoot());
	EXPECT_TRUE(gameplay->GetRoot()->FindChild("Level"));
	EXPECT_TRUE(gameplay->GetRoot()->FindChild("GoalsBubble"));

	// Chips drop through the scene-driven update loop
	AppTestDriver::Wait(1.5f);
	auto chips = gameplay->GetRoot()->FindChild("Level")->FindChild("Chips");
	ASSERT_TRUE(chips);
	EXPECT_GT(chips->GetChildren().Count(), 0);
}
