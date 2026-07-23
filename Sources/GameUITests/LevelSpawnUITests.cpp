#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Chip.h"
#include "Level/LevelBuilder.h"
#include "Level/LevelChipSpawner.h"
#include "Level/LevelController.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
	LevelData MakeSpawnLevel()
	{
		LevelData data;
		data.name = "SpawnTest";
		data.border = { Vec2F(-500.0f, -700.0f), Vec2F(500.0f, -700.0f),
						Vec2F(500.0f, 500.0f), Vec2F(-500.0f, 500.0f) };

		LevelSpawnPoint spawnPoint;
		spawnPoint.position = Vec2F(0.0f, 400.0f);
		spawnPoint.zoneSize = Vec2F(800.0f, 100.0f);
		spawnPoint.colors = { "Green", "Blue" };
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

class LevelSpawnUI: public ::testing::Test
{
protected:
	Ref<CameraActor> camera;
	Ref<Actor>       root;

	void SetUp() override
	{
		camera = mmake<CameraActor>();
		camera->SetFittedSize(Vec2F(1200.0f, 1600.0f));
		camera->AddToScene();

		root = BuildLevel(MakeSpawnLevel());
		AppTestDriver::PumpFrames(5);
	}

	void TearDown() override
	{
		if (root)
			root->Destroy();
		if (camera)
			camera->Destroy();

		AppTestDriver::PumpFrames(2);
	}
};

TEST_F(LevelSpawnUI, SpawnerFillsContainerUpToLimit)
{
	auto spawner = root->FindChild("Spawner0")->GetComponent<LevelChipSpawner>();
	auto chips = root->FindChild("Chips");
	ASSERT_TRUE(spawner);
	ASSERT_TRUE(chips);

	AppTestDriver::Wait(1.5f);

	EXPECT_EQ(chips->GetChildren().Count(), 3);
	EXPECT_EQ(spawner->GetAliveCount(), 3);

	// Spawned chips carry the Chip component with a color from the spawner list
	for (auto& chipActor : chips->GetChildren())
	{
		auto chip = chipActor->GetComponent<Chip>();
		ASSERT_TRUE(chip);
		EXPECT_TRUE(spawner->GetColors().Contains(chip->GetColorType()));
	}
}

TEST_F(LevelSpawnUI, PoppedGroupReportsToController)
{
	auto controller = root->GetComponent<LevelController>();
	ASSERT_TRUE(controller);

	auto chips = root->FindChild("Chips");
	AppTestDriver::Wait(2.0f); // let the field fill and the chips settle

	// Find any pair of same-colored chips and drag them together to form a poppable group
	Ref<Chip> first, second;
	for (auto& actorA : chips->GetChildren())
	{
		auto chipA = actorA->GetComponent<Chip>();
		for (auto& actorB : chips->GetChildren())
		{
			if (actorA == actorB)
				continue;

			auto chipB = actorB->GetComponent<Chip>();
			if (chipB->GetColorType() == chipA->GetColorType())
			{
				first = chipA;
				second = chipB;
				break;
			}
		}
		if (first)
			break;
	}

	ASSERT_TRUE(first) << "expected at least one same-colored pair among spawned chips";

	second->GetActor()->transform->SetWorldPosition2D(
		first->GetActor()->transform->GetWorldPosition2D() + Vec2F(100.0f, 0.0f));
	AppTestDriver::PumpFrames(2);

	String color = first->GetColorType();
	int before = chips->GetChildren().Count();

	first->PopGroup();
	AppTestDriver::PumpFrames(3);

	EXPECT_LT(chips->GetChildren().Count(), before);

	bool colorTracked = false;
	auto& goals = controller->GetGoals();
	for (int i = 0; i < goals.Count(); i++)
	{
		if (goals[i].chipType == color)
		{
			colorTracked = true;
			EXPECT_GE(controller->GetCollected(i), 2);
		}
	}

	if (!colorTracked)
		SUCCEED() << "popped color is not a goal color, nothing to verify";
}
