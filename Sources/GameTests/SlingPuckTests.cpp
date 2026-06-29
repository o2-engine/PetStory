#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"
#include "SlingPuck.h"
#include "SlingRubber.h"

using namespace o2;

namespace
{
	Ref<SlingPuck> MakePuck(int team, const Vec2F& pos, float radius = 30.0f)
	{
		auto puck = mmake<SlingPuck>();
		puck->team = team;
		puck->radius = radius;
		puck->position = pos;
		return puck;
	}

	const float kStep = 1.0f / 60.0f;
}

// ===== SlingPuck =====

TEST(SlingPuck, TeamAndRestingQueries)
{
	auto player = MakePuck(0, Vec2F(0.0f, -100.0f));
	EXPECT_TRUE(player->IsPlayer());

	player->velocity = Vec2F();
	EXPECT_TRUE(player->IsResting(8.0f));
	player->velocity = Vec2F(100.0f, 0.0f);
	EXPECT_FALSE(player->IsResting(8.0f));

	auto bot = MakePuck(1, Vec2F(0.0f, 100.0f));
	EXPECT_FALSE(bot->IsPlayer());
}

// ===== SlingBoard physics =====

TEST(SlingBoard, FrictionBringsPuckToRest)
{
	auto board = mmake<SlingBoard>();
	board->friction = 0.9f;
	board->restSpeed = 8.0f;

	auto puck = MakePuck(0, Vec2F(0.0f, -100.0f));
	puck->velocity = Vec2F(300.0f, 0.0f);
	board->RegisterPuck(puck);

	for (int i = 0; i < 600; ++i)
		board->StepSimulation(kStep);

	EXPECT_TRUE(puck->IsResting(board->restSpeed));
	EXPECT_NEAR(puck->velocity.Length(), 0.0f, 1e-3f);
	EXPECT_LE(puck->position.x, board->halfWidth);
	EXPECT_GE(puck->position.x, -board->halfWidth);
}

TEST(SlingBoard, WallBounceReflectsVelocity)
{
	auto board = mmake<SlingBoard>();
	board->friction = 0.0f;
	board->wallRestitution = 0.55f;

	float r = 30.0f;
	auto puck = MakePuck(0, Vec2F(board->halfWidth - r - 5.0f, -100.0f), r);
	puck->velocity = Vec2F(600.0f, 0.0f);
	board->RegisterPuck(puck);

	for (int i = 0; i < 10; ++i)
		board->StepSimulation(kStep);

	EXPECT_LE(puck->position.x, board->halfWidth - r + 0.001f);
	EXPECT_LT(puck->velocity.x, 0.0f);
}

TEST(SlingBoard, PuckPassesThroughGap)
{
	auto board = mmake<SlingBoard>();
	board->friction = 0.0f;
	board->gapHalf = 85.0f;

	auto puck = MakePuck(0, Vec2F(0.0f, -30.0f), 20.0f);
	puck->velocity = Vec2F(0.0f, 1500.0f);
	board->RegisterPuck(puck);

	for (int i = 0; i < 10; ++i)
		board->StepSimulation(kStep);

	EXPECT_GT(puck->position.y, 0.0f);
	EXPECT_EQ(SlingBoard::SideOfPosition(puck->position), 1);
}

TEST(SlingBoard, DividerBlocksOutsideGap)
{
	auto board = mmake<SlingBoard>();
	board->friction = 0.0f;
	board->gapHalf = 85.0f;

	auto puck = MakePuck(0, Vec2F(200.0f, -30.0f), 20.0f);
	puck->velocity = Vec2F(0.0f, 1500.0f);
	board->RegisterPuck(puck);

	for (int i = 0; i < 30; ++i)
		board->StepSimulation(kStep);

	EXPECT_LT(puck->position.y, 0.0f);
	EXPECT_EQ(SlingBoard::SideOfPosition(puck->position), 0);
}

TEST(SlingBoard, PuckCollisionSeparatesAndExchangesMomentum)
{
	auto board = mmake<SlingBoard>();
	board->friction = 0.0f;
	board->puckRestitution = 0.85f;

	float r = 34.0f;
	auto a = MakePuck(0, Vec2F(0.0f, -100.0f), r);
	auto b = MakePuck(0, Vec2F(40.0f, -100.0f), r);
	a->velocity = Vec2F(100.0f, 0.0f);
	b->velocity = Vec2F(-100.0f, 0.0f);
	board->RegisterPuck(a);
	board->RegisterPuck(b);

	board->StepSimulation(kStep);

	EXPECT_LT(a->velocity.x, 0.0f);
	EXPECT_GT(b->velocity.x, 0.0f);

	float dist = (b->position - a->position).Length();
	EXPECT_GE(dist, 2.0f * r - 0.5f);
}

