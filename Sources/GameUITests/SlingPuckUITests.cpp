#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Render/Render.h"
#include "o2/Render/Text.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/Test/AppTestDriver.h"

#include "Jokes.h"
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

	// Mean RGB over a sparse grid; the dialogs' dim layer must pull it down noticeably
	float AvgBrightness(const Ref<Bitmap>& bitmap)
	{
		if (!bitmap)
			return 0.0f;

		const UInt8* data = bitmap->GetData();
		Vec2I size = bitmap->GetSize();
		double sum = 0.0;
		int count = 0;
		for (int y = 0; y < size.y; y += 16)
		{
			for (int x = 0; x < size.x; x += 16)
			{
				const UInt8* pixel = data + (y * size.x + x) * 4;
				sum += (pixel[0] + pixel[1] + pixel[2]) / 3.0;
				count++;
			}
		}

		return count > 0 ? (float)(sum / count) : 0.0f;
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

		AppTestDriver::PumpFrames(5); // settle transforms, spawn the chips, prime the listeners layer

		bot->difficulty = 0.0f; // slowest bot, it won't interfere during the short test window
		                        // (set after the pump: the initial spawn resets it to startDifficulty)

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

	Vector<Ref<SlingPuck>> ActiveChipsOnSide(int side) const
	{
		Vector<Ref<SlingPuck>> chips;
		for (auto& puck : board->GetPucks())
		{
			if (puck && puck->active && SlingBoard::SideOfPosition(puck->position) == side)
				chips.Add(puck);
		}

		return chips;
	}

	// Spawns are random each round; parks the active chips at deterministic spots near the side
	// walls, keeping the centre column and the gap path free for scripted shots
	void ParkChipsAside()
	{
		for (int side = 0; side < 2; side++)
		{
			int slot = 0;
			for (auto& chip : ActiveChipsOnSide(side))
			{
				float y = 120.0f + 65.0f * (float)(slot / 2);
				chip->position = Vec2F(slot % 2 == 0 ? -180.0f : 180.0f, side == 0 ? -y : y);
				chip->velocity = Vec2F();
				slot++;
			}
		}

		AppTestDriver::PumpFrames(1); // sync the actors to the parked spots
	}

	// Mirrors every chip of the side onto the other half, so the controller declares its winner
	void ClearSide(int side)
	{
		for (auto& puck : ActiveChipsOnSide(side))
			puck->position.y = -puck->position.y;

		AppTestDriver::PumpFrames(3); // detect the win and pop the result window
	}

	void ClickWindowButton(const String& windowName, const String& buttonName)
	{
		auto window = root->GetChild(windowName);
		ASSERT_TRUE(window);
		auto button = DynamicCast<Widget>(window->GetChild(windowName + buttonName));
		ASSERT_TRUE(button);

		// a widget's worldPosition is its left-bottom corner; aim for the middle of its rect
		AppTestDriver::Click(WorldToScreen(button->layout->GetWorldRect().Center()));
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
	// Park the random spawns aside and put one chip near the centre column: pulling it to the
	// band's middle (x = 0) gives a straight vertical launch through the gap onto the bot side
	ParkChipsAside();

	auto playerChips = ActiveChipsOnSide(0);
	ASSERT_FALSE(playerChips.IsEmpty());

	Ref<SlingPuck> chip = playerChips[0];
	chip->position = Vec2F(20.0f, -280.0f);
	AppTestDriver::PumpFrames(1);

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "shot_1_initial.png"));

	Vec2F pullTarget(0.0f, -board->bottomHalfHeight - 60.0f); // cursor overshoots past the wall on purpose

	AppTestDriver::PressCursor(WorldToScreen(chip->position));
	EXPECT_TRUE(chip->held);

	AppTestDriver::MoveCursor(WorldToScreen(pullTarget), 15);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "shot_2_pulled.png")); // stretched band visible

	// the pull is limited by the walls: the chip (and so the band) never leaves the field
	EXPECT_GE(chip->position.y, -board->bottomHalfHeight + chip->radius - 0.5f);

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

