#pragma once

#include "Screens/GameScreen.h"
#include "o2/Utils/Types/Containers/Vector.h"

using namespace o2;

// ------------------------------------------------------------------
// Simple screen manager: keeps registered screens, one screen is
// current. ShowScreen() is deferred to the next Update() so a switch
// requested from UI callbacks doesn't destroy the requesting widget
// mid-event. Switch order: old Deactivate + Unload, new Load +
// Activate. The instance is globally reachable while it exists.
// ------------------------------------------------------------------
class ScreenManager: public RefCounterable
{
public:
	ScreenManager();
	~ScreenManager() override;

	// Returns the current global instance, nullptr when none exists
	static ScreenManager* Instance();

	// Registers a screen; name must be unique
	void AddScreen(const Ref<GameScreen>& screen);

	// Returns registered screen by name or nullptr
	Ref<GameScreen> GetScreen(const String& name) const;

	// Returns currently shown screen, nullptr before the first switch
	const Ref<GameScreen>& GetCurrentScreen() const;

	// Requests switch to the screen; applied on the next Update
	void ShowScreen(const String& name);

	// Applies pending switch and updates the current screen
	void Update(float dt);

	// Deactivates and unloads the current screen, forgets all screens
	void Clear();

private:
	Vector<Ref<GameScreen>> mScreens;
	Ref<GameScreen>         mCurrentScreen;
	Ref<GameScreen>         mPendingScreen;

	static ScreenManager* sInstance;
};
