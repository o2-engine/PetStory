#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Assets/Types/SceneAsset.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

#if IS_SCRIPTING_SUPPORTED

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
				UInt32 color = pixels[y*size.x + x];
				if (!seen.Contains(color))
					seen.Add(color);
			}
		}

		return seen.Count();
	}

	const UInt8* GetPixel(const Ref<Bitmap>& bitmap, int x, int y)
	{
		Vec2I size = bitmap->GetSize();
		x = Math::Clamp(x, 0, size.x - 1);
		y = Math::Clamp(y, 0, size.y - 1);
		return bitmap->GetData() + (y*size.x + x)*4;
	}
}

// Full-stack UI tests of the scripted game: real window, cursor injection through
// AppTestDriver, frame captures. The scene is loaded from the saved TicTacToe.scn asset —
// the same path the shipped game uses — and the script components re-bind to it.
class TicTacToeUI: public ::testing::Test
{
protected:
	Ref<Actor>               root;
	Ref<ScriptableComponent> script;
	Ref<CameraActor>         camera;
	ScriptValue              instance;

	void SetUp() override
	{
		AssetRef<SceneAsset>("TicTacToe.scn")->Load();
		AppTestDriver::PumpFrames(5); // OnStart re-binds the parts, transforms settle

		root = o2Scene.FindActor("TicTacToe");
		ASSERT_TRUE(root);

		for (auto& component : root->GetComponents())
		{
			if (auto scriptable = DynamicCast<ScriptableComponent>(component))
			{
				if (scriptable->GetScript() && scriptable->GetScript()->GetPath() == "Scripts/TicTacToe/TicTacToeGame.js")
					script = scriptable;
			}
		}
		ASSERT_TRUE(script);

		instance = script->GetInstance();
		ASSERT_TRUE(instance.IsObject());

		instance.SetProperty("botDelay", 0.02f);
		InvokeJs("RestartWithSeed", 42); // deterministic stones, tutorial off
		AppTestDriver::PumpFrames(2);

		camera = o2Scene.GetCameras()[0].Lock();
		ASSERT_TRUE(camera);
	}

	void TearDown() override
	{
		camera = nullptr; // owned by the scene root now

		if (root)
			root->Destroy();

		AppTestDriver::PumpFrames(2);
	}

	void InvokeJs(const char* method)
	{
		auto function = instance.GetProperty(method);
		ASSERT_TRUE(function.IsFunction()) << method;
		function.InvokeRaw(instance, {});
	}

	template<typename ... _args>
	void InvokeJs(const char* method, _args ... args)
	{
		auto function = instance.GetProperty(method);
		ASSERT_TRUE(function.IsFunction()) << method;
		function.Invoke<void, _args ...>(instance, args ...);
	}

	int Cell(int row, int col)
	{
		return instance.GetProperty("GetCell").Invoke<int, int, int>(instance, row, col);
	}

	String State()
	{
		return instance.GetProperty("GetStateName").Invoke<String>(instance);
	}

	int CountKind(int kind)
	{
		return instance.GetProperty("CountKind").Invoke<int, int>(instance, kind);
	}

	int Score(int side)
	{
		return instance.GetProperty("GetScore").Invoke<int, int>(instance, side);
	}

	bool ResultShown()
	{
		return instance.GetProperty("IsResultShown").Invoke<bool>(instance);
	}

	bool TokenFlag(const char* method, int row, int col)
	{
		return instance.GetProperty(method).Invoke<bool, int, int>(instance, row, col);
	}

	void DisableBot() { InvokeJs("SetBotEnabled", false); }

	Vec2F CellWorldPos(int row, int col)
	{
		return instance.GetProperty("GetCellPosition").Invoke<Vec2F, int, int>(instance, row, col);
	}

	Vec2F WorldToScreen(const Vec2F& world) const
	{
		return camera->listenersLayer->ScreenFromLocal(world);
	}

	Vec2I ScreenToPixel(const Vec2F& screen) const
	{
		Vec2F screenCenter = (Vec2F)o2Render.GetResolution()*0.5f;
		return Vec2I((int)(screen.x + screenCenter.x), (int)(screenCenter.y - screen.y));
	}

	void ClickCell(int row, int col)
	{
		AppTestDriver::Click(WorldToScreen(CellWorldPos(row, col)));
		AppTestDriver::PumpFrames(2);
	}

	// Pumps frames until the game state leaves "botWait" (the scene runs on a small fixed dt
	// when frames are pumped unthrottled, so waiting must poll the state, not real time)
	void WaitPlayerTurn()
	{
		for (int i = 0; i < 3000 && State() == "botWait"; i++)
			AppTestDriver::PumpFrames(1);
	}

