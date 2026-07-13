#pragma once
#include "o2/Events/CursorAreaEventsListener.h"
#include "o2/Scene/Component.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Utils/Math/Vector2.h"

#include "SlingRubber.h"

using namespace o2;

class SlingBoard;

// A single sliding chip. team is the colour/identity (0 = blue, 1 = red, 2 = green); which side
// a chip belongs to is decided by its position, not its team. Any chip can be grabbed and slung.
// position/velocity are board-local; the owning SlingBoard simulates and renders them.
class SlingPuck: public Component, public CursorAreaEventsListener
{
public:
	int   team = 0;                 // @SERIALIZABLE @EDITOR_PROPERTY
	float radius = 34.0f;           // @SERIALIZABLE @EDITOR_PROPERTY
	float dragPower = 9.0f;         // @SERIALIZABLE @EDITOR_PROPERTY
	float maxLaunchSpeed = 1700.0f; // @SERIALIZABLE @EDITOR_PROPERTY
	float highlightBaseAngle = 45.0f; // @SERIALIZABLE @EDITOR_PROPERTY  baked light direction in the chip art, degrees

	Vec2F position;        // board-local simulation position
	Vec2F velocity;        // board-local simulation velocity
	bool  held = false;    // true while being dragged (excluded from simulation)
	bool  active = true;   // false = benched pool chip: hidden and excluded from play

	bool IsPlayer() const;
	bool IsResting(float minSpeed) const;

	// Sprite rotation (radians) that turns the baked highlight (drawn at baseAngleDegrees) to face
	// the light at `light`, both points board-local. Pure, testable, no scene state.
	static float HighlightAngle(const Vec2F& chipPos, const Vec2F& light, float baseAngleDegrees);

	void OnStart() override;
	void OnUpdate(float dt) override;
	bool IsUnderPoint(const Vec2F& point) override;

	SERIALIZABLE(SlingPuck);
	CLONEABLE_REF(SlingPuck);

private:
	Ref<ImageComponent> mImage;
	WeakRef<SlingBoard> mBoard;
	Ref<SlingRubber>    mRubber;
	Vec2F mGrabPos;
	bool  mDragging = false;

	void FindBoard();
	void UpdateDrag(const Input::Cursor& cursor);

	// Turns the sprite so its baked highlight faces the board light (top-right of the field)
	void UpdateHighlight();

	void OnCursorPressed(const Input::Cursor& cursor) override;
	void OnCursorStillDown(const Input::Cursor& cursor) override;
	void OnCursorMoved(const Input::Cursor& cursor) override;
	void OnCursorReleased(const Input::Cursor& cursor) override;

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(SlingPuck)
{
    BASE_CLASS(Component);
    BASE_CLASS(CursorAreaEventsListener);
}
END_META;
CLASS_FIELDS_META(SlingPuck)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0).NAME(team);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(34.0f).NAME(radius);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(9.0f).NAME(dragPower);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(1700.0f).NAME(maxLaunchSpeed);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(45.0f).NAME(highlightBaseAngle);
    FIELD().PUBLIC().NAME(position);
    FIELD().PUBLIC().NAME(velocity);
    FIELD().PUBLIC().DEFAULT_VALUE(false).NAME(held);
    FIELD().PUBLIC().DEFAULT_VALUE(true).NAME(active);
    FIELD().PRIVATE().NAME(mImage);
    FIELD().PRIVATE().NAME(mBoard);
    FIELD().PRIVATE().NAME(mRubber);
    FIELD().PRIVATE().NAME(mGrabPos);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mDragging);
}
END_META;
CLASS_METHODS_META(SlingPuck)
{

    FUNCTION().PUBLIC().SIGNATURE(bool, IsPlayer);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsResting, float);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(float, HighlightAngle, const Vec2F&, const Vec2F&, float);
    FUNCTION().PUBLIC().SIGNATURE(void, OnStart);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsUnderPoint, const Vec2F&);
    FUNCTION().PRIVATE().SIGNATURE(void, FindBoard);
    FUNCTION().PRIVATE().SIGNATURE(void, UpdateDrag, const Input::Cursor&);
    FUNCTION().PRIVATE().SIGNATURE(void, UpdateHighlight);
    FUNCTION().PRIVATE().SIGNATURE(void, OnCursorPressed, const Input::Cursor&);
    FUNCTION().PRIVATE().SIGNATURE(void, OnCursorStillDown, const Input::Cursor&);
    FUNCTION().PRIVATE().SIGNATURE(void, OnCursorMoved, const Input::Cursor&);
    FUNCTION().PRIVATE().SIGNATURE(void, OnCursorReleased, const Input::Cursor&);
}
END_META;
// --- END META ---
