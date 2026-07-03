#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Utils/Math/Math.h"
#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"
#include "SlingGameFlow.h"
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

TEST(SlingPuck, HighlightTurnsBakedSheenTowardLight)
{
	// Light already along the baked 45 degree direction -> no rotation
	EXPECT_NEAR(SlingPuck::HighlightAngle(Vec2F(), Vec2F(100.0f, 100.0f), 45.0f), 0.0f, 1e-4f);

	// Light straight up (90 degrees) while the art is baked at 45 -> turn +45 (CCW)
	EXPECT_NEAR(SlingPuck::HighlightAngle(Vec2F(), Vec2F(0.0f, 100.0f), 45.0f), Math::Deg2rad(45.0f), 1e-4f);

	// Light to the right (0 degrees) -> turn -45 (CW)
	EXPECT_NEAR(SlingPuck::HighlightAngle(Vec2F(), Vec2F(100.0f, 0.0f), 45.0f), Math::Deg2rad(-45.0f), 1e-4f);

	// A chip left of the light faces more to the right than one sitting under the light
	float left  = SlingPuck::HighlightAngle(Vec2F(-100.0f, 0.0f), Vec2F(100.0f, 100.0f), 45.0f);
	float under = SlingPuck::HighlightAngle(Vec2F(100.0f, 0.0f), Vec2F(100.0f, 100.0f), 45.0f);
	EXPECT_LT(left, under);
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

TEST(SlingRubber, BandPathIsStraightLineWhenIdle)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 0;
	rubber->restY = -300.0f;
	rubber->halfSpan = 200.0f;
	rubber->minStretch = 6.0f;

	// grip in front of the band -> not stretched -> straight band between the posts
	auto path = rubber->BuildBandPath(Vec2F(0.0f, -300.0f), 36.0f, 24);
	ASSERT_EQ(path.Count(), 2);
	EXPECT_NEAR(path[0].x, -200.0f, 1e-3f);
	EXPECT_NEAR(path[1].x, 200.0f, 1e-3f);
}

TEST(SlingRubber, BandPathWrapsChipOnBackSideWithoutCrossing)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 0;
	rubber->restY = -300.0f;
	rubber->halfSpan = 200.0f;
	rubber->minStretch = 6.0f;

	Vec2F grip(20.0f, -380.0f); // pulled 80 behind the band
	const float radius = 36.0f;
	auto path = rubber->BuildBandPath(grip, radius, 12);

	ASSERT_GT(path.Count(), 3);             // posts + tangents + arc samples
	EXPECT_NEAR(path[0].x, -200.0f, 1e-3f); // anchored at the left post
	EXPECT_NEAR(path[0].y, -300.0f, 1e-3f);
	EXPECT_NEAR(path[path.Count() - 1].x, 200.0f, 1e-3f); // and the right post
	EXPECT_NEAR(path[path.Count() - 1].y, -300.0f, 1e-3f);

	for (int i = 1; i < path.Count() - 1; i++)
	{
		EXPECT_NEAR((path[i] - grip).Length(), radius, 1e-2f); // hugs the chip circle
		EXPECT_LE(path[i].y, grip.y + 1e-3f);                  // on the back (-y) side, away from field
	}

	// left post connects to the left of the chip, right post to the right: the legs don't cross
	EXPECT_LT(path[1].x, grip.x);                     // first tangent (from left post) is left of centre
	EXPECT_GT(path[path.Count() - 2].x, grip.x);      // last tangent (from right post) is right of centre
}

