#include "o2/stdafx.h"
#include "SlingBoard.h"

#include "o2/Scene/Actor.h"
#include "o2/Utils/Math/Math.h"

void SlingBoard::RegisterPuck(const Ref<SlingPuck>& puck)
{
	if (puck)
	{
		mPucks.Add(puck);
		mGathered = true;
	}
}

void SlingBoard::ClearPucks()
{
	mPucks.Clear();
}

const Vector<Ref<SlingPuck>>& SlingBoard::GetPucks() const
{
	return mPucks;
}

void SlingBoard::RegisterRubber(const Ref<SlingRubber>& rubber)
{
	if (rubber)
		mRubbers.Add(rubber);
}

Ref<SlingRubber> SlingBoard::GetRubberForSide(int side) const
{
	for (auto& rubber : mRubbers)
	{
		if (rubber && rubber->side == side)
			return rubber;
	}

	return nullptr;
}

Vec2F SlingBoard::ToLocal(const Vec2F& world) const
{
	auto actor = GetActor();
	return actor ? world - actor->transform->worldPosition2D.Get() : world;
}

Vec2F SlingBoard::ClampInside(const Vec2F& pos, float radius) const
{
	return Vec2F(Math::Clamp(pos.x, -halfWidth + radius, halfWidth - radius),
				 Math::Clamp(pos.y, -halfHeight + radius, halfHeight - radius));
}

int SlingBoard::SideOfPosition(const Vec2F& pos)
{
	return pos.y < 0.0f ? 0 : 1;
}

int SlingBoard::CountPucksOnSide(int side) const
{
	int count = 0;
	for (auto& puck : mPucks)
	{
		if (puck && SideOfPosition(puck->position) == side)
			++count;
	}

	return count;
}

bool SlingBoard::AllPucksResting() const
{
	for (auto& puck : mPucks)
	{
		if (puck && !puck->IsResting(restSpeed))
			return false;
	}

	return true;
}

int SlingBoard::GetWinner() const
{
	bool any = false;
	for (auto& puck : mPucks)
	{
		if (puck)
		{
			any = true;
			break;
		}
	}

	if (!any)
		return -1;

	if (CountPucksOnSide(0) == 0)
		return 0;

	if (CountPucksOnSide(1) == 0)
		return 1;

	return -1;
}

bool SlingBoard::IsPlayerInputEnabled() const
{
	return mPlayerInput;
}

void SlingBoard::SetPlayerInputEnabled(bool enabled)
{
	mPlayerInput = enabled;
}

void SlingBoard::StepSimulation(float dt)
{
	if (dt <= 0.0f)
		return;

	for (auto& puck : mPucks)
	{
		if (puck && !puck->held)
			IntegratePuck(*puck, dt);
	}

	ResolveCollisions();

	for (auto& puck : mPucks)
	{
		if (!puck || puck->held)
			continue;

		puck->velocity = puck->velocity * Math::Max(0.0f, 1.0f - friction * dt);
		if (puck->velocity.Length() < restSpeed)
			puck->velocity = Vec2F();
	}
}

Vec2F SlingBoard::SimulateShot(const Ref<SlingPuck>& shooter, const Vec2F& startPos, const Vec2F& launchVelocity,
							   float maxTime /*= 3.0f*/) const
{
	if (!shooter)
		return startPos;

	auto scratch = mmake<SlingBoard>();
	scratch->halfWidth = halfWidth;
	scratch->halfHeight = halfHeight;
	scratch->gapHalf = gapHalf;
	scratch->friction = friction;
	scratch->wallRestitution = wallRestitution;
	scratch->puckRestitution = puckRestitution;
	scratch->restSpeed = restSpeed;

	Ref<SlingPuck> scratchShooter;
	for (auto& puck : mPucks)
	{
		if (!puck || (puck->held && puck != shooter))
			continue;

		auto copy = mmake<SlingPuck>();
		copy->radius = puck->radius;
		copy->position = puck->position;
		copy->velocity = puck->velocity;
		scratch->RegisterPuck(copy);

		if (puck == shooter)
			scratchShooter = copy;
	}

	if (!scratchShooter)
		return startPos;

	scratchShooter->position = ClampInside(startPos, scratchShooter->radius);
	scratchShooter->velocity = launchVelocity;

	const float step = 1.0f / 120.0f;
	for (float time = 0.0f; time < maxTime; time += step)
	{
		scratch->StepSimulation(step);
		if (scratch->AllPucksResting())
			break;
	}

	return scratchShooter->position;
}

