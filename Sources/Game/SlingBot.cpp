#include "o2/stdafx.h"
#include "SlingBot.h"

#include "o2/Utils/Math/Math.h"

Ref<SlingPuck> SlingBot::ChoosePuck() const
{
	if (!board)
		return nullptr;

	Ref<SlingPuck> best;
	float bestDist = 0.0f;
	for (auto& puck : board->GetPucks())
	{
		if (!puck || puck->held)
			continue;

		if (SlingBoard::SideOfPosition(puck->position) != 1)
			continue;

		float dist = puck->position.Length();
		if (!best || dist < bestDist)
		{
			best = puck;
			bestDist = dist;
		}
	}

	return best;
}

bool SlingBot::IsPulling() const
{
	return mPulling;
}

bool SlingBot::TakeTurn()
{
	if (mPulling || !board)
		return false;

	auto puck = ChoosePuck();
	if (!puck)
		return false;

	mRubber = board->GetRubberForSide(1);
	mPuck = puck;
	mStartPos = puck->position;

	float speed = Math::Random(minSpeed, maxSpeed);
	float depth = speed / Math::Max(puck->dragPower, 1.0f);
	float restY = mRubber ? mRubber->restY : board->halfHeight - 56.0f;

	// Draw the chip straight back behind the band; the band's lateral aim sends it toward centre.
	mPullTarget = Vec2F(puck->position.x, restY + depth);

	puck->held = true;
	puck->velocity = Vec2F();
	mPullTime = 0.0f;
	mPulling = true;

	return true;
}

void SlingBot::OnUpdate(float dt)
{
	if (!mPulling)
		return;

	if (!mPuck)
	{
		mPulling = false;
		return;
	}

	mPullTime += dt;
	float t = Math::Min(mPullTime / Math::Max(pullDuration, 0.0001f), 1.0f);

	mPuck->position = Math::Lerp(mStartPos, mPullTarget, t); // draw the chip into the band
	mPuck->held = true;
	mPuck->velocity = Vec2F();

	if (mRubber)
		mRubber->SetGrip(mPuck->position, mPuck->radius);

	if (t >= 1.0f)
		Release();
}

void SlingBot::Release()
{
	Vec2F launch = mRubber ? mRubber->ComputeLaunch(mPuck->position, mPuck->dragPower, mPuck->maxLaunchSpeed)
						   : Vec2F(0.0f, -Math::Random(minSpeed, maxSpeed));

	if (auto b = board.Get())
		mPuck->position = b->ClampInside(mPuck->position, mPuck->radius);

	mPuck->velocity = launch;
	mPuck->held = false;

	if (mRubber)
		mRubber->ClearGrip();

	mPuck = nullptr;
	mRubber = nullptr;
	mPulling = false;
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingBot>);
// --- META ---

DECLARE_CLASS(SlingBot, SlingBot);
// --- END META ---
