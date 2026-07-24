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

void LevelController::SetMoves(int count)
{
	mMovesLimit = Math::Max(0, count);
	mMovesLeft = mMovesLimit;
	mOutOfMovesFired = false;

	onMovesChanged();
}

bool LevelController::HasMovesLimit() const
{
	return mMovesLimit > 0;
}

int LevelController::GetMovesLimit() const
{
	return mMovesLimit;
}

int LevelController::GetMovesLeft() const
{
	return mMovesLeft;
}

void LevelController::AddMoves(int count)
{
	if (count <= 0)
		return;

	mMovesLeft += count;
	mOutOfMovesFired = false;

	onMovesChanged();
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
		onGoalsChanged();

	SpendMove();
	CheckCompletion();

	// Completion wins over running out of moves on the same pop
	if (HasMovesLimit() && mMovesLeft <= 0 && !mCompletedFired && !mOutOfMovesFired)
	{
		mOutOfMovesFired = true;
		onOutOfMoves();
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

void LevelController::SpendMove()
{
	if (!HasMovesLimit() || mMovesLeft <= 0)
		return;

	mMovesLeft--;
	onMovesChanged();
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<LevelController>);
// --- META ---

DECLARE_CLASS(LevelController, LevelController);
// --- END META ---