TEST_F(SlingPuckUI, VictoryWindowShowsJokeOnDimAndNextLevelRaisesDifficulty)
{
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 10.0f); // the run starts at difficulty 10
	EXPECT_EQ(board->CountPucksOnSide(0), 3);      // with the minimum chips per side
	EXPECT_EQ(board->CountPucksOnSide(1), 3);

	float brightnessBefore = AvgBrightness(AppTestDriver::TakeScreenshot());

	ClearSide(0); // player side empty -> player won

	EXPECT_TRUE(flow->IsWindowShown());
	EXPECT_TRUE(root->GetChild("VictoryWindow")->IsEnabled());
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_victory.png"));

	// the dim layer darkens the whole frame behind the dialog
	float brightnessAfter = AvgBrightness(AppTestDriver::TakeScreenshot());
	EXPECT_LT(brightnessAfter, brightnessBefore * 0.95f);

	// the window carries a joke picked from the base, fitted into the plate area
	auto victoryWidget = DynamicCast<Widget>(root->GetChild("VictoryWindow"));
	ASSERT_TRUE(victoryWidget);
	auto joke = victoryWidget->GetLayerDrawable<Text>("joke");
	ASSERT_TRUE(joke);
	EXPECT_FALSE(joke->GetText().IsEmpty());
	EXPECT_LE(joke->GetRealSize().y, joke->GetSize().y + 0.5f);

	// even the longest joke of the base shrinks until it fits instead of spilling onto the button
	WString longest;
	for (int i = 0; i < Jokes::Count(); i++)
	{
		WString candidate((String)Jokes::At(i));
		if (candidate.Length() > longest.Length())
			longest = candidate;
	}
	joke->SetText(longest);
	SlingGameFlow::FitTextHeight(joke);
	EXPECT_LE(joke->GetRealSize().y, joke->GetSize().y + 0.5f);
	AppTestDriver::PumpFrames(1);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_victory_long_joke.png"));

	ClickWindowButton("VictoryWindow", "NextButton"); // NEXT LEVEL

	EXPECT_FALSE(root->GetChild("VictoryWindow")->IsEnabled());
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 20.0f);
	EXPECT_FLOAT_EQ(bot->difficulty, 20.0f);
	EXPECT_FALSE(controller->IsGameOver());
	EXPECT_EQ(board->CountPucksOnSide(0), 4); // difficulty 20 -> one more chip per side
	EXPECT_EQ(board->CountPucksOnSide(1), 4);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_next_level.png"));
}

TEST_F(SlingPuckUI, GameOverWindowRetryRestartsFromTen)
{
	flow->StartLevel(30.0f); // as if the player had climbed a few levels
	AppTestDriver::PumpFrames(1);
	EXPECT_EQ(board->CountPucksOnSide(0), 5); // difficulty 30 -> more chips on the field

	ClearSide(1); // bot side empty -> bot won, player lost

	EXPECT_TRUE(root->GetChild("GameOverWindow")->IsEnabled());
	EXPECT_FALSE(board->IsPlayerInputEnabled());

	// the window carries the red cross badge above the buttons
	auto gameOverWidget = DynamicCast<Widget>(root->GetChild("GameOverWindow"));
	ASSERT_TRUE(gameOverWidget);
	EXPECT_TRUE(gameOverWidget->GetLayer("cross"));

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_gameover.png"));

	ClickWindowButton("GameOverWindow", "RetryButton"); // RETRY

	EXPECT_FALSE(root->GetChild("GameOverWindow")->IsEnabled());
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 10.0f);
	EXPECT_FLOAT_EQ(bot->difficulty, 10.0f);
	EXPECT_TRUE(board->IsPlayerInputEnabled());
	EXPECT_EQ(board->CountPucksOnSide(0), 3); // back to the starting count
	EXPECT_EQ(board->CountPucksOnSide(1), 3);
}

TEST_F(SlingPuckUI, DragOutsideChipsDoesNothing)
{
	ParkChipsAside(); // the centre column is free, the drag below starts over empty wood

	Vector<Vec2F> before;
	for (auto& puck : board->GetPucks())
		before.Add(puck->position);

	AppTestDriver::Drag(WorldToScreen(Vec2F(0.0f, -60.0f)), WorldToScreen(Vec2F(0.0f, -430.0f)), 8);
	AppTestDriver::Wait(0.5f);

	for (int i = 0; i < before.Count(); i++)
		EXPECT_LT((board->GetPucks()[i]->position - before[i]).Length(), 1.0f);
}
