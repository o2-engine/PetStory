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

float SlingPuck::HighlightAngle(const Vec2F& chipPos, const Vec2F& light, float baseAngleDegrees)
{
	Vec2F dir = light - chipPos;
	if (dir.SqrLength() < 1.0f)
		return 0.0f;

	return Math::Atan2F(dir.y, dir.x) - Math::Deg2rad(baseAngleDegrees);
}

void SlingPuck::OnUpdate(float dt)
{
	UpdateHighlight();
}

void SlingPuck::UpdateHighlight()
{
	auto board = mBoard.Lock();
	auto actor = GetActor();
	if (!board || !actor)
		return;

	// One light just past the upper-right corner of the field. Facing the chip's baked sheen toward
	// it makes the reflection slide as the chip moves, instead of every chip lit identically.
	Vec2F light(board->halfWidth + 48.0f, board->topHalfHeight + 96.0f);
	actor->transform->SetAngle(HighlightAngle(position, light, highlightBaseAngle));
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
	if (!active)
		return false;

	auto board = mBoard.Lock();
	if (board && !board->IsPlayerInputEnabled())
		return false;

	auto actor = GetActor();
	if (!actor)
		return false;

	return (point - actor->transform->worldPosition2D.Get()).Length() <= radius;
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
		local = board->ClampInside(local, radius); // the chip and the band never leave the field

	position = local;
	velocity = Vec2F();

	if (mRubber)
		mRubber->SetGrip(position, radius); // the chip stretches the band
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
