#pragma once

#include "GameLib/Screens/ScreenManager.h"
#include "GameLib/Windows/WindowManager.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Editor/Attributes/EditorPropertyAttribute.h"

using namespace o2;

// ------------------------------------------------------------------
// Game entry point component: lives on an actor in the boot scene
// (Boot.scn). When the scene starts playing - in the game or in the
// editor play mode - it loads the level chain, creates the screen
// manager with the meta and gameplay screens, the window manager
// with the popup windows and shows the start screen. The scene
// update drives the manager, so the game runs wherever the scene
// runs. Removal from scene tears everything down.
// ------------------------------------------------------------------
class GameBootstrapComponent: public Component
{
public:
	// Returns the screen manager created by this bootstrap, valid while on scene
	const Ref<ScreenManager>& GetScreens() const;

	// Returns the window manager created by this bootstrap, valid while on scene
	const Ref<WindowManager>& GetWindows() const;

	SERIALIZABLE(GameBootstrapComponent);
	CLONEABLE_REF(GameBootstrapComponent);

private:
	String mChainPath = String("Levels/Chain.json");         // @SERIALIZABLE @EDITOR_PROPERTY
	String mStartScreen = String("Meta");                    // @SERIALIZABLE @EDITOR_PROPERTY
	String mLanguagePath = String("Localization/ru.json");   // @SERIALIZABLE @EDITOR_PROPERTY

	Ref<ScreenManager> mScreens;
	Ref<WindowManager> mWindows;

private:
	void OnStart() override;
	void OnUpdate(float dt) override;
	void OnRemoveFromScene() override;
};
// --- META ---

CLASS_BASES_META(GameBootstrapComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(GameBootstrapComponent)
{
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(String("Levels/Chain.json")).NAME(mChainPath);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(String("Meta")).NAME(mStartScreen);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(String("Localization/ru.json")).NAME(mLanguagePath);
    FIELD().PRIVATE().NAME(mScreens);
    FIELD().PRIVATE().NAME(mWindows);
}
END_META;
CLASS_METHODS_META(GameBootstrapComponent)
{

    FUNCTION().PUBLIC().SIGNATURE(const Ref<ScreenManager>&, GetScreens);
    FUNCTION().PUBLIC().SIGNATURE(const Ref<WindowManager>&, GetWindows);
    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, OnRemoveFromScene);
}
END_META;
// --- END META ---
