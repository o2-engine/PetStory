#include "o2/stdafx.h"
#include "SlingGameFlow.h"

#include "o2/Render/Text.h"
#include "o2/Scene/Components/SoundComponent.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Utils/Math/Math.h"

#include "Jokes.h"
#include "YandexGames.h"

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

void SlingGameFlow::OnWatchAdClicked()
{
	if (mWaitingAdResult)
		return;

	mWaitingAdResult = true;
	YandexGames::ShowRewardedVideo();
}

int SlingGameFlow::PucksPerSideFor(float difficulty, float startDifficulty, int minPucks, int maxPucks)
{
	float t = Math::Clamp01((difficulty - startDifficulty) / Math::Max(100.0f - startDifficulty, 1.0f));
	return Math::Clamp(minPucks + (int)(t * (float)(maxPucks - minPucks) + 0.5f), minPucks, maxPucks);
}

Vector<Vec2F> SlingGameFlow::GenerateSpawns(int count, int side, float halfWidth, float bandY, float radius)
{
	float maxX = Math::Max(halfWidth - radius - 6.0f, 1.0f);
	float minY = radius + 50.0f; // clear of the divider and its cap art
	float maxY = Math::Max(bandY - radius - 16.0f, minY + 1.0f); // in front of the band
	float spacing = radius * 2.2f;

	// Rejection sampling with a best-so-far fallback: chips end up spread out, and when the half
	// gets crowded the most distant candidate still goes in, so the count is always delivered
	Vector<Vec2F> spawns;
	for (int i = 0; i < count; i++)
	{
		Vec2F best;
		float bestDist = -1.0f;
		for (int attempt = 0; attempt < 100; attempt++)
		{
			Vec2F candidate(Math::Random(-maxX, maxX), Math::Random(minY, maxY));
			float minDist = 1e9f;
			for (auto& other : spawns)
				minDist = Math::Min(minDist, (candidate - other).Length());

			if (minDist > bestDist)
			{
				bestDist = minDist;
				best = candidate;
			}

			if (bestDist >= spacing)
				break;
		}

		spawns.Add(best);
	}

	if (side == 0)
	{
		for (auto& spawn : spawns)
			spawn.y = -spawn.y;
	}

	return spawns;
}

void SlingGameFlow::SpawnPucks(float difficulty)
{
	auto b = board.Get();
	if (!b)
		return;

	auto& pucks = b->GetPucks();
	if (pucks.IsEmpty())
		return;

	int perSide = PucksPerSideFor(difficulty, startDifficulty, minPucksPerSide, maxPucksPerSide);
	perSide = Math::Min(perSide, pucks.Count() / 2);

	float radius = pucks[0] ? pucks[0]->radius : 34.0f;
	auto bandY = [&](int side) {
		auto rubber = b->GetRubberForSide(side);
		if (rubber)
			return Math::Abs(rubber->restY);

		return (side == 0 ? b->bottomHalfHeight : b->topHalfHeight) - 56.0f;
	};

	Vector<Vec2F> playerSpawns = GenerateSpawns(perSide, 0, b->halfWidth, bandY(0), radius);
	Vector<Vec2F> botSpawns = GenerateSpawns(perSide, 1, b->halfWidth, bandY(1), radius);

	for (int i = 0; i < pucks.Count(); i++)
	{
		auto& puck = pucks[i];
		if (!puck)
			continue;

		bool activeNow = i < perSide * 2;
		puck->active = activeNow;
		puck->held = false;
		puck->velocity = Vec2F();
		if (activeNow)
			puck->position = i < perSide ? playerSpawns[i] : botSpawns[i - perSide];

		if (auto actor = puck->GetActor())
			actor->SetEnabled(activeNow);
	}
}

void SlingGameFlow::StartLevel(float difficulty)
{
	mDifficulty = difficulty;

	if (bot)
		bot->difficulty = difficulty;

	auto b = board.Get();
	if (b)
	{
		SpawnPucks(difficulty);
		b->SetPlayerInputEnabled(true);

		if (!b->GetPucks().IsEmpty())
			mSpawned = true;
	}

	HideWindows();
	mWindowShown = false;
	mWaitingAdResult = false;

	if (controller)
		controller->ResetGame();
}

