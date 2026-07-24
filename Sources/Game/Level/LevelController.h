#pragma once

#include "Level/LevelData.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Function/Function.h"

using namespace o2;

// ------------------------------------------------------------------
// Sits on the level root actor: tracks goal progress and the moves
// limit. Chips report popped groups here (found by walking up the
// parent chain), each pop spends one move; when every goal is
// collected the controller fires onCompleted once, when the last
// move is spent short of the goals it fires onOutOfMoves once
// (re-armed by AddMoves).
// ------------------------------------------------------------------
class LevelController: public Component
{
public:
	Function<void()> onCompleted;    // Called once when all goals are collected
	Function<void()> onGoalsChanged; // Called on every progress change
	Function<void()> onMovesChanged; // Called on every moves count change
	Function<void()> onOutOfMoves;   // Called when the last move is spent and goals are not done

public:
	// Sets level goals and resets progress
	void SetGoals(const Vector<LevelGoal>& goals);

	const Vector<LevelGoal>& GetGoals() const;

	// Returns collected count for goal index, clamped to the goal count
	int GetCollected(int goalIndex) const;

	// Returns true when every goal is collected
	bool IsCompleted() const;

	// Sets the moves limit and resets the spent count; 0 - unlimited
	void SetMoves(int count);

	// Returns true when the level has a moves limit
	bool HasMovesLimit() const;

	// Returns the moves limit set for the level, 0 - unlimited
	int GetMovesLimit() const;

	// Returns moves left; 0 with no limit set
	int GetMovesLeft() const;

	// Adds extra moves (purchase) and re-arms onOutOfMoves
	void AddMoves(int count);

	// Adds popped chips to the matching goal and spends a move; called by Chip on group pop
	void OnChipsPopped(const String& chipType, int count);

	// Walks up the parent chain of the actor looking for a LevelController
	static Ref<LevelController> FindFor(const Ref<Actor>& actor);

	SERIALIZABLE(LevelController);
	CLONEABLE_REF(LevelController);

private:
	Vector<LevelGoal> mGoals;     // @SERIALIZABLE
	Vector<int>       mCollected;
	bool              mCompletedFired = false;

	int  mMovesLimit = 0; // @SERIALIZABLE Moves limit, 0 - unlimited
	int  mMovesLeft = 0;
	bool mOutOfMovesFired = false;

private:
	void CheckCompletion();
	void SpendMove();
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
    FIELD().PUBLIC().NAME(onMovesChanged);
    FIELD().PUBLIC().NAME(onOutOfMoves);
    FIELD().PRIVATE().SERIALIZABLE_ATTRIBUTE().NAME(mGoals);
    FIELD().PRIVATE().NAME(mCollected);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mCompletedFired);
    FIELD().PRIVATE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0).NAME(mMovesLimit);
    FIELD().PRIVATE().DEFAULT_VALUE(0).NAME(mMovesLeft);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mOutOfMovesFired);
}
END_META;
CLASS_METHODS_META(LevelController)
{

    FUNCTION().PUBLIC().SIGNATURE(void, SetGoals, const Vector<LevelGoal>&);
    FUNCTION().PUBLIC().SIGNATURE(const Vector<LevelGoal>&, GetGoals);
    FUNCTION().PUBLIC().SIGNATURE(int, GetCollected, int);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsCompleted);
    FUNCTION().PUBLIC().SIGNATURE(void, SetMoves, int);
    FUNCTION().PUBLIC().SIGNATURE(bool, HasMovesLimit);
    FUNCTION().PUBLIC().SIGNATURE(int, GetMovesLimit);
    FUNCTION().PUBLIC().SIGNATURE(int, GetMovesLeft);
    FUNCTION().PUBLIC().SIGNATURE(void, AddMoves, int);
    FUNCTION().PUBLIC().SIGNATURE(void, OnChipsPopped, const String&, int);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Ref<LevelController>, FindFor, const Ref<Actor>&);
    FUNCTION().PRIVATE().SIGNATURE(void, CheckCompletion);
    FUNCTION().PRIVATE().SIGNATURE(void, SpendMove);
}
END_META;
// --- END META ---
