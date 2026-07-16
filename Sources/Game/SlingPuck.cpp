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
	// The chip art lives either on the chip actor itself or on its "Body" child (the latter when
	// the chip carries a shadow child that must not spin with the highlight)
	mImage = GetActor()->GetComponent<ImageComponent>();
	if (!mImage)
	{
		if (auto body = GetActor()->GetChild("Body"))
			mImage = body->GetComponent<ImageComponent>();
	}

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
	auto actor = mImage ? mImage->GetActor() : GetActor();
	if (!board || !actor)
		return;

	// One light just past the upper-right corner of the field. Facing the chip's baked sheen toward
	// it makes the reflection slide as the chip moves, instead of every chip lit identically.
	// Only the art actor turns: the chip root (and so its shadow child) stays unrotated.
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

	// Only chips on the player's half answer the cursor: the bot's side and band are out of reach
	if (SlingBoard::SideOfPosition(position) != 0)
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

	if (SlingBoard::SideOfPosition(position) != 0)
		return;

	mDragging = true;
	held = true;
	velocity = Vec2F();
	mGrabPos = position;

	if (board)
		mRubber = board->GetRubberForSide(0); // the player only ever works his own band
}

void SlingPuck::UpdateDrag(const Input::Cursor& cursor)
{
	if (!mDragging)
		return;

	auto board = mBoard.Lock();
	Vec2F local = board ? board->ToLocal(cursor.position) : cursor.position;
	if (board)
	{
		local = board->ClampInside(local, radius); // the chip and the band never leave the field
		local.y = Math::Min(local.y, -radius);     // and never cross the divider by hand
	}

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

		if (mRubber)
			mRubber->onShot(launch.Length());
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
