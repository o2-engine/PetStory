#pragma once
#include "o2/Scene/Actor.h"
#include "o2/Scene/ActorLinkRef.h"
#include "o2/Scene/Component.h"
#include "o2/Scene/ComponentLinkRef.h"
#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"

namespace o2
{
	class Text;
}

using namespace o2;

// Meta-loop over single rounds: the player starts against a difficulty-10 bot; each win shows the
// victory window (with a random joke) and NEXT LEVEL raises the bot difficulty by a step, a loss
// shows the game-over window and RETRY drops back to the start (WATCH AD retries the same
// difficulty). Each round draws pucks from the board's pool: their count per side grows with
// difficulty (minPucksPerSide..maxPucksPerSide) and their spawn spots are randomized every round.
class SlingGameFlow: public Component
{
public:
	LinkRef<SlingBoard>          board;      // @SERIALIZABLE @EDITOR_PROPERTY
	LinkRef<SlingBot>            bot;        // @SERIALIZABLE @EDITOR_PROPERTY
	LinkRef<SlingGameController> controller; // @SERIALIZABLE @EDITOR_PROPERTY

	LinkRef<Actor> victoryWindow;  // @SERIALIZABLE @EDITOR_PROPERTY
	LinkRef<Actor> gameOverWindow; // @SERIALIZABLE @EDITOR_PROPERTY

	float startDifficulty = 10.0f; // @SERIALIZABLE @EDITOR_PROPERTY
	float difficultyStep = 10.0f;  // @SERIALIZABLE @EDITOR_PROPERTY

	int minPucksPerSide = 3;  // @SERIALIZABLE @EDITOR_PROPERTY
	int maxPucksPerSide = 10; // @SERIALIZABLE @EDITOR_PROPERTY

	float GetDifficulty() const;
	bool  IsWindowShown() const;

	// Buttons: next level (win, harder bot), retry (loss, back to start), continue (same difficulty)
	void OnNextLevel();
	void OnRetry();
	void OnContinueSameLevel();

	// Respawns the pucks for `difficulty` and starts a fresh round against it
	void StartLevel(float difficulty);

	// Pucks per side for a difficulty: minPucks at the run start, maxPucks at difficulty 100
	static int PucksPerSideFor(float difficulty, float startDifficulty, int minPucks, int maxPucks);

	// Random spread-out spawn spots on one half of the field, between the divider and the band
	// at |y| = bandY. Pure, testable: takes the field geometry instead of scene state.
	static Vector<Vec2F> GenerateSpawns(int count, int side, float halfWidth, float bandY, float radius);

	// Shrinks the text's font height (maxHeight down to minHeight) until the word-wrapped text
	// fits its drawable area — jokes vary in length. No-op without a font (headless tests).
	static void FitTextHeight(const Ref<Text>& text, int maxHeight = 20, int minHeight = 12);

	void OnStart() override;
	void OnUpdate(float dt) override;

	SERIALIZABLE(SlingGameFlow);
	CLONEABLE_REF(SlingGameFlow);

private:
	float mDifficulty = 10.0f;
	bool  mWindowShown = false;
	bool  mSpawned = false; // the first round spawns lazily, once the board has gathered its pucks

	void SpawnPucks(float difficulty);
	void ShowResultWindow(int winner);
	void HideWindows();

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(SlingGameFlow)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(SlingGameFlow)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(board);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(bot);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(controller);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(victoryWindow);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(gameOverWindow);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(10.0f).NAME(startDifficulty);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(10.0f).NAME(difficultyStep);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(3).NAME(minPucksPerSide);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(10).NAME(maxPucksPerSide);
    FIELD().PRIVATE().DEFAULT_VALUE(10.0f).NAME(mDifficulty);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mWindowShown);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mSpawned);
}
END_META;
CLASS_METHODS_META(SlingGameFlow)
{

    FUNCTION().PUBLIC().SIGNATURE(float, GetDifficulty);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsWindowShown);
    FUNCTION().PUBLIC().SIGNATURE(void, OnNextLevel);
    FUNCTION().PUBLIC().SIGNATURE(void, OnRetry);
    FUNCTION().PUBLIC().SIGNATURE(void, OnContinueSameLevel);
    FUNCTION().PUBLIC().SIGNATURE(void, StartLevel, float);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(int, PucksPerSideFor, float, float, int, int);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Vector<Vec2F>, GenerateSpawns, int, int, float, float, float);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(void, FitTextHeight, const Ref<Text>&, int, int);
    FUNCTION().PUBLIC().SIGNATURE(void, OnStart);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, SpawnPucks, float);
    FUNCTION().PRIVATE().SIGNATURE(void, ShowResultWindow, int);
    FUNCTION().PRIVATE().SIGNATURE(void, HideWindows);
}
END_META;
// --- END META ---
