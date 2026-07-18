#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ParticlesEmitterComponent.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Scene.h"

using namespace o2;

#if IS_SCRIPTING_SUPPORTED

// Headless tests of the fully-scripted TicTacToeGame.js: the C++ side only bootstraps the
// ScriptableComponent, everything else (scene, view, bot, tutorial) is driven through the
// script instance — the same code paths the real buttons use.
class TicTacToeLogic: public ::testing::Test
{
protected:
	Ref<Actor>               root;
	Ref<ScriptableComponent> script;
	ScriptValue              instance;

	void SetUp() override
	{
		root = mmake<Actor>(ActorCreateMode::InScene);
		root->SetName("TicTacToe");

		auto lib = mmake<ScriptableComponent>();
		root->AddComponent(lib);
		lib->SetScript(AssetRef<JavaScriptAsset>("Scripts/TicTacToe/TttLib.js"));

		script = mmake<ScriptableComponent>();
		root->AddComponent(script);
		script->SetScript(AssetRef<JavaScriptAsset>("Scripts/TicTacToe/TicTacToeGame.js"));

		instance = script->GetInstance();
		ASSERT_TRUE(instance.IsObject());

		Tick(2); // OnStart -> BuildScene + NewMatch; the second frame flushes added entities

		instance.SetProperty("botDelay", 0.02f);
		Invoke("RestartWithSeed", 42); // deterministic stones, tutorial off
	}

	void TearDown() override
	{
		if (root)
			root->Destroy();

		for (auto& camera : o2Scene.GetCameras())
		{
			if (auto cameraRef = camera.Lock())
				cameraRef->Destroy();
		}

		Tick(2);
		o2Scene.Clear();
	}

	void Tick(int frames = 1, float dt = 1.0f/60.0f)
	{
		for (int i = 0; i < frames; i++)
		{
			o2Scene.Update(dt);
			o2Scene.UpdateTransforms();
		}
	}

	// Enough frames for the bot turn timer to expire
	void TickBotTurn() { Tick(10); }

	void Invoke(const char* method)
	{
		auto function = instance.GetProperty(method);
		ASSERT_TRUE(function.IsFunction()) << method;
		function.InvokeRaw(instance, {});
	}

	template<typename ... _args>
	void Invoke(const char* method, _args ... args)
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

	int Score(int side)
	{
		return instance.GetProperty("GetScore").Invoke<int, int>(instance, side);
	}

	int CountKind(int kind)
	{
		return instance.GetProperty("CountKind").Invoke<int, int>(instance, kind);
	}

	int QueueLength(int side)
	{
		return instance.GetProperty("GetQueueLength").Invoke<int, int>(instance, side);
	}

	int TutorialStep()
	{
		return instance.GetProperty("GetTutorialStep").Invoke<int>(instance);
	}

	bool TokenFlag(const char* method, int row, int col)
	{
		return instance.GetProperty(method).Invoke<bool, int, int>(instance, row, col);
	}

	bool ResultShown()
	{
		return instance.GetProperty("IsResultShown").Invoke<bool>(instance);
	}

	bool StrikeVisible()
	{
		return instance.GetProperty("IsStrikeVisible").Invoke<bool>(instance);
	}

	void DisableBot() { Invoke("SetBotEnabled", false); }

	void ClickCell(int row, int col) { Invoke("OnCellClicked", row, col); }

	// Waits out the (disabled or real) bot turn and returns to the player
	void ClickCellAndPassBot(int row, int col)
	{
		ClickCell(row, col);
		TickBotTurn();
	}

	// First row that contains `span` consecutive empty cells, or -1; `outCol` gets the first column
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

TEST_F(TicTacToeLogic, SceneIsBuiltEntirelyFromScript)
{
	EXPECT_TRUE(root->GetChild("Cells"));
	EXPECT_TRUE(root->GetChild("Tokens"));
	EXPECT_TRUE(root->GetChild("ResultWindow"));
	EXPECT_TRUE(root->GetChild("Tutorial"));
	EXPECT_TRUE(root->GetChild("Cells/Cell_0_0"));
	EXPECT_TRUE(root->GetChild("Tokens/Token_4_4"));
	EXPECT_EQ(o2Scene.GetCameras().Count(), 1) << "the script must have created the camera";
	EXPECT_EQ(State(), "player");
}

// Regression: the poof was replayed at a stale position while the emitter was sub-track driven
TEST_F(TicTacToeLogic, PoofEffectFollowsPlacedToken)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(1, col);
	ASSERT_GE(row, 0);

	ClickCell(row, col);

	auto poof = root->GetChild("Fx/FxPoof");
	ASSERT_TRUE(poof);

	Vec2F expected = instance.GetProperty("GetCellPosition").Invoke<Vec2F, int, int>(instance, row, col);
	EXPECT_NEAR(poof->transform->GetPosition().x, expected.x, 0.1f);
	EXPECT_NEAR(poof->transform->GetPosition().y, expected.y, 0.1f);

