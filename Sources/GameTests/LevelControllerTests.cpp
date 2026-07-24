#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/LevelController.h"
#include "Scene/SceneTestHelpers.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Scene.h"

using namespace o2;

namespace
{
	Vector<LevelGoal> MakeGoals(std::initializer_list<std::pair<const char*, int>> goals)
	{
		Vector<LevelGoal> result;
		for (auto& [type, count] : goals)
		{
			LevelGoal goal;
			goal.chipType = type;
			goal.count = count;
			result.Add(goal);
		}
		return result;
	}
}

TEST(LevelControllerTests, TracksMatchingColorOnly)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals(MakeGoals({ { "Green", 10 }, { "Red", 5 } }));

	controller->OnChipsPopped("Green", 3);
	controller->OnChipsPopped("Blue", 4);

	EXPECT_EQ(controller->GetCollected(0), 3);
	EXPECT_EQ(controller->GetCollected(1), 0);
	EXPECT_FALSE(controller->IsCompleted());
}

TEST(LevelControllerTests, CollectedIsClampedToGoal)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals(MakeGoals({ { "Green", 5 } }));

	controller->OnChipsPopped("Green", 100);
	EXPECT_EQ(controller->GetCollected(0), 5);
	EXPECT_TRUE(controller->IsCompleted());
}

TEST(LevelControllerTests, CompletionFiresOnceAfterAllGoals)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals(MakeGoals({ { "Green", 4 }, { "Red", 2 } }));

	int completions = 0;
	controller->onCompleted = [&] { completions++; };

	controller->OnChipsPopped("Green", 4);
	EXPECT_EQ(completions, 0);

	controller->OnChipsPopped("Red", 2);
	EXPECT_EQ(completions, 1);

	controller->OnChipsPopped("Red", 2);
	controller->OnChipsPopped("Green", 2);
	EXPECT_EQ(completions, 1);
}

TEST(LevelControllerTests, GoalsChangedNotifies)
{
	auto controller = mmake<LevelController>();

	int changes = 0;
	controller->onGoalsChanged = [&] { changes++; };

	controller->SetGoals(MakeGoals({ { "Green", 5 } }));
	EXPECT_EQ(changes, 1);

	controller->OnChipsPopped("Green", 2);
	EXPECT_EQ(changes, 2);

	// Non-matching pops don't notify
	controller->OnChipsPopped("Blue", 2);
	EXPECT_EQ(changes, 2);
}

TEST(LevelControllerTests, NoGoalsMeansNeverCompleted)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals({});
	EXPECT_FALSE(controller->IsCompleted());
}

TEST(LevelControllerTests, GoalsAreClampedToMax)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals(MakeGoals({ { "Green", 1 }, { "Red", 1 }, { "Blue", 1 },
									 { "Yellow", 1 }, { "Violet", 1 } }));
	EXPECT_EQ(controller->GetGoals().Count(), LevelData::kMaxGoals);
}

TEST(LevelControllerTests, MovesAreSpentPerPop)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals(MakeGoals({ { "Green", 100 } }));
	controller->SetMoves(3);

	EXPECT_TRUE(controller->HasMovesLimit());
	EXPECT_EQ(controller->GetMovesLimit(), 3);
	EXPECT_EQ(controller->GetMovesLeft(), 3);

	int movesChanges = 0;
	controller->onMovesChanged = [&] { movesChanges++; };

	// Any pop spends a move, matching the goals or not
	controller->OnChipsPopped("Green", 5);
	controller->OnChipsPopped("Blue", 2);

	EXPECT_EQ(controller->GetMovesLeft(), 1);
	EXPECT_EQ(movesChanges, 2);
}

TEST(LevelControllerTests, NoLimitMeansNoSpending)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals(MakeGoals({ { "Green", 100 } }));
	controller->SetMoves(0);

	int outOfMoves = 0;
	controller->onOutOfMoves = [&] { outOfMoves++; };

	for (int i = 0; i < 50; i++)
		controller->OnChipsPopped("Green", 1);

	EXPECT_FALSE(controller->HasMovesLimit());
	EXPECT_EQ(controller->GetMovesLeft(), 0);
	EXPECT_EQ(outOfMoves, 0);
}

TEST(LevelControllerTests, OutOfMovesFiresOnceAndRearmsOnAddMoves)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals(MakeGoals({ { "Green", 100 } }));
	controller->SetMoves(2);

	int outOfMoves = 0;
	controller->onOutOfMoves = [&] { outOfMoves++; };

	controller->OnChipsPopped("Blue", 2);
	EXPECT_EQ(outOfMoves, 0);

	controller->OnChipsPopped("Blue", 2);
	EXPECT_EQ(outOfMoves, 1);

	// Popping without moves left doesn't refire
	controller->OnChipsPopped("Blue", 2);
	EXPECT_EQ(outOfMoves, 1);

	controller->AddMoves(2);
	EXPECT_EQ(controller->GetMovesLeft(), 2);

	controller->OnChipsPopped("Blue", 2);
	controller->OnChipsPopped("Blue", 2);
	EXPECT_EQ(outOfMoves, 2);
}

TEST(LevelControllerTests, CompletionOnLastMoveWinsOverOutOfMoves)
{
	auto controller = mmake<LevelController>();
	controller->SetGoals(MakeGoals({ { "Green", 4 } }));
	controller->SetMoves(1);

	int completions = 0;
	int outOfMoves = 0;
	controller->onCompleted = [&] { completions++; };
	controller->onOutOfMoves = [&] { outOfMoves++; };

	controller->OnChipsPopped("Green", 4);

	EXPECT_EQ(controller->GetMovesLeft(), 0);
	EXPECT_EQ(completions, 1);
	EXPECT_EQ(outOfMoves, 0);
}

TEST(LevelControllerTests, FindForWalksUpParents)
{
	SceneCleanGuard guard;

	auto root = mmake<Actor>(ActorCreateMode::InScene);
	auto controller = root->AddComponent<LevelController>();

	auto container = mmake<Actor>(ActorCreateMode::InScene);
	container->SetParent(root);

	auto chip = mmake<Actor>(ActorCreateMode::InScene);
	chip->SetParent(container);

	TickFrame();

	EXPECT_EQ(LevelController::FindFor(chip), controller);
	EXPECT_EQ(LevelController::FindFor(root), controller);

	auto orphan = mmake<Actor>(ActorCreateMode::InScene);
	TickFrame();
	EXPECT_EQ(LevelController::FindFor(orphan), nullptr);
}
