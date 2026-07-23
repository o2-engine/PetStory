#include "o2/stdafx.h"
#include "Screens/ScreenManager.h"

#include "o2/Utils/Debug/Debug.h"

ScreenManager* ScreenManager::sInstance = nullptr;

ScreenManager::ScreenManager()
{
	if (!sInstance)
		sInstance = this;
}

ScreenManager::~ScreenManager()
{
	Clear();

	if (sInstance == this)
		sInstance = nullptr;
}

ScreenManager* ScreenManager::Instance()
{
	return sInstance;
}

void ScreenManager::AddScreen(const Ref<GameScreen>& screen)
{
	if (GetScreen(screen->GetName()))
	{
		o2Debug.LogError("ScreenManager: screen '" + screen->GetName() + "' is already registered");
		return;
	}

	mScreens.Add(screen);
}

Ref<GameScreen> ScreenManager::GetScreen(const String& name) const
{
	for (auto& screen : mScreens)
	{
		if (screen->GetName() == name)
			return screen;
	}

	return nullptr;
}

const Ref<GameScreen>& ScreenManager::GetCurrentScreen() const
{
	return mCurrentScreen;
}

void ScreenManager::ShowScreen(const String& name)
{
	auto screen = GetScreen(name);
	if (!screen)
	{
		o2Debug.LogError("ScreenManager: unknown screen '" + name + "'");
		return;
	}

	if (screen == mCurrentScreen && !mPendingScreen)
		return;

	mPendingScreen = screen;
}

void ScreenManager::Update(float dt)
{
	if (mPendingScreen)
	{
		auto next = mPendingScreen;
		mPendingScreen = nullptr;

		if (mCurrentScreen)
			mCurrentScreen->Unload();

		mCurrentScreen = next;
		mCurrentScreen->Activate();
	}

	if (mCurrentScreen)
		mCurrentScreen->Update(dt);
}

void ScreenManager::Clear()
{
	if (mCurrentScreen)
		mCurrentScreen->Unload();

	mCurrentScreen = nullptr;
	mPendingScreen = nullptr;
	mScreens.Clear();
}
