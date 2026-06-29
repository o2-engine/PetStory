#pragma once
#include "o2/Scene/Component.h"
#include "o2/Scene/ComponentLinkRef.h"
#include "o2/Utils/Math/Vector2.h"
#include "SlingBoard.h"

using namespace o2;

// AI opponent. On its turn it grabs one of its chips, draws it back into the red band (which
// visibly stretches), then releases — so the bot shoots through the band exactly like the player.
class SlingBot: public Component
{
public:
	LinkRef<SlingBoard> board;  // @SERIALIZABLE @EDITOR_PROPERTY
	float minSpeed = 620.0f;    // @SERIALIZABLE @EDITOR_PROPERTY
	float maxSpeed = 1050.0f;   // @SERIALIZABLE @EDITOR_PROPERTY
	float pullDuration = 0.45f; // @SERIALIZABLE @EDITOR_PROPERTY  time to draw the band back

	// Picks the bot-side chip closest to the gap, or null when none remain
	Ref<SlingPuck> ChoosePuck() const;

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
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(620.0f).NAME(minSpeed);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(1050.0f).NAME(maxSpeed);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.45f).NAME(pullDuration);
    FIELD().PRIVATE().NAME(mPuck);
    FIELD().PRIVATE().NAME(mRubber);
    FIELD().PRIVATE().NAME(mStartPos);
    FIELD().PRIVATE().NAME(mPullTarget);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mPullTime);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mPulling);
}
END_META;
CLASS_METHODS_META(SlingBot)
{

    FUNCTION().PUBLIC().SIGNATURE(Ref<SlingPuck>, ChoosePuck);
    FUNCTION().PUBLIC().SIGNATURE(bool, TakeTurn);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsPulling);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, Release);
}
END_META;
// --- END META ---
