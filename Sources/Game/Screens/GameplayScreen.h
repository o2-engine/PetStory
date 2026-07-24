#pragma once

#include "Level/LevelData.h"
#include "GameLib/Screens/GameScreen.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/UI/Widgets/Label.h"

using namespace o2;

class LevelController;

// ------------------------------------------------------------------
// Gameplay screen: builds the current chain level from its config,
// shows the goals bubble at the top right and the moves counter at
// the top left. Collected goals open the win window, running out of
// moves opens the buy-moves window; without the window system the
// win falls back to an immediate switch to the meta screen.
// ------------------------------------------------------------------
class GameplayScreen: public GameScreen
{
public:
	static constexpr auto kName = "Gameplay";

	String GetName() const override;

	const Ref<Actor>& GetRoot() const;

	// Returns the controller of the built level, valid while loaded
	Ref<LevelController> GetLevelController() const;

	// Loads level data by asset path; falls back to a built-in level when missing
	static LevelData LoadLevelData(const String& assetPath);

protected:
	void OnLoad() override;
	void OnUnload() override;
	void OnActivated() override;
	void OnDeactivated() override;
	void OnUpdate(float dt) override;

private:
	Ref<Actor>               mRoot;
	Ref<Actor>               mLevelRoot;
	WeakRef<LevelController> mController;

	Vector<Ref<Label>> mGoalLabels;
	Ref<Label>         mMovesLabel;

	float mCompleteTimer = -1.0f; // Counts down to the win window, negative while inactive

private:
	void BuildGoalsBubble(const LevelData& data);
	void UpdateGoalLabels();
	void UpdateMovesLabel();
	void OnLevelCompleted();
	void OnOutOfMoves();
	void ShowWinWindow();

	// Returns 1..3 by the share of moves left
	int ComputeStars() const;
};
