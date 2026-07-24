#pragma once

#include "o2/Scene/Actor.h"
#include "o2/Utils/Function/Function.h"

using namespace o2;

#if IS_SCRIPTING_SUPPORTED
namespace o2 { class ScriptableComponent; }
#endif

// ------------------------------------------------------------------
// Popup window: owns an actor instantiated from the prototype
// (prefab) specified at construction. Load/Unload manage the actor
// lifetime, Show/Hide toggle visibility keeping the window loaded.
// UI logic lives in the JS script component inside the prototype;
// the script reports button presses through the injected `action`
// callback, surfaced here as onAction.
// ------------------------------------------------------------------
class GameWindow: public RefCounterable
{
public:
	static constexpr float kDrawDepth = 100.0f; // Windows draw above the screen content

	Function<void(const String&)> onAction; // Called with the action id sent by the window script

public:
	GameWindow(const String& name, const String& prototypePath);

	const String& GetName() const;
	const String& GetPrototypePath() const;

	bool IsLoaded() const;
	bool IsShown() const;

	// Instantiates the prototype into the scene, disabled. Without the render
	// device (headless tests) or prototype the root is a plain stub actor
	void Load();

	// Destroys the window actor
	void Unload();

	// Loads if needed and enables the window
	void Show();

	// Disables the window, keeps it loaded
	void Hide();

	// Returns window root actor, valid while loaded
	const Ref<Actor>& GetRoot() const;

	// Fires onAction; the window script calls this through the injected callback
	void EmitAction(const String& actionId);

	// Sets a property on the window script instance; no-op without scripting
	void SetScriptProperty(const String& name, int value);
	void SetScriptProperty(const String& name, bool value);
	void SetScriptProperty(const String& name, const String& value);

protected:
	// Called after the prototype is instantiated
	virtual void OnLoaded();

	virtual void OnShown();
	virtual void OnHidden();

protected:
	String mName;
	String mPrototypePath;

	Ref<Actor> mRoot;
	bool       mShown = false;

#if IS_SCRIPTING_SUPPORTED
	Ref<ScriptableComponent> mScript;
#endif
};