	auto emitter = poof->GetComponent<ParticlesEmitterComponent>();
	ASSERT_TRUE(emitter);
	EXPECT_TRUE(emitter->IsPlaying()) << "the emitter must play directly, not through a sub-track";
	EXPECT_FALSE(emitter->IsSubControlled());
}

TEST_F(TicTacToeLogic, StonesArePlacedDeterministically)
{
	EXPECT_EQ(CountKind(3), 3);
	EXPECT_EQ(Cell(2, 2), 0) << "center must stay free";

	Vector<int> firstLayout;
	for (int i = 0; i < 25; i++)
		firstLayout.Add(Cell(i/5, i%5));

	Invoke("RestartWithSeed", 42);

	for (int i = 0; i < 25; i++)
		EXPECT_EQ(firstLayout[i], Cell(i/5, i%5)) << "same seed must give same stones";
}

TEST_F(TicTacToeLogic, PlayerMovePlacesPawAndBotResponds)
{
	int col = -1;
	int row = FindEmptyRowSpan(1, col);
	ASSERT_GE(row, 0);

	ClickCell(row, col);
	EXPECT_EQ(Cell(row, col), 1);
	EXPECT_EQ(State(), "botWait");

	TickBotTurn();
	EXPECT_EQ(CountKind(2), 1) << "bot must answer after its delay";
	EXPECT_EQ(State(), "player");
}

TEST_F(TicTacToeLogic, CannotPlaceOnOccupiedCellOrStone)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(1, col);
	ASSERT_GE(row, 0);

	ClickCellAndPassBot(row, col);
	ASSERT_EQ(Cell(row, col), 1);

	ClickCell(row, col); // same cell again: ignored
	EXPECT_EQ(CountKind(1), 1);

	for (int i = 0; i < 25; i++)
	{
		if (Cell(i/5, i%5) == 3)
		{
			ClickCell(i/5, i%5);
			EXPECT_EQ(Cell(i/5, i%5), 3);
			break;
		}
	}

	EXPECT_EQ(CountKind(1), 1);
}

TEST_F(TicTacToeLogic, FourInRowWinsAndHighlightsLine)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(4, col);
	ASSERT_GE(row, 0);

	for (int i = 0; i < 3; i++)
	{
		ClickCellAndPassBot(row, col + i);
		EXPECT_EQ(State(), "player");
	}

	ClickCell(row, col + 3);

	EXPECT_EQ(State(), "over");
	EXPECT_EQ(Score(0), 1);
	EXPECT_TRUE(StrikeVisible()) << "the strike stroke must appear right at the win";
	EXPECT_FALSE(ResultShown()) << "the window waits for the strike animation";

	Tick(60);
	EXPECT_TRUE(ResultShown());

	int highlighted = 0;
	for (int i = 0; i < 4; i++)
	{
		if (TokenFlag("IsTokenHighlighted", row, col + i))
			highlighted++;
	}
	EXPECT_EQ(highlighted, 4);
}

TEST_F(TicTacToeLogic, SeventhTokenDissolvesOldest)
{
	DisableBot();

	// Rows 0/2/4 can't chain into a 4-line, so 7 paws never win
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

		if (placed.Count() == 6)
		{
			EXPECT_TRUE(TokenFlag("IsTokenFadeMarked", placed[0].x, placed[0].y))
				<< "at the cap the oldest token must be marked to fade";
		}
	}

	ASSERT_EQ(placed.Count(), 7);
	EXPECT_EQ(Cell(placed[0].x, placed[0].y), 0) << "oldest token must leave the logical board";
	EXPECT_TRUE(TokenFlag("IsTokenDissolving", placed[0].x, placed[0].y));
	EXPECT_EQ(QueueLength(0), 6);
	EXPECT_EQ(CountKind(1), 6);

	Tick(60); // dissolve completes
	EXPECT_FALSE(TokenFlag("IsTokenDissolving", placed[0].x, placed[0].y));
}

TEST_F(TicTacToeLogic, BotBlocksImmediateThreat)
{
	int col = -1;
	int row = FindEmptyRowSpan(5, col);
	ASSERT_GE(row, 0) << "with 3 stones a fully empty row must exist";

	ClickCell(row, col + 1); TickBotTurn();
	ASSERT_EQ(State(), "player");
	if (Cell(row, col + 2) != 0 || Cell(row, col + 3) != 0)
		return; // bot already intruded into the row: threat can't be built, nothing to verify

	ClickCell(row, col + 2); TickBotTurn();
	ASSERT_EQ(State(), "player");
	if (Cell(row, col + 3) != 0)
		return;

	ClickCell(row, col + 3); TickBotTurn();
	ASSERT_EQ(State(), "player") << "player must not have won: bot had to block";

	EXPECT_FALSE(Cell(row, col) == 0 && Cell(row, col + 4) == 0)
		<< "bot must close one end of the open three";
}

