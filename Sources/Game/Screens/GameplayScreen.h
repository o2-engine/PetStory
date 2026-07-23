#pragma once

#include "Level/LevelData.h"
#include "Screens/GameScreen.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/UI/Widgets/Label.h"

using namespace o2;

class LevelController;

// ------------------------------------------------------------------
// Gameplay screen: builds the current chain level from its config,
// shows the goals bubble at the top right and switches back to the
// meta screen shortly after every goal is collected.
// ------------------------------------------------------------------
class GameplayScreen: public GameScreen
{
public:
	static constexpr auto kName = "Gameplay";

	String GetName() const override { return kName; }

	const Ref<Actor>& GetRoot() const { return mRoot; }

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

	float mCompleteTimer = -1.0f; // Counts down to the meta switch, negative while inactive

private:
	void BuildGoalsBubble(const LevelData& data);
	void UpdateGoalLabels();
	void OnLevelCompleted();
};
