#pragma once
#include "o2/Scene/Component.h"
#include "o2/Utils/Math/Vector2.h"
#include "SlingPuck.h"
#include "SlingRubber.h"

using namespace o2;

// The game field. Owns the deterministic top-down simulation of all child pucks:
// integration, friction, wall/divider bounce, gap pass-through and puck-puck collisions.
// Also answers the game-rule queries (side classification, rest state, winner).
// The divider sits at y = 0; the two halves may have different depths (the field art
// is asymmetric), so the top and bottom walls are configured separately.
class SlingBoard: public Component
{
public:
	float halfWidth  = 270.0f;        // @SERIALIZABLE @EDITOR_PROPERTY
	float topHalfHeight = 400.0f;     // @SERIALIZABLE @EDITOR_PROPERTY  divider to the top wall
	float bottomHalfHeight = 400.0f;  // @SERIALIZABLE @EDITOR_PROPERTY  divider to the bottom wall
	float gapHalf    = 85.0f;      // @SERIALIZABLE @EDITOR_PROPERTY
	float friction   = 0.9f;       // @SERIALIZABLE @EDITOR_PROPERTY
	float wallRestitution = 0.55f; // @SERIALIZABLE @EDITOR_PROPERTY
	float puckRestitution = 0.85f; // @SERIALIZABLE @EDITOR_PROPERTY
	float restSpeed  = 8.0f;       // @SERIALIZABLE @EDITOR_PROPERTY

	// Puck registry (children are gathered automatically in the live game; tests register manually)
	void RegisterPuck(const Ref<SlingPuck>& puck);
	void ClearPucks();
	const Vector<Ref<SlingPuck>>& GetPucks() const;

	void RegisterRubber(const Ref<SlingRubber>& rubber);
	Ref<SlingRubber> GetRubberForSide(int side) const;

	// World-space cursor point to board-local simulation space
	Vec2F ToLocal(const Vec2F& world) const;

	// Clamps a position so a chip of the given radius stays fully inside the walls
	Vec2F ClampInside(const Vec2F& pos, float radius) const;

	// Advances the whole board by dt (pure: operates on puck position/velocity only)
	void StepSimulation(float dt);

	// Predicts where `shooter` ends up when launched from `startPos` at `launchVelocity`:
	// runs the same physics on a scratch copy of the board, so walls, the divider and
	// collisions with the other chips are all accounted for. Doesn't touch the real pucks.
	Vec2F SimulateShot(const Ref<SlingPuck>& shooter, const Vec2F& startPos, const Vec2F& launchVelocity,
					   float maxTime = 3.0f) const;

	static int SideOfPosition(const Vec2F& pos); // 0 = player (bottom), 1 = bot (top)
	int  CountPucksOnSide(int side) const;
	bool AllPucksResting() const;
	int  GetWinner() const; // -1 none, 0 player cleared its side, 1 bot cleared its side

	bool IsPlayerInputEnabled() const;
	void SetPlayerInputEnabled(bool enabled);

	void OnStart() override;
	void OnUpdate(float dt) override;

	SERIALIZABLE(SlingBoard);
	CLONEABLE_REF(SlingBoard);

private:
	Vector<Ref<SlingPuck>>   mPucks;
	Vector<Ref<SlingRubber>> mRubbers;
	bool mPlayerInput = true;
	bool mGathered = false;

	void GatherPucks();
	void IntegratePuck(SlingPuck& puck, float dt);
	void ResolveCollisions();
	void SyncTransforms();

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(SlingBoard)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(SlingBoard)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(270.0f).NAME(halfWidth);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(400.0f).NAME(topHalfHeight);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(400.0f).NAME(bottomHalfHeight);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(85.0f).NAME(gapHalf);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.9f).NAME(friction);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.55f).NAME(wallRestitution);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.85f).NAME(puckRestitution);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(8.0f).NAME(restSpeed);
    FIELD().PRIVATE().NAME(mPucks);
    FIELD().PRIVATE().NAME(mRubbers);
    FIELD().PRIVATE().DEFAULT_VALUE(true).NAME(mPlayerInput);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mGathered);
}
END_META;
CLASS_METHODS_META(SlingBoard)
{

    FUNCTION().PUBLIC().SIGNATURE(void, RegisterPuck, const Ref<SlingPuck>&);
    FUNCTION().PUBLIC().SIGNATURE(void, ClearPucks);
    FUNCTION().PUBLIC().SIGNATURE(const Vector<Ref<SlingPuck>>&, GetPucks);
    FUNCTION().PUBLIC().SIGNATURE(void, RegisterRubber, const Ref<SlingRubber>&);
    FUNCTION().PUBLIC().SIGNATURE(Ref<SlingRubber>, GetRubberForSide, int);
    FUNCTION().PUBLIC().SIGNATURE(Vec2F, ToLocal, const Vec2F&);
    FUNCTION().PUBLIC().SIGNATURE(Vec2F, ClampInside, const Vec2F&, float);
    FUNCTION().PUBLIC().SIGNATURE(void, StepSimulation, float);
    FUNCTION().PUBLIC().SIGNATURE(Vec2F, SimulateShot, const Ref<SlingPuck>&, const Vec2F&, const Vec2F&, float);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(int, SideOfPosition, const Vec2F&);
    FUNCTION().PUBLIC().SIGNATURE(int, CountPucksOnSide, int);
    FUNCTION().PUBLIC().SIGNATURE(bool, AllPucksResting);
    FUNCTION().PUBLIC().SIGNATURE(int, GetWinner);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsPlayerInputEnabled);
    FUNCTION().PUBLIC().SIGNATURE(void, SetPlayerInputEnabled, bool);
    FUNCTION().PUBLIC().SIGNATURE(void, OnStart);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, GatherPucks);
    FUNCTION().PRIVATE().SIGNATURE(void, IntegratePuck, SlingPuck&, float);
    FUNCTION().PRIVATE().SIGNATURE(void, ResolveCollisions);
    FUNCTION().PRIVATE().SIGNATURE(void, SyncTransforms);
}
END_META;
// --- END META ---
