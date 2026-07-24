#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/ChipColors.h"
#include "Level/LevelData.h"
#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"

using namespace o2;

TEST(LevelDataTests, SerializationRoundtrip)
{
	LevelData level;
	level.name = "Test";
	level.border = { Vec2F(-100.0f, -100.0f), Vec2F(100.0f, -100.0f), Vec2F(0.0f, 100.0f) };

	LevelWall wall;
	wall.points = { Vec2F(-50.0f, 0.0f), Vec2F(50.0f, 10.0f) };
	wall.width = 42.0f;
	wall.closed = true;
	level.walls.Add(wall);

	LevelSpawnPoint spawnPoint;
	spawnPoint.position = Vec2F(0.0f, 90.0f);
	spawnPoint.zoneSize = Vec2F(150.0f, 30.0f);
	spawnPoint.colors = { "Green", "Blue" };
	spawnPoint.maxOnScreen = 7;
	spawnPoint.spawnDelay = 0.5f;
	level.spawners.Add(spawnPoint);

	LevelGoal goal;
	goal.chipType = "Green";
	goal.count = 80;
	level.goals.Add(goal);

	level.moves = 33;

	DataDocument data;
	level.Serialize(data);

	LevelData restored;
	restored.Deserialize(data);

	EXPECT_EQ(restored, level);
}

TEST(LevelDataTests, ParsesHandWrittenJson)
{
	const char* json = R"({
		"name": "Json",
		"border": [ {"x": -10, "y": -20}, {"x": 10, "y": -20}, {"x": 0, "y": 20} ],
		"walls": [ { "points": [ {"x": 0, "y": 0}, {"x": 5, "y": 5} ], "width": 30, "closed": false } ],
		"spawners": [ { "position": {"x": 0, "y": 15}, "zoneSize": {"x": 10, "y": 4},
						"colors": ["Red"], "maxOnScreen": 3, "spawnDelay": 0.25 } ],
		"goals": [ { "chipType": "Red", "count": 5 } ],
		"moves": 25
	})";

	DataDocument data;
	ASSERT_TRUE(data.LoadFromData(json));

	LevelData level;
	level.Deserialize(data);

	EXPECT_EQ(level.name, "Json");
	ASSERT_EQ(level.border.Count(), 3);
	EXPECT_NEAR(level.border[2].y, 20.0f, 0.001f);
	ASSERT_EQ(level.walls.Count(), 1);
	EXPECT_NEAR(level.walls[0].width, 30.0f, 0.001f);
	ASSERT_EQ(level.spawners.Count(), 1);
	EXPECT_EQ(level.spawners[0].colors, Vector<String>({ "Red" }));
	EXPECT_EQ(level.spawners[0].maxOnScreen, 3);
	ASSERT_EQ(level.goals.Count(), 1);
	EXPECT_EQ(level.goals[0].chipType, "Red");
	EXPECT_EQ(level.goals[0].count, 5);
	EXPECT_EQ(level.moves, 25);
}

TEST(LevelDataTests, MovesDefaultToUnlimited)
{
	const char* json = R"({
		"name": "NoMoves",
		"border": [ {"x": -10, "y": -20}, {"x": 10, "y": -20}, {"x": 0, "y": 20} ]
	})";

	DataDocument data;
	ASSERT_TRUE(data.LoadFromData(json));

	LevelData level;
	level.Deserialize(data);

	EXPECT_EQ(level.moves, 0);
}

TEST(LevelDataTests, GoalsAreClampedToMax)
{
	LevelData level;
	for (int i = 0; i < LevelData::kMaxGoals + 2; i++)
	{
		LevelGoal goal;
		goal.chipType = "Green";
		goal.count = i + 1;
		level.goals.Add(goal);
	}

	DataDocument data;
	level.Serialize(data);

	LevelData restored;
	restored.Deserialize(data);

	EXPECT_EQ(restored.goals.Count(), LevelData::kMaxGoals);
}

TEST(LevelDataTests, LoadFromMissingAssetFails)
{
	LevelData level;
	EXPECT_FALSE(level.LoadFromAsset("Levels/NoSuchLevel.json"));
	EXPECT_TRUE(level.border.IsEmpty());
}

// The whole shipped chain must load: every level parses, has a closed border,
// known spawner colors and 1..4 goals with positive counts
TEST(LevelDataTests, AllChainLevelsAreValid)
{
	ASSERT_TRUE(LevelChain::Load());
	ASSERT_EQ(LevelChain::Count(), 10);

	UserDataModel::SetCurrentLevel(0, LevelChain::Count());
	for (int i = 0; i < LevelChain::Count(); i++)
	{
		String path = LevelChain::LevelPath(UserDataModel::Get().currentLevel);

		LevelData level;
		ASSERT_TRUE(level.LoadFromAsset(path)) << "level " << path.Data();

		EXPECT_GE(level.border.Count(), 3) << "level " << path.Data();
		EXPECT_FALSE(level.name.IsEmpty()) << "level " << path.Data();

		ASSERT_GE(level.goals.Count(), 1) << "level " << path.Data();
		ASSERT_LE(level.goals.Count(), LevelData::kMaxGoals) << "level " << path.Data();

		ASSERT_GE(level.spawners.Count(), 1) << "level " << path.Data();

		Vector<String> spawnableColors;
		for (auto& spawnPoint : level.spawners)
		{
			EXPECT_GE(spawnPoint.maxOnScreen, 1) << "level " << path.Data();
			for (auto& color : spawnPoint.colors)
			{
				EXPECT_TRUE(ChipColors::IsKnownColor(color)) << "level " << path.Data() << " color " << color.Data();
				if (!spawnableColors.Contains(color))
					spawnableColors.Add(color);
			}
		}

		// Every goal color must actually spawn somewhere, or the level can't be completed
		for (auto& goal : level.goals)
		{
			EXPECT_GT(goal.count, 0) << "level " << path.Data();
			EXPECT_TRUE(spawnableColors.Contains(goal.chipType))
				<< "level " << path.Data() << " goal color " << goal.chipType.Data();
		}

		EXPECT_GT(level.moves, 0) << "level " << path.Data();

		UserDataModel::AdvanceLevel(LevelChain::Count());
	}

	// The chain wraps to the first level
	EXPECT_EQ(UserDataModel::Get().currentLevel, 0);

	UserDataModel::Reset();
	LevelChain::Reset();
}