TEST(SlingBoard, SideCountAndNoWinnerWhenBothOccupied)
{
	auto board = mmake<SlingBoard>();
	board->RegisterPuck(MakePuck(0, Vec2F(0.0f, -100.0f)));
	board->RegisterPuck(MakePuck(0, Vec2F(50.0f, -150.0f)));
	board->RegisterPuck(MakePuck(1, Vec2F(0.0f, 100.0f)));

	EXPECT_EQ(board->CountPucksOnSide(0), 2);
	EXPECT_EQ(board->CountPucksOnSide(1), 1);
	EXPECT_EQ(board->GetWinner(), -1);

	EXPECT_EQ(SlingBoard::SideOfPosition(Vec2F(0.0f, -1.0f)), 0);
	EXPECT_EQ(SlingBoard::SideOfPosition(Vec2F(0.0f, 1.0f)), 1);
}

TEST(SlingBoard, WinnerWhenSideCleared)
{
	auto playerCleared = mmake<SlingBoard>();
	playerCleared->RegisterPuck(MakePuck(1, Vec2F(0.0f, 100.0f)));
	EXPECT_EQ(playerCleared->GetWinner(), 0);

	auto botCleared = mmake<SlingBoard>();
	botCleared->RegisterPuck(MakePuck(0, Vec2F(0.0f, -100.0f)));
	EXPECT_EQ(botCleared->GetWinner(), 1);
}

TEST(SlingBoard, HeldChipIsExcludedFromSimulation)
{
	auto board = mmake<SlingBoard>();
	board->friction = 0.9f;

	auto puck = MakePuck(0, Vec2F(0.0f, -100.0f));
	puck->velocity = Vec2F(300.0f, 0.0f);
	puck->held = true;
	board->RegisterPuck(puck);

	board->StepSimulation(kStep);

	EXPECT_FLOAT_EQ(puck->position.x, 0.0f);    // not integrated
	EXPECT_FLOAT_EQ(puck->position.y, -100.0f);
	EXPECT_FLOAT_EQ(puck->velocity.x, 300.0f);  // not damped
}

// ===== SlingRubber =====

TEST(SlingRubber, ClampGripBendsOnlyBackward)
{
	// player band (side 0) at restY = -300 bends only downward (y <= restY)
	Vec2F forward = SlingRubber::ClampGripToBack(Vec2F(20.0f, -250.0f), 0, -300.0f);
	EXPECT_FLOAT_EQ(forward.x, 20.0f);
	EXPECT_FLOAT_EQ(forward.y, -300.0f);

	Vec2F back = SlingRubber::ClampGripToBack(Vec2F(20.0f, -350.0f), 0, -300.0f);
	EXPECT_FLOAT_EQ(back.y, -350.0f);

	// bot band (side 1) at restY = 300 bends only upward (y >= restY)
	Vec2F botForward = SlingRubber::ClampGripToBack(Vec2F(0.0f, 250.0f), 1, 300.0f);
	EXPECT_FLOAT_EQ(botForward.y, 300.0f);

	Vec2F botBack = SlingRubber::ClampGripToBack(Vec2F(0.0f, 350.0f), 1, 300.0f);
	EXPECT_FLOAT_EQ(botBack.y, 350.0f);
}

TEST(SlingRubber, EffectiveGripIsRestCenterWhenIdle)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 0;
	rubber->restY = -320.0f;
	rubber->halfSpan = 170.0f;

	Vec2F idle = rubber->GetEffectiveGrip();
	EXPECT_FLOAT_EQ(idle.x, 0.0f);
	EXPECT_FLOAT_EQ(idle.y, -320.0f);

	rubber->SetGrip(Vec2F(40.0f, -380.0f));
	Vec2F pulled = rubber->GetEffectiveGrip();
	EXPECT_FLOAT_EQ(pulled.x, 40.0f);
	EXPECT_FLOAT_EQ(pulled.y, -380.0f);

	rubber->ClearGrip();
	EXPECT_FLOAT_EQ(rubber->GetEffectiveGrip().y, -320.0f);
}

TEST(SlingRubber, ComputeLaunchFiresForwardScaledByDepth)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 0;
	rubber->restY = -300.0f;
	rubber->minStretch = 6.0f;

	// pulled straight back 60 below the band -> straight forward (up), speed = depth * power
	Vec2F v = rubber->ComputeLaunch(Vec2F(0.0f, -360.0f), 4.0f, 4000.0f);
	EXPECT_NEAR(v.x, 0.0f, 1e-3f);
	EXPECT_NEAR(v.y, 240.0f, 1e-3f);

	// pulled twice as deep -> twice the speed
	Vec2F v2 = rubber->ComputeLaunch(Vec2F(0.0f, -420.0f), 4.0f, 4000.0f);
	EXPECT_NEAR(v2.y, 480.0f, 1e-3f);
}

