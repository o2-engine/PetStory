#pragma once
#include "o2/Scene/ActorLinkRef.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Math/Vector2.h"

using namespace o2;

// Keeps the actor's image covering the whole visible area of the camera. The field art fills only
// its own 500x896; without this everything around it would be flat fill colour, and the margins
// change with the window aspect. Recomputed every frame, so it follows window and camera resizes.
class SlingBackground: public Component
{
public:
	LinkRef<CameraActor> camera;   // @SERIALIZABLE @EDITOR_PROPERTY
	float                overscale = 1.02f; // @SERIALIZABLE @EDITOR_PROPERTY  margin against rounding gaps at the edges

	// Visible world size of a fitted camera at the given render resolution (mirrors Camera::FittedSize)
	static Vec2F FittedViewSize(const Vec2F& fittedSize, const Vec2F& resolution);

	// Smallest size keeping the content aspect that still covers the whole view
	static Vec2F CoverSize(const Vec2F& view, const Vec2F& contentSize);

	void OnUpdate(float dt) override;

	SERIALIZABLE(SlingBackground);
	CLONEABLE_REF(SlingBackground);

private:
	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(SlingBackground)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(SlingBackground)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(camera);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(1.02f).NAME(overscale);
}
END_META;
CLASS_METHODS_META(SlingBackground)
{

    FUNCTION().PUBLIC().SIGNATURE_STATIC(Vec2F, FittedViewSize, const Vec2F&, const Vec2F&);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Vec2F, CoverSize, const Vec2F&, const Vec2F&);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
}
END_META;
// --- END META ---
