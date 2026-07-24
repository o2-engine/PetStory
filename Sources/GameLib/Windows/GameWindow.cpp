#include "o2/stdafx.h"
#include "GameLib/Windows/GameWindow.h"

#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/Widget.h"

#if IS_SCRIPTING_SUPPORTED
#include "o2/Scene/Components/ScriptableComponent.h"
#endif

GameWindow::GameWindow(const String& name, const String& prototypePath):
	mName(name), mPrototypePath(prototypePath)
{}

const String& GameWindow::GetName() const
{
	return mName;
}

const String& GameWindow::GetPrototypePath() const
{
	return mPrototypePath;
}

bool GameWindow::IsLoaded() const
{
	return mRoot != nullptr;
}

bool GameWindow::IsShown() const
{
	return mShown;
}

void GameWindow::Load()
{
	if (mRoot)
		return;

	// Prototypes carry images and fonts, so they are only instantiated with
	// the render device; headless tests get a stub root with the same name
	if (Render::IsSingletonInitialzed() && !mPrototypePath.IsEmpty())
	{
		AssetRef<ActorAsset> proto(mPrototypePath);
		if (proto)
		{
			mRoot = proto->Instantiate();
			mRoot->AddToScene();
		}
	}

	if (!mRoot)
	{
		mRoot = mmake<Actor>(ActorCreateMode::InScene);
		mRoot->SetName(mName);
	}

	// Windows draw over every screen widget regardless of creation order
	if (auto widget = DynamicCast<Widget>(mRoot))
		widget->SetDrawingDepth(kDrawDepth);

	mRoot->SetEnabled(false);

#if IS_SCRIPTING_SUPPORTED
	mScript = mRoot->GetComponent<ScriptableComponent>();
	if (mScript)
	{
		auto instance = mScript->GetInstance();
		if (instance.IsObject())
		{
			WeakRef<GameWindow> weakThis(this);
			Function<void(const String&)> emit = [weakThis](const String& actionId) {
				if (auto window = weakThis.Lock())
					window->EmitAction(actionId);
			};

			ScriptValue emitValue;
			emitValue.SetValue(emit);
			instance.SetProperty("action", emitValue);
		}
	}
#endif

	OnLoaded();
}

void GameWindow::Unload()
{
	if (!mRoot)
		return;

	Hide();

	o2Scene.DestroyActor(mRoot);
	mRoot = nullptr;

#if IS_SCRIPTING_SUPPORTED
	mScript = nullptr;
#endif
}

void GameWindow::Show()
{
	Load();

	if (mShown)
		return;

	mRoot->SetEnabled(true);
	mShown = true;

	OnShown();
}

void GameWindow::Hide()
{
	if (!mShown)
		return;

	if (mRoot)
		mRoot->SetEnabled(false);

	mShown = false;

	OnHidden();
}

const Ref<Actor>& GameWindow::GetRoot() const
{
	return mRoot;
}

void GameWindow::EmitAction(const String& actionId)
{
	onAction(actionId);
}

void GameWindow::OnLoaded()
{}

void GameWindow::OnShown()
{}

void GameWindow::OnHidden()
{}

#if IS_SCRIPTING_SUPPORTED
namespace
{
	template<typename T>
	void SetInstanceProperty(const Ref<ScriptableComponent>& script, const String& name, const T& value)
	{
		if (!script)
			return;

		auto instance = script->GetInstance();
		if (!instance.IsObject())
			return;

		ScriptValue propertyValue;
		propertyValue.SetValue(value);
		instance.SetProperty(name.Data(), propertyValue);
	}
}
#endif

void GameWindow::SetScriptProperty(const String& name, int value)
{
#if IS_SCRIPTING_SUPPORTED
	SetInstanceProperty(mScript, name, value);
#endif
}

void GameWindow::SetScriptProperty(const String& name, bool value)
{
#if IS_SCRIPTING_SUPPORTED
	SetInstanceProperty(mScript, name, value);
#endif
}

void GameWindow::SetScriptProperty(const String& name, const String& value)
{
#if IS_SCRIPTING_SUPPORTED
	SetInstanceProperty(mScript, name, value);
#endif
}
