#pragma once

#include "Screens/GameScreen.h"
#include "o2/Scene/Actor.h"

using namespace o2;

// ------------------------------------------------------------------
// Meta screen: the pet room with the dog and the play button. Play
// switches to the gameplay screen with the current chain level.
// ------------------------------------------------------------------
class MetaScreen: public GameScreen
{
public:
	static constexpr auto kName = "Meta";

	String GetName() const override { return kName; }

	// Returns screen root actor, valid while loaded
	const Ref<Actor>& GetRoot() const { return mRoot; }

protected:
	void OnLoad() override;
	void OnUnload() override;
	void OnActivated() override;
	void OnDeactivated() override;

private:
	Ref<Actor> mRoot;
};
