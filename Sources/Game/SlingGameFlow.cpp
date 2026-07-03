#include "o2/stdafx.h"
#include "SlingGameFlow.h"

#include "o2/Utils/Math/Math.h"

float SlingGameFlow::GetDifficulty() const
{
	return mDifficulty;
}

bool SlingGameFlow::IsWindowShown() const
{
	return mWindowShown;
}

void SlingGameFlow::OnNextLevel()
{
	StartLevel(Math::Min(mDifficulty + difficultyStep, 100.0f));
}

void SlingGameFlow::OnRetry()
{
	StartLevel(startDifficulty);
}

void SlingGameFlow::OnContinueSameLevel()
{
	StartLevel(mDifficulty);
}

void SlingGameFlow::StartLevel(float difficulty)
{
	mDifficulty = difficulty;

	if (bot)
		bot->difficulty = difficulty;

	auto b = board.Get();
	if (b)
	{
		auto& pucks = b->GetPucks();
		for (int i = 0; i < pucks.Count() && i < mSpawnPositions.Count(); i++)
		{
			if (!pucks[i])
				continue;

			pucks[i]->position = mSpawnPositions[i];
			pucks[i]->velocity = Vec2F();
			pucks[i]->held = false;
		}

		b->SetPlayerInputEnabled(true);
	}

	HideWindows();
	mWindowShown = false;

	if (controller)
		controller->ResetGame();
}

void SlingGameFlow::SnapshotSpawns()
{
	mSpawnPositions.Clear();

	auto b = board.Get();
	if (!b)
		return;

	for (auto& puck : b->GetPucks())
		mSpawnPositions.Add(puck ? puck->position : Vec2F());
}

void SlingGameFlow::ShowResultWindow(int winner)
{
	mWindowShown = true;

	if (auto b = board.Get())
		b->SetPlayerInputEnabled(false);

	auto window = winner == 0 ? victoryWindow.Get() : gameOverWindow.Get();
	if (window)
		window->SetEnabled(true);
}

void SlingGameFlow::HideWindows()
{
	if (auto window = victoryWindow.Get())
		window->SetEnabled(false);
	if (auto window = gameOverWindow.Get())
		window->SetEnabled(false);
}

void SlingGameFlow::OnStart()
{
	SnapshotSpawns();
	HideWindows();

	if (bot)
		bot->difficulty = startDifficulty;
	mDifficulty = startDifficulty;
}

void SlingGameFlow::OnUpdate(float dt)
{
	if (mSpawnPositions.IsEmpty())
		SnapshotSpawns();

	if (mWindowShown || !controller)
		return;

	if (controller->IsGameOver())
		ShowResultWindow(controller->GetWinner());
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingGameFlow>);
// --- META ---

DECLARE_CLASS(SlingGameFlow, SlingGameFlow);
// --- END META ---
