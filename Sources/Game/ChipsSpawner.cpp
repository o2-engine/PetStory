#include "o2/stdafx.h"
#include "ChipsSpawner.h"
#include "o2/Scene/Actor.h"

void ChipsSpawnerComponent::OnUpdate(float dt)
{
	mAccumulatedTimer += dt;
	if (mAccumulatedTimer > mSpawnDelay)
	{
		mAccumulatedTimer -= mSpawnDelay;
		CheckChipsCount();
	}
}

void ChipsSpawnerComponent::CheckChipsCount()
{
	if (!mSpawnContainer || !mSpawnZone || !mChipProto)
		return;

	int currentCount = mSpawnContainer->GetChildren().Count([&](auto& actor) {
		return actor->GetPrototype() == mChipProto;
	});

	if (currentCount >= mMaxChipsCount)
		return;

	RectF spawnZone = mSpawnZone->transform->worldRect;

	auto newChip = mChipProto->Instantiate();
	newChip->transform->position2D = Vec2F(Math::Random(spawnZone.left, spawnZone.right),
										 Math::Random(spawnZone.bottom, spawnZone.top));
	newChip->SetParent(mSpawnContainer);
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<ChipsSpawnerComponent>);
// --- META ---

DECLARE_CLASS(ChipsSpawnerComponent, ChipsSpawnerComponent);
// --- END META ---
