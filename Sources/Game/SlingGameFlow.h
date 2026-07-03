#pragma once
#include "o2/Scene/Actor.h"
#include "o2/Scene/ActorLinkRef.h"
#include "o2/Scene/Component.h"
#include "o2/Scene/ComponentLinkRef.h"
#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"

using namespace o2;

// Meta-loop over single rounds: the player starts against a difficulty-10 bot; each win shows the
// victory window and NEXT LEVEL raises the bot difficulty by a step, a loss shows the game-over
// window and RETRY drops back to the start (WATCH AD retries the same difficulty). The windows are
// scene actors built by the scene code; this component only shows/hides them and resets the level.
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

	float GetDifficulty() const;
	bool  IsWindowShown() const;

	// Buttons: next level (win, harder bot), retry (loss, back to start), continue (same difficulty)
	void OnNextLevel();
	void OnRetry();
	void OnContinueSameLevel();

	// Puts every chip back to its spawn spot and starts a fresh round against `difficulty`
	void StartLevel(float difficulty);

	void OnStart() override;
	void OnUpdate(float dt) override;

	SERIALIZABLE(SlingGameFlow);
	CLONEABLE_REF(SlingGameFlow);

private:
	Vector<Vec2F> mSpawnPositions; // per-puck, index-matched to the board's puck list
	float mDifficulty = 10.0f;
	bool  mWindowShown = false;

	void SnapshotSpawns();
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
    FIELD().PRIVATE().NAME(mSpawnPositions);
    FIELD().PRIVATE().DEFAULT_VALUE(10.0f).NAME(mDifficulty);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mWindowShown);
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
    FUNCTION().PUBLIC().SIGNATURE(void, OnStart);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, SnapshotSpawns);
    FUNCTION().PRIVATE().SIGNATURE(void, ShowResultWindow, int);
    FUNCTION().PRIVATE().SIGNATURE(void, HideWindows);
}
END_META;
// --- END META ---
