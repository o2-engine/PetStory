#include "o2/stdafx.h"
#include "SlingBot.h"

#include "o2/Utils/Math/Math.h"

float SlingBot::ShotIntervalFor(float difficulty)
{
	return Math::Lerp(3.0f, 0.2f, Math::Clamp(difficulty, 0.0f, 100.0f) / 100.0f);
}

float SlingBot::MissChanceFor(float difficulty)
{
	return Math::Lerp(0.65f, 0.05f, Math::Clamp(difficulty, 0.0f, 100.0f) / 100.0f);
}

float SlingBot::GetShotInterval() const
{
	return ShotIntervalFor(difficulty);
}

void SlingBot::MarkGrabbed(const Ref<SlingPuck>& puck)
{
	mGrabOrder.RemoveAll([&](const WeakRef<SlingPuck>& other) { return other.Lock() == puck; });
	mGrabOrder.Add(WeakRef(puck));
}

int SlingBot::GrabRankOf(const Ref<SlingPuck>& puck) const
{
	for (int i = 0; i < mGrabOrder.Count(); i++)
	{
		if (mGrabOrder[i].Lock() == puck)
			return i;
	}

	return -1;
}

Ref<SlingPuck> SlingBot::ChoosePuck() const
{
	auto b = board.Get();
	if (!b)
		return nullptr;

	auto lastShot = mLastShotPuck.Lock();

	Ref<SlingPuck> best;
	int   bestRank = 0;
	float bestDist = 0.0f;
	bool  bestIsLastShot = false;

	for (auto& puck : b->GetPucks())
	{
		if (!puck || !puck->active || puck->held)
			continue;

		if (SlingBoard::SideOfPosition(puck->position) != 1)
			continue;

		// Never grab a chip in flight: a fresh shot must be left alone to finish crossing,
		// otherwise the bot keeps relaunching its own last chip and can't clear the side
		if (!puck->IsResting(b->restSpeed))
			continue;

		bool  isLastShot = puck == lastShot;
		int   rank = GrabRankOf(puck);
		float dist = puck->position.Length();

		bool better;
		if (!best)
			better = true;
		else if (isLastShot != bestIsLastShot)
			better = !isLastShot;      // the freshly shot chip only when nothing else remains
		else if (rank != bestRank)
			better = rank < bestRank;  // least recently grabbed first (-1 = never)
		else
			better = dist < bestDist;

		if (better)
		{
			best = puck;
			bestRank = rank;
			bestDist = dist;
			bestIsLastShot = isLastShot;
		}
	}

	return best;
}

float SlingBot::PlanPullX(const Ref<SlingPuck>& puck, float depth) const
{
	auto b = board.Get();
	if (!b || !puck)
		return puck ? puck->position.x : 0.0f;

	auto rubber = b->GetRubberForSide(1);
	if (!rubber)
		return puck->position.x;

	float maxX = b->halfWidth - puck->radius;
	float restY = rubber->restY;

	// Straight behind the chip first (the natural move), then spots spread across the band
	const float fractions[] = { 0.0f, 0.25f, -0.25f, 0.5f, -0.5f, 0.75f, -0.75f, 1.0f, -1.0f };
	Vector<float> candidates;
	candidates.Add(Math::Clamp(puck->position.x, -maxX, maxX));
	for (float fraction : fractions)
		candidates.Add(fraction * maxX);

	float bestX = candidates[0];
	float bestScore = -1e9f;
	for (float x : candidates)
	{
		Vec2F pull(x, restY + depth);
		Vec2F launch = rubber->ComputeLaunch(pull, puck->dragPower, puck->maxLaunchSpeed);
		if (launch.SqrLength() <= 0.0f)
			continue;

		Vec2F end = b->SimulateShot(puck, pull, launch);

		// Crossing to the player side wins; among crossings prefer the smallest hand movement.
		// Failed shots rank by how close to the divider the chip stops.
		float score = end.y < 0.0f ? 1000.0f - Math::Abs(x - puck->position.x) * 0.1f : -end.y;
		if (score > bestScore)
		{
			bestScore = score;
			bestX = x;
		}
	}

	return bestX;
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
	float restY = mRubber ? mRubber->restY : board->topHalfHeight - 56.0f;

	// The pull may not push the chip past the back wall — the band stays inside the field
	float maxDepth = board->topHalfHeight - puck->radius - Math::Abs(restY);
	depth = Math::Min(depth, Math::Max(maxDepth, 0.0f));

	float aimX = PlanPullX(puck, depth);
	if (Math::Random(0.0f, 1.0f) < MissChanceFor(difficulty))
	{
		// A spoiled shot: the hand slips aside and the chip goes wide of the planned line
		float maxX = board->halfWidth - puck->radius;
		aimX = Math::Clamp(aimX + Math::Random(-1.0f, 1.0f) * maxX * 0.6f, -maxX, maxX);
	}

	mPullTarget = Vec2F(aimX, restY + depth);

	puck->held = true;
	puck->velocity = Vec2F();
	mPullTime = 0.0f;
	mPulling = true;

	MarkGrabbed(puck);

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
	{
		if (launch.SqrLength() > 0.0f)
			mRubber->onShot(launch.Length());

		mRubber->ClearGrip();
	}

	mLastShotPuck = mPuck;
	mPuck = nullptr;
	mRubber = nullptr;
	mPulling = false;
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingBot>);
// --- META ---

DECLARE_CLASS(SlingBot, SlingBot);
// --- END META ---
