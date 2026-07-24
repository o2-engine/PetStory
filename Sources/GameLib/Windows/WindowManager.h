#pragma once

#include "GameLib/Windows/GameWindow.h"
#include "o2/Utils/Types/Containers/Vector.h"

using namespace o2;

// ------------------------------------------------------------------
// Keeps the registered popup windows and shows them by name over the
// current screen. Windows stay loaded between Show/Hide; screens
// unload them on switch. The instance is globally reachable while it
// exists, like ScreenManager.
// ------------------------------------------------------------------
class WindowManager: public RefCounterable
{
public:
	WindowManager();
	~WindowManager() override;

	// Returns the current global instance, nullptr when none exists
	static WindowManager* Instance();

	// Registers a window; name must be unique
	void AddWindow(const Ref<GameWindow>& window);

	// Returns registered window by name or nullptr
	Ref<GameWindow> GetWindow(const String& name) const;

	// Shows the window by name and returns it for callback wiring
	Ref<GameWindow> ShowWindow(const String& name);

	// Hides the window by name, keeping it loaded
	void HideWindow(const String& name);

	// Hides every shown window
	void HideAll();

	// Unloads every window; called by screens on switch
	void UnloadAll();

	// Unloads and forgets all windows
	void Clear();

private:
	Vector<Ref<GameWindow>> mWindows;

	static WindowManager* sInstance;
};