void SlingBoard::IntegratePuck(SlingPuck& puck, float dt)
{
	float r = puck.radius;
	Vec2F prev = puck.position;
	Vec2F next = prev + puck.velocity * dt;

	float minX = -halfWidth + r, maxX = halfWidth - r;
	float minY = -halfHeight + r, maxY = halfHeight - r;

	if (next.x < minX) { next.x = minX; puck.velocity.x = -puck.velocity.x * wallRestitution; }
	else if (next.x > maxX) { next.x = maxX; puck.velocity.x = -puck.velocity.x * wallRestitution; }

	if (next.y < minY) { next.y = minY; puck.velocity.y = -puck.velocity.y * wallRestitution; }
	else if (next.y > maxY) { next.y = maxY; puck.velocity.y = -puck.velocity.y * wallRestitution; }

	// Center divider at y = 0, solid everywhere except the central gap (|x| <= gapHalf)
	bool solidColumn = Math::Abs(next.x) > gapHalf;
	bool crossing = (prev.y > 0.0f) != (next.y > 0.0f);
	bool inBand = Math::Abs(next.y) < r;
	if (solidColumn && (crossing || inBand))
	{
		float side = prev.y >= 0.0f ? 1.0f : -1.0f;
		next.y = side * r;
		puck.velocity.y = -puck.velocity.y * wallRestitution;
	}

	puck.position = next;

	// Rounded gap edges: bounce off the two posts at the mouth of the gap
	auto resolvePost = [&](const Vec2F& post) {
		Vec2F d = puck.position - post;
		float dist = d.Length();
		if (dist > 0.0f && dist < r)
		{
			Vec2F n = d / dist;
			puck.position = post + n * r;
			float vn = puck.velocity.Dot(n);
			if (vn < 0.0f)
				puck.velocity = puck.velocity - n * (vn * (1.0f + wallRestitution));
		}
	};

	resolvePost(Vec2F(gapHalf, 0.0f));
	resolvePost(Vec2F(-gapHalf, 0.0f));
}

void SlingBoard::ResolveCollisions()
{
	for (int iter = 0; iter < 2; ++iter)
	{
		for (int i = 0; i < mPucks.Count(); ++i)
		{
			if (!mPucks[i])
				continue;

			for (int j = i + 1; j < mPucks.Count(); ++j)
			{
				if (!mPucks[j])
					continue;

				SlingPuck& a = *mPucks[i];
				SlingPuck& b = *mPucks[j];

				if (a.held || b.held)
					continue;

				Vec2F d = b.position - a.position;
				float dist = d.Length();
				float minDist = a.radius + b.radius;

				if (dist > 0.0f && dist < minDist)
				{
					Vec2F n = d / dist;
					float overlap = minDist - dist;
					a.position = a.position - n * (overlap * 0.5f);
					b.position = b.position + n * (overlap * 0.5f);

					Vec2F rv = b.velocity - a.velocity;
					float vn = rv.Dot(n);
					if (vn < 0.0f)
					{
						Vec2F impulse = n * (-(1.0f + puckRestitution) * vn * 0.5f);
						a.velocity = a.velocity - impulse;
						b.velocity = b.velocity + impulse;
					}
				}
				else if (dist == 0.0f && minDist > 0.0f)
				{
					a.position.x -= a.radius * 0.5f;
					b.position.x += b.radius * 0.5f;
				}
			}
		}
	}
}

void SlingBoard::GatherPucks()
{
	mPucks.Clear();

	auto self = GetActor();
	if (!self)
		return;

	mRubbers.Clear();

	for (auto& child : self->GetChildren())
	{
		if (auto puck = child->GetComponent<SlingPuck>())
		{
			puck->position = child->transform->GetPosition2D();
			mPucks.Add(puck);
		}

		if (auto rubber = child->GetComponent<SlingRubber>())
			mRubbers.Add(rubber);
	}

	mGathered = true;
}

void SlingBoard::SyncTransforms()
{
	for (auto& puck : mPucks)
	{
		if (!puck)
			continue;

		if (auto actor = puck->GetActor())
			actor->transform->SetPosition2D(puck->position);
	}
}

void SlingBoard::OnStart()
{
	if (!mGathered)
		GatherPucks();
}

void SlingBoard::OnUpdate(float dt)
{
	if (!mGathered)
		GatherPucks();

	StepSimulation(dt);
	SyncTransforms();
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingBoard>);
// --- META ---

DECLARE_CLASS(SlingBoard, SlingBoard);
// --- END META ---