TEST(SlingRubber, ComputeLaunchAddsLateralAimFromOffset)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 0;
	rubber->restY = -300.0f;

	// pulled back and to the left -> fired forward and toward centre (right)
	Vec2F v = rubber->ComputeLaunch(Vec2F(-50.0f, -360.0f), 4.0f, 4000.0f);
	EXPECT_GT(v.x, 0.0f);
	EXPECT_GT(v.y, 0.0f);
}

TEST(SlingRubber, ComputeLaunchIsZeroWhenBandNotStretched)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 0;
	rubber->restY = -300.0f;
	rubber->minStretch = 6.0f;

	// grip in front of the band (not pulled back) -> no shot
	Vec2F v = rubber->ComputeLaunch(Vec2F(40.0f, -200.0f), 4.0f, 4000.0f);
	EXPECT_FLOAT_EQ(v.x, 0.0f);
	EXPECT_FLOAT_EQ(v.y, 0.0f);
}

TEST(SlingRubber, ComputeLaunchBotBandFiresDownward)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 1;
	rubber->restY = 300.0f;

	Vec2F v = rubber->ComputeLaunch(Vec2F(0.0f, 360.0f), 4.0f, 4000.0f);
	EXPECT_LT(v.y, 0.0f); // forward for the bot side is downward
}

TEST(SlingRubber, ComputeLaunchClampsToMaxSpeed)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 0;
	rubber->restY = -300.0f;

	Vec2F v = rubber->ComputeLaunch(Vec2F(0.0f, -600.0f), 10.0f, 500.0f);
	EXPECT_NEAR(v.Length(), 500.0f, 1e-2f);
}

TEST(SlingBoard, ClampInsideKeepsChipWithinWalls)
{
	auto board = mmake<SlingBoard>();
	board->halfWidth = 200.0f;
	board->halfHeight = 300.0f;

	Vec2F c = board->ClampInside(Vec2F(250.0f, -400.0f), 30.0f);
	EXPECT_FLOAT_EQ(c.x, 170.0f);
	EXPECT_FLOAT_EQ(c.y, -270.0f);
}

TEST(SlingBoard, GetRubberForSideReturnsMatchingBand)
{
	auto board = mmake<SlingBoard>();

	auto playerBand = mmake<SlingRubber>();
	playerBand->side = 0;
	auto botBand = mmake<SlingRubber>();
	botBand->side = 1;
	board->RegisterRubber(playerBand);
	board->RegisterRubber(botBand);

	EXPECT_EQ(board->GetRubberForSide(0), playerBand);
	EXPECT_EQ(board->GetRubberForSide(1), botBand);
}

// ===== SlingBot =====

TEST(SlingBot, ChoosePuckPicksBotSidePuck)
{
	auto board = mmake<SlingBoard>();
	board->RegisterPuck(MakePuck(0, Vec2F(0.0f, -100.0f)));
	board->RegisterPuck(MakePuck(1, Vec2F(20.0f, 150.0f)));

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	auto chosen = bot->ChoosePuck();
	ASSERT_TRUE(chosen.IsValid());
	EXPECT_EQ(SlingBoard::SideOfPosition(chosen->position), 1);
}

TEST(SlingBot, ChoosePuckNullWhenNoBotSidePuck)
{
	auto board = mmake<SlingBoard>();
	board->RegisterPuck(MakePuck(0, Vec2F(0.0f, -100.0f)));

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	EXPECT_FALSE(bot->ChoosePuck().IsValid());
}

TEST(SlingBot, TakeTurnDrawsChipIntoBandThenFiresDownward)
{
	auto board = mmake<SlingBoard>();
	board->halfHeight = 378.0f;

	auto puck = MakePuck(1, Vec2F(0.0f, 150.0f));
	board->RegisterPuck(puck);

	auto rubber = mmake<SlingRubber>();
	rubber->side = 1;
	rubber->restY = 322.0f;
	board->RegisterRubber(rubber);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());
	bot->pullDuration = 0.4f;

	ASSERT_TRUE(bot->TakeTurn());
	EXPECT_TRUE(bot->IsPulling());
	EXPECT_TRUE(puck->held); // chip is being drawn back, not launched yet

	// partway through the draw the chip is behind the band (above restY) and still held
	bot->OnUpdate(0.3f);
	EXPECT_GT(puck->position.y, rubber->restY);
	EXPECT_TRUE(bot->IsPulling());

	// once the draw completes the band fires the chip downward
	bot->OnUpdate(0.3f);
	EXPECT_FALSE(bot->IsPulling());
	EXPECT_FALSE(puck->held);
	EXPECT_GT(puck->velocity.Length(), 0.0f);
	EXPECT_LT(puck->velocity.y, 0.0f);
}

