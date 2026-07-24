#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/GameFieldBorder.h"
#include "Level/LevelBuilder.h"
#include "Level/LevelChipSpawner.h"
#include "Level/LevelController.h"
#include "Scene/SceneTestHelpers.h"
#include "o2/Scene/Physics/RigidBody.h"
#include "o2/Scene/Physics/SplineMeshCollider.h"
#include "o2/Scene/Scene.h"

using namespace o2;

namespace
{
	LevelData MakeTestLevel()
	{
		LevelData data;
		data.name = "BuilderTest";
		data.border = { Vec2F(-500.0f, -700.0f), Vec2F(500.0f, -700.0f),
						Vec2F(500.0f, 500.0f), Vec2F(-500.0f, 500.0f) };

		LevelWall shelf;
		shelf.points = { Vec2F(-500.0f, 0.0f), Vec2F(-100.0f, -50.0f) };
		shelf.width = 40.0f;
		data.walls.Add(shelf);

		LevelWall pyramid;
		pyramid.points = { Vec2F(0.0f, -300.0f), Vec2F(150.0f, -500.0f), Vec2F(-150.0f, -500.0f) };
		pyramid.closed = true;
		data.walls.Add(pyramid);

		LevelSpawnPoint spawnPoint;
		spawnPoint.position = Vec2F(0.0f, 400.0f);
		spawnPoint.zoneSize = Vec2F(800.0f, 100.0f);
		spawnPoint.colors = { "Green", "Blue", "NoSuchColor" };
		spawnPoint.maxOnScreen = 3;
		spawnPoint.spawnDelay = 0.05f;
		data.spawners.Add(spawnPoint);

		LevelGoal goal;
		goal.chipType = "Green";
		goal.count = 10;
		data.goals.Add(goal);

		return data;
	}
}

// Structure checks don't tick the scene: a scene update would run the spawners,
// and instantiating image-bearing chip prototypes crashes without the render device
TEST(LevelBuilderTests, BuildsFieldWallsSpawnersAndController)
{
	SceneCleanGuard guard;

	auto root = BuildLevel(MakeTestLevel());
	ASSERT_TRUE(root);
	o2Scene.UpdateAddedEntities();
	o2Scene.UpdateTransforms();

	auto controller = root->GetComponent<LevelController>();
	ASSERT_TRUE(controller);
	ASSERT_EQ(controller->GetGoals().Count(), 1);
	EXPECT_EQ(controller->GetGoals()[0].chipType, "Green");

	auto field = root->FindChild("Field");
	ASSERT_TRUE(field);
	auto border = field->GetComponent<GameFieldBorder>();
	ASSERT_TRUE(border);
	EXPECT_TRUE(border->IsLoop());
	EXPECT_EQ(border->spline->GetKeys().Count(), 4);
	EXPECT_EQ(DynamicCast<RigidBody>(field)->GetBodyType(), RigidBody::Type::Static);

	auto shelf = root->FindChild("Wall0");
	ASSERT_TRUE(shelf);
	auto shelfCollider = shelf->GetComponent<SplineMeshCollider>();
	ASSERT_TRUE(shelfCollider);
	EXPECT_FALSE(shelfCollider->IsLoop());
	EXPECT_EQ(shelfCollider->spline->GetKeys().Count(), 2);
	EXPECT_NEAR(shelfCollider->GetWidth(), 40.0f, 0.001f);

	auto pyramid = root->FindChild("Wall1");
	ASSERT_TRUE(pyramid);
	EXPECT_TRUE(pyramid->GetComponent<SplineMeshCollider>()->IsLoop());

	auto spawnerActor = root->FindChild("Spawner0");
	ASSERT_TRUE(spawnerActor);
	auto spawner = spawnerActor->GetComponent<LevelChipSpawner>();
	ASSERT_TRUE(spawner);
	EXPECT_EQ(spawner->GetMaxOnScreen(), 3);

	// Unknown colors are filtered out
	EXPECT_EQ(spawner->GetColors(), Vector<String>({ "Green", "Blue" }));

	auto chips = root->FindChild("Chips");
	ASSERT_TRUE(chips);
	EXPECT_EQ(spawner->GetContainer().Get(), chips.Get());
}

TEST(LevelBuilderTests, EmptyLevelBuildsBareRoot)
{
	SceneCleanGuard guard;

	auto root = BuildLevel(LevelData());
	ASSERT_TRUE(root);
	TickFrame();

	EXPECT_FALSE(root->FindChild("Field"));
	EXPECT_TRUE(root->GetComponent<LevelController>());
	EXPECT_TRUE(root->FindChild("Chips"));
}