TEST(SlingRubber, BandPathBotWrapsOnBackSide)
{
	auto rubber = mmake<SlingRubber>();
	rubber->side = 1;
	rubber->restY = 300.0f;
	rubber->halfSpan = 200.0f;
	rubber->minStretch = 6.0f;

	Vec2F grip(0.0f, 380.0f);
	const float radius = 36.0f;
	auto path = rubber->BuildBandPath(grip, radius, 12);

	ASSERT_GT(path.Count(), 3);
	for (int i = 1; i < path.Count() - 1; i++)
	{
		EXPECT_NEAR((path[i] - grip).Length(), radius, 1e-2f);
		EXPECT_GE(path[i].y, grip.y - 1e-3f); // bot wraps the +y (back) side, away from field
	}

	EXPECT_LT(path[1].x, grip.x);                // no leg crossing
	EXPECT_GT(path[path.Count() - 2].x, grip.x);
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

	// partway through the draw the chip is moving back toward the band and still held
	bot->OnUpdate(0.3f);
	EXPECT_GT(puck->position.y, 150.0f);
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

TEST(SlingBot, PullDepthStaysInsideBoard)
{
	auto board = mmake<SlingBoard>();
	board->halfHeight = 378.0f;

	auto puck = MakePuck(1, Vec2F(0.0f, 150.0f)); // dragPower 9 asks for a ~70-115 deep pull
	board->RegisterPuck(puck);

	auto rubber = mmake<SlingRubber>();
	rubber->side = 1;
	rubber->restY = 322.0f;
	board->RegisterRubber(rubber);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	ASSERT_TRUE(bot->TakeTurn());
	bot->OnUpdate(bot->pullDuration + 0.01f); // full draw and release

	// the launch depth was clamped to the room between the band and the back wall
	float maxDepth = board->halfHeight - puck->radius - rubber->restY;
	EXPECT_LE(Math::Abs(puck->velocity.y), maxDepth * puck->dragPower + 1.0f);
}

TEST(SlingBot, ShotIntervalScalesWithDifficulty)
{
	EXPECT_FLOAT_EQ(SlingBot::ShotIntervalFor(0.0f), 3.0f);
	EXPECT_FLOAT_EQ(SlingBot::ShotIntervalFor(100.0f), 0.2f);
	EXPECT_LT(SlingBot::ShotIntervalFor(50.0f), SlingBot::ShotIntervalFor(0.0f));
	EXPECT_GT(SlingBot::ShotIntervalFor(50.0f), SlingBot::ShotIntervalFor(100.0f));

	// out-of-range difficulties clamp
	EXPECT_FLOAT_EQ(SlingBot::ShotIntervalFor(-10.0f), 3.0f);
	EXPECT_FLOAT_EQ(SlingBot::ShotIntervalFor(200.0f), 0.2f);

	auto bot = mmake<SlingBot>();
	bot->difficulty = 100.0f;
	EXPECT_FLOAT_EQ(bot->GetShotInterval(), 0.2f);
}

TEST(SlingBot, MissChanceFallsWithDifficulty)
{
	EXPECT_GT(SlingBot::MissChanceFor(0.0f), 0.5f);
	EXPECT_LT(SlingBot::MissChanceFor(100.0f), 0.1f);
	EXPECT_GT(SlingBot::MissChanceFor(0.0f), SlingBot::MissChanceFor(50.0f));
	EXPECT_GT(SlingBot::MissChanceFor(50.0f), SlingBot::MissChanceFor(100.0f));
}

// Drives the bot through a full grab-and-release so the next ChoosePuck sees it as "just shot"
static void ShootOnce(const Ref<SlingBot>& bot)
{
	ASSERT_TRUE(bot->TakeTurn());
	bot->OnUpdate(bot->pullDuration + 0.01f);
	ASSERT_FALSE(bot->IsPulling());
}

TEST(SlingBot, ChoosePuckSkipsJustShotChip)
{
	auto board = mmake<SlingBoard>();
	auto closest = MakePuck(1, Vec2F(0.0f, 100.0f));
	auto other = MakePuck(1, Vec2F(100.0f, 200.0f));
	board->RegisterPuck(closest);
	board->RegisterPuck(other);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	ShootOnce(bot); // picks `closest`
	closest->velocity = Vec2F();
	closest->position = Vec2F(0.0f, 100.0f); // shot chip settled back, again the closest
	other->velocity = Vec2F();

	// a settled chip isn't regrabbed right after its shot — the other one goes first
	EXPECT_EQ(bot->ChoosePuck(), other);
}

TEST(SlingBot, ChoosePuckIgnoresFlyingChips)
{
	auto board = mmake<SlingBoard>();
	auto flying = MakePuck(1, Vec2F(0.0f, 100.0f));
	flying->velocity = Vec2F(0.0f, -700.0f);
	board->RegisterPuck(flying);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	// the only chip is mid-flight: the bot waits instead of snatching its own shot
	EXPECT_FALSE(bot->ChoosePuck().IsValid());
	EXPECT_FALSE(bot->TakeTurn());

	flying->velocity = Vec2F();
	EXPECT_EQ(bot->ChoosePuck(), flying);
}

TEST(SlingBot, ChoosePuckTakesJustShotChipWhenAlone)
{
	auto board = mmake<SlingBoard>();
	auto only = MakePuck(1, Vec2F(0.0f, 100.0f));
	board->RegisterPuck(only);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	ShootOnce(bot);
	only->velocity = Vec2F();

	EXPECT_EQ(bot->ChoosePuck(), only);
}

TEST(SlingBot, ChoosePuckPrefersRestingChips)
{
	auto board = mmake<SlingBoard>();
	auto moving = MakePuck(1, Vec2F(0.0f, 100.0f));
	auto resting = MakePuck(1, Vec2F(150.0f, 250.0f));
	moving->velocity = Vec2F(500.0f, 0.0f);
	board->RegisterPuck(moving);
	board->RegisterPuck(resting);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	EXPECT_EQ(bot->ChoosePuck(), resting); // the closer chip is still sliding
}

TEST(SlingBot, ChoosePuckPrefersLeastRecentlyGrabbed)
{
	auto board = mmake<SlingBoard>();
	auto a = MakePuck(1, Vec2F(0.0f, 100.0f));
	auto b = MakePuck(1, Vec2F(30.0f, 120.0f));
	auto c = MakePuck(1, Vec2F(60.0f, 140.0f));
	board->RegisterPuck(a);
	board->RegisterPuck(b);
	board->RegisterPuck(c);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	auto settle = [&] {
		for (auto& puck : board->GetPucks())
			puck->velocity = Vec2F();
	};

	ShootOnce(bot); settle(); // grabs a (closest)
	ShootOnce(bot); settle(); // a excluded as just shot; grabs b (never grabbed, closer than c)

	// b is excluded as just shot; c has never been grabbed while a has -> c goes first
	EXPECT_EQ(bot->ChoosePuck(), c);
}

// ===== SlingBoard shot simulation (collision-aware trajectory) =====

TEST(SlingBoardSimulate, ShotThroughOpenGapCrosses)
{
	auto board = mmake<SlingBoard>();
	auto shooter = MakePuck(1, Vec2F(0.0f, 200.0f));
	board->RegisterPuck(shooter);

	Vec2F end = board->SimulateShot(shooter, Vec2F(0.0f, 344.0f), Vec2F(0.0f, -900.0f));
	EXPECT_LT(end.y, 0.0f); // straight down the middle, through the gap
}

TEST(SlingBoardSimulate, ShotBlockedByChipParkedInGap)
{
	auto board = mmake<SlingBoard>();
	auto shooter = MakePuck(1, Vec2F(0.0f, 200.0f));
	auto blocker = MakePuck(1, Vec2F(0.0f, 40.0f)); // plugs the central gap
	board->RegisterPuck(shooter);
	board->RegisterPuck(blocker);

	Vec2F end = board->SimulateShot(shooter, Vec2F(0.0f, 344.0f), Vec2F(0.0f, -900.0f));
	EXPECT_GT(end.y, 0.0f); // rams the parked chip and stays on the bot side
}

TEST(SlingBoardSimulate, DoesNotTouchRealPucks)
{
	auto board = mmake<SlingBoard>();
	auto shooter = MakePuck(1, Vec2F(0.0f, 200.0f));
	auto other = MakePuck(1, Vec2F(50.0f, 100.0f));
	board->RegisterPuck(shooter);
	board->RegisterPuck(other);

	board->SimulateShot(shooter, Vec2F(0.0f, 344.0f), Vec2F(0.0f, -900.0f));

	EXPECT_EQ(shooter->position, Vec2F(0.0f, 200.0f));
	EXPECT_EQ(shooter->velocity, Vec2F());
	EXPECT_EQ(other->position, Vec2F(50.0f, 100.0f));
}

TEST(SlingBot, PlanPullXFindsCrossingShotOnClearBoard)
{
	auto board = mmake<SlingBoard>();
	auto shooter = MakePuck(1, Vec2F(0.0f, 200.0f));
	board->RegisterPuck(shooter);

	auto rubber = mmake<SlingRubber>();
	rubber->side = 1;
	rubber->restY = 344.0f;
	board->RegisterRubber(rubber);

	auto bot = mmake<SlingBot>();
	bot->board.Set(board.Get());

	float depth = 100.0f;
	float pullX = bot->PlanPullX(shooter, depth);

	Vec2F pull(pullX, rubber->restY + depth);
	Vec2F launch = rubber->ComputeLaunch(pull, shooter->dragPower, shooter->maxLaunchSpeed);
	Vec2F end = board->SimulateShot(shooter, pull, launch);
	EXPECT_LT(end.y, 0.0f); // the planned shot really crosses to the player side
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
	botAi->difficulty = 100.0f; // hardest -> shoots every 0.2 s

	auto controller = mmake<SlingGameController>();
	controller->board.Set(board.Get());
	controller->bot.Set(botAi.Get());
	controller->ResetGame();

	controller->Step(0.1f); // before the interval -> bot hasn't acted
	EXPECT_FALSE(botAi->IsPulling());

	controller->Step(0.15f); // crosses the interval -> bot starts drawing a chip into the band
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
	bot->difficulty = 0.0f; // slowest bot (3 s interval) stays out of this fast deterministic check

	auto controller = mmake<SlingGameController>();
	controller->board.Set(board.Get());
	controller->bot.Set(bot.Get());
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

// ===== SlingGameFlow (meta-loop: difficulty ladder and result windows) =====

namespace
{
	struct FlowRig
	{
		Ref<SlingBoard>          board;
		Ref<SlingBot>            bot;
		Ref<SlingGameController> controller;
		Ref<SlingGameFlow>       flow;
		Ref<Actor>               victoryWindow;
		Ref<Actor>               gameOverWindow;
		Ref<SlingPuck>           playerPuck;
		Ref<SlingPuck>           botPuck;

		FlowRig()
		{
			board = mmake<SlingBoard>();
			playerPuck = MakePuck(0, Vec2F(0.0f, -100.0f));
			botPuck = MakePuck(1, Vec2F(50.0f, 100.0f));
			board->RegisterPuck(playerPuck);
			board->RegisterPuck(botPuck);

			bot = mmake<SlingBot>();
			bot->board.Set(board.Get());

			controller = mmake<SlingGameController>();
			controller->board.Set(board.Get());
			controller->bot.Set(bot.Get());
			controller->ResetGame();

			victoryWindow = mmake<Actor>();
			gameOverWindow = mmake<Actor>();

			flow = mmake<SlingGameFlow>();
			flow->board.Set(board.Get());
			flow->bot.Set(bot.Get());
			flow->controller.Set(controller.Get());
			flow->victoryWindow.Set(victoryWindow.Get());
			flow->gameOverWindow.Set(gameOverWindow.Get());
			flow->OnStart();
		}

		// Clears the given side and steps the game once so the controller declares the winner
		void FinishRound(int clearedSide)
		{
			auto& puck = clearedSide == 0 ? playerPuck : botPuck;
			puck->position.y = clearedSide == 0 ? 100.0f : -100.0f;
			controller->Step(kStep);
			flow->OnUpdate(kStep);
		}
	};
}

TEST(SlingGameFlow, StartsAtDifficulty10)
{
	FlowRig rig;
	EXPECT_FLOAT_EQ(rig.flow->GetDifficulty(), 10.0f);
	EXPECT_FLOAT_EQ(rig.bot->difficulty, 10.0f);
	EXPECT_FALSE(rig.flow->IsWindowShown());
	EXPECT_FALSE(rig.victoryWindow->IsEnabled());
	EXPECT_FALSE(rig.gameOverWindow->IsEnabled());
}

TEST(SlingGameFlow, WinShowsVictoryAndNextLevelAddsTen)
{
	FlowRig rig;
	rig.FinishRound(0);

	EXPECT_TRUE(rig.flow->IsWindowShown());
	EXPECT_TRUE(rig.victoryWindow->IsEnabled());
	EXPECT_FALSE(rig.gameOverWindow->IsEnabled());
	EXPECT_FALSE(rig.board->IsPlayerInputEnabled());

	rig.flow->OnNextLevel();

	EXPECT_FLOAT_EQ(rig.flow->GetDifficulty(), 20.0f);
	EXPECT_FLOAT_EQ(rig.bot->difficulty, 20.0f);
	EXPECT_FALSE(rig.flow->IsWindowShown());
	EXPECT_FALSE(rig.victoryWindow->IsEnabled());
	EXPECT_FALSE(rig.controller->IsGameOver());
	EXPECT_TRUE(rig.board->IsPlayerInputEnabled());
}

TEST(SlingGameFlow, LossShowsGameOverAndRetryDropsToStart)
{
	FlowRig rig;
	rig.flow->StartLevel(30.0f);
	rig.FinishRound(1);

	EXPECT_TRUE(rig.gameOverWindow->IsEnabled());
	EXPECT_FALSE(rig.victoryWindow->IsEnabled());

	rig.flow->OnRetry();

	EXPECT_FLOAT_EQ(rig.flow->GetDifficulty(), 10.0f);
	EXPECT_FLOAT_EQ(rig.bot->difficulty, 10.0f);
	EXPECT_FALSE(rig.gameOverWindow->IsEnabled());
	EXPECT_FALSE(rig.controller->IsGameOver());
}

TEST(SlingGameFlow, WatchAdContinuesSameDifficulty)
{
	FlowRig rig;
	rig.flow->StartLevel(30.0f);
	rig.FinishRound(1);

	rig.flow->OnContinueSameLevel();

	EXPECT_FLOAT_EQ(rig.flow->GetDifficulty(), 30.0f);
	EXPECT_FLOAT_EQ(rig.bot->difficulty, 30.0f);
	EXPECT_FALSE(rig.gameOverWindow->IsEnabled());
}

TEST(SlingGameFlow, DifficultyCapsAtHundred)
{
	FlowRig rig;
	rig.flow->StartLevel(95.0f);
	rig.flow->OnNextLevel();
	EXPECT_FLOAT_EQ(rig.flow->GetDifficulty(), 100.0f);
	rig.flow->OnNextLevel();
	EXPECT_FLOAT_EQ(rig.flow->GetDifficulty(), 100.0f);
}

TEST(SlingGameFlow, StartLevelResetsChipsToSpawns)
{
	FlowRig rig;

	rig.playerPuck->position = Vec2F(90.0f, 200.0f);
	rig.playerPuck->velocity = Vec2F(300.0f, 0.0f);
	rig.playerPuck->held = true;
	rig.botPuck->position = Vec2F(-10.0f, -50.0f);

	rig.flow->StartLevel(10.0f);

	EXPECT_EQ(rig.playerPuck->position, Vec2F(0.0f, -100.0f));
	EXPECT_EQ(rig.playerPuck->velocity, Vec2F());
	EXPECT_FALSE(rig.playerPuck->held);
	EXPECT_EQ(rig.botPuck->position, Vec2F(50.0f, 100.0f));
}

TEST(SlingGameFlow, WindowShownOnlyOncePerRound)
{
	FlowRig rig;
	rig.FinishRound(0);

	rig.victoryWindow->SetEnabled(false); // pretend something hid it manually
	rig.flow->OnUpdate(kStep);            // the flow must not pop it again within the same round

	EXPECT_FALSE(rig.victoryWindow->IsEnabled());
}
