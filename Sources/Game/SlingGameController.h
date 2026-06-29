#pragma once
#include "o2/Scene/Component.h"
#include "o2/Scene/ComponentLinkRef.h"
#include "SlingBoard.h"
#include "SlingBot.h"

using namespace o2;

// Real-time controller: the player (via input) and the bot (on a timer) shoot simultaneously,
// not in turns. A side's owner wins the moment that side is cleared of chips.
class SlingGameController: public Component
{
public:
	LinkRef<SlingBoard> board;  // @SERIALIZABLE @EDITOR_PROPERTY
	LinkRef<SlingBot>   bot;    // @SERIALIZABLE @EDITOR_PROPERTY
	float botInterval = 3.0f;   // @SERIALIZABLE @EDITOR_PROPERTY  seconds between bot shots

	int  GetWinner() const; // -1 none, 0 player, 1 bot
	bool IsGameOver() const;

	void ResetGame();
	void Step(float dt);

	void OnStart() override;
	void OnUpdate(float dt) override;

	SERIALIZABLE(SlingGameController);
	CLONEABLE_REF(SlingGameController);

private:
	int   mWinner = -1;
	bool  mGameOver = false;
	float mBotTimer = 0.0f;

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(SlingGameController)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(SlingGameController)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(board);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(bot);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(3.0f).NAME(botInterval);
    FIELD().PRIVATE().DEFAULT_VALUE(-1).NAME(mWinner);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mGameOver);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mBotTimer);
}
END_META;
CLASS_METHODS_META(SlingGameController)
{

    FUNCTION().PUBLIC().SIGNATURE(int, GetWinner);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsGameOver);
    FUNCTION().PUBLIC().SIGNATURE(void, ResetGame);
    FUNCTION().PUBLIC().SIGNATURE(void, Step, float);
    FUNCTION().PUBLIC().SIGNATURE(void, OnStart);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
}
END_META;
// --- END META ---
