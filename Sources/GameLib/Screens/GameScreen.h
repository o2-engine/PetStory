#pragma once

#include "o2/Utils/Types/Ref.h"
#include "o2/Utils/Types/String.h"

using namespace o2;

// ------------------------------------------------------------------
// Base game screen: owns its scene content and resources. The screen
// manager drives the lifecycle: Load -> Activate -> ... -> Deactivate
// -> Unload. Load builds actors and loads resources, Unload destroys
// them; Activate/Deactivate toggle the loaded content on and off.
// ------------------------------------------------------------------
class GameScreen: public RefCounterable
{
public:
	virtual ~GameScreen();

	// Returns unique screen name used by the manager
	virtual String GetName() const = 0;

	// Loads resources and builds scene content
	void Load();

	// Destroys scene content and releases resources
	void Unload();

	// Makes loaded content active (visible, updating)
	void Activate();

	// Deactivates content before switch or unload
	void Deactivate();

	// Updates active screen
	void Update(float dt);

	bool IsLoaded() const;
	bool IsActive() const;

protected:
	virtual void OnLoad();
	virtual void OnUnload();
	virtual void OnActivated();
	virtual void OnDeactivated();
	virtual void OnUpdate(float dt);

private:
	bool mLoaded = false;
	bool mActive = false;
};
