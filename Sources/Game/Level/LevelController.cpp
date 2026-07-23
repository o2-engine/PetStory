#include "o2/stdafx.h"
#include "Level/LevelController.h"

#include "o2/Scene/Actor.h"

void LevelController::SetGoals(const Vector<LevelGoal>& goals)
{
	mGoals = goals;
	if (mGoals.Count() > LevelData::kMaxGoals)
		mGoals.Resize(LevelData::kMaxGoals);

	mCollected.Clear();
	mCollected.Resize(mGoals.Count());
	for (int i = 0; i < mCollected.Count(); i++)
		mCollected[i] = 0;

	mCompletedFired = false;

	onGoalsChanged();
}

const Vector<LevelGoal>& LevelController::GetGoals() const
{
	return mGoals;
}

int LevelController::GetCollected(int goalIndex) const
{
	if (goalIndex < 0 || goalIndex >= mCollected.Count())
		return 0;

	return mCollected[goalIndex];
}

bool LevelController::IsCompleted() const
{
	if (mGoals.IsEmpty())
		return false;

	for (int i = 0; i < mGoals.Count(); i++)
	{
		if (mCollected[i] < mGoals[i].count)
			return false;
	}

	return true;
}

void LevelController::OnChipsPopped(const String& chipType, int count)
{
	bool changed = false;
	for (int i = 0; i < mGoals.Count(); i++)
	{
		if (mGoals[i].chipType != chipType)
			continue;

		int newValue = Math::Min(mCollected[i] + count, mGoals[i].count);
		if (newValue != mCollected[i])
		{
			mCollected[i] = newValue;
			changed = true;
		}
	}

	if (changed)
	{
		onGoalsChanged();
		CheckCompletion();
	}
}

Ref<LevelController> LevelController::FindFor(const Ref<Actor>& actor)
{
	auto current = actor;
	while (current)
	{
		if (auto controller = current->GetComponent<LevelController>())
			return controller;

		current = current->GetParent().Lock();
	}

	return nullptr;
}

void LevelController::CheckCompletion()
{
	if (mCompletedFired || !IsCompleted())
		return;

	mCompletedFired = true;
	onCompleted();
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<LevelController>);
// --- META ---

DECLARE_CLASS(LevelController, LevelController);
// --- END META ---