TEST(SlingBot, TakeTurnIgnoredWhileAlreadyPulling)
{
	auto board = mmake<SlingBoard>();
	board->RegisterPuck(MakePuck(1, Vec2F(0.0f, 150.0f)));
	board->RegisterPuck(MakePuck(1, Vec2F(60.0f, 150.0f)));

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	EXPECT_TRUE(bot->TakeTurn());
	EXPECT_FALSE(bot->TakeTurn()); // a second call mid-draw does nothing
}

TEST(SlingBot, TakeTurnFailsWithoutBotPuck)
{
	auto board = mmake<SlingBoard>();
	board->RegisterPuck(MakePuck(0, Vec2F(0.0f, -100.0f)));

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	EXPECT_FALSE(bot->TakeTurn());
}

// ===== SlingGameController (real-time, simultaneous play) =====

TEST(SlingGameController, PlayerInputStaysEnabledWhilePlaying)
{
	auto board = mmake<SlingBoard>();
	board->RegisterPuck(MakePuck(0, Vec2F(0.0f, -100.0f)));
	board->RegisterPuck(MakePuck(1, Vec2F(0.0f, 100.0f)));

	auto controller = mmake<SlingGameController>();
	controller->board.Set(board.Get());
	controller->ResetGame();

	EXPECT_FALSE(controller->IsGameOver());
	EXPECT_TRUE(board->IsPlayerInputEnabled());

	controller->Step(kStep); // input is never taken away for a "bot turn"
	EXPECT_TRUE(board->IsPlayerInputEnabled());
}

TEST(SlingGameController, BotShootsEveryInterval)
{
	auto board = mmake<SlingBoard>();
	auto player = MakePuck(0, Vec2F(0.0f, -100.0f));
	auto botPuck = MakePuck(1, Vec2F(0.0f, 100.0f));
	board->RegisterPuck(player);
	board->RegisterPuck(botPuck);

	auto botAi = mmake<SlingBot>();
	botAi->board.Set(board.Get());

	auto controller = mmake<SlingGameController>();
	controller->board.Set(board.Get());
	controller->bot.Set(botAi.Get());
	controller->botInterval = 0.5f;
	controller->ResetGame();

	controller->Step(0.2f); // before the interval -> bot hasn't acted
	EXPECT_FALSE(botAi->IsPulling());

	controller->Step(0.4f); // crosses the interval -> bot starts drawing a chip into the band
	EXPECT_TRUE(botAi->IsPulling());
	EXPECT_TRUE(botPuck->held);
}

TEST(SlingGameController, DetectsPlayerWinWhenSideCleared)
{
	auto board = mmake<SlingBoard>();
	board->RegisterPuck(MakePuck(1, Vec2F(0.0f, 100.0f))); // player side already empty

	auto controller = mmake<SlingGameController>();
	controller->board.Set(board.Get());
	controller->ResetGame();

	controller->Step(kStep);

	EXPECT_TRUE(controller->IsGameOver());
	EXPECT_EQ(controller->GetWinner(), 0);
}

TEST(SlingGameController, DetectsBotWinWhenSideCleared)
{
	auto board = mmake<SlingBoard>();
	board->RegisterPuck(MakePuck(0, Vec2F(0.0f, -100.0f))); // bot side already empty

	auto controller = mmake<SlingGameController>();
	controller->board.Set(board.Get());
	controller->ResetGame();

	controller->Step(kStep);

	EXPECT_TRUE(controller->IsGameOver());
	EXPECT_EQ(controller->GetWinner(), 1);
}

// ===== Integration: board simulation + controller together =====

TEST(SlingGameIntegration, PlayerFlickThroughGapClearsSideAndWins)
{
	auto board = mmake<SlingBoard>();
	board->friction = 3.0f;        // damped so a flick settles where it lands
	board->wallRestitution = 0.2f;
	board->gapHalf = 85.0f;

	auto playerPuck = MakePuck(0, Vec2F(0.0f, -200.0f), 24.0f);
	auto botPuck = MakePuck(1, Vec2F(120.0f, 200.0f), 24.0f);
	board->RegisterPuck(playerPuck);
	board->RegisterPuck(botPuck);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	auto controller = mmake<SlingGameController>();
	controller->board.Set(board.Get());
	controller->bot.Set(bot.Get());
	controller->botInterval = 1000.0f; // keep the bot out of this deterministic check
	controller->ResetGame();

	// Player flicks its puck straight up through the central gap onto the bot's side.
	playerPuck->velocity = Vec2F(0.0f, 900.0f);

	int guard = 0;
	while (!controller->IsGameOver() && guard++ < 4000)
	{
		board->StepSimulation(kStep);
		controller->Step(kStep);
	}

	EXPECT_TRUE(controller->IsGameOver());
	EXPECT_EQ(controller->GetWinner(), 0);    // player cleared its side first
	EXPECT_GT(playerPuck->position.y, 0.0f);  // crossed to the bot's side
}
