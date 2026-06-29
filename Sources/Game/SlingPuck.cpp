#include "o2/stdafx.h"
#include "SlingPuck.h"

#include "SlingBoard.h"
#include "o2/Scene/Actor.h"
#include "o2/Utils/Math/Math.h"

bool SlingPuck::IsPlayer() const
{
	return team == 0;
}

bool SlingPuck::IsResting(float minSpeed) const
{
	return velocity.Length() <= minSpeed;
}

void SlingPuck::OnStart()
{
	mImage = GetActor()->GetComponent<ImageComponent>();
	if (mImage)
		mImage->onDraw = [&] { OnDrawn(); };

	FindBoard();
}

void SlingPuck::FindBoard()
{
	auto actor = GetActor();
	auto parent = actor ? actor->GetParent().Lock() : nullptr;
	while (parent)
	{
		if (auto board = parent->GetComponent<SlingBoard>())
		{
			mBoard = board;
			break;
		}

		parent = parent->GetParent().Lock();
	}
}

bool SlingPuck::IsUnderPoint(const Vec2F& point)
{
	auto board = mBoard.Lock();
	if (board && !board->IsPlayerInputEnabled())
		return false;

	auto actor = GetActor();
	if (!actor)
		return false;

	return (point - actor->transform->worldPosition.Get()).Length() <= radius;
}

void SlingPuck::OnCursorPressed(const Input::Cursor& cursor)
{
	auto board = mBoard.Lock();
	if (board && !board->IsPlayerInputEnabled())
		return;

	mDragging = true;
	held = true;
	velocity = Vec2F();
	mGrabPos = position;

	if (board)
		mRubber = board->GetRubberForSide(SlingBoard::SideOfPosition(position));
}

void SlingPuck::UpdateDrag(const Input::Cursor& cursor)
{
	if (!mDragging)
		return;

	auto board = mBoard.Lock();
	Vec2F local = board ? board->ToLocal(cursor.position) : cursor.position;
	if (board)
	{
		// Stay within the side walls, but allow pulling past the back wall to load the band deeply
		const float backPull = 110.0f;
		local.x = Math::Clamp(local.x, -board->halfWidth + radius, board->halfWidth - radius);
		local.y = Math::Clamp(local.y, -board->halfHeight - backPull, board->halfHeight + backPull);
	}

	position = local;
	velocity = Vec2F();

	if (mRubber)
		mRubber->SetGrip(position); // the chip stretches the band
}

void SlingPuck::OnCursorStillDown(const Input::Cursor& cursor)
{
	UpdateDrag(cursor);
}

void SlingPuck::OnCursorMoved(const Input::Cursor& cursor)
{
	UpdateDrag(cursor);
}

void SlingPuck::OnCursorReleased(const Input::Cursor& cursor)
{
	if (!mDragging)
		return;

	mDragging = false;
	held = false;

	// The shot comes entirely from the band: the chip is flung from where it stretched it.
	Vec2F launch = mRubber ? mRubber->ComputeLaunch(position, dragPower, maxLaunchSpeed) : Vec2F();

	if (launch.SqrLength() > 0.0f)
	{
		auto board = mBoard.Lock();
		if (board)
			position = board->ClampInside(position, radius); // start the flight on the board
		velocity = launch;
	}
	else
	{
		position = mGrabPos; // band wasn't stretched -> no shot, return chip to its spot
	}

	if (mRubber)
	{
		mRubber->ClearGrip();
		mRubber = nullptr;
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingPuck>);
// --- META ---

DECLARE_CLASS(SlingPuck, SlingPuck);
// --- END META ---