void SlingGameFlow::ShowResultWindow(int winner)
{
	mWindowShown = true;

	if (auto b = board.Get())
		b->SetPlayerInputEnabled(false);

	auto window = winner == 0 ? victoryWindow.Get() : gameOverWindow.Get();
	if (window)
		window->SetEnabled(true);

	if (winner == 0)
	{
		if (auto widget = dynamic_cast<Widget*>(victoryWindow.Get()))
		{
			if (auto joke = widget->GetLayerDrawable<Text>("joke"))
			{
				joke->SetText(Jokes::Random());
				FitTextHeight(joke);
			}
		}
	}
}

void SlingGameFlow::FitTextHeight(const Ref<Text>& text, int maxHeight /*= 20*/, int minHeight /*= 12*/)
{
	if (!text || !text->GetFont())
		return;

	Vec2F area = text->GetSize2D();
	if (area.x < 1.0f || area.y < 1.0f)
		return;

	// Measure by the actually built mesh: predicted sizes (GetTextSize) are wrong for glyph
	// heights the vector font hasn't rasterized yet
	for (int height = maxHeight; height > minHeight; height--)
	{
		text->SetHeight(height);
		if (text->GetRealSize().y <= area.y)
			return;
	}

	text->SetHeight(minHeight);
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
	WireResultButtons();
	HideWindows();

	if (bot)
		bot->difficulty = startDifficulty;
	mDifficulty = startDifficulty;
}

void SlingGameFlow::WireResultButtons()
{
	// Wired here rather than at scene-build time: a lambda onClick can't be serialized, so a
	// scene loaded from an asset (the editor's Game window) comes back with empty handlers.
	Ref<SoundComponent> clickSound;
	if (auto owner = GetActor())
	{
		if (auto soundActor = owner->GetChild("ButtonClickSound"))
			clickSound = soundActor->GetComponent<SoundComponent>();
	}

	WeakRef<SlingGameFlow>  weakFlow(this);
	WeakRef<SoundComponent> weakClick(clickSound.Get());

	auto bind = [&](Actor* windowActor, const String& buttonName, auto&& action)
	{
		auto window = dynamic_cast<Widget*>(windowActor);
		if (!window)
			return;

		if (auto button = DynamicCast<Button>(window->GetChildWidget(buttonName)))
		{
			button->onClick = action;
			button->onClick += [weakClick] { if (auto sound = weakClick.Lock()) sound->RewindAndPlay(); };
		}
	};

	bind(victoryWindow.Get(), "VictoryWindowNextButton",
		 [weakFlow] { if (auto f = weakFlow.Lock()) f->OnNextLevel(); });
	bind(gameOverWindow.Get(), "GameOverWindowRetryButton",
		 [weakFlow] { if (auto f = weakFlow.Lock()) f->OnRetry(); });
	bind(gameOverWindow.Get(), "GameOverWindowWatchAdButton",
		 [weakFlow] { if (auto f = weakFlow.Lock()) f->OnWatchAdClicked(); });
}

void SlingGameFlow::OnUpdate(float dt)
{
	if (!mSpawned)
	{
		auto b = board.Get();
		if (b && !b->GetPucks().IsEmpty())
			StartLevel(startDifficulty);
	}

	// The rewarded video result arrives from the SDK between frames; consume it on the game loop
	if (mWaitingAdResult)
	{
		int result = YandexGames::PopRewardedResult();
		if (result >= 0)
		{
			mWaitingAdResult = false;
			if (result == 1)
				OnContinueSameLevel(); // reward granted: same difficulty, fresh round
		}
	}

	if (mWindowShown || !controller)
		return;

	if (controller->IsGameOver())
		ShowResultWindow(controller->GetWinner());
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingGameFlow>);
// --- META ---

DECLARE_CLASS(SlingGameFlow, SlingGameFlow);
// --- END META ---
