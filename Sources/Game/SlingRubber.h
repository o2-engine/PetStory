#pragma once
#include "o2/Scene/Actor.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Math/Vector2.h"

using namespace o2;

// Elastic band along the back of one side. Rendered as two legs (post -> grip) that bend
// toward a nocked chip while it is pulled back, and lie straight when idle. Legs are plain
// sprite actors supplied by the scene builder; this component only lays them out each frame.
class SlingRubber: public Component
{
public:
	int   side = 0;          // @SERIALIZABLE @EDITOR_PROPERTY  0 = player (bottom), 1 = bot (top)
	float restY = -320.0f;   // @SERIALIZABLE @EDITOR_PROPERTY
	float halfSpan = 175.0f; // @SERIALIZABLE @EDITOR_PROPERTY
	float thickness = 16.0f; // @SERIALIZABLE @EDITOR_PROPERTY
	float minStretch = 6.0f; // @SERIALIZABLE @EDITOR_PROPERTY  no shot below this pull depth

	void SetLegs(const Ref<Actor>& left, const Ref<Actor>& right);

	void SetGrip(const Vec2F& grip);
	void ClearGrip();

	// Velocity the stretched band imparts to a chip pulled to `grip`: forward (away from the band)
	// scaled by how deep it was pulled, plus a gentle lateral aim. Returns zero if not stretched.
	Vec2F ComputeLaunch(const Vec2F& grip, float power, float maxSpeed) const;

	// Grip used for rendering: clamped so the band only bends away from the field
	Vec2F GetEffectiveGrip() const;

	// Clamps a grip point so the band bends only backward (testable, no scene state)
	static Vec2F ClampGripToBack(const Vec2F& grip, int side, float restY);

	void OnStart() override;
	void OnUpdate(float dt) override;

	SERIALIZABLE(SlingRubber);
	CLONEABLE_REF(SlingRubber);

private:
	Ref<Actor> mLegLeft;
	Ref<Actor> mLegRight;
	Vec2F mGrip;
	bool  mActive = false;

	void LayoutLeg(const Ref<Actor>& leg, const Vec2F& post, const Vec2F& grip);

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(SlingRubber)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(SlingRubber)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0).NAME(side);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(-320.0f).NAME(restY);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(175.0f).NAME(halfSpan);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(16.0f).NAME(thickness);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(6.0f).NAME(minStretch);
    FIELD().PRIVATE().NAME(mLegLeft);
    FIELD().PRIVATE().NAME(mLegRight);
    FIELD().PRIVATE().NAME(mGrip);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mActive);
}
END_META;
CLASS_METHODS_META(SlingRubber)
{

    FUNCTION().PUBLIC().SIGNATURE(void, SetLegs, const Ref<Actor>&, const Ref<Actor>&);
    FUNCTION().PUBLIC().SIGNATURE(void, SetGrip, const Vec2F&);
    FUNCTION().PUBLIC().SIGNATURE(void, ClearGrip);
    FUNCTION().PUBLIC().SIGNATURE(Vec2F, ComputeLaunch, const Vec2F&, float, float);
    FUNCTION().PUBLIC().SIGNATURE(Vec2F, GetEffectiveGrip);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Vec2F, ClampGripToBack, const Vec2F&, int, float);
    FUNCTION().PUBLIC().SIGNATURE(void, OnStart);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, LayoutLeg, const Ref<Actor>&, const Vec2F&, const Vec2F&);
}
END_META;
// --- END META ---
