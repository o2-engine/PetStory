#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/LevelController.h"
#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"
#include "Screens/GameBootstrap.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MetaScreen.h"
#include "Windows/BuyMovesWindow.h"
#include "Windows/WinWindow.h"
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
			UserDataModel::Reset();
			LevelChain::Reset();
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
	EXPECT_EQ(LevelChain::Count(), 10);

	ASSERT_TRUE(bootstrap->GetWindows());
	EXPECT_EQ(WindowManager::Instance(), bootstrap->GetWindows().Get());
	EXPECT_TRUE(bootstrap->GetWindows()->GetWindow(WinWindow::kName));
	EXPECT_TRUE(bootstrap->GetWindows()->GetWindow(BuyMovesWindow::kName));
	EXPECT_TRUE(bootstrap->GetWindows()->GetWindow("Settings"));

	auto current = bootstrap->GetScreens()->GetCurrentScreen();
	ASSERT_TRUE(current);
	EXPECT_EQ(current->GetName(), MetaScreen::kName);
	EXPECT_TRUE(DynamicCast<MetaScreen>(current)->GetRoot());
}

TEST(GameBootstrapTests, FullLevelCycleThroughWinWindow)
{
	BootGuard guard;

	auto bootstrap = MakeBootActor();
	TickFrame();

	auto manager = bootstrap->GetScreens();
	manager->ShowScreen(GameplayScreen::kName);
	TickFrame();

	auto gameplay = DynamicCast<GameplayScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	int levelBefore = UserDataModel::Get().currentLevel;

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);
	for (auto& goal : controller->GetGoals())
		controller->OnChipsPopped(goal.chipType, goal.count);

	EXPECT_TRUE(controller->IsCompleted());

	TickFrames(90, 1.0f/30.0f); // wait out the completion delay

	// The win window is shown instead of an immediate switch
	auto winWindow = bootstrap->GetWindows()->GetWindow(WinWindow::kName);
	ASSERT_TRUE(winWindow);
	EXPECT_TRUE(winWindow->IsShown());
	EXPECT_EQ(manager->GetCurrentScreen()->GetName(), GameplayScreen::kName);
	EXPECT_EQ(UserDataModel::Get().currentLevel, levelBefore);

	// Next: as if the window script reported the button press
	winWindow->EmitAction("next");
	TickFrame();

	EXPECT_FALSE(winWindow->IsShown());
	EXPECT_EQ(manager->GetCurrentScreen()->GetName(), MetaScreen::kName);
	EXPECT_EQ(UserDataModel::Get().currentLevel, levelBefore + 1);
}

TEST(GameBootstrapTests, OutOfMovesBuyAndGiveUpFlow)
{
	BootGuard guard;

	auto bootstrap = MakeBootActor();
	TickFrame();

	auto manager = bootstrap->GetScreens();
	manager->ShowScreen(GameplayScreen::kName);
	TickFrame();

	auto gameplay = DynamicCast<GameplayScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);
	ASSERT_TRUE(controller->HasMovesLimit());

	// Waste every move on pops that don't complete the goals
	while (controller->GetMovesLeft() > 0)
		controller->OnChipsPopped("NoSuchColor", 1);

	auto buyWindow = bootstrap->GetWindows()->GetWindow(BuyMovesWindow::kName);
	ASSERT_TRUE(buyWindow);
	EXPECT_TRUE(buyWindow->IsShown());

	// Buying adds moves, spends coins and hides the window
	int coinsBefore = UserDataModel::Get().coins;
	buyWindow->EmitAction("buy");

	EXPECT_EQ(UserDataModel::Get().coins, coinsBefore - BuyMovesWindow::kPrice);
	EXPECT_EQ(controller->GetMovesLeft(), BuyMovesWindow::kMoves);
	EXPECT_FALSE(buyWindow->IsShown());

	// Running dry again reopens the window; giving up returns to the meta
	while (controller->GetMovesLeft() > 0)
		controller->OnChipsPopped("NoSuchColor", 1);

	EXPECT_TRUE(buyWindow->IsShown());

	int levelBefore = UserDataModel::Get().currentLevel;
	buyWindow->EmitAction("close");
	TickFrame();

	EXPECT_EQ(manager->GetCurrentScreen()->GetName(), MetaScreen::kName);
	EXPECT_EQ(UserDataModel::Get().currentLevel, levelBefore);
}

TEST(GameBootstrapTests, BuyWithoutCoinsKeepsWindowShown)
{
	BootGuard guard;

	auto bootstrap = MakeBootActor();
	TickFrame();

	auto manager = bootstrap->GetScreens();
	manager->ShowScreen(GameplayScreen::kName);
	TickFrame();

	auto gameplay = DynamicCast<GameplayScreen>(manager->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);

	UserDataModel::SetCoins(BuyMovesWindow::kPrice - 1);

	while (controller->GetMovesLeft() > 0)
		controller->OnChipsPopped("NoSuchColor", 1);

	auto buyWindow = bootstrap->GetWindows()->GetWindow(BuyMovesWindow::kName);
	ASSERT_TRUE(buyWindow);
	EXPECT_TRUE(buyWindow->IsShown());

	buyWindow->EmitAction("buy");

	EXPECT_TRUE(buyWindow->IsShown());
	EXPECT_EQ(controller->GetMovesLeft(), 0);
	EXPECT_EQ(UserDataModel::Get().coins, BuyMovesWindow::kPrice - 1);
}

TEST(GameBootstrapTests, SceneClearTearsGameDown)
{
	BootGuard guard;

	MakeBootActor();
	TickFrame();
	ASSERT_NE(ScreenManager::Instance(), nullptr);
	ASSERT_NE(WindowManager::Instance(), nullptr);

	o2Scene.Clear(true);
	o2Scene.UpdateDestroyingEntities();

	EXPECT_EQ(ScreenManager::Instance(), nullptr);
	EXPECT_EQ(WindowManager::Instance(), nullptr);
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
