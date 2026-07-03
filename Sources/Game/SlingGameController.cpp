#include "o2/stdafx.h"
#include "SlingGameController.h"

int SlingGameController::GetWinner() const
{
	return mWinner;
}

bool SlingGameController::IsGameOver() const
{
	return mGameOver;
}

void SlingGameController::ResetGame()
{
	mWinner = -1;
	mGameOver = false;
	mBotTimer = 0.0f;

	if (auto b = board.Get())
		b->SetPlayerInputEnabled(true);
}

void SlingGameController::Step(float dt)
{
	auto b = board.Get();
	if (!b || mGameOver)
		return;

	b->SetPlayerInputEnabled(true); // the player can shoot at any time

	mBotTimer += dt;
	float interval = bot ? bot->GetShotInterval() : botInterval;
	if (mBotTimer >= interval)
	{
		mBotTimer = 0.0f;
		if (bot)
			bot->TakeTurn();
	}

	int winner = b->GetWinner();
	if (winner >= 0)
	{
		mWinner = winner;
		mGameOver = true;
	}
}

void SlingGameController::OnStart()
{
	ResetGame();
}

void SlingGameController::OnUpdate(float dt)
{
	Step(dt);
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingGameController>);
// --- META ---

DECLARE_CLASS(SlingGameController, SlingGameController);
// --- END META ---
