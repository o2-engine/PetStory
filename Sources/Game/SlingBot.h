#pragma once
#include "o2/Scene/Component.h"
#include "o2/Scene/ComponentLinkRef.h"
#include "o2/Utils/Math/Vector2.h"
#include "SlingBoard.h"

using namespace o2;

// AI opponent. On its turn it grabs one of its chips, draws it back into the red band (which
// visibly stretches), then releases — so the bot shoots through the band exactly like the player.
// difficulty scales both the fire rate (3 s at 0 to 0.2 s at 100) and the accuracy: the aim is
// planned by simulating candidate shots against the current board (collisions included), and a
// difficulty-driven share of shots is deliberately spoiled.
class SlingBot: public Component
{
public:
	LinkRef<SlingBoard> board;  // @SERIALIZABLE @EDITOR_PROPERTY
	float difficulty = 50.0f;   // @SERIALIZABLE @EDITOR_PROPERTY  0 = easiest, 100 = hardest
	float minSpeed = 620.0f;    // @SERIALIZABLE @EDITOR_PROPERTY
	float maxSpeed = 1050.0f;   // @SERIALIZABLE @EDITOR_PROPERTY
	float pullDuration = 0.45f; // @SERIALIZABLE @EDITOR_PROPERTY  time to draw the band back

	// How long a fresh shot is left alone to finish crossing before the bot may regrab it
	// (even in motion) when it is the last chip on the side @SERIALIZABLE @EDITOR_PROPERTY
	float lastShotCrossDelay = 1.5f;

	// Seconds between shots for a difficulty: 3.0 at 0 down to 0.2 at 100 (pure, testable)
	static float ShotIntervalFor(float difficulty);

	// Chance to fire a spoiled shot for a difficulty: 0.65 at 0 down to 0.05 at 100 (pure, testable)
	static float MissChanceFor(float difficulty);

	float GetShotInterval() const;

	// Picks a chip to shoot without waiting for the board to settle: settled chips go first but
	// moving ones are fair game too. The chip it just shot is skipped while alternatives exist;
	// when it is the last one and still in flight it stays untouchable until lastShotCrossDelay
	// passes (a fresh shot gets its chance to cross), then is regrabbed even in motion. Null when
	// nothing on the side is ready — the bot waits for the next tick.
	Ref<SlingPuck> ChoosePuck() const;

	// Picks the pull x for a chip drawn to `depth`: simulates candidate shots against the current
	// board and returns the one that ends on the player side, or the closest to crossing
	float PlanPullX(const Ref<SlingPuck>& puck, float depth) const;

	// Begins drawing a chip back into the band. Returns true if a draw was started.
	bool TakeTurn();

	bool IsPulling() const;

	void OnUpdate(float dt) override;

	SERIALIZABLE(SlingBot);
	CLONEABLE_REF(SlingBot);

private:
	Ref<SlingPuck>   mPuck;
	Ref<SlingRubber> mRubber;
	Vec2F mStartPos;
	Vec2F mPullTarget;
	float mPullTime = 0.0f;
	bool  mPulling = false;
	float mTimeSinceShot = 0.0f; // seconds since the last release; gates regrabbing a fresh shot

	WeakRef<SlingPuck>         mLastShotPuck; // skipped on the next turn so a fresh shot isn't regrabbed
	Vector<WeakRef<SlingPuck>> mGrabOrder;    // most recent grabs last; drives least-recently-used choice

	void MarkGrabbed(const Ref<SlingPuck>& puck);
	int  GrabRankOf(const Ref<SlingPuck>& puck) const; // -1 = never grabbed (most preferred)

	void Release();

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(SlingBot)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(SlingBot)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(board);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(50.0f).NAME(difficulty);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(620.0f).NAME(minSpeed);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(1050.0f).NAME(maxSpeed);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.45f).NAME(pullDuration);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(1.5f).NAME(lastShotCrossDelay);
    FIELD().PRIVATE().NAME(mPuck);
    FIELD().PRIVATE().NAME(mRubber);
    FIELD().PRIVATE().NAME(mStartPos);
    FIELD().PRIVATE().NAME(mPullTarget);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mPullTime);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mPulling);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mTimeSinceShot);
    FIELD().PRIVATE().NAME(mLastShotPuck);
    FIELD().PRIVATE().NAME(mGrabOrder);
}
END_META;
CLASS_METHODS_META(SlingBot)
{

    FUNCTION().PUBLIC().SIGNATURE_STATIC(float, ShotIntervalFor, float);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(float, MissChanceFor, float);
    FUNCTION().PUBLIC().SIGNATURE(float, GetShotInterval);
    FUNCTION().PUBLIC().SIGNATURE(Ref<SlingPuck>, ChoosePuck);
    FUNCTION().PUBLIC().SIGNATURE(float, PlanPullX, const Ref<SlingPuck>&, float);
    FUNCTION().PUBLIC().SIGNATURE(bool, TakeTurn);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsPulling);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, MarkGrabbed, const Ref<SlingPuck>&);
    FUNCTION().PRIVATE().SIGNATURE(int, GrabRankOf, const Ref<SlingPuck>&);
    FUNCTION().PRIVATE().SIGNATURE(void, Release);
}
END_META;
// --- END META ---
