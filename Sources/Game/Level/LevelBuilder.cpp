#include "o2/stdafx.h"
#include "Level/LevelBuilder.h"

#include "Level/GameFieldBorder.h"
#include "Level/LevelChipSpawner.h"
#include "Level/LevelController.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Physics/RigidBody.h"
#include "o2/Scene/Physics/SplineMeshCollider.h"
#include "o2/Scene/Scene.h"

namespace
{
	void SetSplinePoints(const Ref<Spline>& spline, const Vector<Vec2F>& points, bool closed)
	{
		spline->BeginKeysBatchChange();
		spline->RemoveAllKeys();
		for (auto& point : points)
			spline->AppendKey(point, 0.0f, Vec2F(), Vec2F());
		spline->CompleteKeysBatchingChange();
		spline->SetClosed(closed);
	}
}

Ref<Actor> BuildLevel(const LevelData& data)
{
	auto root = mmake<Actor>(ActorCreateMode::InScene);
	root->SetName("Level");
	root->transform->SetPosition2D(Vec2F());

	auto controller = root->AddComponent<LevelController>();
	controller->SetGoals(data.goals);
	controller->SetMoves(data.moves);

	if (data.border.Count() >= 3)
	{
		auto field = mmake<RigidBody>();
		field->SetName("Field");
		field->SetBodyType(RigidBody::Type::Static);

		auto border = mmake<GameFieldBorder>();
		field->AddComponent(border);
		SetSplinePoints(border->spline, data.border, true);

		field->SetParent(root);
	}

	int wallIndex = 0;
	for (auto& wall : data.walls)
	{
		if (wall.points.Count() < 2)
			continue;

		auto wallActor = mmake<RigidBody>();
		wallActor->SetName(String("Wall") + (String)wallIndex);
		wallActor->SetBodyType(RigidBody::Type::Static);

		auto collider = mmake<SplineMeshCollider>();
		wallActor->AddComponent(collider);

		// Atlas image loading touches the render device, so the strip is drawing-only
		if (Render::IsSingletonInitialzed())
			collider->SetImage(AssetRef<ImageAsset>("Game field/FieldBorderTile.png"));

		collider->SetWidth(wall.width);
		collider->SetIsLoop(wall.closed);
		SetSplinePoints(collider->spline, wall.points, wall.closed);

		wallActor->SetParent(root);
		wallIndex++;
	}

	auto chipsContainer = mmake<Actor>(ActorCreateMode::InScene);
	chipsContainer->SetName("Chips");
	chipsContainer->transform->SetPosition2D(Vec2F());
	chipsContainer->SetParent(root);

	int spawnerIndex = 0;
	for (auto& spawnPoint : data.spawners)
	{
		auto spawnerActor = mmake<Actor>(ActorCreateMode::InScene);
		spawnerActor->SetName(String("Spawner") + (String)spawnerIndex);
		spawnerActor->transform->SetSize2D(spawnPoint.zoneSize);
		spawnerActor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		spawnerActor->transform->SetPosition2D(spawnPoint.position);

		auto spawner = spawnerActor->AddComponent<LevelChipSpawner>();
		spawner->SetColors(spawnPoint.colors);
		spawner->SetMaxOnScreen(spawnPoint.maxOnScreen);
		spawner->SetSpawnDelay(spawnPoint.spawnDelay);
		spawner->SetContainer(chipsContainer);

		spawnerActor->SetParent(root);
		spawnerIndex++;
	}

	return root;
}
