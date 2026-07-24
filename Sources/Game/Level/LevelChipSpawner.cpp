#include "o2/stdafx.h"
#include "Level/LevelChipSpawner.h"

#include "Level/ChipsSpawner.h"
#include "Level/ChipColors.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Scene.h"

void LevelChipSpawner::SetColors(const Vector<String>& colors)
{
	mColors.Clear();
	mProtos.Clear();
	mProtosResolved = false;

	for (auto& color : colors)
	{
		if (ChipColors::IsKnownColor(color))
			mColors.Add(color);
	}
}

void LevelChipSpawner::ResolveProtos()
{
	if (mProtosResolved)
		return;

	mProtosResolved = true;

	if (!Render::IsSingletonInitialzed())
		return;

	for (auto& color : mColors)
	{
		AssetRef<ActorAsset> proto(ChipColors::GetPrototypePath(color));
		if (proto)
			mProtos.Add(proto);
	}
}

const Vector<String>& LevelChipSpawner::GetColors() const
{
	return mColors;
}

void LevelChipSpawner::SetMaxOnScreen(int count)
{
	mMaxOnScreen = count;
}

int LevelChipSpawner::GetMaxOnScreen() const
{
	return mMaxOnScreen;
}

void LevelChipSpawner::SetSpawnDelay(float delay)
{
	mSpawnDelay = delay;
}

float LevelChipSpawner::GetSpawnDelay() const
{
	return mSpawnDelay;
}

void LevelChipSpawner::SetContainer(const Ref<Actor>& container)
{
	mContainer = container;
}

const LinkRef<Actor>& LevelChipSpawner::GetContainer() const
{
	return mContainer;
}

int LevelChipSpawner::GetAliveCount() const
{
	if (!mContainer)
		return 0;

	return mContainer->GetChildren().Count();
}

void LevelChipSpawner::OnUpdate(float dt)
{
	mTimer += dt;
	if (mTimer < mSpawnDelay)
		return;

	mTimer = 0.0f;
	TrySpawn();
}

void LevelChipSpawner::TrySpawn()
{
	ResolveProtos();

	if (!mContainer || mProtos.IsEmpty())
		return;

	if (GetAliveCount() >= mMaxOnScreen)
		return;

	RectF zone = mOwner.Lock()->transform->worldRect;

	Vector<Vec2F> occupied;
	for (auto& chip : mContainer->GetChildren())
		occupied.Add(chip->transform->GetWorldPosition2D());

	Vec2F position;
	if (!ChipsSpawnerComponent::FindFreeSpawnPosition(zone, occupied, mSpawnClearance, 8, position))
		return;

	int index = Math::Random(0, mProtos.Count() - 1);
	auto chip = mProtos[index]->Instantiate();
	chip->transform->position2D = position;
	chip->SetParent(mContainer);
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<LevelChipSpawner>);
// --- META ---

DECLARE_CLASS(LevelChipSpawner, LevelChipSpawner);
// --- END META ---
