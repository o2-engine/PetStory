#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/LevelController.h"
#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MetaScreen.h"
#include "GameLib/Screens/ScreenManager.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";
}

class ScreenFlowUI: public ::testing::Test
{
protected:
	Ref<ScreenManager> manager;

	void SetUp() override
	{
		UserDataModel::Reset();
		LevelChain::Reset();
		ASSERT_TRUE(LevelChain::Load());

		manager = mmake<ScreenManager>();
		manager->AddScreen(mmake<MetaScreen>());
		manager->AddScreen(mmake<GameplayScreen>());
		manager->ShowScreen(MetaScreen::kName);

		PumpManager(3);
	}

	void TearDown() override
	{
		manager->Clear();
		manager = nullptr;
		UserDataModel::Reset();
		LevelChain::Reset();

		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}

	// The manager is driven by the app's OnUpdate in the game; tests pump it manually
	void PumpManager(int frames)
	{
		for (int i = 0; i < frames; i++)
		{
			manager->Update(o2Time.GetDeltaTime());
			AppTestDriver::PumpFrames(1);
		}
	}

	void PumpManagerTime(float seconds)
	{
		float time = 0.0f;
		while (time < seconds)
		{
			float dt = Math::Max(o2Time.GetDeltaTime(), 1.0f/60.0f);
			manager->Update(dt);
			AppTestDriver::PumpFrames(1);
			time += dt;
		}
	}

	Vec2F WorldToScreen(const Vec2F& world) const
	{
		auto camera = o2Scene.GetCameras()[0].Lock();
		return camera->listenersLayer->ScreenFromLocal(world);
	}
};

TEST_F(ScreenFlowUI, MetaScreenShowsDogAndPlayButton)
{
	auto metaScreen = DynamicCast<MetaScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(metaScreen);
	ASSERT_TRUE(metaScreen->GetRoot());

	EXPECT_TRUE(metaScreen->GetRoot()->FindChild("Dog"));
	EXPECT_TRUE(metaScreen->GetRoot()->FindChild("Back"));

	auto playButton = DynamicCast<Button>(metaScreen->GetRoot()->FindChild("PlayButton"));
	ASSERT_TRUE(playButton);
	EXPECT_TRUE(playButton->IsEnabledInHierarchy());

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "meta_screen.png");
}

TEST_F(ScreenFlowUI, PlayButtonClickOpensGameplay)
{
	auto metaScreen = DynamicCast<MetaScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(metaScreen);

	auto playButton = metaScreen->GetRoot()->FindChild("PlayButton");
	ASSERT_TRUE(playButton);

	// Widgets keep the pivot at the rect corner, so aim at the rect center
	Vec2F screenPos = WorldToScreen(playButton->transform->GetWorldRect().Center());
	AppTestDriver::Click(screenPos);

	PumpManager(3);

	auto gameplay = DynamicCast<GameplayScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(gameplay);
	EXPECT_TRUE(gameplay->IsActive());
	EXPECT_FALSE(manager->GetScreen(MetaScreen::kName)->IsLoaded());
}

TEST_F(ScreenFlowUI, GameplayBuildsLevelWithGoalsBubble)
{
	manager->ShowScreen(GameplayScreen::kName);
	PumpManager(3);

	auto gameplay = DynamicCast<GameplayScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	auto root = gameplay->GetRoot();
	ASSERT_TRUE(root);
	EXPECT_TRUE(root->FindChild("Level"));
	EXPECT_TRUE(root->FindChild("GoalsBubble"));
	EXPECT_TRUE(root->FindChild("GoalIcon0"));
	EXPECT_TRUE(root->FindChild("GoalLabel0"));

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);
	EXPECT_EQ(controller->GetGoals().Count(), 1); // Level01 has a single Green goal

	// Chips start dropping from the spawners
	AppTestDriver::Wait(1.5f);
	auto chips = root->FindChild("Level")->FindChild("Chips");
	ASSERT_TRUE(chips);
	EXPECT_GT(chips->GetChildren().Count(), 0);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "gameplay_screen.png");
}

TEST_F(ScreenFlowUI, CompletingGoalsReturnsToMetaAndAdvancesLevel)
{
	manager->ShowScreen(GameplayScreen::kName);
	PumpManager(3);

	auto gameplay = DynamicCast<GameplayScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	int levelBefore = UserDataModel::Get().currentLevel;

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);

	for (auto& goal : controller->GetGoals())
		controller->OnChipsPopped(goal.chipType, goal.count);

	EXPECT_TRUE(controller->IsCompleted());

	PumpManagerTime(2.0f); // wait out the completion delay and the deferred switch

	EXPECT_EQ(manager->GetCurrentScreen()->GetName(), MetaScreen::kName);
	EXPECT_EQ(UserDataModel::Get().currentLevel, levelBefore + 1);
}

TEST_F(ScreenFlowUI, GoalLabelShowsRemainingCount)
{
	manager->ShowScreen(GameplayScreen::kName);
	PumpManager(3);

	auto gameplay = DynamicCast<GameplayScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);
	int total = controller->GetGoals()[0].count;

	auto label = DynamicCast<Label>(gameplay->GetRoot()->FindChild("GoalLabel0"));
	ASSERT_TRUE(label);
	EXPECT_EQ(label->GetText(), (WString)(String)total);

	controller->OnChipsPopped(controller->GetGoals()[0].chipType, 3);
	PumpManager(1);

	EXPECT_EQ(label->GetText(), (WString)(String)(total - 3));
}