	void ClickCellAndPassBot(int row, int col)
	{
		ClickCell(row, col);
		WaitPlayerTurn();
	}

	int FindEmptyRowSpan(int span, int& outCol)
	{
		for (int row = 0; row < 5; row++)
		{
			int runStart = -1, run = 0;
			for (int col = 0; col < 5; col++)
			{
				if (Cell(row, col) == 0)
				{
					if (run == 0)
						runStart = col;
					run++;
					if (run >= span)
					{
						outCol = runStart;
						return row;
					}
				}
				else
					run = 0;
			}
		}

		return -1;
	}
};

TEST_F(TicTacToeUI, ScreenshotCapturesRealFrame)
{
	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);
	EXPECT_EQ(bitmap->GetSize(), o2Render.GetResolution());
	EXPECT_GT(CountDistinctColors(bitmap), 10) << "lawn, board, panels — far from a blank frame";

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "ttt_initial.png"));
}

TEST_F(TicTacToeUI, CursorClickPlacesPawToken)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(1, col);
	ASSERT_GE(row, 0);

	ClickCell(row, col);

	EXPECT_EQ(Cell(row, col), 1) << "cursor click must reach the JS logic through the cell button";
	EXPECT_EQ((instance.GetProperty("GetTokenKind").Invoke<int, int, int>(instance, row, col)), 1);

	AppTestDriver::PumpFrames(60); // mid-poof frame: the dust puff around the landing cell
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "ttt_poof.png");

	AppTestDriver::PumpFrames(250); // let the spawn bounce settle (scene runs on a small fixed dt)

	// The paw token is drawn: its deep-red coin rim stands out from the tan wood around it
	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);

	Vec2I center = ScreenToPixel(WorldToScreen(CellWorldPos(row, col)));
	bool foundToken = false;
	for (int dy = -44; dy <= 44 && !foundToken; dy += 4)
	{
		for (int dx = -44; dx <= 44 && !foundToken; dx += 4)
		{
			const UInt8* pixel = GetPixel(bitmap, center.x + dx, center.y + dy);
			if (pixel[0] > 140 && pixel[1] < pixel[0] - 70 && pixel[2] < pixel[0] - 70)
				foundToken = true;
		}
	}
	EXPECT_TRUE(foundToken) << "paw token pixels must be visible at the clicked cell";

	AppTestDriver::SaveScreenshot(kScreenshotsDir + "ttt_paw_placed.png");
}

TEST_F(TicTacToeUI, BotAnswersAfterPlayerMove)
{
	int col = -1;
	int row = FindEmptyRowSpan(1, col);
	ASSERT_GE(row, 0);

	ClickCell(row, col);
	WaitPlayerTurn();

	EXPECT_EQ(CountKind(2), 1);
	EXPECT_EQ(State(), "player");
}

TEST_F(TicTacToeUI, VictoryShowsWindowAndPlayAgainRestartsRound)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(4, col);
	ASSERT_GE(row, 0);

	for (int i = 0; i < 3; i++)
		ClickCellAndPassBot(row, col + i);
	ClickCell(row, col + 3);

	EXPECT_EQ(State(), "over");
	EXPECT_TRUE((instance.GetProperty("IsStrikeVisible").Invoke<bool>(instance)))
		<< "the strike stroke must appear right at the win";

	AppTestDriver::PumpFrames(220); // mid-strike frame for the screenshot
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "ttt_strike.png");

	for (int i = 0; i < 3000 && !ResultShown(); i++)
		AppTestDriver::PumpFrames(1);
	ASSERT_TRUE(ResultShown());

	AppTestDriver::PumpFrames(400); // window pop tween + confetti
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "ttt_victory.png");

	ASSERT_TRUE(root->GetChild("ResultWindow/Panel/PlayAgainButton"));
	AppTestDriver::Click(WorldToScreen(Vec2F(0.0f, -166.0f))); // PLAY AGAIN center: panel (0,10) + button (0,-176)
	AppTestDriver::PumpFrames(3);
	WaitPlayerTurn(); // the round starter alternates: the bot may open this round

	EXPECT_FALSE(ResultShown());
	EXPECT_EQ(State(), "player");
	EXPECT_EQ(CountKind(1), 0) << "board must be fresh after PLAY AGAIN";
}