TEST_F(TicTacToeLogic, BotTakesWinningMove)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(4, col);
	ASSERT_GE(row, 0);

	Invoke("DebugPlace", row, col, 2);
	Invoke("DebugPlace", row, col + 1, 2);
	Invoke("DebugPlace", row, col + 2, 2);

	Invoke("DebugBotMove");

	EXPECT_EQ(State(), "over");
	EXPECT_EQ(Score(1), 1);
	EXPECT_EQ(Cell(row, col + 3), 2) << "bot must complete its four";
}

TEST_F(TicTacToeLogic, PlayAgainKeepsScoreNewGameResetsIt)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(4, col);
	ASSERT_GE(row, 0);

	for (int i = 0; i < 3; i++)
		ClickCellAndPassBot(row, col + i);
	ClickCell(row, col + 3);

	ASSERT_EQ(Score(0), 1);
	Tick(60); // the strike animation plays before the window opens
	ASSERT_TRUE(ResultShown());

	Invoke("OnPlayAgainClicked");
	EXPECT_EQ(Score(0), 1) << "play again keeps the match score";
	EXPECT_FALSE(ResultShown());
	EXPECT_FALSE(StrikeVisible()) << "fresh round clears the strike";
	EXPECT_EQ(CountKind(1), 0);
	EXPECT_EQ(CountKind(3), 3) << "fresh stones each round";

	Invoke("OnNewGameClicked");
	EXPECT_EQ(Score(0), 0) << "new game resets the match score";
	EXPECT_EQ(Score(1), 0);
}

TEST_F(TicTacToeLogic, ClicksIgnoredWhileRoundIsOver)
{
	DisableBot();

	int col = -1;
	int row = FindEmptyRowSpan(4, col);
	ASSERT_GE(row, 0);

	for (int i = 0; i < 3; i++)
		ClickCellAndPassBot(row, col + i);
	ClickCell(row, col + 3);
	ASSERT_EQ(State(), "over");

	int pawsBefore = CountKind(1);

	int otherCol = -1;
	int otherRow = FindEmptyRowSpan(1, otherCol);
	ASSERT_GE(otherRow, 0);
	ClickCell(otherRow, otherCol);

	EXPECT_EQ(CountKind(1), pawsBefore);
}

// ---- tutorial ----

TEST_F(TicTacToeLogic, TutorialOpensAndAdvancesByTaps)
{
	Invoke("SetTutorialEnabled", true);
	Invoke("OnNewGameClicked");

	EXPECT_EQ(State(), "tutorial");
	EXPECT_EQ(TutorialStep(), 0);
	EXPECT_TRUE(root->GetChild("Tutorial")->IsEnabled());

	Invoke("OnTutorialNextClicked");
	EXPECT_EQ(TutorialStep(), 1);

	Invoke("OnTutorialNextClicked"); // -> interactive step
	EXPECT_EQ(TutorialStep(), 2);

	Invoke("OnTutorialNextClicked"); // tap catcher must not skip the interactive step
	EXPECT_EQ(TutorialStep(), 2);
}

TEST_F(TicTacToeLogic, TutorialInteractiveStepPlacesPawAndBotAnswers)
{
	Invoke("SetTutorialEnabled", true);
	Invoke("OnNewGameClicked");
	Invoke("OnTutorialNextClicked");
	Invoke("OnTutorialNextClicked");
	ASSERT_EQ(TutorialStep(), 2);

	// The board is disabled during the tutorial: only the hint button places the paw
	int col0 = -1;
	int row0 = FindEmptyRowSpan(1, col0);
	ASSERT_GE(row0, 0);
	ClickCell(row0, col0);
	EXPECT_EQ(CountKind(1), 0) << "board clicks must be ignored during the tutorial";

	Invoke("OnTutorialHintClicked");
	EXPECT_EQ(CountKind(1), 1);

	Tick(70); // the demo pause, then the bot answers and the tutorial advances
	EXPECT_EQ(CountKind(2), 1);
	EXPECT_EQ(TutorialStep(), 3);
}

TEST_F(TicTacToeLogic, TutorialSkipStartsCleanRound)
{
	Invoke("SetTutorialEnabled", true);
	Invoke("OnNewGameClicked");
	ASSERT_EQ(State(), "tutorial");

	DisableBot();
	Invoke("OnTutorialSkipClicked");
	TickBotTurn(); // the round starter alternates: the bot may open the fresh round

	EXPECT_EQ(State(), "player");
	EXPECT_EQ(TutorialStep(), -1);
	EXPECT_FALSE(root->GetChild("Tutorial")->IsEnabled());
	EXPECT_EQ(CountKind(1), 0);
	EXPECT_EQ(CountKind(3), 3);

	// Once skipped it doesn't reopen on NEW GAME
	Invoke("OnNewGameClicked");
	TickBotTurn();
	EXPECT_EQ(State(), "player");
}

#endif // IS_SCRIPTING_SUPPORTED
