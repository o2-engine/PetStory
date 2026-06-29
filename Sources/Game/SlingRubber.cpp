#include "o2/stdafx.h"
#include "SlingRubber.h"

#include "o2/Utils/Math/Math.h"

void SlingRubber::SetLegs(const Ref<Actor>& left, const Ref<Actor>& right)
{
	mLegLeft = left;
	mLegRight = right;
}

void SlingRubber::SetGrip(const Vec2F& grip)
{
	mGrip = grip;
	mActive = true;
}

void SlingRubber::ClearGrip()
{
	mActive = false;
}

Vec2F SlingRubber::ClampGripToBack(const Vec2F& grip, int side, float restY)
{
	Vec2F clamped = grip;
	if (side == 0)
		clamped.y = Math::Min(clamped.y, restY); // player band bends downward only
	else
		clamped.y = Math::Max(clamped.y, restY); // bot band bends upward only

	return clamped;
}

Vec2F SlingRubber::GetEffectiveGrip() const
{
	if (!mActive)
		return Vec2F(0.0f, restY);

	return ClampGripToBack(mGrip, side, restY);
}

Vec2F SlingRubber::ComputeLaunch(const Vec2F& grip, float power, float maxSpeed) const
{
	Vec2F pulled = ClampGripToBack(grip, side, restY);
	float depth = side == 0 ? (restY - pulled.y) : (pulled.y - restY); // how far behind the band
	if (depth < minStretch)
		return Vec2F();

	float forward = side == 0 ? depth : -depth; // band normal, into the field
	Vec2F launch(-pulled.x * 0.5f * power, forward * power);

	float len = launch.Length();
	if (len > maxSpeed && len > 0.0f)
		launch = launch * (maxSpeed / len);

	return launch;
}

void SlingRubber::LayoutLeg(const Ref<Actor>& leg, const Vec2F& post, const Vec2F& grip)
{
	if (!leg)
		return;

	Vec2F dir = grip - post;
	float len = Math::Max(dir.Length(), 1.0f);

	leg->transform->SetPivot(Vec2F(0.5f, 0.5f));
	leg->transform->SetSize(Vec2F(len, thickness));
	leg->transform->SetPosition((post + grip) * 0.5f);
	leg->transform->SetAngle(Math::Atan2F(dir.y, dir.x));
}

void SlingRubber::OnStart()
{
	// Leg sprites are this actor's children; gather them so a loaded scene reconnects them
	auto self = GetActor();
	if (!self)
		return;

	auto& children = self->GetChildren();
	if (children.Count() > 0)
		mLegLeft = children[0];
	if (children.Count() > 1)
		mLegRight = children[1];
}

void SlingRubber::OnUpdate(float dt)
{
	Vec2F leftPost(-halfSpan, restY);
	Vec2F rightPost(halfSpan, restY);

	if (mActive)
	{
		Vec2F grip = ClampGripToBack(mGrip, side, restY);
		LayoutLeg(mLegLeft, leftPost, grip);
		LayoutLeg(mLegRight, rightPost, grip);
	}
	else
	{
		// idle: one straight band across the full span, second leg hidden
		LayoutLeg(mLegLeft, leftPost, rightPost);
		if (mLegRight)
			mLegRight->transform->SetSize(Vec2F());
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingRubber>);
// --- META ---

DECLARE_CLASS(SlingRubber, SlingRubber);
// --- END META ---
