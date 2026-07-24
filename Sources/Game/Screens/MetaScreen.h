#pragma once

#include "GameLib/Screens/GameScreen.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/UI/Widget.h"

using namespace o2;

// ------------------------------------------------------------------
// Meta screen: the pet room with the dog, the lives and coins HUD
// and the settings / play / facebook buttons. Play switches to the
// gameplay screen with the current chain level, settings opens the
// settings window.
// ------------------------------------------------------------------
class MetaScreen: public GameScreen
{
public:
	static constexpr auto kName = "Meta";

	String GetName() const override;

	// Returns screen root actor, valid while loaded
	const Ref<Actor>& GetRoot() const;

	// Shows the settings window with the current sound state
	static void OpenSettings();

protected:
	void OnLoad() override;
	void OnUnload() override;
	void OnActivated() override;
	void OnDeactivated() override;

private:
	Ref<Actor> mRoot;

private:
	void BuildLivesPanel(const Ref<Widget>& ui);
	void BuildCoinsPanel(const Ref<Widget>& ui);
	void BuildButtons(const Ref<Widget>& ui);
};