TEST_F(TicTacToeUI, NewGameButtonResetsScores)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(4, col);
	ASSERT_GE(row, 0);

	for (int i = 0; i < 3; i++)
		ClickCellAndPassBot(row, col + i);
	ClickCell(row, col + 3);

	ASSERT_EQ(Score(0), 1);

	ASSERT_TRUE(root->GetChild("Hud/NewGameButton"));
	AppTestDriver::Click(WorldToScreen(Vec2F(0.0f, -452.0f))); // NEW GAME center
	AppTestDriver::PumpFrames(3);
	WaitPlayerTurn();

	EXPECT_EQ(Score(0), 0);
	EXPECT_EQ(State(), "player");
}

TEST_F(TicTacToeUI, SeventhTokenPlaysDissolveShaderAndDisappears)
{
	DisableBot();

	const int safeCells[][2] = { {0, 0}, {0, 1}, {2, 0}, {2, 1}, {4, 0}, {4, 1},
								 {0, 3}, {2, 3}, {4, 3}, {0, 4}, {2, 4}, {4, 4} };

	Vector<Vec2I> placed;
	for (auto& cell : safeCells)
	{
		if (placed.Count() >= 7)
			break;

		if (Cell(cell[0], cell[1]) != 0)
			continue;

		ClickCellAndPassBot(cell[0], cell[1]);
		ASSERT_EQ(State(), "player");
		placed.Add(Vec2I(cell[0], cell[1]));
	}

	ASSERT_EQ(placed.Count(), 7);

	EXPECT_TRUE(TokenFlag("IsTokenDissolving", placed[0].x, placed[0].y))
		<< "the 7th token must start the oldest one's dissolve";
	EXPECT_EQ(Cell(placed[0].x, placed[0].y), 0);

	AppTestDriver::PumpFrames(150); // mid-dissolve: the shader is on screen here
	EXPECT_TRUE(TokenFlag("IsTokenDissolving", placed[0].x, placed[0].y));
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "ttt_dissolve.png");

	for (int i = 0; i < 3000 && TokenFlag("IsTokenDissolving", placed[0].x, placed[0].y); i++)
		AppTestDriver::PumpFrames(1);

	EXPECT_FALSE(TokenFlag("IsTokenDissolving", placed[0].x, placed[0].y));

	// The dissolved cell shows plain wood again: no strong paw-red pixels at its center
	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);

	Vec2I center = ScreenToPixel(WorldToScreen(CellWorldPos(placed[0].x, placed[0].y)));
	const UInt8* pixel = GetPixel(bitmap, center.x, center.y);
	EXPECT_FALSE(pixel[0] > 190 && pixel[1] < 130 && pixel[2] < 110)
		<< "paw token must be gone from the frame after the dissolve";
}

TEST_F(TicTacToeUI, BotWinStrikesLineAndShowsWindow)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(4, col);
	ASSERT_GE(row, 0);

	InvokeJs("DebugPlace", row, col, 2);
	InvokeJs("DebugPlace", row, col + 1, 2);
	InvokeJs("DebugPlace", row, col + 2, 2);
	InvokeJs("DebugBotMove");
	AppTestDriver::PumpFrames(2);

	EXPECT_EQ(State(), "over");
	EXPECT_EQ(Score(1), 1);
	EXPECT_TRUE((instance.GetProperty("IsStrikeVisible").Invoke<bool>(instance)));

	AppTestDriver::PumpFrames(220); // mid-strike frame: the steel-blue loss stroke
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "ttt_strike_loss.png");

	for (int i = 0; i < 3000 && !ResultShown(); i++)
		AppTestDriver::PumpFrames(1);
	EXPECT_TRUE(ResultShown());
}

TEST_F(TicTacToeUI, TutorialShowsAndSkipsByButtons)
{
	InvokeJs("SetTutorialEnabled", true);
	InvokeJs("OnNewGameClicked");
	AppTestDriver::PumpFrames(3);

	ASSERT_EQ(State(), "tutorial");
	AppTestDriver::PumpFrames(30);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "ttt_tutorial.png");

	// Tap anywhere advances the first steps through the fullscreen catcher button
	AppTestDriver::Click(WorldToScreen(Vec2F(0.0f, 100.0f)));
	AppTestDriver::PumpFrames(2);
	int step = instance.GetProperty("GetTutorialStep").Invoke<int>(instance);
	EXPECT_EQ(step, 1) << "a tap must advance the tutorial";

	// SKIP drops into a clean round; widget world position is its corner, click the known center
	ASSERT_TRUE(root->GetChild("Tutorial/TutorialSkipButton"));
	AppTestDriver::Click(WorldToScreen(Vec2F(500.0f, -452.0f)));
	AppTestDriver::PumpFrames(3);
	WaitPlayerTurn();

	EXPECT_EQ(State(), "player");
	EXPECT_FALSE(root->GetChild("Tutorial")->IsEnabled());
}

#endif // IS_SCRIPTING_SUPPORTED
