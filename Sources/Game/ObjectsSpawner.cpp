#include "o2/stdafx.h"
#include "ObjectsSpawner.h"

#include "ObjectChip.h"
#include "o2/Scene/Actor.h"

void ObjectsSpawnerComponent::OnUpdate(float dt)
{
	mTimer += dt;
	if (mTimer > mSpawnDelay)
	{
		mTimer = 0.0f;
		TrySpawn();
	}
}

void ObjectsSpawnerComponent::TrySpawn()
{
	if (!mSpawnContainer || !mSpawnZone || mObjectProtos.IsEmpty())
		return;

	int currentCount = mSpawnContainer->GetChildren().Count([](auto& actor) {
		return actor->template GetComponent<ObjectChipComponent>() != nullptr;
	});

	if (currentCount >= mMaxCount)
		return;

	auto& proto = mObjectProtos[Math::Random(0, mObjectProtos.Count() - 1)];
	if (!proto)
		return;

	RectF spawnZone = mSpawnZone->transform->worldRect;

	auto newObject = proto->Instantiate();
	newObject->transform->position2D = Vec2F(Math::Random(spawnZone.left, spawnZone.right),
											 Math::Random(spawnZone.bottom, spawnZone.top));
	newObject->transform->angleDegrees = Math::Random(-30.0f, 30.0f);
	newObject->SetParent(mSpawnContainer);
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<ObjectsSpawnerComponent>);
// --- META ---

DECLARE_CLASS(ObjectsSpawnerComponent, ObjectsSpawnerComponent);
// --- END META ---
