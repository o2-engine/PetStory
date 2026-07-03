#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Render/Render.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/Test/AppTestDriver.h"

#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"
#include "SlingGameFlow.h"
#include "SlingPuck.h"
#include "SlingPuckScene.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";

	// Counts distinct colors on a sparse grid; a real game frame has many, a blank frame one
	int CountDistinctColors(const Ref<Bitmap>& bitmap)
	{
		if (!bitmap)
			return 0;

		Vector<UInt32> seen;
		const UInt32* pixels = reinterpret_cast<const UInt32*>(bitmap->GetData());
		Vec2I size = bitmap->GetSize();
		for (int y = 0; y < size.y; y += 16)
		{
			for (int x = 0; x < size.x; x += 16)
			{
				UInt32 color = pixels[y * size.x + x];
				if (!seen.Contains(color))
					seen.Add(color);
			}
		}

		return seen.Count();
	}
}

class SlingPuckUI: public ::testing::Test
{
protected:
	Ref<Actor>               root;
	Ref<SlingBoard>          board;
	Ref<SlingBot>            bot;
	Ref<SlingGameController> controller;
	Ref<SlingGameFlow>       flow;
	Ref<CameraActor>         camera;

	void SetUp() override
	{
		root = BuildSlingPuckScene();
		board = root->GetComponent<SlingBoard>();
		bot = root->GetComponent<SlingBot>();
		controller = root->GetComponent<SlingGameController>();
		flow = root->GetComponent<SlingGameFlow>();

		bot->difficulty = 0.0f; // slowest bot, it won't interfere during the short test window

		AppTestDriver::PumpFrames(5); // settle transforms and prime the camera listeners layer

		camera = o2Scene.GetCameras()[0].Lock();
		ASSERT_TRUE(camera);
	}

	void TearDown() override
	{
		if (camera)
			camera->Destroy();
		if (root)
			root->Destroy();

		AppTestDriver::PumpFrames(2); // let the scene flush destroyed actors
	}

	Vec2F WorldToScreen(const Vec2F& world) const
	{
		return camera->listenersLayer->ScreenFromLocal(world);
	}

	Ref<SlingPuck> FindPuckAt(const Vec2F& boardPos) const
	{
		for (auto& puck : board->GetPucks())
		{
			if (puck && (puck->position - boardPos).Length() < 1.0f)
				return puck;
		}

		return nullptr;
	}

	// Mirrors every chip of the side onto the other half, so the controller declares its winner
	void ClearSide(int side)
	{
		for (auto& puck : board->GetPucks())
		{
			if (puck && SlingBoard::SideOfPosition(puck->position) == side)
				puck->position.y = -puck->position.y;
		}

		AppTestDriver::PumpFrames(3); // detect the win and pop the result window
	}

	void ClickWindowButton(const String& windowName, const String& buttonName)
	{
		auto window = root->GetChild(windowName);
		ASSERT_TRUE(window);
		auto button = window->GetChild(windowName + buttonName);
		ASSERT_TRUE(button);

		AppTestDriver::Click(WorldToScreen(button->transform->worldPosition.Get()));
		AppTestDriver::PumpFrames(2);
	}
};

TEST_F(SlingPuckUI, ScreenshotCapturesRealFrame)
{
	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);
	EXPECT_EQ(bitmap->GetSize(), o2Render.GetResolution());
	EXPECT_GT(CountDistinctColors(bitmap), 8); // wooden field, chips, bands — far from a blank frame

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "capture_check.png"));
}

