#pragma once

#include "Level/LevelData.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Function/Function.h"

using namespace o2;

// ------------------------------------------------------------------
// Sits on the level root actor: tracks goal progress. Chips report
// popped groups here (found by walking up the parent chain); when
// every goal is collected the controller fires onCompleted once.
// ------------------------------------------------------------------
class LevelController: public Component
{
public:
	Function<void()> onCompleted;    // Called once when all goals are collected
	Function<void()> onGoalsChanged; // Called on every progress change

public:
	// Sets level goals and resets progress
	void SetGoals(const Vector<LevelGoal>& goals);

	const Vector<LevelGoal>& GetGoals() const;

	// Returns collected count for goal index, clamped to the goal count
	int GetCollected(int goalIndex) const;

	// Returns true when every goal is collected
	bool IsCompleted() const;

	// Adds popped chips to the matching goal; called by Chip on group pop
	void OnChipsPopped(const String& chipType, int count);

	// Walks up the parent chain of the actor looking for a LevelController
	static Ref<LevelController> FindFor(const Ref<Actor>& actor);

	SERIALIZABLE(LevelController);
	CLONEABLE_REF(LevelController);

private:
	Vector<LevelGoal> mGoals;     // @SERIALIZABLE
	Vector<int>       mCollected;
	bool              mCompletedFired = false;

private:
	void CheckCompletion();
};
// --- META ---

CLASS_BASES_META(LevelController)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(LevelController)
{
    FIELD().PUBLIC().NAME(onCompleted);
    FIELD().PUBLIC().NAME(onGoalsChanged);
    FIELD().PRIVATE().SERIALIZABLE_ATTRIBUTE().NAME(mGoals);
    FIELD().PRIVATE().NAME(mCollected);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mCompletedFired);
}
END_META;
CLASS_METHODS_META(LevelController)
{

    FUNCTION().PUBLIC().SIGNATURE(void, SetGoals, const Vector<LevelGoal>&);
    FUNCTION().PUBLIC().SIGNATURE(const Vector<LevelGoal>&, GetGoals);
    FUNCTION().PUBLIC().SIGNATURE(int, GetCollected, int);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsCompleted);
    FUNCTION().PUBLIC().SIGNATURE(void, OnChipsPopped, const String&, int);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Ref<LevelController>, FindFor, const Ref<Actor>&);
    FUNCTION().PRIVATE().SIGNATURE(void, CheckCompletion);
}
END_META;
// --- END META ---
