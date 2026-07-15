#pragma once
#include "o2/Assets/Types/ImageAsset.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Math/Color.h"
#include "o2/Utils/Math/Vector2.h"
#include "o2/Utils/Types/Containers/Vector.h"

using namespace o2;

// Elastic band along the back of one side. Drawn as a single textured strip (rubber_red for the
// player side, rubber_blue for the bot side) that runs from one post, wraps the field-facing arc of
// a nocked chip, and continues to the other post; it lies as a straight line across the span when
// idle. Building and drawing happen here; the scene only places the actor and sets the colour.
class SlingRubber: public Component
{
public:
	int    side = 0;                 // @SERIALIZABLE @EDITOR_PROPERTY  0 = player (bottom), 1 = bot (top)
	float  restY = -320.0f;          // @SERIALIZABLE @EDITOR_PROPERTY
	float  halfSpan = 175.0f;        // @SERIALIZABLE @EDITOR_PROPERTY
	float  thickness = 16.0f;        // @SERIALIZABLE @EDITOR_PROPERTY
	float  minStretch = 6.0f;        // @SERIALIZABLE @EDITOR_PROPERTY  no shot/wrap below this pull depth
	float  lateralAim = 0.2f;        // @SERIALIZABLE @EDITOR_PROPERTY  sideways speed per unit of pull x offset
	float  maxAimAngle = 25.0f;      // @SERIALIZABLE @EDITOR_PROPERTY  max deviation from straight forward, degrees
	Color4 color = Color4::White();  // @SERIALIZABLE @EDITOR_PROPERTY

	// Called when a chip is actually flung off this band (not during the bot's shot planning),
	// with the launch speed
	Function<void(float launchSpeed)> onShot;

	void SetGrip(const Vec2F& grip, float chipRadius = 34.0f);
	void ClearGrip();

	// Velocity the stretched band imparts to a chip pulled to `grip`: forward (away from the band)
	// scaled by how deep it was pulled, plus a gentle lateral aim capped at maxAimAngle from
	// straight forward. Returns zero if not stretched.
	Vec2F ComputeLaunch(const Vec2F& grip, float power, float maxSpeed) const;

	// Grip used for rendering: clamped so the band only bends away from the field
	Vec2F GetEffectiveGrip() const;

	// Clamps a grip point so the band bends only backward (testable, no scene state)
	static Vec2F ClampGripToBack(const Vec2F& grip, int side, float restY);

	// Centerline of the band: post -> tangent -> arc hugging the chip's field-facing side -> tangent
	// -> post. Returns the two posts straight when the chip isn't pulled behind the band.
	Vector<Vec2F> BuildBandPath(const Vec2F& grip, float chipRadius, int arcSegments) const;

	void OnDraw() override;

	SERIALIZABLE(SlingRubber);
	CLONEABLE_REF(SlingRubber);

private:
	Vec2F mGrip;
	float mChipRadius = 34.0f;
	bool  mActive = false;

	AssetRef<ImageAsset> mBandTexture; // rubber strip art, lazily loaded by side

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
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.2f).NAME(lateralAim);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(25.0f).NAME(maxAimAngle);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(Color4::White()).NAME(color);
    FIELD().PUBLIC().NAME(onShot);
    FIELD().PRIVATE().NAME(mGrip);
    FIELD().PRIVATE().DEFAULT_VALUE(34.0f).NAME(mChipRadius);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mActive);
    FIELD().PRIVATE().NAME(mBandTexture);
}
END_META;
CLASS_METHODS_META(SlingRubber)
{

    FUNCTION().PUBLIC().SIGNATURE(void, SetGrip, const Vec2F&, float);
    FUNCTION().PUBLIC().SIGNATURE(void, ClearGrip);
    FUNCTION().PUBLIC().SIGNATURE(Vec2F, ComputeLaunch, const Vec2F&, float, float);
    FUNCTION().PUBLIC().SIGNATURE(Vec2F, GetEffectiveGrip);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Vec2F, ClampGripToBack, const Vec2F&, int, float);
    FUNCTION().PUBLIC().SIGNATURE(Vector<Vec2F>, BuildBandPath, const Vec2F&, float, int);
    FUNCTION().PUBLIC().SIGNATURE(void, OnDraw);
}
END_META;
// --- END META ---