TEST_F(SlingPuckUI, PlayerDragsChipAndShootsThroughGap)
{
	// Grab the chip near the centre column and pull it down to the band's middle: the grip at
	// x = 0 gives a straight vertical launch through the central gap onto the bot side
	Ref<SlingPuck> chip = FindPuckAt(Vec2F(20.0f, -280.0f));
	ASSERT_TRUE(chip);

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "shot_1_initial.png"));

	Vec2F pullTarget(0.0f, -board->halfHeight - 60.0f); // cursor overshoots past the wall on purpose

	AppTestDriver::PressCursor(WorldToScreen(chip->position));
	EXPECT_TRUE(chip->held);

	AppTestDriver::MoveCursor(WorldToScreen(pullTarget), 15);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "shot_2_pulled.png")); // stretched band visible

	// the pull is limited by the walls: the chip (and so the band) never leaves the field
	EXPECT_GE(chip->position.y, -board->halfHeight + chip->radius - 0.5f);

	AppTestDriver::ReleaseCursor();
	EXPECT_FALSE(chip->held);

	AppTestDriver::Wait(1.5f);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "shot_3_after.png"));

	EXPECT_GT(chip->position.y, 0.0f); // crossed to the bot side through the gap
}

TEST_F(SlingPuckUI, BotPullsBandAndShoots)
{
	bot->difficulty = 100.0f; // fastest bot: fires 0.2 s after the game starts

	Vector<Vec2F> before;
	for (auto& puck : board->GetPucks())
		before.Add(puck->position);

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "bot_1_initial.png"));

	int guard = 0;
	while (!bot->IsPulling() && guard++ < 300)
		AppTestDriver::PumpFrames(1);
	ASSERT_TRUE(bot->IsPulling());

	AppTestDriver::Wait(bot->pullDuration * 0.8f); // deep in the draw, the red band is stretched
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "bot_2_pulling.png"));

	AppTestDriver::Wait(1.5f); // release and flight
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "bot_3_after.png"));

	// the bot really shot something: a chip from its side moved substantially
	bool anyMoved = false;
	for (int i = 0; i < before.Count(); i++)
	{
		if (before[i].y > 0.0f && (board->GetPucks()[i]->position - before[i]).Length() > 50.0f)
			anyMoved = true;
	}
	EXPECT_TRUE(anyMoved);
}

TEST_F(SlingPuckUI, VictoryWindowNextLevelRaisesDifficulty)
{
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 10.0f); // the run starts at difficulty 10

	ClearSide(0); // player side empty -> player won

	EXPECT_TRUE(flow->IsWindowShown());
	EXPECT_TRUE(root->GetChild("VictoryWindow")->IsEnabled());
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_victory.png"));

	ClickWindowButton("VictoryWindow", "LeftButton"); // NEXT LEVEL

	EXPECT_FALSE(root->GetChild("VictoryWindow")->IsEnabled());
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 20.0f);
	EXPECT_FLOAT_EQ(bot->difficulty, 20.0f);
	EXPECT_FALSE(controller->IsGameOver());
	EXPECT_TRUE(FindPuckAt(Vec2F(20.0f, -280.0f)).IsValid()); // chips are back at their spawns
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_next_level.png"));
}

TEST_F(SlingPuckUI, GameOverWindowRetryRestartsFromTen)
{
	flow->StartLevel(30.0f); // as if the player had climbed a few levels
	AppTestDriver::PumpFrames(1);

	ClearSide(1); // bot side empty -> bot won, player lost

	EXPECT_TRUE(root->GetChild("GameOverWindow")->IsEnabled());
	EXPECT_FALSE(board->IsPlayerInputEnabled());
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_gameover.png"));

	ClickWindowButton("GameOverWindow", "LeftButton"); // RETRY

	EXPECT_FALSE(root->GetChild("GameOverWindow")->IsEnabled());
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 10.0f);
	EXPECT_FLOAT_EQ(bot->difficulty, 10.0f);
	EXPECT_TRUE(board->IsPlayerInputEnabled());
}

TEST_F(SlingPuckUI, DragOutsideChipsDoesNothing)
{
	Vector<Vec2F> before;
	for (auto& puck : board->GetPucks())
		before.Add(puck->position);

	AppTestDriver::Drag(WorldToScreen(Vec2F(0.0f, -60.0f)), WorldToScreen(Vec2F(0.0f, -430.0f)), 8);
	AppTestDriver::Wait(0.5f);

	for (int i = 0; i < before.Count(); i++)
		EXPECT_LT((board->GetPucks()[i]->position - before[i]).Length(), 1.0f);
}
