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

	Vector<Vec2F> occupied;
	for (auto& actor : mSpawnContainer->GetChildren())
		occupied.Add(actor->transform->GetWorldPosition2D());

	Vec2F position;
	if (!FindFreeSpawnPosition(spawnZone, occupied, mSpawnClearance, 8, position))
		return;

	auto newChip = mChipProto->Instantiate();
	newChip->transform->position2D = position;
	newChip->SetParent(mSpawnContainer);
}

bool ChipsSpawnerComponent::FindFreeSpawnPosition(const RectF& zone, const Vector<Vec2F>& occupied,
                                                  float clearance, int attempts, Vec2F& result)
{
	float clearanceSqr = clearance*clearance;
	for (int i = 0; i < attempts; i++)
	{
		Vec2F candidate(Math::Random(zone.left, zone.right), Math::Random(zone.bottom, zone.top));

		bool free = true;
		for (auto& point : occupied)
		{
			if ((point - candidate).SqrLength() < clearanceSqr)
			{
				free = false;
				break;
			}
		}

		if (free)
		{
			result = candidate;
			return true;
		}
	}

	return false;
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<ChipsSpawnerComponent>);
// --- META ---

DECLARE_CLASS(ChipsSpawnerComponent, ChipsSpawnerComponent);
// --- END META ---
